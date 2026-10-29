#ifndef EXPERIMENTS_H
#define EXPERIMENTS_H

#include <utility>
#include <limits.h>
#include <array>
#include <algorithm>

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


	std::cout << "\n------------------------------------------------------------------\n";
	std::cout << "                          RANDOM HEURISTIC                            ";
	std::cout << "\n------------------------------------------------------------------\n";


	std::cout << "\n\nNumber of random solutions generated:  "<<NUM_RAND_SOLS<<"\n";
	std::cout << "Number of random solutions chosen:  "<<NUM_RAND_SOLS_CHOSEN<<"\n\t";
	std::cout << "Best random solution: " <<"\n\t";
	std::cout << "Depth: " << random_depth << "\n\t"; 
	std::cout << "Gates: " << random_gates << "\n\t";
    std::cout << "Swaps: " << random_swaps << "\n";

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
	std::cout << "Elapsed rec. pruning 3-change: " << k3elapsed_pruning_rec << "\n\t";

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