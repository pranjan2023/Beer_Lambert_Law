#include <random>
#include <cmath>
#include <iostream>
using namespace std;


double run_simulation(double length,double omega,double mu,int N,unsigned seed)
{
    mt19937 engine(seed);
    
    exponential_distribution<double> dist(mu);
    uniform_real_distribution<double> uniform(0.0,1.0);

    double transmitted=0;

    bool alive=true;
    double position=0;
    for(int i=0;i<N;i++)
    {
        position=0;
        alive=true;
        while(alive)
        {
            position+=dist(engine);
            if(position>length)
            {
                transmitted+=1;
                alive=false;
            }
            else
            {
                if(uniform(engine)>=omega)
                {
                    alive=false;
                }
            }
        }
    }

    return transmitted/N;
}