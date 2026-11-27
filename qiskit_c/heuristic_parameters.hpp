#ifndef H_PAR_HPP
#define H_PAR_HPP

#include <iostream>
#include <string>
#include <random>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

struct HeuristicParameters {
    size_t num_trials = 5;
    uint32_t seed = std::random_device{}();
    int64_t sabre_seed = std::random_device{}();
    size_t heuristic = 2;
    uint8_t optimization_level = 0;
    bool translate = false;
    bool optimize = false;
    bool schedule = false;
    std::string scheduling = "asap";
    std::string topology = "montreal";
};

inline void print_heuristic_results(std::vector<uint32_t> initial_mapping,  
    MappingResult initial_result, std::vector<uint32_t> best_mapping,
    MappingResult best_result,
    const HeuristicParameters &params, 
    size_t evaluations,
    double total_routing_time,
    double total_time
    ){

        /*
         * Step 6: Report the best result.
         */
        std::cout << "\n=== HEURISTIC RESULTS ===\n";


        std::cout << "Initial mapping: ";
        print_mapping(initial_mapping);
        std::cout << "Initial depth: "
                  << initial_result.depth << "\n";
        std::cout << "Initial gates: "
          << initial_result.gates << "\n";
        std::cout << "Initial SWAPs: "
                  << initial_result.swaps << "\n";
        if (params.schedule) std::cout << std::fixed << std::setprecision(10)
          << "Initial makespan (s): "
          << initial_result.makespan_seconds
          << std::defaultfloat << std::setprecision(6) << '\n';

        std::cout << "\n====================\n";
        
        std::cout << "Best mapping: ";
        print_mapping(best_mapping);
        std::cout << "Best depth: "
                  << best_result.depth << "\n";
        std::cout << "Best gates: "
          << best_result.gates << "\n";
        std::cout << "Best SWAPs: "
                  << best_result.swaps << "\n";
        if (params.schedule) std::cout << std::fixed << std::setprecision(10)
          << "Best makespan (s): "
          << best_result.makespan_seconds
          << std::defaultfloat << std::setprecision(6) << '\n';

    
        std::cout << "\n====================\n";
        std::cout << "Neighbors evaluated: "
                  << evaluations << "\n";
        std::cout << "Total SABRE Routing time: "
                  << total_routing_time << " seconds\n";
        std::cout << "Total heuristic evaluation time: "
          << total_time << " seconds\n";


}


inline void print_parmeters(const HeuristicParameters &params){

    std::cout<<"\nPARAMETERS: \n";
    std::cout << "Topology: " << params.topology << "\n";

    std::cout << "Heuristic: "
          << ( params.heuristic == 2 ? "2-change" : "3-change") << "\n";
    std::cout << "Mapping seed: " <<  params.seed << "\n";
    std::cout << "SABRE seed: " <<  params.sabre_seed << "\n";
    std::cout << "SABRE Routing trials: " <<  params.num_trials << "\n";
    std::cout << "Optimization level: "
            << static_cast<unsigned int>(params.optimization_level) << "\n";
    std::cout << "Translating in the pipeline: "
            << ( params.translate ? "Yes" : "No") << "\n";
    std::cout << "Optimizing in the pipeline: "
            << ( params.optimize ? "Yes" : "No") << "\n";
    std::cout << "Scheduling in the pipeline: "
            << ( params.schedule ? "Yes" : "No") << "\n";
    std::cout << "Scheduler: "
            << ( params.schedule ? params.scheduling : "No") << "\n";

    std::cout<<"############################################\n\n";

}

inline HeuristicParameters parameters_reading(int argc, char* argv[]) {

    HeuristicParameters params;

    if (argc < 4 || argc > 12) {
        std::cerr << "Usage: " << argv[0]
                  << " <file.qasm> <heuristic> <num_trials>"
                  << " [optimization_level] [translate: 0|1]"
                  << " [optimize: 0|1] [schedule: 0|1]"
                  << " [topology] [asap|alap]"
                  << " [mapping_seed] [sabre_seed]\n";

        throw std::invalid_argument("Invalid number of arguments");
    }

    params.heuristic = std::stoul(argv[2]);
    params.num_trials = std::stoull(argv[3]);

    if (params.num_trials == 0)
        throw std::invalid_argument("num_trials must be greater than zero");

    if (argc >= 5) {
        const unsigned long level = std::stoul(argv[4]);

        if (level > 3)
            throw std::invalid_argument("optimization_level must be between 0 and 3");

        params.optimization_level = static_cast<uint8_t>(level);
    }

    if (argc >= 6) {
        const unsigned long value = std::stoul(argv[5]);

        if (value > 1)
            throw std::invalid_argument("translate must be 0 or 1");

        params.translate = (value == 1);
    }

    if (argc >= 7) {
        const unsigned long value = std::stoul(argv[6]);

        if (value > 1)
            throw std::invalid_argument("optimize must be 0 or 1");

        params.optimize = (value == 1);
    }

    if (argc >= 8) {
        const unsigned long value = std::stoul(argv[7]);

        if (value > 1)
            throw std::invalid_argument("schedule must be 0 or 1");

        params.schedule = (value == 1);
    }

    if (argc >= 9)
        params.topology = argv[8];

    if (argc >= 10)
        params.scheduling = argv[9];

    if (argc >= 11)
        params.seed = std::stoul(argv[10]);

    if (argc >= 12)
        params.sabre_seed = std::stoll(argv[11]);

    if (params.scheduling != "asap" && params.scheduling != "alap")
        throw std::invalid_argument("scheduling must be asap or alap");

    if (params.schedule && (!params.translate || !params.optimize))
        throw std::invalid_argument("Scheduling requires Translation and Optimization");

    return params;
}

#endif