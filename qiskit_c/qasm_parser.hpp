#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include <fstream>
#include <regex>
#include <stdexcept>

#include <vector>
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <algorithm>
#include <cctype>
// ---------------------------------------------------------------------------
// Minimal QASM parser.
// ---------------------------------------------------------------------------
// Reads a QASM file and produces `gates_flat` in the same layout that
// utils.get_circuit_array() emits on the Python side:
//   * flat length 2 * num_gates
//   * for gate i: gates_flat[2*i] = q1, gates_flat[2*i + 1] = q2 (or -1
//     for single-qubit gates)
// Also reads `n` from the `qreg q[n];` declaration.
//
// Kept intentionally simple: assumes a single qreg named `q`, no measurement
// gates in the middle, no OpenQASM 3 syntax. Adequate for the small
// hand-crafted test.qasm circuits used to validate SABRE_routing_many.
struct ParsedCircuit {
    std::vector<double> gate_params;
    std::vector<int> gates_flat;
    std::vector<std::string> gate_names;
    int num_gates = 0;
    int n = 0;
};
static double parse_parameter(std::string expr) {

    const double pi = std::acos(-1.0);

    expr.erase(
        std::remove_if(expr.begin(), expr.end(),
            [](unsigned char c) { return std::isspace(c); }),
        expr.end()
    );

    // Numeric parameter
    if (expr.find("pi") == std::string::npos)
        return std::stod(expr);

    // Expressions: pi, -pi/2, 3*pi/4, etc.
    const std::regex pattern(
        R"(^([+-]?)(?:([0-9]+(?:\.[0-9]+)?)\*)?pi(?:/([0-9]+(?:\.[0-9]+)?))?$)"
    );

    std::smatch match;

    if (!std::regex_match(expr, match, pattern))
        throw std::runtime_error("Unsupported parameter: " + expr);

    double sign = match[1] == "-" ? -1.0 : 1.0;
    double coefficient = match[2].matched ? std::stod(match[2]) : 1.0;
    double divisor = match[3].matched ? std::stod(match[3]) : 1.0;

    if (divisor == 0.0)
        throw std::runtime_error("Division by zero");

    return sign * coefficient * pi / divisor;
}
static ParsedCircuit parse_qasm(const std::string& filename)
{
    std::ifstream f(filename);
    if (!f) {
        throw std::runtime_error("Cannot open QASM file: " + filename);
    }

    ParsedCircuit pc;
    const std::regex qreg_re (R"(qreg\s+\w+\s*\[\s*(\d+)\s*\])");
    const std::regex qubit_re(R"(q\s*\[\s*(\d+)\s*\])");

    std::string line;
    while (std::getline(f, line)) {
        // Strip end-of-line comment, then leading whitespace.
        if (auto pos = line.find("//"); pos != std::string::npos)
            line = line.substr(0, pos);
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip QASM header / classical-only / boundary lines.
        if (line.rfind("OPENQASM", 0) == 0) continue;
        if (line.rfind("include",  0) == 0) continue;
        if (line.rfind("barrier",  0) == 0) continue;
        if (line.rfind("measure",  0) == 0) continue;
        if (line.rfind("reset",    0) == 0) continue;
        if (line.rfind("creg",     0) == 0) continue;

        // Read qreg size.
        if (std::smatch m; std::regex_search(line, m, qreg_re)) {
            pc.n = std::stoi(m[1]);
            continue;
        }

        // Anything else is treated as a gate line; collect its q[N] operands.
        std::vector<int> qubits;
        for (auto it = std::sregex_iterator(line.begin(), line.end(), qubit_re);
             it != std::sregex_iterator();
             ++it)
        {
            qubits.push_back(std::stoi((*it)[1]));
        }
        if (qubits.empty()) continue;
        // Extract gate name
        size_t end = line.find_first_of(" \t(");
        pc.gate_names.push_back(line.substr(0, end));

                // Extract gate parameter
        double parameter = 0.0;

        size_t open = line.find('(');
        size_t close = line.find(')');

        if (open != std::string::npos &&
            close != std::string::npos &&
            close > open) {

           parameter = parse_parameter(
                line.substr(open + 1, close - open - 1)
            );

        }

        pc.gate_params.push_back(parameter);


        pc.gates_flat.push_back(qubits[0]);
        pc.gates_flat.push_back(qubits.size() >= 2 ? qubits[1] : -1);
        pc.num_gates++;
    }

    if (pc.n == 0) {
        throw std::runtime_error("No qreg declaration found in " + filename);
    }
    return pc;
}


#endif