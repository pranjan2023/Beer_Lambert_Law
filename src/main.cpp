#include "simulation.h"
#include <iostream>
#include <cmath>

using namespace std;

int main()
{
    double l = 0.5;
    double omega = 0.5;
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 42;

    cout << "Transmitted fraction: " << run_simulation(l, omega, mu, N, seed) << endl;
    cout << "Formula: " << exp(-mu*l*(1-omega)) << endl;
}