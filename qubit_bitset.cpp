
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


#ifndef MAX_BOARDSIZE
#define MAX_BOARDSIZE 32
#endif

#define MAX_ELEMENTS 100000

#define MIN_BOARDSIZE 2



using Clock = std::chrono::steady_clock;
 

typedef struct subproblem_t{
    int mapping[12];
    long long  aQueenBitCol; 
}Subproblem;




inline unsigned long long factorial(unsigned long long n)
{
    return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}


void check(const long long m, const long long d, const unsigned long long nsols)
{
    unsigned long long mfat   =   factorial((unsigned long long)m);
    unsigned long long mmdfat =   factorial((unsigned long long)(m - d));

    if (mfat / (mmdfat) != nsols)
    {
        printf("\n############ ERROR - WRONG NUMBER OF SOLS #############\n");
        exit(1);
    }
    else
    {
        printf("\n############ %llu NUM SOLS OK #############\n", nsols);
    }
}




unsigned long long  mcore_final_search_64(int *PHYSIC_MACHINE, int *circuit,  const int num_gates, const long long physic, 
    const long long logic,const Subproblem* subproblem_pool, 
    const long long cutoff_depth, 
    int* shared_best_depth, 
    int *shared_best_num_gates, 
    int *shared_best_mapping, 
    unsigned long long *shared_sols_counter,
    const int NUMBER_OF_SABRE_RUNS, Clock::time_point start, 
    std::vector<unsigned long long> &number_of_sols, const unsigned long long num_sols_to_check)
{


    unsigned int depth = 0U;
    int mapping[MAX_BOARDSIZE];
    long long aQueenBitCol[MAX_BOARDSIZE];
    long long aStack[MAX_BOARDSIZE];
    unsigned long long numSolutions = 0ULL;

    int local_best_depth;
    int local_best_num_gates;

    std::vector<RoutingResult> results;
    std::vector<int> best_mapping;

    long long int *pnStack;

    long long int pnStackPos = 0LLU;

    long long numrows = cutoff_depth; //because we are doing the final search
    unsigned long long lsb;
    unsigned long long bitfield;
    long long i;

    long long mask = (1LL << physic) - 1LL;

    unsigned long long not_improving = 0ULL;
    

    /* Initialize stack */
    aStack[0] = -1LL; /* set sentinel -- signifies end of stack */

    bitfield = 0ULL;

    bitfield = (1LL << physic) - 1LL;
    pnStack = aStack + 1LL;

    pnStackPos++;

    //starting the search from where we stopped
    aQueenBitCol[numrows] =  subproblem_pool->aQueenBitCol; 
    memcpy(mapping, subproblem_pool->mapping, cutoff_depth * sizeof(int));
    bitfield = mask & ~(aQueenBitCol[numrows]);    

    for (;;)
    {

        lsb = -((signed long long)bitfield) & bitfield;
        if (0ULL == bitfield)
        {

            bitfield = *--pnStack;
            pnStackPos--;

            if (pnStack == aStack)
            {
                break;
            }

            --numrows;
            continue;
        }

        bitfield &= ~lsb;
        mapping[numrows] = (int)(63 - __builtin_clzll(lsb));

        if (numrows < logic)
        {
            long long n = numrows++;
            aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

            pnStackPos++;

            *pnStack++ = bitfield;

            bitfield = mask & ~(aQueenBitCol[numrows]);


            if (numrows == logic) /////// IT IS A SOLUTION!
            {

                ++numSolutions;
                
                #ifdef SABRE
                results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic,logic, 1, mapping, 1 , NUMBER_OF_SABRE_RUNS, 1);
               

                #ifdef SOLREPORT
                number_of_sols[results[0].depth]++;
                #endif

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

                        (*shared_sols_counter)++;

                        std::cout<<"\nNew solution found at: "<< std::chrono::duration<double>(Clock::now() - start).count()<< "\n\tSolution: "<< *shared_sols_counter <<", From "<<local_best_depth<<" to "<<results[0].depth<<"\n\tDepth: "<<results[0].depth<<"\n\tNum gates: "<<results[0].num_gates<<"\n\tMapping: ";
                        std::cout<<"[";
                        for(int m = 0;m<logic-1;++m)
                            std::cout<<mapping[m]<<", ";
                        std::cout<<mapping[logic-1]<<"]"<<std::endl;

                        }//critical
                        
                    }
    
                }/// if, new sol found that improves the current solution...
                else{

                    ++not_improving; //no... not improving
                }
                
                #endif //end sabre


                if(num_sols_to_check>0ULL && not_improving>num_sols_to_check){
                    //std::cout<<"Im not improving at all... - "<<not_improving<<std::endl;
                    return numSolutions;
                }
                   

            }//a leaf

            continue;
        }
        else
        {

            bitfield = *--pnStack;
            pnStackPos--;
            --numrows;
            continue;
        }
    }

    return numSolutions;
}

unsigned long long partial_search_64( const long long physic, const long long cutoff_depth, unsigned long long *num_subproblems,
    Subproblem *subproblem_pool)
{

    unsigned int depth = 0U;
    int mapping[MAX_BOARDSIZE];
    long long aQueenBitCol[MAX_BOARDSIZE];
    long long aStack[MAX_BOARDSIZE];
    unsigned long long numSolutions = 0ULL;


    std::vector<RoutingResult> results;
    std::vector<int> best_mapping;

    long long int *pnStack;

    long long int pnStackPos = 0LLU;

    long long numrows = 0LL;
    unsigned long long lsb;
    unsigned long long bitfield;
    long long i;

    long long mask = (1LL << physic) - 1LL;

    unsigned long long tree_size = 0ULL;
    
    /* Initialize stack */
    aStack[0] = -1LL; /* set sentinel -- signifies end of stack */

    bitfield = 0ULL;

    bitfield = (1LL << physic) - 1LL;
    pnStack = aStack + 1LL;

    pnStackPos++;

    mapping[0] = 0;
    aQueenBitCol[0] =  0LL;

    int best_depth = INT_MAX;
    int best_num_gates = INT_MAX;


    for (;;)
    {

        lsb = -((signed long long)bitfield) & bitfield;
        if (0ULL == bitfield)
        {

            bitfield = *--pnStack;
            pnStackPos--;

            if (pnStack == aStack)
            {
                break;
            }

            --numrows;
            continue;
        }

        bitfield &= ~lsb;
        mapping[numrows] = (int)(63 - __builtin_clzll(lsb));

        if (numrows < cutoff_depth)
        {
            long long n = numrows++;
            aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

            pnStackPos++;

            *pnStack++ = bitfield;

            bitfield = mask & ~(aQueenBitCol[numrows]);

            ++tree_size;

            if (numrows == cutoff_depth)
            {
                
                for(int i = 0; i<cutoff_depth;++i)
                    subproblem_pool[numSolutions].mapping[i] = mapping[i];
                    subproblem_pool[numSolutions].aQueenBitCol =  aQueenBitCol[numrows];

                ++numSolutions;
            }

            continue;
        }
        else
        {

            bitfield = *--pnStack;
            pnStackPos--;
            --numrows;
            continue;
        }
    }

    *num_subproblems = numSolutions;
    return tree_size;
}



void call_RANDOM_mcore_search(int *PHYSIC_MACHINE, int *circuit, const int num_gates, const long long physic,  
    const long long logic,  const long long cutoff_depth, int *best_depth, 
    int *best_num_gates,
    int *vec_best_mapping, 
    const float PERCENT,
    const int NUMBER_OF_SABRE_RUNS, const unsigned long long num_sols_to_check){
    

    Subproblem *subproblem_pool = (Subproblem*)(malloc(sizeof(Subproblem)*(unsigned)100000000));
    
    unsigned long long num_subproblems = 0ULL;
    unsigned long long num_sols_search = 0ULL;
    unsigned long long mcore_tree_size[num_subproblems];
    unsigned long long mcore_num_sols[num_subproblems];
    unsigned long long total_mcore_num_sols = 0ULL;
    unsigned long long total_mcore_tree_size = 0ULL;
    unsigned long long num_sols = 0ULL;
    unsigned long long shared_sols_counter = 0ULL;

    unsigned long long BIGGEST_SOL = 100000ULL;

   
    std::vector<unsigned long long> number_of_sols_value(100000, 0ULL);

    const Clock::time_point start = Clock::now();

    unsigned long long initial_tree_size = partial_search_64(physic, cutoff_depth, &num_subproblems, subproblem_pool);

    

    /////////////////////////////////////////////////////////////////////////
    const std::size_t sample_size = PERCENT * num_subproblems; 

    if(sample_size<1){
        std::cerr << "\n### ERROR: Number of sobproblems < 1: "<<sample_size<<std::endl;        
    }


    std::vector<unsigned long long> values(num_subproblems);
    std::iota(values.begin(), values.end(), 0);

    // Seed from the operating system
    std::random_device rd;
    std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    std::mt19937_64 rng(seed);


    // Random permutation
    std::shuffle(values.begin(), values.end(), rng);

    // Keep only 1%
    values.resize(sample_size);

    std::cout<<"\n################# STARTING THE RANDOM DFS SEARCH #################\n\tWorking with "<< values.size() <<" elements out of "<<num_subproblems<<"\n "; 

    /////////////////////////////////////////////////////////////////////////////

    for(unsigned long long i = 0; i<num_subproblems;++i){
        mcore_num_sols[i] = 0ULL;
        mcore_tree_size[i] = 0ULL;
    }

    printf("\nPartial tree: %llu -- Number of subproblems: %llu \n", initial_tree_size, num_subproblems);
    printf("\n### MCORE Search ###\n\tNumber of subproblems: %lld - Physic: %lld, Logic: %lld, Initial depth: %lld,  Max threads: %d\n", num_subproblems, physic, logic, cutoff_depth, omp_get_max_threads());
    
    #ifdef VERBOSE
    for(unsigned long long subproblem = 0; subproblem<num_subproblems;++subproblem){
        std::cout<<"\nSubproblem: "<<subproblem<<" \n\t";
        for(int l = 0; l<cutoff_depth;++l){
            std::cout<< subproblem_pool[subproblem].mapping[l]<< " - ";
        }
    }
    #endif

    #pragma omp parallel for schedule(runtime) default(none) shared(num_sols_to_check, number_of_sols_value,start,values, best_depth, best_num_gates, vec_best_mapping,shared_sols_counter,num_subproblems,PHYSIC_MACHINE, circuit, num_gates, physic, logic, subproblem_pool, cutoff_depth,NUMBER_OF_SABRE_RUNS) reduction(+:num_sols)
    for(unsigned long long subproblem = 0; subproblem<values.size();++subproblem){
        num_sols += mcore_final_search_64(PHYSIC_MACHINE, circuit, num_gates, physic, logic, subproblem_pool+values[subproblem], 
            cutoff_depth, best_depth, best_num_gates,vec_best_mapping,
            &shared_sols_counter, NUMBER_OF_SABRE_RUNS,start,number_of_sols_value,
            num_sols_to_check);
    }

    
    std::cout<<"\n######################################################################\n";
    std::cout<<"\nNumber of solutions that improved the incumbent: "<< shared_sols_counter<<"\n";
    std::cout<<"\nNumber of complete solutions found: "<< num_sols<<"\n";
    std::cout<<"\tNumber of SABRE runs: "<< num_sols*NUMBER_OF_SABRE_RUNS<<"\n";
    std::cout<<"Elapsed time: "<< std::chrono::duration<double>(Clock::now() - start).count()<<std::endl;
    std::cout<<"\n######################################################################\n";

    #ifdef CHECK
    check(physic, logic, num_sols);
    #endif

  

    #ifdef SOLREPORT
    for(unsigned long long index = 0; index<BIGGEST_SOL;++index){ 
        if(number_of_sols_value[index]>0){
             std::cout<<"Solution Value: "<<index<<"\n\tNumber of mappings: "<<number_of_sols_value[index]<<std::endl;
        }
    }
    #endif

}////////////////////////////////////////////////


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
    


    call_RANDOM_mcore_search(PHYSIC_MACHINE, circuit_flat.gates_flat.data(), circuit_flat.num_gates, (long long)nb_physic, 
        (long long)nb_logic,(long long)cutoff_depth,&best_depth,&best_num_gates,best_mapping,PERCENT, number_of_sabre_runs,num_sols_to_check );


    return 0;
}
