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







void call_kchange_vs_jurema(
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	const long long physic, const long long logic,
	const int NUMBER_OF_SABRE_RUNS, const int NUM_RAND_SOLS, const int NUM_RAND_SOLS_CHOSEN, const int cutoff_jurema)
{

	int shared_best_depth = INT_MAX;
	int shared_best_num_gates = INT_MAX;
	int shared_best_num_swaps = INT_MAX;

	unsigned long long shared_sols_counter = 0, 
		jurema_sols_counter = 0ULL, jurema_pruning_sols_counter = 0ULL,
		kchange_sols_counter = 0ULL, kchange_pruning_sols_counter = 0ULL,
		k3change_sols_counter = 0ULL, k3change_pruning_sols_counter = 0ULL,
		rec_sols_counter = 0ULL, rec_pruning_sols_counter = 0ULL,
		k3rec_sols_counter = 0ULL, k3rec_pruning_sols_counter = 0ULL;

	unsigned long long jurema_total_nums_sols = 0ULL;
	

	int *shared_best_mapping = (int *)malloc(sizeof(int) * logic);
	int *mapping =             (int *)malloc(sizeof(int) * logic);
	int *rand_best_mapping =   (int *)malloc(sizeof(int) * logic);

	int 
		random_depth, kchange_depth, k3change_depth, kchange_pruning_depth, k3change_pruning_depth ,jurema_depth, jurema_pruning_depth, rec_depth, rec_pruning_depth, 
		k3rec_depth, k3rec_pruning_depth,
		random_swaps, jurema_swaps, jurema_pruning_swaps,   kchange_swaps, k3change_swaps, kchange_pruning_swaps, k3change_pruning_swaps, 
		rec_swaps, rec_pruning_swaps, k3rec_swaps, k3rec_pruning_swaps,
		random_gates, kchange_gates, k3change_gates, kchange_pruning_gates, k3change_pruning_gates, jurema_gates, jurema_pruning_gates, 
		rec_gates, rec_pruning_gates, k3rec_gates, k3rec_pruning_gates;

	unsigned long long num_sols = 0ULL, kchange_num_sols = 0ULL, kchange_pruning_num_sols = 0ULL, k3change_num_sols = 0ULL, k3change_pruning_num_sols = 0ULL, 
		jurema_num_sols = 0ULL, jurema_pruning_num_sols = 0ULL, 
		rec_num_sols = 0ULL, rec_pruning_num_sols = 0ULL, 
		k3rec_num_sols = 0ULL, k3rec_pruning_num_sols = 0ULL;

	double elapsed_kchange = 0.f, elapsed_pruning_kchange = 0.f, elapsed_k3change = 0.f, elapsed_pruning_k3change = 0.f, elapsed_jurema = 0.f, 
		elapsed_pruning_jurema = 0.f, elapsed_rec = 0.f, elapsed_pruning_rec = 0.f, k3elapsed_rec = 0.f, k3elapsed_pruning_rec = 0.f;


	std::vector<int> solutions;

	std::cout << "########################## STARTING THE COMPARISON ######################" << std::endl;

	std::cout << "########################## GENERATING RAND SOL(S) ##########################" << std::endl;

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

	random_depth = shared_best_depth;
	random_gates = shared_best_num_gates;
	random_swaps = shared_best_num_swaps;


	std::cout << "Random solutions generated: "<< NUM_RAND_SOLS << "\n\tRandom sols chonsen: " << NUM_RAND_SOLS_CHOSEN << "\n"<<std::endl;
	std::cout << "Initial best solution: \n\tSabre runs: " << NUMBER_OF_SABRE_RUNS << "\n\tDepth: " << random_depth << "\n\tGates: " << random_gates << std::endl;

	std::cout << "########################## SOLUTION(S) GENERATED ##########################" << std::endl;



	std::cout << "\n\n########################## Starting 2-change ##########################" << std::endl;

	
	memcpy(rand_best_mapping, shared_best_mapping, sizeof(int) * logic);

	Clock::time_point start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;

		num_sols+=k2_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, false,false);
	}

	kchange_num_sols = num_sols;
	kchange_depth = shared_best_depth;
	kchange_gates = shared_best_num_gates;
	kchange_swaps = shared_best_num_swaps;
	kchange_sols_counter = shared_sols_counter;
	elapsed_kchange = std::chrono::duration<double>(Clock::now() - start).count();


	std::cout << "\n\n########################## Starting PRUNING 2-change ##########################" << std::endl;
	
	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i <  NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;

		num_sols+=k2_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, false, true);
	}

	kchange_pruning_num_sols = num_sols;
	kchange_pruning_depth = shared_best_depth;
	kchange_pruning_gates = shared_best_num_gates;
	kchange_pruning_swaps = shared_best_num_swaps;
	kchange_pruning_sols_counter  = shared_sols_counter;

	elapsed_pruning_kchange = std::chrono::duration<double>(Clock::now() - start).count();



	std::cout << "\n\n########################## Starting RECURSIVE 2-change ##########################" << std::endl;
	
	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	start = Clock::now();
 
	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i <  NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		
		num_sols+=k2_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, true, false);
	}
 
	rec_num_sols = num_sols;
	rec_depth = shared_best_depth;
	rec_gates = shared_best_num_gates;
	rec_swaps = shared_best_num_swaps;
	rec_sols_counter  = shared_sols_counter;


	elapsed_rec = std::chrono::duration<double>(Clock::now() - start).count();


	std::cout << "\n\n########################## Starting RECURSIVE PRUNING 2-change ##########################" << std::endl;
	
	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		
		num_sols+=k2_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, true, true);
	} 

	rec_pruning_num_sols = num_sols;
	rec_pruning_depth = shared_best_depth;
	rec_pruning_gates = shared_best_num_gates;
	rec_pruning_swaps = shared_best_num_swaps;
	rec_pruning_sols_counter  = shared_sols_counter;


	elapsed_pruning_rec = std::chrono::duration<double>(Clock::now() - start).count();


	std::cout << "\n\n########################## Starting 3-change ##########################" << std::endl;

	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	
	memcpy(rand_best_mapping, shared_best_mapping, sizeof(int) * logic);

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;

		num_sols+=k3_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, 
				&shared_best_num_gates, &shared_best_num_swaps,
				shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, false,false);
	}

	k3change_num_sols = num_sols;
	k3change_depth = shared_best_depth;
	k3change_gates = shared_best_num_gates;
	k3change_swaps = shared_best_num_swaps;
	k3change_sols_counter = shared_sols_counter;
	elapsed_k3change = std::chrono::duration<double>(Clock::now() - start).count();


	std::cout << "\n\n########################## Starting PRUNING 3-change ##########################" << std::endl;
	
	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i <  NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;

		num_sols+=k3_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, false, true);
	}

	k3change_pruning_num_sols = num_sols;
	k3change_pruning_depth = shared_best_depth;
	k3change_pruning_gates = shared_best_num_gates;
	k3change_pruning_swaps = shared_best_num_swaps;
	k3change_pruning_sols_counter  = shared_sols_counter;

	elapsed_pruning_k3change = std::chrono::duration<double>(Clock::now() - start).count();



	std::cout << "\n\n########################## Starting RECURSIVE 3-change ##########################" << std::endl;
	
	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	start = Clock::now();
 
	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i <  NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		
		num_sols+=k3_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, true, false);
	}
 
	k3rec_num_sols = num_sols;
	k3rec_depth = shared_best_depth;
	k3rec_gates = shared_best_num_gates;
	k3rec_swaps = shared_best_num_swaps;
	k3rec_sols_counter  = shared_sols_counter;


	k3elapsed_rec = std::chrono::duration<double>(Clock::now() - start).count();


	std::cout << "\n\n########################## Starting RECURSIVE PRUNING 3-change ##########################" << std::endl;
	
	//RESTARTING
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		
		num_sols+=k3_change_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates, &shared_best_num_swaps,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, true, true);
	} 

	k3rec_pruning_num_sols = num_sols;
	k3rec_pruning_depth = shared_best_depth;
	k3rec_pruning_gates = shared_best_num_gates;
	k3rec_pruning_swaps = shared_best_num_swaps;
	k3rec_pruning_sols_counter  = shared_sols_counter;


	k3elapsed_pruning_rec = std::chrono::duration<double>(Clock::now() - start).count();

	std::cout << "\n\n########################## Start of Jurema ##########################" << std::endl;

	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;
	std::vector<unsigned long long> number_of_sols_depth(1500, 0ULL);
	std::vector<unsigned long long> number_of_sols_swaps(1500, 0ULL);
	

	start = Clock::now();
 /* 
	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i <  NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		num_sols+= jurema_search_64(PHYSIC_MACHINE, circuit, num_gates,
			physic, logic,
			mapping,
			cutoff_jurema,
			&shared_best_depth,
			&shared_best_num_gates,
			&shared_best_num_swaps,
			shared_best_mapping,
			&shared_sols_counter,
			NUMBER_OF_SABRE_RUNS, 
			start,
			number_of_sols_depth,
			number_of_sols_swaps,
			0,false
		);
	}
  */
	jurema_num_sols = num_sols;
	jurema_depth = shared_best_depth;
	jurema_gates = shared_best_num_gates;
	jurema_sols_counter = shared_sols_counter;
	jurema_swaps = shared_best_num_swaps;
	jurema_sols_counter = shared_sols_counter;

	elapsed_jurema = std::chrono::duration<double>(Clock::now() - start).count();



	std::cout << "\n\n########################## Start of PRUNING Jurema ##########################" << std::endl;
	
	memcpy(shared_best_mapping, rand_best_mapping, sizeof(int) * logic);

	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_best_num_swaps = random_swaps;
	shared_sols_counter = 0;
	num_sols = 0;
	

	start = Clock::now();

	/* #pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i <  NUM_RAND_SOLS_CHOSEN; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		num_sols+= jurema_search_64(PHYSIC_MACHINE, circuit, num_gates,
			physic, logic,
			mapping,
			cutoff_jurema,
			&shared_best_depth,
			&shared_best_num_gates,
			&shared_best_num_swaps,
			shared_best_mapping,
			&shared_sols_counter,
			NUMBER_OF_SABRE_RUNS, 
			start,
			number_of_sols_depth,
			number_of_sols_swaps,
			0,true
		);
	} 
 */
	jurema_pruning_num_sols = num_sols;
	jurema_pruning_depth = shared_best_depth;
	jurema_pruning_gates = shared_best_num_gates;
	jurema_pruning_sols_counter = shared_sols_counter;
	jurema_pruning_swaps = shared_best_num_swaps;
	jurema_pruning_sols_counter = shared_sols_counter;

	elapsed_pruning_jurema = std::chrono::duration<double>(Clock::now() - start).count();

	std::cout << "\n\n########################## End of PRUNING Jurema ##########################" << std::endl;

	std::cout << "########################## REPORT ##########################" << std::endl;

	#ifdef ODEPTH
	std::cout << "### Optimizing DEPTH" << std::endl;
	#elif defined(OGATES)
	std::cout << "### Optimizing GATES" << std::endl;
	#endif

	std::cout << "\n\nNumber of random solutions generated:  "<<NUM_RAND_SOLS<<"\n";
	std::cout << "Number of random solutions chosen:  "<<NUM_RAND_SOLS_CHOSEN<<"\n\t";
	std::cout << "Initial solution: " <<"\n\t";
	std::cout << "Depth: " << random_depth << "\n\t";
	std::cout << "Num gates: " << random_gates << "\n";

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                              2-CHANGE                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\n2-change best sol: \n\t";
	std::cout << "Depth: " << kchange_depth << "\n\t";
	std::cout << "Gates: " << kchange_gates << "\n\t";
	std::cout << "Swaps: " << kchange_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << kchange_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << kchange_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << kchange_num_sols  * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed 2-change: " << elapsed_kchange << "\n\t";
	
	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                        PRUNING 2-CHANGE                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\n2-change best sol: \n\t";
	std::cout << "Depth: " << kchange_pruning_depth << "\n\t";
	std::cout << "Gates: " << kchange_pruning_gates << "\n\t";
	std::cout << "Swaps: " << kchange_pruning_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << kchange_pruning_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << kchange_pruning_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << kchange_pruning_num_sols  * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed pruning 2-changes: " << elapsed_pruning_kchange << "\n\t";


	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                      RECURSIVE-2-CHANGE                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nPruning 2-change best sol: \n\t";
	std::cout << "Depth: " << rec_depth << "\n\t";
	std::cout << "Gates: " << rec_gates << "\n\t";
	std::cout << "Swaps: " << rec_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << rec_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << rec_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << rec_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed rec. 2-change: " << elapsed_rec << "\n\t";

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                    PRUNING RECURSIVE 2-CHANGE                       ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nPruning 2-change best sol: \n\t";
	std::cout << "Depth: " << rec_pruning_depth << "\n\t";
	std::cout << "Gates: " << rec_pruning_gates << "\n\t";
	std::cout << "Swaps: " << rec_pruning_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << rec_pruning_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << rec_pruning_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << rec_pruning_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed rec. pruning 2-change: " << elapsed_pruning_rec << "\n\t";


	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                              3-CHANGE                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\n3-change best sol: \n\t";
	std::cout << "Depth: " << k3change_depth << "\n\t";
	std::cout << "Gates: " << k3change_gates << "\n\t";
	std::cout << "Swaps: " << k3change_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << k3change_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << k3change_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << k3change_num_sols  * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed 3-change: " << elapsed_k3change << "\n\t";
	
	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                        PRUNING 3-CHANGE                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\n3-change best sol: \n\t";
	std::cout << "Depth: " << k3change_pruning_depth << "\n\t";
	std::cout << "Gates: " << k3change_pruning_gates << "\n\t";
	std::cout << "Swaps: " << k3change_pruning_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << k3change_pruning_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << k3change_pruning_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << k3change_pruning_num_sols  * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed 3-change: " << elapsed_pruning_k3change << "\n\t";

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                      RECURSIVE-3-CHANGE                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nPruning 3-change best sol: \n\t";
	std::cout << "Depth: " << k3rec_depth << "\n\t";
	std::cout << "Gates: " << k3rec_gates << "\n\t";
	std::cout << "Swaps: " << k3rec_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << k3rec_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << k3rec_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << k3rec_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed rec. 3-change: " << k3elapsed_rec << "\n\t";

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                    PRUNING RECURSIVE 3-CHANGE                       ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nPruning 2-change best sol: \n\t";
	std::cout << "Depth: " << k3rec_pruning_depth << "\n\t";
	std::cout << "Gates: " << k3rec_pruning_gates << "\n\t";
	std::cout << "Swaps: " << k3rec_pruning_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << k3rec_pruning_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << k3rec_pruning_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << k3rec_pruning_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed rec. pruning 2-change: " << k3elapsed_pruning_rec << "\n\t";

/*

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                              JUREMA                                ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nJurema best sol: \n\t";
	std::cout << "Depth: " << jurema_depth << "\n\t";
	std::cout << "Gates: " << jurema_gates << "\n\t";
	std::cout << "Swaps: " << jurema_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << jurema_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << jurema_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << jurema_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Jurema elapsed time: " << elapsed_jurema << "\n\t";

	
	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                             PRUNING JUREMA                          ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nJurema best sol: \n\t";
	std::cout << "Depth: " << jurema_pruning_depth << "\n\t";
	std::cout << "Gates: " << jurema_pruning_gates << "\n\t";
	std::cout << "Swaps: " << jurema_pruning_swaps << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << jurema_pruning_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << jurema_pruning_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << jurema_pruning_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Jurema elapsed time: " << elapsed_pruning_jurema << "\n\t";

	std::cout << "\n######################################################################\n"; */

}

#endif