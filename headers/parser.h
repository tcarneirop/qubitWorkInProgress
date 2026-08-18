#ifndef PARSER_H
#define PARSER_H


#include <vector>

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
    std::vector<int> gates_flat;
    int num_gates = 0;
    int n = 0;
};

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