#ifndef SANITY_H
#define SANITY_H

#include <vector>

void sanity_test(const Parameters *my_params){

	std::cout << "########### SANITY TEST ################# " << "\n";

	std::vector<int> solutions;
	std::vector<int> mapping( my_params->nb_logic );
    	std::iota(mapping.begin(), mapping.end(), 0);

	for (auto m : mapping)
		std::cout << m << " ";
	std::cout << "\n";
	std::vector<RoutingResult> results = SABRE_routing_many(my_params->circuit_flat_gates_data,my_params->circuit_flat_num_gates, my_params->PHYSIC_MACHINE, my_params->nb_physic, my_params->nb_logic, 1, mapping.data(), 1, 1, 1);

	std::cout << "depth: " << results[0].depth << "\n";
	std::cout << "num_gates: " << results[0].num_gates << "\n";

	std::cout << "################# END OF SANITY TEST ##########################" << std::endl;

}

#endif