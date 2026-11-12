#include <iostream>
#include <qiskit.h>

int main() {
    std::cout << "Qiskit C API sanity test\n";
    std::cout << "Header version: " << QISKIT_VERSION << "\n";
    std::cout << "Library version (hex): 0x"
              << std::hex << qk_api_version() << "\n";

    if (qk_api_version() != QISKIT_VERSION_HEX) {
        std::cerr << "ERROR: Header/library version mismatch\n";
        return 1;
    }

    std::cout << "SUCCESS: Qiskit C API is working!\n";
    return 0;
}
