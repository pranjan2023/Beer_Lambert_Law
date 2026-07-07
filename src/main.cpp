#include "simulation.h"
#include "thread_pool.h"
#include "shower.h"

#include <mutex>
#include <vector>
#include <iostream>
#include <cmath>
#include <atomic>

// ---------- Part A: multithreaded slab transport ----------
void run_part_a()
{
    // Config
    double l = 0.5;
    double omega = 0.5;
    double mu = 1.0;
    unsigned seed = 42;
    int num_threads = 5;
    int total_photons = 5000000;
    int photons_per_thread = total_photons / num_threads;

    std::vector<double> partial_results(num_threads, 0.0);

    {
        ThreadPool pool(num_threads);
        for (int t = 0; t < num_threads; ++t) {
            pool.submit([&partial_results, t, photons_per_thread, l, omega, mu, seed]() {
                partial_results[t] = run_simulation(l, omega, mu, photons_per_thread, seed + t*100);
            });
        }
    } // pool destructor joins all threads here

    double total = 0.0;
    for (double frac : partial_results) total += frac;
    double ans = total / num_threads;

    std::cout << "--- Part A: Slab Transport ---\n";
    std::cout << "Transmitted fraction: " << ans << std::endl;
    std::cout << "Formula: " << exp(-mu*l*(1-omega)) << std::endl;
}

// ---------- Part B: branching shower ----------
void run_part_b()
{
    // Config
    double length = 100.0;
    double mu = 1.0;
    double omega = 0.5;
    double energy_cutoff = 1.0;
    double initial_energy = 100000.0;
    unsigned root_seed = 999;
    int num_threads = 4;

    double total_energy_out = 0.0;
    std::mutex result_mutex;

    std::atomic<int> transmitted_count{0};
    std::atomic<int> absorbed_count{0};
    std::atomic<int> scattered_count{0};
    std::atomic<int> split_count{0};

    ThreadPool shower_pool(num_threads);
    shower_pool.submit([&]()
    {
        simulate_particle(initial_energy, 0.0, root_seed, length, mu, omega, energy_cutoff,
                           shower_pool, total_energy_out, result_mutex,
                           transmitted_count, absorbed_count, scattered_count, split_count);
    });
    shower_pool.wait_all();

    std::cout << "--- Part B: Branching Shower ---\n";
    std::cout << "Initial energy: " << initial_energy << std::endl;
    std::cout << "Total energy out: " << total_energy_out << std::endl;
    std::cout << "Transmitted: " << transmitted_count << std::endl;
    std::cout << "Absorbed: " << absorbed_count << std::endl;
    std::cout << "Scattered: " << scattered_count << std::endl;
    std::cout << "Split: " << split_count << std::endl;
}

int main()
{
    //run_part_a();
    std::cout << std::endl;
    run_part_b();
}