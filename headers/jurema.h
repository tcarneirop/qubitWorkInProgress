#ifndef JUREMA_H
#define JUREMA_H


#ifndef MAX_BOARDSIZE
#define MAX_BOARDSIZE 32
#endif


typedef struct jurema_subproblem_t{
    int mapping[MAX_BOARDSIZE];
    long long  aQueenBitCol[MAX_BOARDSIZE];
    long long aStack[MAX_BOARDSIZE];
    long long pnStack[MAX_BOARDSIZE];
}Jurema_subproblem;



void jurema_search_64(int *PHYSIC_MACHINE, int *circuit,  const int num_gates, const long long physic, const long long logic,
         int *mapping)
{

        unsigned int depth = 0U;
        long long aQueenBitCol[MAX_BOARDSIZE];
        long long aStack[MAX_BOARDSIZE];
        unsigned long long numSolutions = 0ULL;

        long long int *pnStack;

        long long int pnStackPos = 0LLU;

        long long numrows = 0LL;
        unsigned long long lsb;
        unsigned long long bitfield;
        long long i;

        long long mask = (1LL << physic) - 1LL;


        aStack[0] = -1LL;

        pnStack = aStack + 1;
        pnStackPos = 1;

        numrows = 0;

        aQueenBitCol[0] = 0;

        bitfield = mask;

        for (int depth = 0; depth < logic; ++depth)
        {
                unsigned long long lsb = 1ULL << mapping[depth];


                bitfield &= ~lsb;

                numrows++;

                aQueenBitCol[numrows] =
                        aQueenBitCol[numrows - 1] | lsb;

                
                ++pnStackPos;
                *pnStack++ = bitfield;

                
                bitfield = mask & ~aQueenBitCol[numrows];
        }

        bitfield = *--pnStack;
        pnStackPos--;
        --numrows;

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


                if (numrows == logic)
                {
                        std::cout<<"Stack position: "<<pnStackPos<<std::endl;
                        std::cout<<"Numrows: "<<numrows<<std::endl;
                        for(int i = 0; i<logic;++i){
                                std::cout<<mapping[i]<<" ";
                        }
                        std::cout<<std::endl;
                        ++numSolutions;
                        //exit(1);

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

    std::cout<<"Num sols: "<<numSolutions<<std::endl;
}

#endif

