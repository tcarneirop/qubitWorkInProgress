#ifndef ROUTING_CORE_HPP
#define ROUTING_CORE_HPP


#pragma once

#include <cstdint>
#include <vector>

struct RoutingResult {
    int num_gates;
    int depth;
};

std::vector<RoutingResult> SABRE_routing_many(
    const int* gates_flat,
    int num_gates_in,
    const int* dist,
    int N,
    int n,
    int P,
    const int* mappings_data,
    uint32_t base_seed,
    int num_trials,
    int num_threads
);


#endif