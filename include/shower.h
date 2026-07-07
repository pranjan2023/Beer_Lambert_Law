#pragma once
#include "thread_pool.h"
#include <mutex>
#include <atomic>

void simulate_particle(double energy, double position, unsigned seed,
                        double length, double mu, double omega, double energy_cutoff,
                        ThreadPool& pool, double& total_energy_out, std::mutex& result_mutex,
                        std::atomic<int>& transmitted_count, std::atomic<int>& absorbed_count,
                        std::atomic<int>& scattered_count, std::atomic<int>& split_count);