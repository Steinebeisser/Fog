#include "hash_cmd.h"
#include "utils/hashes/blake2b.h"
#include "utils/file_helper.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define HASH_ALGOS \
    X(blake2b_256, HASH_BLAKE2B_256, b256, 32) \
    X(blake2b_512, HASH_BLAKE2B_512, b512, 64)

typedef enum {
#define X(name, enumv, short_name, size) \
    enumv,
HASH_ALGOS
#undef X
} HashAlgorithm;

static HashAlgorithm parse_algorithm(const char *s) {
#define X(name, enumv, short_name, size) \
    if (strcmp(s, #name) == 0 || strcmp(s, #short_name) == 0) return enumv;
    HASH_ALGOS
#undef X
    return HASH_BLAKE2B_256;
}

#define X(name, enumv, short_name, size) \
    #name "|" #short_name "|"
const char algo_valid[] = HASH_ALGOS;
#undef X

static size_t algo_digest_size(HashAlgorithm a) {
    switch (a) {
#define X(name, enumv, short_name, size) \
        case enumv: return size;
    HASH_ALGOS
#undef X
    }
    return 64;
}


#undef PGS_ARGS
#define PGS_ARGS \
    PGS_ARG(PGS_ARG_OPTIONAL, help, 'h', "help", "Show help message", NULL) \
    PGS_ARG(PGS_ARG_VALUE, algorithm, 'a', "algorithm", "Choose the algorithm", algo_valid) \
    PGS_ARG(PGS_ARG_VALUE, input, 'i', "input", "Hash a given input", NULL) \
    PGS_ARG(PGS_ARG_VALUE, check, 'c', "check", "Check a file with a known value", NULL)


#define PGS_ARGS_FUNC_PREFIX hash_args
#define PGS_ARGS_IMPLEMENTATION
#include "third_party/pgs_args.h"

typedef struct {
    HashAlgorithm algorithm;
} CmdHashData;

hash_args_t hash_args;

#define HASH_MAX_SIZE 64

typedef struct {
    uint8_t bytes[HASH_MAX_SIZE];
    size_t  len;
} Hash;

static void hash_to_hex(const Hash *hash, char *out) {
    for (size_t i = 0; i < hash->len; ++i)
        sprintf(out + i * 2, "%02x", hash->bytes[i]);
    out[hash->len * 2] = '\0';
}

bool cmd_hash(int argc, char **argv) {
    if (!hash_args_parse(&hash_args, argc, argv, false)) {
        return true;
    }

    if (hash_args.help_present) {
        if (hash_args.help_value) {
            hash_args_print_help_specific_name(hash_args.help_value);
        } else {
            hash_args_print_help();
        }
        return true;
    }

    CmdHashData data = {0};

    if (hash_args.algorithm_present) {
        data.algorithm = parse_algorithm(hash_args.algorithm_value);
    }

    if (hash_args.positional_count < 1 && !hash_args.input_present) {
        fprintf(stderr, "Usage: hash <path> [options]\n");
        return false;
    }

    char *content = NULL;
    if (hash_args.input_present) {
        content = (char *)hash_args.input_value;
    }

    bool is_file_present = false;
    if (!content) {
        struct STAT info;
        if (!get_file_info(hash_args.positionals[0], &info)) {
            fprintf(stderr, "Couldnt get Information about current path and no manual content provided\n");
            return false;
        }

        if (!is_file(info)) {
            fprintf(stderr, "No file or manual content provided");
            return false;
        }
        is_file_present = true;
    }

    Hash hash = {0};
    hash.len = algo_digest_size(data.algorithm);

    switch (data.algorithm) {
        case HASH_BLAKE2B_256:
        case HASH_BLAKE2B_512:
            if (is_file_present)
                blake2b_file(hash_args.positionals[0], hash.bytes, hash.len);
            else
                blake2b(content, strlen(content), hash.bytes, hash.len);
            break;
    }

    char hex[HASH_MAX_SIZE * 2 + 1];
    hash_to_hex(&hash, hex);

    if (hash_args.check_present) {
        if (strcmp(hex, hash_args.check_value) == 0)
            printf("[OK]\n");
        else
            printf("[FAILED]\n");
    }

    printf("%s\n", hex);
    printf("  %s\n", is_file_present ? hash_args.positionals[0] : content);

    return true;
}
