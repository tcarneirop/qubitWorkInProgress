#ifndef JUREMA_H
#define JUREMA_H

using Clock = std::chrono::steady_clock;

unsigned long long jurema_search_64(int *PHYSIC_MACHINE, int *circuit, const int num_gates,
				    const long long physic, const long long logic,
				    int *mapping,
				    const long long cutoff_depth,
				    int *shared_best_depth,
				    int *shared_best_num_gates,
				    int *shared_best_mapping,
				    unsigned long long *shared_sols_counter,
				    const int NUMBER_OF_SABRE_RUNS, Clock::time_point start,
				    std::vector<unsigned long long> &number_of_sols,
				    const unsigned long long num_sols_to_check)
{

	unsigned int depth = 0U;
	long long aQueenBitCol[MAX_BOARDSIZE];
	long long aStack[MAX_BOARDSIZE];

	long long int *pnStack;

	long long int pnStackPos = 0LLU;

	long long numrows = 0LL;
	unsigned long long lsb;
	unsigned long long bitfield;
	long long i;

	long long mask = (1LL << physic) - 1LL;

	//////////////////////////////////////////////////
	// Sols for sabre
	//////////////////////////////////////////////////
	int local_best_depth;
	int local_best_num_gates;

	std::vector<RoutingResult> results;
	std::vector<int> best_mapping;

	unsigned long long not_improving = 0ULL;
	unsigned long long numSolutions = 0ULL;

	////////////////////////////////////////////////////////
	// Initializing the search
	///////////////////////////////////////////////////////

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

	////////////////////////////////////////////////////////
	//  End of initialization
	///////////////////////////////////////////////////////

	for (;;)
	{

		if (numrows == 1)
			break;
		
		if(cutoff_depth>0 && numrows == cutoff_depth){
			//std::cout<<"Cutoff depth reached: "<<cutoff_depth << " -- Numrows: "<<numrows<<std::endl;
			break;
		}

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

				++numSolutions;

				#ifdef SABRE
				results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, mapping, 1, NUMBER_OF_SABRE_RUNS, 1);

				#ifdef SOLREPORT
				number_of_sols[results[0].depth]++;
				#endif

				#pragma omp atomic read
				local_best_depth = *shared_best_depth;

				if (results[0].depth < local_best_depth)
				{ // improves the solution

					bool improved = false;
					#pragma omp critical(check_sol)
					{
						local_best_depth = *shared_best_depth;
						if (*shared_best_depth > results[0].depth)
						{
							*shared_best_depth = results[0].depth;
							*shared_best_num_gates = results[0].num_gates;
							memcpy(shared_best_mapping, mapping, logic * sizeof(int));
							improved = true;
						}
					} // omp critical

					if (improved)
					{
						#pragma omp critical(printsol)
						{

							(*shared_sols_counter)++;

							std::cout << "\nNew solution found at: " << std::chrono::duration<double>(Clock::now() - start).count() << "\n\tSolution: " << *shared_sols_counter << ", From " << local_best_depth << " to " << results[0].depth << "\n\tDepth: " << results[0].depth << "\n\tNum gates: " << results[0].num_gates << "\n\tMapping: ";
							std::cout << "[";
							for (int m = 0; m < logic - 1; ++m)
								std::cout << mapping[m] << ", ";
							std::cout << mapping[logic - 1] << "]" << std::endl;

						} // critical
					}

				} /// if, new sol found that improves the current solution...
				else
				{

					++not_improving; // no... not improving
				}

				#endif // end sabre

				if (num_sols_to_check > 0ULL && not_improving > num_sols_to_check)
				{
					// std::cout<<"Im not improving at all... - "<<not_improving<<std::endl;
					return numSolutions;
				}
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

	return numSolutions;
}

void call_jurema(
    int *PHYSIC_MACHINE, int *circuit, const int num_gates,
    int physic, int logic,
    int *solutions,
    const long long cutoff_depth,
    int *shared_best_depth,
    int *shared_best_num_gates,
    int *shared_best_mapping,
    unsigned long long *shared_sols_counter,
    const unsigned long long num_sols_to_check,
    const int NUMBER_OF_SABRE_RUNS,
    const int num_random_sols

)
{

	unsigned long long num_sols = 0ULL;

	const Clock::time_point start = Clock::now();
	std::vector<unsigned long long> number_of_sols_value(100000, 0ULL);

	#pragma omp parallel for schedule(runtime) reduction(+ : num_sols)
	for (int i = 0; i < num_random_sols; ++i)
	{

		int *mapping = solutions + i * logic;

		num_sols += jurema_search_64(
		    PHYSIC_MACHINE,
		    circuit,
		    num_gates,
		    (long long)physic,
		    (long long)logic,
		    mapping,
		    (long long)cutoff_depth,
		    shared_best_depth,
		    shared_best_num_gates,
		    shared_best_mapping,
		    shared_sols_counter,
		    NUMBER_OF_SABRE_RUNS,
		    start,
		    number_of_sols_value,
		    num_sols_to_check);
	}

	std::cout << "\n######################################################################\n";
	std::cout << "\nBest solution found: \n\t";
	std::cout << "\nDepth: " << *shared_best_depth << "\n";
	std::cout << "\nNum gates: " << *shared_best_num_gates << "\n";
	std::cout << "\nNumber of solutions that improved the incumbent: " << *shared_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed time: " << std::chrono::duration<double>(Clock::now() - start).count() << std::endl;
	std::cout << "\n######################################################################\n";

} // end of call jurema

#endif
