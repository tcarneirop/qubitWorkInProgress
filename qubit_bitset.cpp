
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


    int *PHYSIC_MACHINE; 
    int best_depth = 0;
    int best_num_gates = 0;
    int best_mapping[MAX_BOARDSIZE];

    int nb_logic = 0;
    int flat_circuit_size = 0;
    
    
    const std::string qasm_file = argv[1];
    ParsedCircuit circuit_flat = parse_qasm(qasm_file);


    int nb_physic;
    float percent_permutation;
    float PERCENT;
    int number_of_sabre_runs = 1;
    int num_random_sols = 10;
    unsigned long long num_sols_to_check = 0ULL;
    unsigned long long shared_sols_counter = 0ULL;
    char search = 'j';

    int cutoff_depth = 1 ;
    std::vector<int> solutions;

    std::cout<<"argc: "<<argc<<std::endl;

    nb_logic = circuit_flat.n;

    if(argc == 9){
    
        search  = argv[2][0];
        nb_physic = atoi(argv[3]);

        std::cout<<"search: "<<search<<std::endl;

       

       // if(search == 'd'){
        percent_permutation = atof(argv[4]);
        cutoff_depth = percent_permutation * nb_logic;            
        //}
        //else{
        //    cutoff_depth = atoi(argv[4]);
        //}

        number_of_sabre_runs = atoi(argv[5]);
        
        PERCENT = atof(argv[6]);
        num_sols_to_check = atoi(argv[7]);

        if(number_of_sabre_runs < 1){
            std::cout<<"####### ERROR ########\n\t"<<"Number of SABRE runs < 1.\n";
            return EXIT_FAILURE;
        }

        if(num_sols_to_check < 0){
            std::cout<<"####### ERROR ########\n\t"<<"Number of Sols to check in each subproblem > 0.\n";
            return EXIT_FAILURE;
        }

       
        num_random_sols = atoi(argv[8]);

        if(num_random_sols < 1){
            std::cerr<<"####### ERROR ########\n\t"<<"Number of Random sols runs < 1.\n";
            return EXIT_FAILURE;
        }
    }
    else{
        std::cerr << "Usage: " << argv[0] << "<qasm_file> <d or j> <n physic gates of the desired machine> <percent of the depth for dfs> <number_of_sabre_runs> <percent of the pool> <sols to skip> <num of random sols> \n";
        return EXIT_FAILURE;

    }


    if(nb_physic<nb_logic){
        std::cout<<"####### ERROR ########\n\t"<<"Number of physic gantes needs to be >= number of logic gates.\n";
        return EXIT_FAILURE;
    }

    if(cutoff_depth > nb_logic){
        std::cout<<"####### ERROR ########\n\t"<<"cutoff depth ( "<<cutoff_depth<< " ) needs to be <= nb_logic."<<std::endl;
        return EXIT_FAILURE;
    }

    if(search == 'd' && cutoff_depth<1){
        std::cout<<"####### ERROR ########\n\t"<<"Resulting cutoff depth ( "<<cutoff_depth<< " ) needs to be > 0 "<<std::endl;
        return EXIT_FAILURE;
    }
    


    std::cout<<"circuit_flat.n: "<<circuit_flat.n<<std::endl;
    std::cout<<"circuit_flat.num_gates:"<<circuit_flat.num_gates<<std::endl;
    std::cout<<"Number of SABRE runs: "<<number_of_sabre_runs<<std::endl;
    std::cout<<"Physic QUBITS: "<< (long long)(nb_physic)<<" Logic QUBITS: "<< (long long)(nb_logic)<<std::endl;
    std::cout<<"Number of random sols: "<< num_random_sols << std::endl;
    std::cout<<"Search: "<< search << std::endl;
    std::cout<<"Cutoff depth: "<< cutoff_depth<<std::endl;
    std::cout<<"\tPercentage of the permutation: "<< percent_permutation*100<<"%"<<std::endl;
    std::cout<<"Number of random sols: "<< num_random_sols<<std::endl;
    std::cout<<"Percentage of the pool to explore: "<< PERCENT*100<<"\%"<<std::endl;


    std::cout<<"########### SANITY TEST ################# "<<"\n";
    
    PHYSIC_MACHINE = ALBATROZ;
    std::vector<int> mapping( nb_logic );
    std::iota(mapping.begin(), mapping.end(), 0);
    for(auto m: mapping)
    std::cout<<m<<" ";
    std::cout<<"\n";
    std::vector<RoutingResult> results = SABRE_routing_many(circuit_flat.gates_flat.data(), circuit_flat.num_gates, PHYSIC_MACHINE , nb_physic,circuit_flat.n, 1, mapping.data(), 1,1, 1);

    std::cout<<"depth: "<< results[0].depth<<"\n";
    std::cout<<"num_gates: "<< results[0].num_gates<<"\n";
    
    std::cout<<"################# END OF SANITY TEST ##########################"<<std::endl;
    

    if(num_random_sols>0){
    
        std::cout<<"################# STARTING THE RANDOM SEARCH ##########################"<<std::endl;

        best_depth = results[0].depth;
        best_num_gates = results[0].num_gates;
        memcpy(best_mapping, mapping.data(), nb_logic * sizeof(int));
            
            solutions = random_heuristic(
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

    }


    if(search == 'd'){
        std::cout<<"############ STARTING THE DFS SEARCH ################"<<std::endl;
        call_RANDOM_mcore_search(PHYSIC_MACHINE, 
            circuit_flat.gates_flat.data(), 
            circuit_flat.num_gates, 
            (long long)nb_physic, 
            (long long)nb_logic,
            (long long)cutoff_depth,
            &best_depth,&best_num_gates,
            best_mapping,PERCENT, 
            number_of_sabre_runs,
            num_sols_to_check );
    }
    else{
        if(search == 'j' && num_random_sols>0){ //we can only do jurema having complete solutions
            std::cout<<"############ STARTING THE JUREMA SEARCH ################"<<std::endl;
            call_jurema(
                PHYSIC_MACHINE,
                circuit_flat.gates_flat.data(), 
                circuit_flat.num_gates,
                nb_physic,   
                nb_logic,   
                solutions.data(),
                cutoff_depth, 
                &best_depth,
                &best_num_gates,
                best_mapping,
                &shared_sols_counter,
                num_sols_to_check,
                number_of_sabre_runs,
                num_random_sols
            );

        }
        else{
            std::cout<<"############ ERROR: Wrong search parameters ################";
            exit(1);
        }
    }


    return 0;
}
