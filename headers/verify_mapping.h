#ifndef VERIFY_MAPPING_H
#define VERIFY_MAPPING_H

void verify_mapping(const Parameters *my_params){

	std::cout<<"\n\n############## MAPPING VERIFICATION ##############"<<std::endl;

	std::vector<RoutingResult> results;
    if(my_params->permutation.size() != my_params->nb_logic){
		std::cerr<<"########## ERROR: The mapping size is different of the #logic qubits."<<std::endl;
		exit(1);
	}
	
	std::cout << "circuit_flat.filename: " << my_params->qasm_file << std::endl;
	std::cout<<"\nMapping: \n";
	for(auto m: my_params->permutation){
		std::cout<<m<<" ";
	}
	
	
	results = SABRE_routing_many(my_params->circuit_flat_gates_data, my_params->circuit_flat_num_gates, my_params->PHYSIC_MACHINE,my_params->nb_physic,
		 my_params->nb_logic, 1, my_params->permutation.data(), 1, my_params->number_of_sabre_runs, 1);

	std::cout<<"\nNumber of SABRE runs: "<< my_params->number_of_sabre_runs<<std::endl;	 
	std::cout<<"\tDepth: "<< results[0].depth<<std::endl;
	std::cout<<"\tGates: "<< results[0].num_gates <<std::endl;
	
}

#endif