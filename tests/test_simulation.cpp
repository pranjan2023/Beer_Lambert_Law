#include <gtest/gtest.h>
#include "simulation.h"
#include <cmath>

TEST(BeerLambertTest, MatchesAnalyticalFormula) 
{
    double length = 0.5;
    double omega = 0.5;
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 42;

    double simulated = run_simulation(length, omega, mu, N, seed);
    double expected = std::exp(-mu * length * (1 - omega));

    EXPECT_NEAR(simulated, expected, 0.01);
}
TEST(BeerLambertTest, PureAbsorptionMatchesBeerLambert) 
{
    double length = 0.5;
    double omega = 0.0;   // no scattering at all
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 123;

    double simulated = run_simulation(length, omega, mu, N, seed);
    double expected = std::exp(-mu * length);

    EXPECT_NEAR(simulated, expected, 0.01);
}
TEST(BeerLambertTest, PureScatteringMatchesBeerLambert) 
{
    double length = 0.5;
    double omega = 1.0;   // no absorbtion all scattered
    double mu = 1.0;
    int N = 1000000;
    unsigned seed = 123;

    double simulated = run_simulation(length, omega, mu, N, seed);
    double expected = std::exp(0);

    EXPECT_NEAR(simulated, expected, 0.01);
}

