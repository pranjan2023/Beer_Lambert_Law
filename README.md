# Monte Carlo Photon Transport with a Hand-Built Thread Pool

A multithreaded Monte Carlo simulation of particle transport, built as a C++20 concurrency and software-engineering exercise ahead of a CERN openlab / HSF-aligned portfolio. Two parts:

- **Part A** — 1D slab photon transport, validated against the analytical Beer-Lambert law, parallelized with a fixed, upfront split of work across threads.
- **Part B** — a branching electromagnetic shower toy, where particles can dynamically spawn children mid-simulation, stressing the thread pool's task queue under an unpredictable, growing workload.

**This is a concurrency and correctness exercise wearing HEP clothing — not a physics result.** Coefficients are toy parameters chosen for clean testability, not real material data.

## Part A: Slab Transport

A photon enters a homogeneous slab of thickness `L`. Repeatedly:
1. Sample a flight distance from an exponential distribution with rate `mu`.
2. If the accumulated position exceeds `L` → the photon **transmits**.
3. Otherwise: **scatter** (continue, probability `omega`) or **absorb** (terminate, probability `1-omega`).

Closed-form correctness oracle: `T = e^(-mu * L * (1 - omega))`.

## Part B: Branching Shower Toy

Loosely modeled on electromagnetic shower development (bremsstrahlung/pair production cascades in real calorimeters, where an electron radiates a photon, that photon pair-produces into an electron-positron pair, and so on, until individual particle energies drop low enough that ionization losses dominate and multiplication stops). The toy collapses this into one generic rule:

- A particle undergoes transport identically to Part A.
- If it interacts and its energy is **above** `energy_cutoff`: it **splits** into two children, each with half the parent's energy, at the same position. Each child is submitted as a new, independent job to the pool.
- If it interacts and its energy is **at or below** `energy_cutoff`: it behaves exactly like Part A — scatter (continue as a new job from the current position) or absorb (terminate).

**Non-goal, stated explicitly:** real showers involve two distinct particle species (electron/positron vs. photon) with different physical processes and a hard pair-production energy threshold (~1.022 MeV, twice the electron rest mass). This toy uses one generic particle type and one generic branching rule — a deliberate simplification, not an oversight.

### Why this needs more than a fixed thread split

Part A's total workload (N independent photons) is known before the simulation starts, so it can be split evenly across threads upfront. Part B's total workload is **emergent** — a single root particle can recursively spawn an unknown number of descendants, discovered only as the simulation runs. This requires:

- **Jobs that submit new jobs from inside themselves.** Safe here specifically because `ThreadPool::worker_loop` releases its queue lock *before* executing a job's body — a job (running on a worker thread) can freely call `submit()` again without any risk of a thread deadlocking on a lock it already holds.
- **An `active_jobs` counter and a second condition variable (`wait_all()`).** Incremented on every submission, decremented on every job completion; `main()` blocks on `wait_all()` until the count reaches zero — i.e., until every particle in the whole recursive tree, however deep, has fully terminated. A naive "is the queue empty" check is insufficient: the queue can be momentarily empty between a job finishing and its children being submitted.

### Correctness: energy conservation

There's no closed-form transmitted-fraction formula once branching is involved, so the test oracle instead is a physical invariant: **total energy summed across every eventual descendant of a particle must equal that particle's original energy**, since splits divide energy exactly and never lose or create it.

Validated across shower configurations of very different shapes:

| Configuration | Initial energy | Total energy out | Splits | Transmitted | Absorbed | Scattered |
|---|---|---|---|---|---|---|
| Thin slab (L=0.5), low branching | 100 | 100 (exact) | 4 | 5 | 0 | 0 |
| Thick slab (L=5.0), high branching | 100 | 100 (exact) | 71 | 58 | 14 | 36 |
| Deep shower (stress test) | 1,000,000 | 1,000,000 (exact) | 1,048,575 | 0 | 1,048,576 | 591,641 |
| Deep shower (stress test) | 10,000,000 | 10,000,000 (exact) | 16,777,215 | 0 | 16,777,216 | 13,657,932 |

At `initial_energy = 1,000,000`, `Split = 2^20 - 1` and `Absorbed = 2^20` exactly — a complete binary branching tree, 20 levels deep, with over a million termination events and zero energy lost or double-counted.

## Architecture

- `src/simulation.cpp` / `include/simulation.h` — Part A's single-photon transport logic, independent of threading.
- `src/thread_pool.cpp` / `include/thread_pool.h` — general-purpose `ThreadPool`: fixed worker threads pulling `std::function<void()>` jobs from a shared queue, synchronized with `std::mutex` + `std::condition_variable`, plus an `active_jobs` counter and `wait_all()` for dynamic, recursively-generated workloads. The pool has no knowledge of photons or physics.
- `src/shower.cpp` / `include/shower.h` — Part B's recursive `simulate_particle`, using the pool's self-submission capability.
- `src/main.cpp` — `run_part_a()` and `run_part_b()`, each with its own independent configuration.
- `tests/test_simulation.cpp` — GoogleTest suite: single-threaded and multithreaded Part A correctness, plus energy-conservation invariants for Part B across multiple shower shapes.

## Concurrency design notes

- **Per-thread/per-particle independent RNG streams.** Every job seeds its own `std::mt19937` — sharing an engine across threads is a data race and destroys statistical independence.
- **Part A: lock-free result aggregation.** Each thread writes to its own exclusive slot in a result vector — no contention during simulation, combined only after all threads join.
- **Part B: shared accumulator, real mutex contention.** Since job count is unknown upfront, results accumulate into one shared total under a mutex, and event counts use `std::atomic<int>` (cheaper than a mutex for simple increments).
- **RAII-style shutdown.** The `ThreadPool` destructor sets a stop flag, wakes all workers, and joins every thread.

## Benchmark & profiling

### Part A: thread count vs wall-clock time

Measured on an Apple M3 Pro, 1,000,000 photons total, `omega=0.5`, `L=0.5`:

| Threads | Wall-clock time | Speedup vs 1 thread |
|---|---|---|
| 1 | 37.0 s | 1.0x |
| 4 | 13.3 s | 2.78x |
| 8 | 10.9 s | 3.39x |

Sublinear, diminishing-returns scaling consistent with Amdahl's Law as thread count approaches the machine's physical core count. Xcode Instruments (Time Profiler) confirmed near-perfectly balanced per-thread CPU time in both configurations — the sublinear scaling is a hardware/contention effect, not a load-imbalance bug.

### Part B: a genuine lock-contention finding

Profiling a deep shower (`initial_energy = 1,000,000`, ~1 million splits, ~2 million termination events) with Xcode Instruments' Time Profiler revealed a very different picture from Part A: **nearly half of total measured thread time (9.09s of 19.50s) was spent inside `psynch_cvwait`/`psynch_cvcontinue`** — the OS primitives underneath `std::condition_variable::wait()` — rather than in `simulate_particle` itself.

**Why:** each split submits two jobs (two lock/unlock/notify cycles), and each job completion updates `active_jobs` under the same lock (another lock/unlock cycle) — several million synchronization operations total, for a workload where each individual job's actual computation (one exponential sample, one comparison) takes microseconds. Once job granularity is this fine, **synchronization overhead dominates actual computation** — a well-known tradeoff in parallel task design (fine-grained parallelism maximizes theoretical concurrency but maximizes synchronization cost; coarse-grained parallelism reduces overhead at the cost of some potential parallelism).

This is the honest "before" half of an optimization story. A natural "after" would introduce a **sequential cutoff** — below some energy or tree-depth threshold, process remaining sub-branches serially within one job rather than submitting new jobs for every split, trading some theoretical parallelism for drastically reduced lock contention (the same technique used in real parallel divide-and-conquer implementations, e.g. parallel quicksort/mergesort). Not yet implemented — noted here as a known, understood limitation rather than an unexamined one.

## Build & test

```bash
mkdir build && cd build
cmake ..
cmake --build .
./mc_transport      # runs both Part A and Part B
./run_tests         # GoogleTest suite (Part A + Part B)
```

## CI

GitHub Actions builds and tests on every push across Ubuntu (gcc, clang) and macOS 14 (clang, arm64).

## Roadmap

- [x] Part A: validated single- and multi-threaded baseline
- [x] CMake + GoogleTest + CI (gcc/clang × x86/arm64)
- [x] Hand-built `ThreadPool` (`std::mutex` + `std::condition_variable`, fixed-split jobs)
- [x] Part B: branching shower toy, dynamic task generation, energy conservation verified at scale
- [x] Profiled and diagnosed a genuine lock-contention bottleneck in Part B
- [ ] Sequential-cutoff optimization for Part B (the "after" half of the optimization story)
- [ ] ThreadSanitizer CI job
- [ ] DSA-STL reps: priority-queue event scheduling, hash-map vs tree-map cross-section lookup benchmark