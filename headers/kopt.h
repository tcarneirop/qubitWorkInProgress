#ifndef KOPT_H
#define KOPT_H

#include <utility>
#include <limits.h>


unsigned long long kchange_SABRE(
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	const long long physic, const long long logic,
	int *mapping,
	int *shared_best_depth,
	int *shared_best_num_gates,
	int *shared_best_mapping,
	unsigned long long *shared_sols_counter,
	const int NUMBER_OF_SABRE_RUNS, Clock::time_point start, const bool recursive)
{


	int *new_mapping = (int *)malloc(sizeof(int) * logic);
	memcpy(new_mapping, mapping, sizeof(int) * logic);



	int local_best_depth = *shared_best_depth;
	std::vector<RoutingResult> results;
	unsigned long long num_sols = 0ULL;

	for (int index = 0; index < logic; ++index)
	{
		for (int kchange_index = 0; kchange_index < logic; ++kchange_index)
		{
			++num_sols;
			std::swap(new_mapping[index], new_mapping[kchange_index]);

			#ifdef SABRE
			results = SABRE_routing_many(circuit, num_gates, PHYSIC_MACHINE, physic, logic, 1, new_mapping, 1, NUMBER_OF_SABRE_RUNS, 1);

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
						memcpy(shared_best_mapping, new_mapping, logic * sizeof(int));
						improved = true;
					}
				} // omp critical

				if (improved)
				{
					#pragma omp critical(printsol)
					{

						(*shared_sols_counter)++;
						std::cout << "New solution found at: " << std::chrono::duration<double>(Clock::now() - start).count() << "\n\tSolution: " << *shared_sols_counter << ", From " << local_best_depth << " to " << results[0].depth << "\n\tDepth: " << results[0].depth << "\n\tNum gates: " << results[0].num_gates << "\n\tMapping: ";
						std::cout << "[";
						for (int m = 0; m < logic - 1; ++m)
							std::cout << new_mapping[m] << ", ";
						std::cout << new_mapping[logic - 1] << "]" << std::endl;
					} // critical
					
					//needs to be outside the  critical
					if(recursive == true){
						//std::cout<<"RECURSIVE"<<std::endl;

						num_sols+=kchange_SABRE(
							PHYSIC_MACHINE, circuit, num_gates,
							physic, logic,
							shared_best_mapping,
							shared_best_depth,
							shared_best_num_gates,
							shared_best_mapping,
							shared_sols_counter,
							NUMBER_OF_SABRE_RUNS, start, recursive
						);
					}
					
				}

			} /// if, new sol found that improves the current solution...

			#endif
			std::swap(new_mapping[index], new_mapping[kchange_index]);
		} // kchange

	} // index


	delete new_mapping;
	return num_sols;
}

void call_kchange(
	int *PHYSIC_MACHINE, int *circuit, const int num_gates,
	const long long physic, const long long logic,
	const int NUMBER_OF_SABRE_RUNS, const int NUM_RAND_SOLS, const bool recursive)
{

	int shared_best_depth = INT_MAX;
	int shared_best_num_gates = INT_MAX;
	unsigned long long shared_sols_counter = 0ULL;

	int *shared_best_mapping = (int *)malloc(sizeof(int) * logic);
	int *mapping = (int *)malloc(sizeof(int) * logic);
	unsigned long long num_sols = 0ULL;

	std::vector<int> solutions;

	std::cout << "\n\n########################## GENERATING RAND SOL(S) ##########################" << std::endl;

	solutions = random_heuristic(
		PHYSIC_MACHINE,
		circuit,
		num_gates,
		physic, logic,
		&shared_best_depth,
		&shared_best_num_gates,
		shared_best_mapping,
		NUMBER_OF_SABRE_RUNS, NUM_RAND_SOLS);

	std::cout << "\n\n########################## "<< NUM_RAND_SOLS<<" SOLUTION(S) GENERATED ##########################" << std::endl;

	memcpy(mapping, shared_best_mapping, sizeof(int) * logic);

	const Clock::time_point start = Clock::now();

	std::cout << "########################## STARTING THE K-Changes ##########################" << std::endl;
	if(recursive){
		std::cout << "\n########################## RECURSIVE K-Changes ##########################" << std::endl;
	}
		
	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		num_sols+=kchange_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, recursive);
	
	}

	std::cout << "\n########################## ENF OF THE K-Changes ##########################" << std::endl;
	if(recursive)
		std::cout << "\n########################## RECURSIVE K-Changes ##########################" << std::endl;
		
	std::cout << "Best solution found: \n\t";
	std::cout << "Depth: " << shared_best_depth << "\n\t";
	std::cout << "Num gates: " << shared_best_num_gates << "\n\t";
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
	const int NUMBER_OF_SABRE_RUNS, const int NUM_RAND_SOLS, const int cutoff_jurema, const bool recursive)
{

	int shared_best_depth = INT_MAX;
	int shared_best_num_gates = INT_MAX;

	unsigned long long shared_sols_counter = 0, jurema_sols_counter = 0ULL, kchange_sols_counter = 0ULL, rec_sols_counter = 0ULL;
	unsigned long long jurema_total_nums_sols = 0ULL;
	

	int *shared_best_mapping = (int *)malloc(sizeof(int) * logic);
	int *mapping = (int *)malloc(sizeof(int) * logic);

	int random_depth, kchange_depth, jurema_depth, rec_depth, 
		random_gates, kchange_gates, jurema_gates,rec_gates;

	unsigned long long num_sols = 0ULL, kchange_num_sols = 0ULL, jurema_num_sols = 0ULL, rec_num_sols = 0ULL;

	double elapsed_kchange = 0.f, elapsed_jurema = 0.f, elapsed_rec = 0.f;


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
		shared_best_mapping,
		NUMBER_OF_SABRE_RUNS, NUM_RAND_SOLS);
	random_depth = shared_best_depth;
	random_gates = shared_best_num_gates;

	std::cout << "Baseline " << NUMBER_OF_SABRE_RUNS << " sabre run value: \n\tDepth: " << random_depth << "\n\tGates: " << random_gates << std::endl;

	std::cout << "########################## SOLUTION(S) GENERATED ##########################" << std::endl;



	std::cout << "\n\n########################## Starting 2-changes ##########################" << std::endl;

	memcpy(mapping, shared_best_mapping, sizeof(int) * logic);

	Clock::time_point start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		num_sols+=kchange_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, false);
	}

	kchange_num_sols = num_sols;
	kchange_depth = shared_best_depth;
	kchange_gates = shared_best_num_gates;
	kchange_sols_counter = shared_sols_counter;
	elapsed_kchange = std::chrono::duration<double>(Clock::now() - start).count();

	std::cout << "\n\n########################## Starting RECURSIVE 2-changes ##########################" << std::endl;
	
	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_sols_counter = 0;


	memcpy(mapping, shared_best_mapping, sizeof(int) * logic);

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		num_sols+=kchange_SABRE(PHYSIC_MACHINE, circuit, num_gates, physic, logic, mapping, &shared_best_depth, &shared_best_num_gates,
				  shared_best_mapping, &shared_sols_counter, NUMBER_OF_SABRE_RUNS, start, true);
	}

	rec_num_sols = num_sols;
	rec_depth = shared_best_depth;
	rec_gates = shared_best_num_gates;
	rec_sols_counter = shared_sols_counter;
	elapsed_rec = std::chrono::duration<double>(Clock::now() - start).count();

	std::cout << "\n\n########################## Start of Jurema ##########################" << std::endl;

	shared_best_depth = random_depth;
	shared_best_num_gates = random_gates;
	shared_sols_counter = 0;
	std::vector<unsigned long long> number_of_sols;

	start = Clock::now();

	#pragma omp parallel for schedule(runtime) reduction(+:num_sols)
	for (int i = 0; i < NUM_RAND_SOLS; ++i)
	{
		int *mapping = solutions.data() + i * logic;
		num_sols+=jurema_total_nums_sols = jurema_search_64(PHYSIC_MACHINE, circuit, num_gates,
			physic, logic,
			mapping,
			cutoff_jurema,
			&shared_best_depth,
			&shared_best_num_gates,
			shared_best_mapping,
			&shared_sols_counter,
			NUMBER_OF_SABRE_RUNS, start,
			number_of_sols,
			0);
	}
	
	jurema_num_sols = num_sols;
	jurema_depth = shared_best_depth;
	jurema_gates = shared_best_num_gates;
	jurema_sols_counter = shared_sols_counter;
	elapsed_jurema = std::chrono::duration<double>(Clock::now() - start).count();

	std::cout << "\n\n########################## End of Jurema ##########################" << std::endl;

	std::cout << "########################## REPORT ##########################" << std::endl;

	std::cout << "\nInitial SABRE " << NUMBER_OF_SABRE_RUNS << " solution:  \n\t";
	std::cout << "Depth: " << random_depth << "\n\t";
	std::cout << "Num gates: " << random_gates << "\n";
	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                              K-CHANGES                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nK-changes best sol: \n\t";
	std::cout << "Depth: " << kchange_depth << "\n\t";
	std::cout << "Num gates: " << kchange_gates << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << kchange_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << logic * (logic - 1) << "\n";
	std::cout << "\tNumber of SABRE runs: " << logic * (logic - 1) * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed k-changes: " << elapsed_kchange << "\n\t";

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                              RECURSIVE-CHANGES                             ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nRecursive K-changes best sol: \n\t";
	std::cout << "Depth: " << rec_depth << "\n\t";
	std::cout << "Num gates: " << rec_gates << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << rec_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << rec_num_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << rec_num_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Elapsed k-changes: " << elapsed_rec << "\n\t";

	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                              JUREMA                                ";
	std::cout << "\n------------------------------------------------------------------\n";

	std::cout << "\nJurema best sol: \n\t";
	std::cout << "Depth: " << jurema_depth << "\n\t";
	std::cout << "Num gates: " << jurema_gates << "\n\t";
	std::cout << "\nNumber of solutions that improved the incumbent: " << jurema_sols_counter << "\n";
	std::cout << "\nNumber of complete solutions found: " << jurema_total_nums_sols << "\n";
	std::cout << "\tNumber of SABRE runs: " << jurema_total_nums_sols * NUMBER_OF_SABRE_RUNS << "\n";
	std::cout << "Jurema elapsed time: " << elapsed_jurema << "\n\t";

	std::cout << "\n######################################################################\n";
}

#endif