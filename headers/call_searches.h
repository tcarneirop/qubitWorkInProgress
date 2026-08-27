#ifndef CALL_SEARCHERS_H
#define CALL_SEARCHERS_H

void call_searchers(const Parameters *my_params){

	sanity_test(my_params);

/* 
	if (my_params->search == 'o' || my_params->search == 't')
	{
		if (my_params->search == '0')
		{
			call_kchange(
			    my_params->PHYSIC_MACHINE, my_params->circuit_flat_gates_data,
			    my_params->circuit_flat_num_gates,
			    (long long)my_params->nb_physic,
			    (long long)my_params->nb_logic,
			    my_params->number_of_sabre_runs, my_params->num_random_sols);
		}
		else
		{
			if (my_params->search == 't')
			{
				call_kchange_vs_jurema(
				    my_params->PHYSIC_MACHINE, my_params->circuit_flat_gates_data,
				    my_params->circuit_flat_num_gates,
				    (long long) my_params->nb_physic,
				    (long long) my_params->nb_logic,
				    my_params->number_of_sabre_runs, my_params->num_random_sols, my_params->cutoff_depth);
			}
		}
	}
	else
	{ //// not k-changes

		if (my_params->num_random_sols > 0)
		{

			std::cout << "################# STARTING THE RANDOM SEARCH ##########################" << std::endl;

			best_depth = results[0].depth;
			best_num_gates = results[0].num_gates;
			memcpy(best_mapping, mapping.data(), my_params->nb_logic * sizeof(int));

			solutions = random_heuristic(
			    my_params->PHYSIC_MACHINE,
				my_params->circuit_flat_gates_data,
			    my_params->circuit_flat_num_gates,
			    my_params->nb_physic, my_params->nb_logic,
			    &best_depth,
			    &best_num_gates,
			    best_mapping,
			    my_params->number_of_sabre_runs, my_params->num_random_sols);

			std::cout << "################# END OF THE RANDOM SEARCH ##########################" << std::endl;
		}
		if (my_params->search == 'd')
		{
			std::cout << "############ STARTING THE DFS SEARCH ################" << std::endl;
			call_RANDOM_mcore_search(my_params->PHYSIC_MACHINE,
						 my_params->circuit_flat_gates_data,
						 my_params->circuit_flat_num_gates,
						 (long long)my_params->nb_physic,
						 (long long)my_params->nb_logic,
						 (long long)my_params->cutoff_depth,
						 &best_depth, &best_num_gates,
						 best_mapping, my_params->pool_percent,
						 my_params->number_of_sabre_runs,
						 my_params->num_sols_to_skip);
		}
		else
		{
			if (my_params->search == 'j' && my_params->num_random_sols > 0)
			{ // we can only do jurema having complete solutions
				std::cout << "############ STARTING THE JUREMA SEARCH ################" << std::endl;
				call_jurema(
				    my_params->PHYSIC_MACHINE,
				    my_params->circuit_flat_gates_data,
				    my_params->circuit_flat_num_gates,
				    my_params->nb_physic,
				    my_params->nb_logic,
				    solutions.data(),
				    my_params->cutoff_depth,
				    &best_depth,
				    &best_num_gates,
				    best_mapping,
				    &shared_sols_counter,
				    my_params->num_sols_to_skip,
				    my_params->number_of_sabre_runs,
				    my_params->num_random_sols);
			}
			else
			{
				std::cout << "############ ERROR: Wrong search parameters ################";
				exit(1);
			}
		} // else search d
	}
 */

}

#endif