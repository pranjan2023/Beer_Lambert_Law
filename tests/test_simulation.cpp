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