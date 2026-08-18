#ifndef DFS_S_H
#define DFS_S_H

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



#endif
