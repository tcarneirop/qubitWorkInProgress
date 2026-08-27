
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


struct Parameters {

   	std::string qasm_file;
	float percent_of_the_permutation = 0.5;
	int number_of_sabre_runs = 20;
	float pool_percent = 1.f;
	int num_random_sols = 100;
	unsigned long long num_sols_to_skip = 0ULL;
	std::string topology = "albatroz";
	char search = 'j';
	int *PHYSIC_MACHINE;
	int nb_logic = 0;
	int nb_physic = 0;
	int cutoff_depth = 0;
	int *circuit_flat_gates_data;
	int circuit_flat_num_gates;
	std::vector<int> permutation;

};

/*
	Obs: im adding code to the headers.
	I know it is not ideal, but this is mainly for organizing code that is related.
	-- This is very sloppy.
	@Todo: Organize real headers/.o files.
*/

#include "headers/sabre.h"
#include "headers/parser.h"
#include "headers/random.h"
#include "headers/utils.h"
#include "headers/dfs_search.h"
#include "headers/jurema.h"
#include "headers/kopt.h"
#include "headers/parameters.h"
#include "headers/sanity_test.h"
#include "headers/call_searches.h"

/* main routine for N Queens program.*/
int main(int argc, char **argv)
{


	Parameters my_params;
	int best_depth = 0;
	int best_num_gates = 0;
	int best_mapping[MAX_BOARDSIZE];

	////////////////////////////
	// STARTING PARAMETERS
	//////////////////////////


	cli_parameters_parser(&my_params,argc, argv);
	ParsedCircuit circuit_flat = parse_qasm(my_params.qasm_file);
	start_parameters_circuit(&my_params, circuit_flat.n, circuit_flat.num_gates, circuit_flat.gates_flat.data());
	sanity_test(&my_params);	


	return 0;
}
