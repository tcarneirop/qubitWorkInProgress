#ifndef RANDOM_H
#define RANDOM_H


#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <unordered_set>
#include <vector>
#include <omp.h>
#include <limits>



struct VectorHash {
    std::size_t operator()(const std::vector<int>& v) const {
        std::size_t h = 0;

        for (int x : v)
            h ^= std::hash<int>{}(x) + 0x9e3779b9 + (h << 6) + (h >> 2);

        return h;
    }
};

std::vector<int> random_heuristic(
    int *PHYSIC_MACHINE, 
    int *circuit,  
    const int num_gates,
    const long long physic,  
    const long long logic,   
    int *shared_best_depth, 
    int *shared_best_num_gates,
    int *shared_best_mapping, 
    const int NUMBER_OF_SABRE_RUNS, const unsigned long long num_random_sols)
{


    // X solutions, each containing D integers
    std::vector<int> solutions(num_random_sols * logic);
    std::unordered_set<std::vector<int>, VectorHash> generated;

    #pragma omp parallel
    {
        std::mt19937 rng(
            std::random_device{}() +
            omp_get_thread_num()
        );

        #pragma omp for
        for (int i = 0; i < num_random_sols; ++i) {

            std::vector<int> solution;
            bool unique = false;

            while (!unique) {

                solution.resize(physic);
                std::iota(solution.begin(), solution.end(), 0);

                std::shuffle(solution.begin(), solution.end(), rng);

                solution.resize(logic);

                #pragma omp critical
                {
                    unique = generated.insert(solution).second;
                }
            }

            // Copy into the fixed-size pool
            std::copy(
                solution.begin(),
                solution.end(),
                solutions.begin() + i * logic
            );
        }
    }

    // Example: process each solution with OpenMP
    #pragma omp parallel for
    for (int i = 0; i < num_random_sols; ++i) {
        
        std::vector<RoutingResult> results;
        int local_best_depth = INT_MAX;

        int* mapping= solutions.data() + i * logic;



        results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic,logic, 1, mapping, 1 , NUMBER_OF_SABRE_RUNS, 1);

        #pragma omp atomic read
        local_best_depth = *shared_best_depth;

        if(results[0].depth<local_best_depth){ //improves the solution

            bool improved = false;
            #pragma omp critical(check_sol)
            {
                local_best_depth = *shared_best_depth;
                if (*shared_best_depth > results[0].depth){
                    *shared_best_depth = results[0].depth;
                    *shared_best_num_gates = results[0].num_gates;
                    memcpy(shared_best_mapping, mapping, logic * sizeof(int));
                    improved = true;
                }  
            }//omp critical

            if(improved){
                #pragma omp critical(printsol)
                {

                //std::cout<<"\nThread id: "<<omp_get_thread_num()<<std::endl;
                std::cout<<"New solution: \n\tFrom "<<local_best_depth<<" to "<<results[0].depth<<"\n\tDepth: "<<results[0].depth<<"\n\tNum gates: "<<results[0].num_gates<<"\n\tMapping: ";
                std::cout<<"[";
                for(int m = 0;m<logic-1;++m)
                    std::cout<<mapping[m]<<", ";
                std::cout<<mapping[logic-1]<<"]"<<std::endl;

                }//critical
                
            }

        }/// if, new sol found that improves the current solution...
        
    }
    return solutions;
          
}


#endif
