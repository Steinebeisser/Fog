#include <stdio.h>
#include <string.h>
#include "scan.h"

#define PGS_LOG_IMPLEMENTATION
#ifndef PGS_LOG_ENABLED
#   define PGS_LOG_ENABLED false
#endif
#include "third_party/pgs_log.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [options]\n", argv[0]);
        return 1;
    }
    // pgs_log_minimal_log_level = PGS_LOG_DEBUG;
    PGS_LOG_DEBUG("Calling %s with %d args", argv[0], argc);
    char *command = argv[1];

    if (strcmp(command, "scan") == 0) {
        if (!cmd_scan(argc - 1, argv + 1)) {
            return 1;
        }
    }

    return 0;
}
