#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "commands/dupes_cmd.h"
#include "commands/scan_cmd.h"
#include "utils/blake2b.h"

#define PGS_LOG_IMPLEMENTATION
#ifndef PGS_LOG_ENABLED
#   define PGS_LOG_ENABLED false
#endif
#include "third_party/pgs_log.h"

#define PGS_ARGS \
    PGS_ARG(PGS_ARG_SUBCOMMAND, scan, 0, NULL, "Scan a folder/file", NULL) \
    PGS_ARG(PGS_ARG_SUBCOMMAND, dupe, 0, NULL, "Scans a folder and returns duplicte files", NULL) \
    PGS_ARG(PGS_ARG_OPTIONAL, help, 'h', "help", "Show help message", NULL)
#define PGS_ARGS_IMPLEMENTATION
#include "third_party/pgs_args.h"

int main(int argc, char **argv) {
    pgs_args_t args = {0};
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [options]\n", argv[0]);
        return 1;
    }

    if (!pgs_args_parse(&args, argc, argv, true)) {
        printf("Failed to parse args\n");
        return false;
    }



    if (args.scan_present) {
        return !cmd_scan(argc - 1, argv + 1);
    } else if (args.dupe_present) {
        return !cmd_dupe(argc - 1, argv + 1);
    }


    if (args.help_present) {
        if (args.help_value) {
            pgs_args_print_help_specific_name(args.help_value);
        } else {
            pgs_args_print_help();
        }
        return true;
    }

    // pgs_log_minimal_log_level = PGS_LOG_DEBUG;
    return 0;
}
