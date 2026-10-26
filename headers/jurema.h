#ifndef JUREMA_H
#define JUREMA_H

using Clock = std::chrono::steady_clock;

unsigned long long jurema_search_64(int *PHYSIC_MACHINE, int *circuit, const int num_gates,
									const long long physic, const long long logic,
									int *current_mapping,
									const long long cutoff_depth,
									int *shared_best_depth,
									int *shared_best_num_gates,
									int *shared_best_num_swaps,
									int *shared_best_mapping,
									unsigned long long *shared_sols_counter,
									const int NUMBER_OF_SABRE_RUNS, Clock::time_point start,
									std::vector<unsigned long long> &number_depth_values,
									std::vector<unsigned long long> &number_swaps_values,
									const unsigned long long num_sols_to_check,
									const bool pruning
								)
{
	std::vector<int> mapping(current_mapping, current_mapping + logic);

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
	int local_best_num_swaps;
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

	aQueenBitCol[0] = 0;

	for (int d = 0; d < logic; ++d)
	{
		unsigned long long col = mapping[d];
		unsigned long long lsb = 1ULL << col;

		// All candidates available at this depth
		bitfield = mask & ~aQueenBitCol[d];

		// The current solution and every smaller candidate
		// have already been explored.
		bitfield &= ~((1ULL << (col + 1)) - 1ULL);

		++pnStackPos;
		*pnStack++ = bitfield;

		aQueenBitCol[d + 1] =
			aQueenBitCol[d] | lsb;
	}

	numrows = logic;

	// Backtrack the supplied solution once.
	bitfield = *--pnStack;
	--pnStackPos;
	--numrows;

	////////////////////////////////////////////////////////
	//  End of initialization
	///////////////////////////////////////////////////////

	for (;;)
	{

		if (numrows == cutoff_depth)
			break;

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

		if (numrows < logic)
		{
			mapping[numrows] = (int)(63 - __builtin_clzll(lsb));
			long long n = numrows++;
			aQueenBitCol[numrows] = aQueenBitCol[n] | lsb;

			pnStackPos++;

			*pnStack++ = bitfield;

			bitfield = mask & ~(aQueenBitCol[numrows]);

			if (numrows == logic)
			{

				++numSolutions;


				//results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, mapping, 1, NUMBER_OF_SABRE_RUNS, 1);

				#ifdef ODEPTH
				results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, mapping.data(), 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_depth, pruning);
				#elif defined(OGATES)
				results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, mapping.data(), 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_num_swaps, pruning);
				#endif


				#if defined(SOLREPORTDEPTH) || defined(SOLREPORTGATES)
				number_depth_values[results[0].depth]++;
				number_swaps_values[results[0].swaps]++;
				#endif

				#pragma omp atomic read
				local_best_num_swaps = *shared_best_num_swaps;
				#pragma omp atomic read
				local_best_depth = *shared_best_depth;
				
				bool improved = false;

				#ifdef ODEPTH
				if (results[0].depth < local_best_depth || (results[0].depth == local_best_depth && results[0].swaps < local_best_num_swaps))
				{
					
					#pragma omp critical(check_sol)
					{
						// Read the current pair again
						local_best_num_gates = *shared_best_num_gates;
						local_best_depth = *shared_best_depth;
						local_best_num_swaps = *shared_best_num_swaps;

						if (results[0].depth < local_best_depth || (results[0].depth == local_best_depth && results[0].swaps < local_best_num_swaps))
						{
							improved = true;

							*shared_best_num_gates = results[0].num_gates;
							*shared_best_depth = results[0].depth;
							*shared_best_num_swaps = results[0].swaps;

							memcpy(shared_best_mapping,mapping.data(), logic * sizeof(int) );
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

						if (results[0].swaps < local_best_num_swaps || (results[0].swaps == local_best_num_swaps && results[0].depth < local_best_depth))
						{
							improved = true;
							
							*shared_best_num_gates = results[0].num_gates;
							*shared_best_depth = results[0].depth;
							*shared_best_num_swaps = results[0].swaps;

							memcpy(shared_best_mapping,mapping.data(), logic * sizeof(int) );
						}
					}

				#endif

					if (improved)
					{
						#pragma omp critical(printsol)
						{

							(*shared_sols_counter)++;
							#ifdef ODEPTH
							std::cout << "New solution found at: " << std::chrono::duration<double>(Clock::now() - start).count() << "\n\tSolution (depth): " << *shared_sols_counter << ", From " << local_best_depth << " to " << results[0].depth << "\n\tDepth: " << results[0].depth << "\n\tNum gates: " << results[0].num_gates << "\n\tSwaps: " << results[0].swaps << "\n\tMapping: ";
							#elif defined(OGATES)
							std::cout << "New solution found at: " << std::chrono::duration<double>(Clock::now() - start).count() << "\n\tSolution (gates): " << *shared_sols_counter << ", From " << local_best_num_gates << " to " << results[0].num_gates << "\n\tDepth: " << results[0].depth << "\n\tNum gates: " << results[0].num_gates << "\n\tSwaps: " << results[0].swaps << "\n\tMapping: ";
							#endif
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

unsigned long long call_jurema(
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	int physic, int logic,
	int *solutions,
	const long long cutoff_depth,
	int *shared_best_depth,
	int *shared_best_num_gates,
	int *shared_best_num_swaps,
	int *shared_best_mapping,
	unsigned long long *shared_sols_counter,
	const unsigned long long num_sols_to_check,
	const int NUMBER_OF_SABRE_RUNS,
	const int num_random_sols, const int num_random_sols_chosen,
	const bool pruning,
	Clock::time_point start

)
{

	unsigned long long num_sols = 0ULL;

	
	std::vector<unsigned long long> number_of_sols_depth(1000000, 0ULL);
	std::vector<unsigned long long> number_of_sols_swaps(1000000, 0ULL);
	

	#pragma omp parallel for schedule(runtime) reduction(+ : num_sols)
	for (int i = 0; i < num_random_sols_chosen; ++i)
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
			shared_best_num_swaps,
			shared_best_mapping,
			shared_sols_counter,
			NUMBER_OF_SABRE_RUNS,
			start,
			number_of_sols_depth,
			number_of_sols_swaps,
			num_sols_to_check,
			pruning
		);
	}

	
	
	#if defined(SOLREPORTDEPTH) || defined(SOLREPORTGATES)
	std::cout<<"############################################"<<std::endl;
	std::cout<<"Number of complete sols: "<< num_sols <<std::endl;
	std::cout<<"############################################"<<std::endl;
	std::cout<<"Depths: " <<std::endl;
	std::cout<<"############################################"<<std::endl;
	for(int i = 0; i<  number_of_sols_depth.size(); ++i){
		std::cout<<i<<" "<< number_of_sols_depth[i]<<std::endl;
	}
	std::cout<<"############################################"<<std::endl;
	std::cout<<"Swaps: " << std::endl;
	std::cout<<"############################################"<<std::endl;
	for(int i = 0; i<  number_of_sols_swaps.size(); ++i){
		std::cout<<i<<" "<< number_of_sols_swaps[i]<<std::endl;
	}
	std::cout<<"############################################"<<std::endl;
	#endif

	
	return num_sols;

} // end of call jurema

#endif
