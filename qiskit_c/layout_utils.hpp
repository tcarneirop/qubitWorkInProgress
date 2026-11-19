#ifndef LAYOUT_HPP
#define LAYOUT_HPP

#include <random>

/*
 * Execute SABRE Layout + Routing.
 *
 * Parameters:
 *   circuit           - Input circuit, modified in place.
 *   target            - Hardware topology.
 *   seed              - Random seed.
 *   max_iterations    - Forward-backward routing iterations.
 *   num_swap_trials   - Routing trials per iteration.
 *   num_random_trials - Random initial layout trials.
 * These are for getting a better mapping
 *
 * Returns:
 *   Transpile layout. The caller must release it.
 */
inline QkTranspileLayout* run_sabre_layout(
    QkCircuit* circuit,
    const QkTarget* target,
    uint64_t seed = 42,
    size_t max_iterations = 4,
    size_t num_swap_trials = 20,
    size_t num_random_trials = 20
) {

    
    std::mt19937_64 rng(std::random_device{}());
     
    ///for us, the   size_t num_swap_trials = 20 
    /* Initialize SABRE options. */
    QkSabreLayoutOptions options = qk_sabre_layout_options_default();
    //optionsis seed, max it, swap trails and random trails

    /* Configure the search parameters. */

    
    options.max_iterations = max_iterations;
    options.num_swap_trials = num_swap_trials;
    options.num_random_trials = num_random_trials;

    /* Execute SABRE Layout and Routing. */
    return qk_transpiler_pass_standalone_sabre_layout(
        circuit,
        target,
        &options
    );
}

#endif