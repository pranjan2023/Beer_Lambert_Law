#include <gtest/gtest.h>
#include "simulation.h"
#include "thread_pool.h"
#include <cmath>
#include <vector>

TEST(BeerLambertTest, MatchesAnalyticalFormula) {
    double length = 0.5;
    double omega = 0.5;
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 42;

    double simulated = run_simulation(length, omega, mu, N, seed);
    double expected = std::exp(-mu * length * (1 - omega));

    EXPECT_NEAR(simulated, expected, 0.01);
}

TEST(BeerLambertTest, PureAbsorptionMatchesBeerLambert) {
    double length = 0.5;
    double omega = 0.0;
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 123;

    double simulated = run_simulation(length, omega, mu, N, seed);
    double expected = std::exp(-mu * length);

    EXPECT_NEAR(simulated, expected, 0.01);
}

TEST(BeerLambertTest, PureScatteringMatchesBeerLambert) {
    double length = 0.5;
    double omega = 1.0;
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 123;

    double simulated = run_simulation(length, omega, mu, N, seed);
    double expected = std::exp(0);

    EXPECT_NEAR(simulated, expected, 0.01);
}

TEST(ThreadPoolTest, ParallelResultMatchesAnalyticalFormula) {
    double length = 0.5;
    double omega = 0.5;
    double mu = 1.0;
    unsigned base_seed = 42;
    int num_threads = 4;
    int total_photons = 1000000;
    int photons_per_thread = total_photons / num_threads;

    std::vector<double> partial_results(num_threads, 0.0);

    {
        ThreadPool pool(num_threads);
        for (int t = 0; t < num_threads; ++t) {
            pool.submit([&partial_results, t, photons_per_thread, length, omega, mu, base_seed]() {
                partial_results[t] = run_simulation(length, omega, mu, photons_per_thread, base_seed + t * 100);
            });
        }
    } // pool destructor joins all threads here

    double total = 0.0;
    for (double frac : partial_results) total += frac;
    double simulated = total / num_threads;

    double expected = std::exp(-mu * length * (1 - omega));
    EXPECT_NEAR(simulated, expected, 0.01);
}




#include "shower.h"
#include <atomic>

TEST(ShowerTest, EnergyConservedThinSlabFewSplits) {
    double length = 0.5;
    double mu = 1.0;
    double omega = 0.5;
    double energy_cutoff = 1.0;
    double initial_energy = 100.0;
    unsigned root_seed = 999;
    int num_threads = 4;

    double total_energy_out = 0.0;
    std::mutex result_mutex;
    std::atomic<int> transmitted_count{0};
    std::atomic<int> absorbed_count{0};
    std::atomic<int> scattered_count{0};
    std::atomic<int> split_count{0};

    ThreadPool pool(num_threads);
    pool.submit([&]() {
        simulate_particle(initial_energy, 0.0, root_seed, length, mu, omega, energy_cutoff,
                           pool, total_energy_out, result_mutex,
                           transmitted_count, absorbed_count, scattered_count, split_count);
    });
    pool.wait_all();

    EXPECT_NEAR(total_energy_out, initial_energy, 1e-6);
}

TEST(ShowerTest, EnergyConservedThickSlabManySplits) {
    double length = 5.0;
    double mu = 1.0;
    double omega = 0.5;
    double energy_cutoff = 1.0;
    double initial_energy = 100.0;
    unsigned root_seed = 999;
    int num_threads = 4;

    double total_energy_out = 0.0;
    std::mutex result_mutex;
    std::atomic<int> transmitted_count{0};
    std::atomic<int> absorbed_count{0};
    std::atomic<int> scattered_count{0};
    std::atomic<int> split_count{0};

    ThreadPool pool(num_threads);
    pool.submit([&]() {
        simulate_particle(initial_energy, 0.0, root_seed, length, mu, omega, energy_cutoff,
                           pool, total_energy_out, result_mutex,
                           transmitted_count, absorbed_count, scattered_count, split_count);
    });
    pool.wait_all();

    EXPECT_NEAR(total_energy_out, initial_energy, 1e-6);

    // Sanity check on bookkeeping: every particle ever created either
    // terminated (transmitted/absorbed) or is accounted for by splitting.
    int total_particles_created = 1 + 2 * split_count.load();
    int total_terminated = transmitted_count.load() + absorbed_count.load();
    EXPECT_LE(total_terminated, total_particles_created);
}