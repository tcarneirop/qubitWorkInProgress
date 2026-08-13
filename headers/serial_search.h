#ifndef SS_H
#define SS_H

#include <vector>

std::vector<int> SERIAL_search_64(int *PHYSIC_MACHINE, int *circuit,  const int num_gates, const long long physic, const long long logic)
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

        if (numrows < logic)
        {
            long long n = numrows++;
            aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

            pnStackPos++;

            *pnStack++ = bitfield;

            bitfield = mask & ~(aQueenBitCol[numrows]);

            ++tree_size;

            if (numrows == logic)
            {

                ++numSolutions;
                //what should we do here with parameters for enumeration?
                //remove 
                results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic,logic, 1, mapping, 1 ,1, 1);
                #ifdef VERBOSE
                std::cout<<"results[0].depth: "<< results[0].depth<<"\n";
                std::cout<<"results[0].num_gates: "<< results[0].num_gates<<"\n";
                #endif
                if(results[0].depth<best_depth){
                    std::cout<<"\nNew solution found: \n\tSolution: "<< numSolutions<<", From "<<best_depth<<" to "<<results[0].depth<<"\n";
                    best_depth = results[0].depth;
                    best_num_gates = results[0].num_gates;
                    best_mapping.resize(logic);
                    std::copy(mapping, mapping + logic, best_mapping.begin());
                    for(auto m: best_mapping)
                        std::cout<<m<<" ";
                    std::cout<<"\n";
                }
               // exit(1);

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

#ifdef CHECK
    check(physic, logic, numSolutions);
#endif

    std::cout<<"############# BEST SOL: ##################\n";
    std::cout<<"results[0].depth: "<< best_depth<<"\n";
    std::cout<<"results[0].num_gates: "<< best_num_gates<<"\n";
    std::cout<<"Best mapping:\n";
    for(auto m: best_mapping)
        std::cout<<"  "<<m<<" ";
    std::cout<<std::endl;

    return best_mapping;
}

#endif