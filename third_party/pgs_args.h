/* PGS_ARGS - v0.1.0 - Public Domain - https://github.com/Steinebeisser/pgs/blob/master/pgs_args.h
 *
 * USAGE:
 * Define PGS_ARGS macro with your arguments before including this header.
 * Each argument is defined as: PGS_ARG(type, name, short_flag, long_flag, description, valid_values)
 *
 * ARGUMENT TYPES:
 *   - PGS_ARG_FLAG: Boolean flag (no value)
 *       Usage: -v, --verbose
 *       Sets name_present=true, name_value="1"
 *
 *   - PGS_ARG_VALUE: Requires a value
 *       Short form: -o file.txt  OR  -ofile.txt
 *       Long form:  --output file.txt
 *       Sets name_present=true, name_value="file.txt"
 *
 *   - PGS_ARG_OPTIONAL: Optional value (must use prefix syntax if provided)
 *       Short form without value: -t              -> name_value=NULL
 *       Short form with value:    -t=file.txt     -> name_value="file.txt"
 *                                 -tfile.txt       -> name_value="file.txt"
 *                                 -t:file.txt      -> name_value="file.txt"
 *       Long form without value:  --dump-tokens   -> name_value=NULL
 *       Long form with value:     --dump-tokens=file.txt  -> name_value="file.txt"
 *                                 --dump-tokens:file.txt  -> name_value="file.txt"
 *
 *       Note: For optional args, the value MUST be attached/prefixed. Arguments like
 *       "-t nextarg" will treat "nextarg" as a positional argument, not as the value for -t.
 *       This prevents optional args from accidentally consuming positional arguments.
 *
 * valid_values: Pipe-separated list of valid values (e.g., "0|1|2|3|s" for optimization levels)
 *               Use NULL for no validation
 *
 * EXAMPLE:
 * #define PGS_ARGS \
 *     PGS_ARG(PGS_ARG_OPTIONAL, help, 'h', "help", "Show help message", NULL) \
 *     PGS_ARG(PGS_ARG_FLAG, verbose, 'v', "verbose", "Enable verbose output", NULL) \
 *     PGS_ARG(PGS_ARG_VALUE, output, 'o', "output", "Output file", NULL) \
 *     PGS_ARG(PGS_ARG_VALUE, optimize, 'O', "optimize", "Optimization level", "0|1|2|3|s")
 *
 * USAGE EXAMPLES:
 *   program -v file.txt                  # verbose flag, file.txt is positional
 *   program -o output.txt file.txt       # output value, file.txt is positional
 *   program -h topic file.txt            # help has no value, "topic" and "file.txt" are positional
 *   program -h=topic file.txt            # help value is "topic", file.txt is positional
 *   program --help=advanced              # help value is "advanced"
 */

#ifndef PGS_ARGS_H
#define PGS_ARGS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
    PGS_ARG_FLAG,
    PGS_ARG_VALUE,
    PGS_ARG_OPTIONAL
} PgsArgType;

typedef struct {
#define PGS_ARG(arg_type, name, short_flag, long_flag, description, valid_values) \
    bool name##_present; \
    const char *name##_value;
PGS_ARGS
#undef PGS_ARG

    const char **positionals;
    int positional_count;
} PgsArgs;

typedef enum {
#define PGS_ARG(arg_type, name, short_flag, long_flag, description, valid_values) \
    PGS_ARG_##name,
PGS_ARGS
#undef PGS_ARG
    PGS_ARG_COUNT
} PgsArgId;

typedef struct {
    const char *name;
    char short_flag;
    const char *long_flag;
    const char *description;
    const char *valid_values;
    PgsArgType type;
} PgsArgMeta;

static const PgsArgMeta pgs_arg_meta[PGS_ARG_COUNT] = {
#define PGS_ARG(arg_type, name, short_flag, long_flag, description, valid_values) \
    [PGS_ARG_##name] = { #name, short_flag, long_flag, description, valid_values, arg_type },
    PGS_ARGS
#undef PGS_ARG
};

bool pgs_args_parse(PgsArgs *args, int argc, char** argv);
void pgs_args_print_help(void);
void pgs_args_print_help_specific_id(PgsArgId arg_id);
void pgs_args_print_help_specific_name(const char *name);

#endif // PGS_ARGS_H

#ifdef PGS_ARGS_IMPLEMENTATION

static PgsArgId pgs_args_find_by_short(char flag) {
    for (int i = 0; i < PGS_ARG_COUNT; ++i) {
        if (pgs_arg_meta[i].short_flag == flag) {
            return i;
        }
    }
    return PGS_ARG_COUNT;
}

static PgsArgId pgs_args_find_by_long(const char *flag) {
    for (int i = 0; i < PGS_ARG_COUNT; ++i) {
        if (pgs_arg_meta[i].long_flag && strcmp(pgs_arg_meta[i].long_flag, flag) == 0) {
            return i;
        }
    }
    return PGS_ARG_COUNT;
}

static PgsArgId pgs_args_find_by_name(const char *name) {
    for (int i = 0; i < PGS_ARG_COUNT; ++i) {
        if (strcmp(pgs_arg_meta[i].name, name) == 0) {
            return i;
        }
    }
    return PGS_ARG_COUNT;
}

bool pgs_args_validate_value(PgsArgId arg_id, const char *value) {
    if (arg_id >= PGS_ARG_COUNT) return false;

    const char *valid_values = pgs_arg_meta[arg_id].valid_values;
    if (!valid_values || !value) return true;

    char *valid_copy = strdup(valid_values);
    char *token = strtok(valid_copy, "|");

    while (token) {
        if (strcmp(token, value) == 0) {
            free(valid_copy);
            return true;
        }
        token = strtok(NULL, "|");
    }

    free(valid_copy);
    return false;
}

static void pgs_args_set_value(PgsArgs *args, PgsArgId arg_id, const char *value) {
    switch (arg_id) {
#define PGS_ARG(arg_type, name, short_flag, long_flag, description, valid_values) \
        case PGS_ARG_##name: \
            args->name##_present = true; \
            args->name##_value = value; \
            break;
        PGS_ARGS
#undef PGS_ARG
        default: break;
    }
}

bool pgs_args_parse(PgsArgs *args, int argc, char** argv) {
    if (!args || !argv || argc < 0) return false;

    memset(args, 0, sizeof(PgsArgs));
    args->positionals = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (arg[0] == '-' && arg[1] == '-') {
            const char *flag_name = arg + 2;

            const char *eq_pos = strchr(flag_name, '=');
            const char *colon_pos = strchr(flag_name, ':');
            const char *sep_pos = NULL;

            if (eq_pos && (!colon_pos || eq_pos < colon_pos)) {
                sep_pos = eq_pos;
            } else if (colon_pos) {
                sep_pos = colon_pos;
            }

            char flag_name_only[256];
            if (sep_pos) {
                size_t len = sep_pos - flag_name;
                if (len >= sizeof(flag_name_only)) len = sizeof(flag_name_only) - 1;
                strncpy(flag_name_only, flag_name, len);
                flag_name_only[len] = '\0';
                flag_name = flag_name_only;
            }

            PgsArgId arg_id = pgs_args_find_by_long(flag_name);

            if (arg_id == PGS_ARG_COUNT) {
                fprintf(stderr, "Error: Unknown argument '--%s'\n", flag_name);
                return false;
            }

            const PgsArgMeta *meta = &pgs_arg_meta[arg_id];

            if (meta->type == PGS_ARG_FLAG) {
                pgs_args_set_value(args, arg_id, "1");
            } else if (meta->type == PGS_ARG_VALUE) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: Argument '--%s' requires a value\n", flag_name);
                    return false;
                }
                const char *value = argv[++i];
                if (!pgs_args_validate_value(arg_id, value)) {
                    fprintf(stderr, "Error: Invalid value '%s' for '--%s'. Valid values: %s\n",
                            value, flag_name, meta->valid_values);
                    return false;
                }
                pgs_args_set_value(args, arg_id, value);
            } else if (meta->type == PGS_ARG_OPTIONAL) {
                const char *value = sep_pos ? (sep_pos + 1) : NULL;
                pgs_args_set_value(args, arg_id, value);
            }
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            for (const char *f = arg + 1; *f; ++f) {
                PgsArgId arg_id = pgs_args_find_by_short(*f);
                if (arg_id == PGS_ARG_COUNT) {
                    fprintf(stderr, "Error: Unknown argument '-%c'\n", *f);
                    return false;
                }
                const PgsArgMeta *meta = &pgs_arg_meta[arg_id];

                if (meta->type == PGS_ARG_FLAG) {
                    pgs_args_set_value(args, arg_id, "1");
                } else if (meta->type == PGS_ARG_OPTIONAL) {
                    const char *value = NULL;
                    if (f[1] == '=' || f[1] == ':') {
                        value = f + 2;
                    } else if (f[1] != '\0') {
                        value = f + 1;
                    }
                    pgs_args_set_value(args, arg_id, value);
                    break;
                } else {
                    const char *value = (f[1] != '\0') ? (f + 1) : NULL;
                    if (!value) {
                        if (i + 1 >= argc) {
                            fprintf(stderr, "Error: Argument '-%c' requires a value\n", *f);
                            return false;
                        }
                        value = argv[++i];
                    }
                    if (value && !pgs_args_validate_value(arg_id, value)) {
                        fprintf(stderr, "Error: Invalid value '%s' for '-%c'\n", value, *f);
                        return false;
                    }
                    pgs_args_set_value(args, arg_id, value);
                    break;
                }
            }
        }
        else {
            args->positionals = realloc(args->positionals, sizeof(char*) * (args->positional_count + 1));
            if (!args->positionals) {
                fprintf(stderr, "Error: Out of memory\n");
                return false;
            }
            args->positionals[args->positional_count++] = arg;
        }
    }

    return true;
}

void pgs_args_print_help(void) {
    printf("Available arguments:\n\n");

    for (int i = 0; i < PGS_ARG_COUNT; ++i) {
        const PgsArgMeta *meta = &pgs_arg_meta[i];

        printf("  ");
        if (meta->short_flag) {
            printf("-%c", meta->short_flag);
            if (meta->long_flag) printf(", ");
        }
        if (meta->long_flag) {
            printf("--%s", meta->long_flag);
        }

        printf("\n      %s\n", meta->description);

        if (meta->valid_values) {
            printf("      Valid values: %s\n", meta->valid_values);
        }

        printf("\n");
    }
}

void pgs_args_print_help_specific_name(const char *name) {
    PgsArgId arg_id = pgs_args_find_by_name(name);
    if (arg_id == PGS_ARG_COUNT) {
        fprintf(stderr, "Failed to find argument for `%s`\n", name);
        return;
    }

    pgs_args_print_help_specific_id(arg_id);
}

void pgs_args_print_help_specific_id(PgsArgId arg_id) {

    if (arg_id == PGS_ARG_COUNT) {
        fprintf(stderr, "Error: Unknown argument");
        pgs_args_print_help();
        return;
    }

    const PgsArgMeta *meta = &pgs_arg_meta[arg_id];

    printf("Help for '%s':\n\n", meta->name);
    printf("  ");
    if (meta->short_flag) {
        printf("-%c", meta->short_flag);
        if (meta->long_flag) printf(", ");
    }
    if (meta->long_flag) {
        printf("--%s", meta->long_flag);
    }
    printf("\n\n");

    printf("  %s\n\n", meta->description);

    if (meta->valid_values) {
        printf("  Valid values: %s\n", meta->valid_values);
    }

    const char *type_str = "unknown";
    switch (meta->type) {
        case PGS_ARG_FLAG: type_str = "flag (no value needed)"; break;
        case PGS_ARG_VALUE: type_str = "requires value"; break;
        case PGS_ARG_OPTIONAL: type_str = "optional value"; break;
    }
    printf("  Type: %s\n", type_str);
}

#endif // PGS_ARGS_IMPLEMENTATION

#ifndef PGS_ARGS_STRIP_PREFIX_GUARD_
#define PGS_ARGS_STRIP_PREFIX_GUARD_

    #ifdef PGS_ARGS_STRIP_PREFIX

        #define args_parse pgs_args_parse
        #define args_print_help pgs_args_print_help
        #define args_print_help_specific_id pgs_args_print_help_specific_id
        #define args_print_help_specific_name pgs_args_print_help_specific_name

    #endif // PGS_ARGS_STRIP_PREFIX

#endif // PGS_ARGS_STRIP_PREFIX_GUARD_

/*
    Revision History:

        0.1.0 (2025-12-22) Initial Release
*/


/*
   ------------------------------------------------------------------------------
   This software is available under 2 licenses -- choose whichever you prefer.
   ------------------------------------------------------------------------------
   ALTERNATIVE A - MIT License
   Copyright (c) 2025 Paul Geisthardt
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   ------------------------------------------------------------------------------
   ALTERNATIVE B - Public Domain (www.unlicense.org)
   This is free and unencumbered software released into the public domain.
   Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
   software, either in source code form or as a compiled binary, for any purpose,
   commercial or non-commercial, and by any means.
   In jurisdictions that recognize copyright laws, the author or authors of this
   software dedicate any and all copyright interest in the software to the public
   domain. We make this dedication for the benefit of the public at large and to
   the detriment of our heirs and successors. We intend this dedication to be an
   overt act of relinquishment in perpetuity of all present and future rights to
   this software under copyright law.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
   ------------------------------------------------------------------------------
*/
