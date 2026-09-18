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
    int *shared_best_num_swaps,
    int *shared_best_mapping, 
    const int NUMBER_OF_SABRE_RUNS, const unsigned long long num_random_sols, const bool plot)
{


    // X solutions, each containing D integers
    std::vector<int> solutions(num_random_sols * logic);
    std::vector<RoutingResult> set_of_results(num_random_sols);
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
        int local_best_num_gates = INT_MAX;
        int local_best_num_swaps = INT_MAX;
        int* mapping= solutions.data() + i * logic;
       


        #ifdef ODEPTH
        results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, mapping, 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_depth, false);
        #elif defined(OGATES)
        results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, mapping, 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_num_swaps, false);
        #endif
        

        // Fast path
        #pragma omp atomic read
        local_best_num_swaps = *shared_best_num_swaps;
        #pragma omp atomic read
        local_best_depth = *shared_best_depth;
        
        bool improved = false;

        set_of_results[i] = results[0];

        #ifdef ODEPTH
		if (results[0].depth < local_best_depth || (results[0].depth == local_best_depth && results[0].swaps < local_best_num_swaps))
        {

            #pragma omp critical(check_sol)
            {
                // Read the current pair again
                local_best_num_gates = *shared_best_num_gates;
                local_best_depth = *shared_best_depth;
                local_best_num_swaps = *shared_best_num_swaps;

                if(results[0].depth < local_best_depth || (results[0].depth == local_best_depth && results[0].swaps < local_best_num_swaps))
				{
                    improved = true;

                    *shared_best_num_gates = results[0].num_gates;
                    *shared_best_depth = results[0].depth;
                    *shared_best_num_swaps = results[0].swaps;

                    memcpy(shared_best_mapping,mapping, logic * sizeof(int) );
                }
            }
        
        #elif defined(OGATES)
        
        if (results[0].swaps < local_best_num_swaps || (results[0].swaps == local_best_num_swaps && results[0].depth < local_best_depth))
		{


            #pragma omp critical(check_sol)
            {
                // Read the current pair again
                local_best_num_gates = *shared_best_num_gates;
                local_best_depth = *shared_best_depth;
                local_best_num_swaps = *shared_best_num_swaps;

                
                if(results[0].swaps < local_best_num_swaps || (results[0].swaps == local_best_num_swaps && results[0].depth < local_best_depth))
                {
                    improved = true;
                    *shared_best_num_gates = results[0].num_gates;
                    *shared_best_depth = results[0].depth;
                    *shared_best_num_swaps = results[0].swaps;
                    memcpy(shared_best_mapping,mapping, logic * sizeof(int) );
                }
            }


        #endif

            if (!plot && improved)
            {
                #pragma omp critical(printsol)
                {
                    std::cout<< "New solution: \n" << "\tFrom depth " << local_best_depth << " to " << results[0].depth << "\n\tFrom gates " 
                    << local_best_num_gates
                    << " to " << results[0].num_gates
                    << "\n\tDepth: " << results[0].depth
                    << "\n\tNum gates: " << results[0].num_gates
                    << "\n\tNum swaps: " << results[0].swaps
                    << "\n\tMapping: [";
                    for (int m = 0; m < logic - 1; ++m)
                        std::cout << mapping[m] << ", ";
                        std::cout << mapping[logic - 1] << "]"<< std::endl;
                }//critical
            } //if improved
        

        }/// if, new sol found that improves the current solution...
        
    }

    if(plot) for (int i = 0; i < num_random_sols; ++i)
    {
        int *mapping = solutions.data() + i * logic;
        std::cout<< "\n Solution: " << i 
                << "\n\tDepth: " << set_of_results[i].depth
                << "\n\tNum gates: " << set_of_results[i].num_gates
                << "\n\tNum swaps: " << set_of_results[i].swaps
                << "\n\tMapping: [";
                for (int m = 0; m < logic - 1; ++m)
                    std::cout << mapping[m] << ", ";
                    std::cout << mapping[logic - 1] << "]"<< std::endl;
    
    }    


    return solutions;
          
}




#endif
