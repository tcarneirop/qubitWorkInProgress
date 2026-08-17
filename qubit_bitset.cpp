
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <cmath>
#include <omp.h>
#include <sys/time.h>
#include <math.h>
#include <tgmath.h>
#include <vector>
#include <limits.h>
#include <iostream>
#include <regex>
#include <fstream>
#include <cstdint>
#include <numeric>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <set>
#include <unordered_set>


#include "headers/sabre.h"
#include "headers/parser.h"
#include "headers/random.h"
#include "headers/utils.h"
#include "headers/dfs_search.h"
#include "headers/jurema.h"





/* main routine for N Queens program.*/
int main(int argc, char **argv)
{


    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <qasm_file> <n physic gates of the desired machine> <cutoff depth> <percent of the pool> <number_of_sabre_runs> \n";
        return EXIT_FAILURE;
    }


    int nb_logic = 0;
    int flat_circuit_size = 0;
    
    
    const std::string qasm_file = argv[1];
    ParsedCircuit circuit_flat = parse_qasm(qasm_file);


    int nb_physic = atoi(argv[2]);
    float percent_permutation = atof(argv[3]);
    float PERCENT = atof(argv[4]);
    int number_of_sabre_runs = 1;
    int num_random_sols = 10;
    unsigned long long num_sols_to_check = 0ULL;

    std::cout<<"argc: "<<argc<<std::endl;
    
    if(argc >= 7){

        number_of_sabre_runs = atoi(argv[5]);
        num_sols_to_check = atoi(argv[6]);

        if(number_of_sabre_runs < 1){
            std::cout<<"####### ERROR ########\n\t"<<"Number of SABRE runs < 1.\n";
            exit(1);
        }

        if(num_sols_to_check < 0){
            std::cout<<"####### ERROR ########\n\t"<<"Number of Sols to check in each subproblem > 0.\n";
            exit(1);
        }

        if(argc == 8){
            num_random_sols = atoi(argv[7]);

            if(num_random_sols < 1){
                std::cout<<"####### ERROR ########\n\t"<<"Number of Random sols runs < 1.\n";
                exit(1);
            }
        }

    }

    int *PHYSIC_MACHINE; 
    int best_depth = 0;
    int best_num_gates = 0;
    int best_mapping[MAX_BOARDSIZE];


    if(nb_physic<nb_logic){
        std::cout<<"####### ERROR ########\n\t"<<"Number of physic gantes needs to be >= number of logic gates.\n";
        exit(1);
    }


    nb_logic = circuit_flat.n;

    int cutoff_depth = percent_permutation * nb_logic;

    if(cutoff_depth > nb_logic ||  cutoff_depth < 1){
        std::cout<<"####### ERROR ########\n\t"<<"cutoff depth ( "<<cutoff_depth<< " ) needs to be <= nb_logic and >=1."<<std::endl;
        exit(1);
    }


    std::cout<<"circuit_flat.n: "<<circuit_flat.n<<std::endl;
    std::cout<<"circuit_flat.num_gates:"<<circuit_flat.num_gates<<std::endl;
    std::cout<<"Number of SABRE runs: "<<number_of_sabre_runs<<std::endl;
    std::cout<<"Physic QUBITS: "<< (long long)(nb_physic)<<" Logic QUBITS: "<< (long long)(nb_logic)<<std::endl;
    std::cout<<"Cutoff depth: "<< cutoff_depth<<std::endl;
    std::cout<<"\tPercentage of the permutation: "<< percent_permutation*100<<"%"<<std::endl;
    std::cout<<"Number of random sols: "<< num_random_sols<<std::endl;
    

    std::cout<<"########### SANITY TEST ################# "<<"\n";
    
    PHYSIC_MACHINE = ALBATROZ;
    std::vector<int> mapping( nb_logic );
    std::iota(mapping.begin(), mapping.end(), 0);
    for(auto m: mapping)
    std::cout<<m<<" ";
    std::cout<<"\n";
    std::vector<RoutingResult> results = SABRE_routing_many(circuit_flat.gates_flat.data(), circuit_flat.num_gates, PHYSIC_MACHINE , nb_physic,circuit_flat.n, 1, mapping.data(), 1,1, 1);

    std::cout<<"results[0].depth: "<< results[0].depth<<"\n";
    std::cout<<"results[0].num_gates: "<< results[0].num_gates<<"\n";

    
    std::cout<<"################# END OF SANITY TEST ##########################"<<std::endl;



    std::cout<<"################# STARTING THE RANDOM SEARCH ##########################"<<std::endl;

    best_depth = results[0].depth;
    best_num_gates = results[0].num_gates;
    memcpy(best_mapping, mapping.data(), nb_logic * sizeof(int));
    
    random_heuristic(
        PHYSIC_MACHINE, 
        circuit_flat.gates_flat.data(), 
        circuit_flat.num_gates,
        nb_physic,  nb_logic,   
        &best_depth, 
        &best_num_gates,
        best_mapping, 
        number_of_sabre_runs,  num_random_sols
    );
    
    std::cout<<"################# END OF THE RANDOM SEARCH ##########################"<<std::endl;
    


    //call_RANDOM_mcore_search(PHYSIC_MACHINE, circuit_flat.gates_flat.data(), circuit_flat.num_gates, (long long)nb_physic, 
    //    (long long)nb_logic,(long long)cutoff_depth,&best_depth,&best_num_gates,best_mapping,PERCENT, number_of_sabre_runs,num_sols_to_check );

  
    jurema_search_64(PHYSIC_MACHINE, circuit_flat.gates_flat.data(),  circuit_flat.num_gates, (long long)nb_physic, 
        (long long)nb_logic, mapping.data());


    return 0;
}
