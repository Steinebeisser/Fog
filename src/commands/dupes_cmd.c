#include "dupes_cmd.h"
#include "scan/scanner.h"
#include "utils/hashes/blake2b.h"
#include "utils/file_helper.h"
#include <fnmatch.h>
#include "utils/fog_timer.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "third_party/stb_ds.h"
#include "utils/formatter.h"

#define XXH_INLINE_ALL
#include "third_party/xxhash.h"

#undef PGS_ARGS
#define PGS_ARGS \
    PGS_ARG(PGS_ARG_OPTIONAL, help, 'h', "help", "Show help message", NULL) \
    PGS_ARG(PGS_ARG_FLAG, verbose, 'v', "verbose", "Enable verbose output", NULL) \
    PGS_ARG(PGS_ARG_FLAG, recursive, 'r', "recurseive", "Scan recusively", NULL) \
    PGS_ARG(PGS_ARG_FLAG, noexluce, 'e', "no-exluce", "By default some directories are excluded, this scans them anyways, can lead to weird behavior", NULL) \
    PGS_ARG(PGS_ARG_FLAG, include_hidden, 'H', "include-hidden", "Include Hidden Files in the scan", NULL) \
    PGS_ARG(PGS_ARG_FLAG, include_mount, 'm', "include-mount", "Include Mount Files in the scan", NULL) \
    PGS_ARG(PGS_ARG_VALUE, notify_step, 'n', "notify", "After how many files it should send a life signal", NULL) \
    PGS_ARG(PGS_ARG_VALUE, min_size, 's', "min-size", "Will ignore every file under this limit", NULL) \
    PGS_ARG(PGS_ARG_VALUE, top, 't', "top", "largest duplicate groups", NULL) \
    PGS_ARG(PGS_ARG_VALUE, exclude, 'E', "exclude", "Exclude files, comma separated list", NULL) \
    PGS_ARG(PGS_ARG_FLAG, summary, 'S', "summary", "Create Summary", NULL)




#define PGS_ARGS_FUNC_PREFIX dupe_args
#define PGS_ARGS_IMPLEMENTATION
#include "third_party/pgs_args.h"

#define STB_DS_IMPLEMENTATION
#include "third_party/stb_ds.h"

#define HEAD_SIZE 65536

dupe_args_t dupe_args;

typedef struct {
    uint64_t key;
    char **value;
} FileHashEntry;

typedef struct {
    FogTimer timer;
    FileHashEntry* size_map;
    int notify_step;
    int file_count;
    uint64_t min_size;
    char **exclude_pattern;
} CmdDupeData;

static uint64_t hash_file_head(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    uint8_t buf[65536];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n <= 0) return 0;

    return XXH3_64bits(buf, (size_t)n);
    // uint64_t out = 0;
    // blake2b(buf, (size_t)n, &out, sizeof(out));
    // return out;
}

static uint64_t hash_file_full(const char *path) {
    XXH3_state_t state;
    // XXH3_state_t *state = XXH3_createState();
    // if (!state) return 0;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    XXH3_64bits_reset(&state);

    uint8_t buf[131072];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
        XXH3_64bits_update(&state, buf, n);

    close(fd);

    return XXH3_64bits_digest(&state);;
}

static bool should_execute(const char *path, char **patterns) {
    for (int i = 0; i < arrlen(patterns); ++i) {
      const char *pat = patterns[i];

        if (fnmatch(pat, path, 0) == 0)
            return true;

        const char *p = path;
        while (*p) {
            const char *slash = strchr(p, '/');
            size_t len = slash ? (size_t)(slash - p) : strlen(p);

            char segment[256];
            if (len >= sizeof(segment))
                len = sizeof(segment) - 1;

            memcpy(segment, p, len);
            segment[len] = '\0';

            if (fnmatch(pat, segment, 0) == 0)
                return true;

            if (!slash)
                break;

            p = slash + 1;
        }
    }
    return false;
}

static void on_file(int dirfd, const char *name, const char *full_path,
                    struct STAT *info, void *userdata) {
    if (!full_path)
        return;
    (void)dirfd;
    (void)name;
    CmdDupeData *data = (CmdDupeData *)userdata;

    if (get_filesize(*info, true) < data->min_size)
            return;

    if (data->exclude_pattern && should_execute(full_path, data->exclude_pattern))
        return;

    data->file_count++;
    if (data->notify_step > 0 && data->file_count % data->notify_step == 0)
        fprintf(stderr, "  scanning... %d files found\r", data->file_count);

    uint64_t file_size = get_filesize(*info, true);

    FileHashEntry *entry = stbds_hmgetp_null(data->size_map, file_size);
    if (!entry) {
        char **paths = NULL;
        arrput(paths, strdup(full_path));
        hmput(data->size_map, file_size, paths);
    } else {
        arrput(entry->value, strdup(full_path));
    }
}

static bool on_dir(const char *full_path, void *userdata) {
    CmdDupeData *data = (CmdDupeData *)userdata;

    if (should_execute(full_path, data->exclude_pattern))
        return false;

    return true;
}

bool cmd_dupe(int argc, char **argv) {
    if (!dupe_args_parse(&dupe_args, argc, argv, false)) {
        return true;
    }

    if (dupe_args.help_present) {
        if (dupe_args.help_value) {
            dupe_args_print_help_specific_name(dupe_args.help_value);
        } else {
            dupe_args_print_help();
        }
        return true;
    }

    if (dupe_args.positional_count < 1) {
        fprintf(stderr, "Usage: scan <path> [options]\n");
        return false;
    }

    const char *path = dupe_args.positionals[0];

    CmdDupeData data = {0};

    data.notify_step = dupe_args.notify_step_present
        ? atoi(dupe_args.notify_step_value)
        : 0;

    data.min_size = dupe_args.min_size_present
        ? pgs_args_parse_size(dupe_args.min_size_value)
        : 0;

    if (dupe_args.exclude_present) {
        char *tmp = strdup(dupe_args.exclude_value);
        char *tok = strtok(tmp, ",");

        while (tok) {
            arrput(data.exclude_pattern, tok);
            tok = strtok(NULL, ",");
        }
    }

    ScanOptions opts = {
        .userdata = &data,
        .skip_hidden = !dupe_args.include_hidden_present,
        .skip_mnt = !dupe_args.include_mount_present,
        .on_dir = on_dir,
        .on_file = on_file,
        .recursive = dupe_args.recursive_present,
        .no_exclude = dupe_args.noexluce_present
    };

    timer_start(&data.timer);
    if (scan_path(path, &opts) != true) {
        printf("Failed to scan path\n");
        return false;
    }
    printf("    scan done: %d files\n", data.file_count);

    int max_size_groups = hmlen(data.size_map);
    int candidates = 0;

    for (int i = 0; i < max_size_groups; i++) {
        int len = arrlen(data.size_map[i].value);
        if (len >= 2)
            candidates += arrlen(data.size_map[i].value);
    }

    printf("    %d candidate files to hash\n", candidates);

    int candidates_left = candidates;

    FileHashEntry *hash_map = NULL;
    hmdefault(hash_map, NULL);
    hmreserve(hash_map, candidates);

    int hashed = 0;
    int small_files = 0;
    int large_files = 0;



    FileHashEntry *head_map = NULL;
    hmdefault(head_map, NULL);
    hmreserve(head_map, candidates);

    for (int i = 0; i < max_size_groups; ++i) {
        char **paths = data.size_map[i].value;
        int len = arrlen(paths);
        if (len < 2) continue;

        uint64_t file_size = data.size_map[i].key;

        if (file_size <= HEAD_SIZE) {
            small_files += len;
            candidates_left -= len;

            for (int j = 0; j < len; ++j) {
                FileHashEntry *entry = stbds_hmgetp_null(head_map, 0);
                if (!entry) {
                    char **paths1 = NULL;
                    arrput(paths1, paths[j]);
                    hmput(head_map, 0, paths1);
                } else {
                    arrput(entry->value, paths[j]);
                }
            }
            continue;
        }


        large_files += len;
        for (int j = 0; j < len; ++j) {
            uint64_t hash = hash_file_head(data.size_map[i].value[j]);
            hashed += 1;

            if (data.notify_step > 0 && hashed % data.notify_step == 0)
                fprintf(stderr, "    head hashing... %d/%d\r", hashed, candidates_left);

            FileHashEntry *entry = stbds_hmgetp_null(head_map, hash);
            if (!entry) {
                char **paths1 = NULL;
                arrput(paths1, paths[j]);
                hmput(head_map, hash, paths1);
            } else {
                arrput(entry->value, paths[j]);
            }

        }
    }
    fprintf(stderr, "\n    head hash done (%d small, %d large)\n", small_files, large_files);

    int max_head_groups = hmlen(head_map);
    int survivors = 0;

    for (int i = 0; i < max_head_groups; ++i) {
        int len = arrlen(head_map[i].value);
        if (len >= 2)
            survivors += len;
    }

    printf("    %d files need full hashing\n", survivors);

    hashed = 0;

    for (int i = 0; i < max_head_groups; ++i) {
        char **paths = head_map[i].value;
        int len = arrlen(paths);
        if (len < 2) continue;

        for (int j = 0; j < len; ++j) {
            uint64_t h = hash_file_full(paths[j]);
            hashed += 1;

            if (data.notify_step > 0 && hashed % data.notify_step == 0)
                fprintf(stderr, "    full hashing... %d/%d\r", hashed, survivors);

            FileHashEntry *entry = hmgetp_null(hash_map, h);
            if (!entry) {
                char **ps = NULL;
                arrput(ps, paths[j]);
                hmput(hash_map, h, ps);
            } else {
                arrput(entry->value, paths[j]);
            }
        }
    }

    fprintf(stderr, "\n   full hash done\n");

    hmfree(head_map);
    timer_end(&data.timer);


    int groups = 0;
    int dupe_files = 0;
    uint64_t wasted = 0;
    int max_group_member = 0;
    uint64_t max_group_size = 0;
    int max_group_members = 0;
    uint64_t max_members_size = 0;

    for (int i = 0; i < hmlen(hash_map); i++) {
        char **paths = hash_map[i].value;
        int len = arrlen(paths);
        if (len < 2) continue;

        struct STAT info;
        get_file_info(paths[0], &info);

        groups += 1;
        dupe_files += len;
        size_t size = get_filesize(info, true);
        wasted += size * (len - 1);

        if (len > max_group_members) {
            max_group_members = len;
            max_members_size = size;
        }

        if (size > max_group_size) {
            max_group_size = size;
            max_group_member = len;
        }
    }

    if (dupe_args.summary_present) {
        printf("\n| Duplicate scan summary\n");
        printf("-----------------------\n");
        printf("| Groups: %d\n", groups);
        printf("| Duplicate files: %d\n", dupe_files);
        printf("| Wasted: ");
        print_human_size(wasted, false);
        printf("\n| Largest Group By Size: %d files (", max_group_member);
        print_human_size(max_group_size, false);
        printf(" per member)\n");
        printf("| Largest Group By Members: %d files (", max_group_members);
        print_human_size(max_members_size, false);
        printf(" per member)\n");
        printf("\n\n\n");
    }


    printf("Time taken : %ld ms\n", timer_ms(data.timer));
    printf("             %lu  sec\n", timer_ms(data.timer) / 1000);
    printf("             %0.1f  min\n", (double)timer_ms(data.timer) / 1000 / 60);

    hmfree(hash_map);
    hmfree(data.size_map);

    return true;
}
