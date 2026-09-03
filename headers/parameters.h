#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <CLI/CLI.hpp>

int cli_parameters_parser(Parameters *my_params, int argc, char *argv[])
{
	int ret = 0;

	CLI::App app{"Qubit mapper"};

	app.add_option("--topology", my_params->topology, "Topology: albatroz (16), boeblingen (20), cairo (27), melbourne(15)")->required()->check(CLI::IsMember({"albatroz", "cairo", "boeblingen", "melbourne"}));

	app.add_option("--qasmfile", my_params->qasm_file, "QASM input file")
		->required();

	app.add_option("--depth-percent", my_params->percent_of_the_permutation, "Fixed percent of the permutation to generate a pool / fix the Jurema search")->check([](const std::string &value)
																																								   {
		double v = std::stod(value);
		if (v < 0.1 || v > 1.0)
			return std::string{"must be between 0.1 and 1.0"};
		return std::string{}; });

	app.add_option("--pool-percent", my_params->pool_percent,
				   "Percentage of the pool if DFS is used/percentage of the solutions pool to explore.")
		->check([](const std::string &value)
				{
		double v = std::stod(value);
		if (v < 0.1 || v > 1.0)
			return std::string{"must be between 0.1 and 1.0"
		};
	return std::string{}; });

	app.add_option("--sabre-runs", my_params->number_of_sabre_runs, "Number of SABRE runs - default: 20 runs")->check(CLI::PositiveNumber);
	app.add_option("--num-rand-sols", my_params->num_random_sols, "Number of random sols to get a solution and also serve as guide for jurema - default: 100 sols")->check(CLI::PositiveNumber);

	app.add_option("--sols-skip", my_params->num_sols_to_skip, "Number of complete sols found that do not improve the current incumbent - default: 0, do not check this condition. ");

	std::string search;

	app.add_option(
		   "--search",
		   search,
		   "Search: DFS - d, Jurema - j, K-changes - k, tests - t")
		->check(CLI::IsMember({"d", "j", "k", "t", "r"}));


	app.add_option("--permutation", my_params->permutation, "Permutation");

	if ((my_params->search == 'd' || my_params->search == 'j') &&
		app.get_option("--depth-percent")->count() == 0)
	{
		throw CLI::ValidationError(
			"--depth-percent is required when --search is 'd' or 'j'");
	}
	

	CLI11_PARSE(app, argc, argv);

	


	my_params->search = (char)search[0];

	return ret;
}

void start_parameters_circuit(Parameters *my_params, const int circuit_flat_n, const int circuit_flat_num_gates, int *circuit_flat_gates_flat)
{

	if (my_params->topology == "albatroz")
	{
		my_params->PHYSIC_MACHINE = ALBATROZ;
		my_params->nb_physic = 16;
	}
	else 
		if (my_params->topology == "cairo")
		{
			my_params->PHYSIC_MACHINE = CAIRO;
			my_params->nb_physic = 27;
		}
		else
		{
			if (my_params->topology == "boeblingen")
			{
				my_params->PHYSIC_MACHINE = BOEBLINGEN;
				my_params->nb_physic = 20;
			}
			else
				if(my_params->topology == "melbourne"){
					my_params->PHYSIC_MACHINE = MELBOURNE_15;
					my_params->nb_physic = 15;
				}
				else
					throw std::runtime_error("Unknown topology: " + my_params->topology);
	}


	my_params->nb_logic = circuit_flat_n;
	my_params->circuit_flat_num_gates = circuit_flat_num_gates;
	my_params->circuit_flat_gates_data = circuit_flat_gates_flat;
	my_params->cutoff_depth = my_params->percent_of_the_permutation * my_params->nb_logic;


    std::cout<<"################# PRINTING PARAMETERS: ################# "<<"\n";

    	std::cout << "circuit_flat.filename: " << my_params->qasm_file << std::endl;
	std::cout << "circuit_flat.n (logic): " << my_params->nb_logic << std::endl;
	std::cout << "circuit_flat.num_gates:" << my_params->circuit_flat_num_gates << std::endl;
	std::cout << "Number of SABRE runs: " << my_params->number_of_sabre_runs << std::endl;
	std::cout << "Physic QUBITS: " << (long long)(my_params->nb_physic) << " Logic QUBITS: " << (long long)(my_params->nb_logic) << std::endl;
	std::cout << "Number of random sols: " << my_params->num_random_sols << std::endl;
	std::cout << "Number of sols to skip: " << my_params->num_sols_to_skip << std::endl;
	std::cout << "Search: " << my_params->search << std::endl;
	std::cout << "Cutoff depth: " << my_params->cutoff_depth << std::endl;
	std::cout << "\tPercentage of the permutation: " << my_params->percent_of_the_permutation * 100 << "%" << std::endl;
	std::cout << "Number of random sols: " << my_params->num_random_sols << std::endl;
	std::cout << "Percentage of the pool to explore: " << my_params->pool_percent * 100 << "\%" << std::endl;
	if(my_params->permutation.size()>0){
		std::cout<<"Permutation to check: \n";
		for(int x : my_params->permutation)
    		std::cout << x << " ";	
			std::cout<<std::endl;
		if(my_params->permutation.size()>0 && my_params->permutation.size()!=my_params->nb_logic){
			std::cout<<"########### ERROR! \n\t"<<"-- Permutation size != nb_logic"<<std::endl;
			exit(1);
		}
	}
		
	 std::cout<<"######################################################## "<<"\n";
}

#endif