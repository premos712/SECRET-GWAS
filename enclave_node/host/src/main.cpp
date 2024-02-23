/*
 * Initialize and run a Server.
 */

#include "enclave_node.h"
#include <iostream>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: <EXE> [config_file] (optional: --simlation or --debug)" << std::endl;
        return 1;
    }

    // initialize our server and run it forever
    EnclaveNode* instance = EnclaveNode::get_instance(argv[1]);
    EnclaveNode::get_instance(argv[1]);
    instance->run();

    return 0;
}
