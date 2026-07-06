#include "simulation.h"
#include "thread_pool.h"
#include <vector>
#include <iostream>
#include <cmath>

int main()
{
    double l = 0.5;
    double omega = 0.5;
    double mu = 1.0;
    unsigned seed = 42;
    int num_threads = 1;
    int total_photons = 500000000;
    int photons_per_thread = total_photons / num_threads;

    std::vector<double> partial_results(num_threads, 0.0);

    {
        ThreadPool pool(num_threads);
        for (int t = 0; t < num_threads; ++t) {
            pool.submit([&partial_results, t, photons_per_thread, l, omega, mu, seed]() {
                partial_results[t] = run_simulation(l, omega, mu, photons_per_thread, seed + t*100);
            });
        }
    } // pool destructor joins all threads here — safe to read partial_results after this point

    double total = 0.0;
    for (double frac : partial_results) {
        total += frac;
    }
    double ans = total / num_threads;

    std::cout << "Transmitted fraction: " << ans << std::endl;
    std::cout << "Formula: " << exp(-mu*l*(1-omega)) << std::endl;
}