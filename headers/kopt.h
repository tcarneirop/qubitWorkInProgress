#ifndef KOPT_H
#define KOPT_H

#include <utility>
#include <limits.h>
#include <array>
#include <algorithm>

unsigned long long k2_change_SABRE(
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	const long long physic, const long long logic,
	int *mapping,
	int *shared_best_depth,
	int *shared_best_num_gates,
	int *shared_best_num_swaps,
	int *shared_best_mapping,
	unsigned long long *shared_sols_counter,
	const int NUMBER_OF_SABRE_RUNS, Clock::time_point start, 
	const bool recursive, const bool pruning)
{

	unsigned long long num_sols = 0ULL;

	int local_best_depth = *shared_best_depth;
	int local_best_num_gates = *shared_best_num_gates;
	int local_best_num_swaps = *shared_best_num_swaps;

	std::vector<RoutingResult> results;

	int *new_mapping = new int[logic];
    memcpy(new_mapping, mapping, logic * sizeof(int));

	for (int index = 0; index < logic; ++index)
	{
		for (int kchange_index = index + 1; kchange_index < logic; ++kchange_index)
		{

			++num_sols;
			std::swap(new_mapping[index], new_mapping[kchange_index]);


			#ifdef ODEPTH
			results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, new_mapping, 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_depth, pruning);
			#elif defined(OGATES)
			results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, new_mapping, 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_num_swaps, pruning);
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

					if(results[0].depth < local_best_depth || (results[0].depth == local_best_depth && results[0].swaps < local_best_num_swaps))
					{
						improved = true;

						*shared_best_num_gates = results[0].num_gates;
						*shared_best_depth = results[0].depth;
						*shared_best_num_swaps = results[0].swaps;
						memcpy(shared_best_mapping,new_mapping, logic * sizeof(int) );
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
							memcpy(shared_best_mapping,new_mapping, logic * sizeof(int) );
						}
					}

			#endif
			} /// if, new sol found that improves the current solution...
			
			
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
							std::cout << new_mapping[m] << ", ";
						std::cout << new_mapping[logic - 1] << "]" << std::endl;
					} // critical
					
					//needs to be outside the  critical
					if(recursive){

						//std::cout<<"\nSTRTING RECURSIVE 2opt "<<std::endl;
						num_sols+=k2_change_SABRE(
							PHYSIC_MACHINE, circuit, num_gates,
							physic, logic,
							new_mapping,
							shared_best_depth,
							shared_best_num_gates,
							shared_best_num_swaps,
							shared_best_mapping,
							shared_sols_counter,
							NUMBER_OF_SABRE_RUNS,
							start, recursive, pruning
						);
					}
						
			}//ifimproved
			std::swap(new_mapping[index], new_mapping[kchange_index]);

			
		} // kchange

	} // index

	delete[] new_mapping;
	return num_sols;
}




unsigned long long k3_change_SABRE(
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	const long long physic, const long long logic,
	int *mapping,
	int *shared_best_depth,
	int *shared_best_num_gates,
	int *shared_best_num_swaps,
	int *shared_best_mapping,
	unsigned long long *shared_sols_counter,
	const int NUMBER_OF_SABRE_RUNS, Clock::time_point start, 
	const bool recursive, const bool pruning)
{




	int local_best_depth = *shared_best_depth;
	int local_best_num_gates = *shared_best_num_gates;
	int local_best_num_swaps = *shared_best_num_swaps;

	std::vector<RoutingResult> results;
	unsigned long long num_sols = 0ULL;

	std::vector<int> original(mapping, mapping + logic);
	std::vector<int> new_mapping = original;



	for (int i = 0; i < logic; ++i)	{
    	for (int j = i + 1; j < logic; ++j){
        	for (int k = j + 1; k < logic; ++k){

				std::array<int, 3> order = {0, 1, 2};
				std::array<int, 3> positions = {i, j, k};

				do
				{
					std::copy(original.begin(), original.end(),
							new_mapping.begin());

					new_mapping[i] = original[positions[order[0]]];
					new_mapping[j] = original[positions[order[1]]];
					new_mapping[k] = original[positions[order[2]]];

					++num_sols;
					//evaluating the new sol

					#ifdef ODEPTH
					results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, new_mapping.data(), 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_depth, pruning);
					#elif defined(OGATES)
					results = pruning_SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, new_mapping.data(), 1, NUMBER_OF_SABRE_RUNS, 1, shared_best_num_swaps, pruning);
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

							if(results[0].depth < local_best_depth || (results[0].depth == local_best_depth && results[0].swaps < local_best_num_swaps))
							{
								improved = true;

								*shared_best_num_gates = results[0].num_gates;
								*shared_best_depth = results[0].depth;
								*shared_best_num_swaps = results[0].swaps;
								memcpy(shared_best_mapping,new_mapping.data(), logic * sizeof(int) );
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
								memcpy(shared_best_mapping,new_mapping.data(), logic * sizeof(int) );
							}
						}

					#endif
					} /// if, new sol found that improves the current solution...
			
					
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
								std::cout << new_mapping[m] << ", ";
							std::cout << new_mapping[logic - 1] << "]" << std::endl;
						} // critical
						
						if(recursive){

							//std::cout<<"\nSTRTING RECURSIVE 3opt "<<std::endl;
							num_sols+=k3_change_SABRE(
								PHYSIC_MACHINE, circuit, num_gates,
								physic, logic,
								new_mapping.data(),
								shared_best_depth,
								shared_best_num_gates,
								shared_best_num_swaps,
								shared_best_mapping,
								shared_sols_counter,
								NUMBER_OF_SABRE_RUNS,
								start, recursive, pruning
							);
						}

						

					}//ifimproved

				 } while (std::next_permutation(order.begin(), order.end()));

				// Restauraring 
				new_mapping[i] = original[i];
				new_mapping[j] = original[j];
				new_mapping[k] = original[k];
				
			} // kchange

		} // index
	}
	return num_sols;

}




void call_kchange( const char heuristic,
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	const long long physic, const long long logic,
	const int NUMBER_OF_SABRE_RUNS, const int NUM_RAND_SOLS, const int NUM_RAND_SOLS_CHOSEN,
	const bool recursive, const bool pruning)
{

	int shared_best_depth = INT_MAX;
	int shared_best_num_gates = INT_MAX;
	int shared_best_num_swaps = INT_MAX;
	unsigned long long shared_sols_counter = 0ULL;

	int *shared_best_mapping = (int *)malloc(sizeof(int) * logic);
	int *mapping = (int *)malloc(sizeof(int) * logic);
	unsigned long long num_sols = 0ULL;

	std::vector<int> solutions;

	std::cout << "\n\n########################## GENERATING "<< NUM_RAND_SOLS <<" RAND SOL(S) ##########################" << std::endl;

	solutions = random_heuristic(
		PHYSIC_MACHINE,
		circuit,
		num_gates,
		physic, logic,
		&shared_best_depth,
		&shared_best_num_gates,
		&shared_best_num_swaps,
		shared_best_mapping,
		NUMBER_OF_SABRE_RUNS, NUM_RAND_SOLS, false);

	std::cout << "\n\n########################## "<< NUM_RAND_SOLS<<" SOLUTION(S) GENERATED ##########################" << std::endl;

	memcpy(mapping, shared_best_mapping, sizeof(int) * logic);

	const Clock::time_point start = Clock::now();

	
	std::cout << "########################## STARTING THE "<< heuristic <<"-Changes ##########################" << std::endl;
	std::cout << "\t ######## Number of changes: "<< heuristic << ". \n";
	std::cout << "Number of rand sols chosen: "<< NUM_RAND_SOLS_CHOSEN << std::endl;

	if(recursive){
		std::cout << "\n########################## RECURSIVE "<< heuristic <<"-Changes ##########################" << std::endl;
	}
		
	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;

		num_sols+= (heuristic == '2' ? k2_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, recursive, pruning) : 
				k3_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, recursive, pruning)
				);
	}

	std::cout << "\n########################## END OF THE "<< heuristic <<"-Changes ##########################" << std::endl;
	if(recursive)
		std::cout << "\n########################## RECURSIVE "<< heuristic <<"-Changes ##########################" << std::endl;
		
	std::cout << "Best solution found: \n\t";
	std::cout << "Depth: " << shared_best_depth << "\n\t";
	std::cout << "Num gates: " << shared_best_num_gates << "\n\t";
	std::cout << "Num swaps: " << shared_best_num_swaps << "\n\t";
	std::cout << "Mapping: ";
	std::cout << "[";
						for (int m = 0; m < logic - 1; ++m)
							std::cout << shared_best_mapping[m] << ", ";
						std::cout << shared_best_mapping[logic - 1] << "]" << std::endl;

	std::cout << "Number of complete solutions found: " << num_sols << "\n";
	std::cout << "\tNumber of solutions that improved the incumbent: " << shared_sols_counter << "\n";
	std::cout << "Number of SABRE runs (rand+kchange): " << (num_sols+NUM_RAND_SOLS) * NUMBER_OF_SABRE_RUNS  << "\n";
	std::cout << "Elapsed time: " << std::chrono::duration<double>(Clock::now() - start).count() << std::endl;
	std::cout << "\n######################################################################\n";

}





#endif