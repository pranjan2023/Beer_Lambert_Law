#include "shower.h"
#include <random>
#include <atomic>

using namespace std;


void simulate_particle(double energy, double position, unsigned seed,
                        double length, double mu, double omega, double energy_cutoff,
                        ThreadPool& pool, double& total_energy_out, std::mutex& result_mutex,
                        std::atomic<int>& transmitted_count, std::atomic<int>& absorbed_count,
                        std::atomic<int>& scattered_count, std::atomic<int>& split_count)
{
    mt19937 engine(seed);
    exponential_distribution<double> dist(mu);
    uniform_real_distribution<double> uniform(0.0, 1.0);

    position += dist(engine);
    if (position >= length)
    {
        std::lock_guard<std::mutex> lock(result_mutex);

        transmitted_count+=1;

        total_energy_out += energy;
        return;
    }
    else
    {
        if (energy > energy_cutoff)
        {   
            
            split_count+=1;

            pool.submit([energy, position, seed, length, mu, omega, energy_cutoff, &pool, &total_energy_out, &result_mutex,&transmitted_count, &absorbed_count, &scattered_count, &split_count]()
            {
                simulate_particle(energy/2, position, seed + 1, length, mu, omega, energy_cutoff, pool, total_energy_out, result_mutex ,transmitted_count,absorbed_count,scattered_count,split_count);
            });
            
            pool.submit([energy, position, seed, length, mu, omega, energy_cutoff, &pool, &total_energy_out, &result_mutex,&transmitted_count, &absorbed_count, &scattered_count, &split_count]()
            {
                simulate_particle(energy/2, position, seed + 2, length, mu, omega, energy_cutoff, pool, total_energy_out, result_mutex ,transmitted_count,absorbed_count,scattered_count,split_count);
            });
        }
        else
        {
            if (uniform(engine) >= omega)
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                absorbed_count+=1;
                total_energy_out += energy;
                return;
            }
            else
            {
                scattered_count+=1;

                pool.submit([energy, position, seed, length, mu, omega, energy_cutoff, &pool, &total_energy_out, &result_mutex,&transmitted_count, &absorbed_count, &scattered_count, &split_count]()
                {
                    simulate_particle(energy, position, seed + 3, length, mu, omega, energy_cutoff, pool, total_energy_out, result_mutex,transmitted_count,absorbed_count,scattered_count,split_count);
                });
            }
        }
    }
}


