# Monte Carlo Photon Transport with a Hand-Built Thread Pool

A multithreaded Monte Carlo simulation of 1D photon transport through a slab, validated against the analytical Beer-Lambert law, built as a C++20 concurrency and software-engineering exercise ahead of a CERN openlab / HSF-aligned portfolio.

**This is a concurrency and correctness exercise.** Coefficients are toy parameters chosen for clean testability, not real material data.

## What this simulates

A photon enters a homogeneous slab of thickness `L`. Repeatedly:
1. Sample a flight distance from an exponential distribution with rate `mu` (interaction rate per unit length).
2. If the accumulated position exceeds `L` → the photon **transmits**.
3. Otherwise, resolve an interaction: **scatter** (continue, with probability `omega`) or **absorb** (terminate, with probability `1 - omega`).

For a fixed geometry, the transmitted fraction has a known closed form:
$ T = e^{-\mu L (1 - \omega)}  $

This is used as the correctness oracle throughout — every simulated result is checked against it.

## Architecture

- `src/simulation.cpp` / `include/simulation.h` — single-photon transport logic (`run_simulation`), independent of any threading concerns.
- `src/thread_pool.cpp` / `include/thread_pool.h` — a general-purpose `ThreadPool`: a fixed set of `std::thread` workers pulling `std::function<void()>` jobs from a shared queue, synchronized with `std::mutex` + `std::condition_variable`. The pool has no knowledge of photons or physics — it runs arbitrary jobs, which keeps it reusable for the branching shower extension (Part B).
- `src/main.cpp` — splits a total photon count across `N` threads (one job per thread, disjoint result slots, no locking needed on the result vector), runs them through the pool, and combines the partial results.
- `tests/test_simulation.cpp` — GoogleTest suite covering both the single-threaded path (normal case, pure absorption, pure scattering) and the multithreaded path, all checked against the analytical formula.

## Correctness validation

Every test compares simulated output against the closed-form Beer-Lambert prediction, at `N = 1,000,000` samples:

| Case | omega | Simulated | Formula | 
|---|---|---|---|
| Partial scattering | 0.5 | 0.778579 | 0.778801 |
| Pure absorption | 0.0 | ~0.0075 | 0.006738 (at L=5) |
| Pure scattering | 1.0 | ~1.0 | 1.0 |
| Multithreaded (4 threads) | 0.5 | 0.778785 | 0.778801 |

All four are encoded as automated GoogleTest cases, run on every push via CI.

## Concurrency design notes

- **Per-thread independent RNG streams.** Each thread seeds its own `std::mt19937` — sharing one engine across threads would be a data race and would also corrupt statistical independence between photon histories.
- **Lock-free result aggregation.** Rather than a shared counter behind a mutex, each thread writes to its own exclusive slot in a `std::vector`, combined only after all threads have joined. No contention during the actual simulation work.
- **RAII-style shutdown.** The `ThreadPool` destructor sets a stop flag, wakes all workers via `notify_all()`, and joins every thread.

## Benchmark: thread count vs wall-clock time

Measured on an Apple M3 Pro, 50,000,000 photons total,
 `omega = 0.5`, `L = 0.5`:

| Threads | Wall-clock time | Speedup vs 1 thread |
|---|---|---|
| 1 | 37.0 s | 1.0x |
| 4 | 13.3 s | 2.78x |
| 8 | 10.9 s | 3.39x |

**Observation:** speedup is sublinear and shows clearly diminishing returns — doubling thread count from 4→8 only yields a ~1.2x additional speedup, versus the ~2.78x gained going from 1→4. 
This is consistent with Amdahl's Law: as thread count approaches the machine's physical core count, contention for shared resources (CPU cores, memory bandwidth) and thread-management overhead increasingly offset the benefit of additional parallelism.
 Profiling with Xcode Instruments (Time Profiler) confirmed near-perfectly balanced load across worker threads in both the 4- and 8-thread configurations (per-thread CPU time within ~1.5s of each other at 4 threads, within ~0.5s at 8 threads) — the sublinear scaling is a hardware/contention effect, not a load-imbalance bug in the thread pool.

## Build & test

```bash
mkdir build && cd build
cmake ..
cmake --build .
./mc_transport      # run the simulation
./run_tests         # run the GoogleTest suite
```

## CI

GitHub Actions builds and tests on every push across:
- Ubuntu (gcc, clang)
- macOS 14 (clang, arm64 — native to Apple Silicon, exercising `-march=native` on the same architecture used for development)

## Roadmap

- [x] Single-threaded validated baseline
- [x] CMake + GoogleTest + CI (gcc/clang × x86/arm64)
- [x] Hand-built `ThreadPool` (`std::mutex` + `std::condition_variable`)
- [x] Multithreaded transport, verified correct and profiled
- [ ] Part B: branching shower toy (dynamic task generation — a photon's interaction can spawn child photons, testing the pool's queue under unpredictable, growing workload)
- [ ] ThreadSanitizer CI job
- [ ] DSA-STL reps: priority-queue event scheduling, hash-map vs tree-map cross-section lookup benchmark