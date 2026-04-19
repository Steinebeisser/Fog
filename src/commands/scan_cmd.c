#include "scan_cmd.h"
#include "utils/formatter.h"
#include <inttypes.h>
#include <stdint.h>

#ifdef __linux__
#include <sys/statvfs.h>
#elif defined(__WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define IGNORE_FS_SHORT_FLAG 'f'
#define IGNORE_FS_LONG_FLAG "ignore-fs"

#undef PGS_ARGS
#define PGS_ARGS \
    PGS_ARG(PGS_ARG_OPTIONAL, help, 'h', "help", "Show help message", NULL) \
    PGS_ARG(PGS_ARG_FLAG, verbose, 'v', "verbose", "Enable verbose output", NULL) \
    PGS_ARG(PGS_ARG_FLAG, recursive, 'r', "recurseive", "Scan recusively", NULL) \
    PGS_ARG(PGS_ARG_FLAG, noexluce, 'e', "no-exluce", "By default some directories are excluded, this scans them anyways, can lead to weird behavior", NULL) \
    PGS_ARG(PGS_ARG_FLAG, include_hidden, 'H', "include-hidden", "Include Hidden Files in the scan", NULL) \
    PGS_ARG(PGS_ARG_FLAG, include_mount, 'm', "include-mount", "Include Mount Files in the scan", NULL) \
    PGS_ARG(PGS_ARG_VALUE, notify_step, 'n', "notify", "After how many files it should send a life signal", NULL) \
    PGS_ARG(PGS_ARG_FLAG, decimal, 'd', "decimal", "Use decimal units (TB, GB, MB) instead of binary (TiB, GiB, MiB)", NULL) \
    PGS_ARG(PGS_ARG_FLAG, ignore_file_size, IGNORE_FS_SHORT_FLAG, IGNORE_FS_LONG_FLAG, "Ignores File Size, improves speed but doesnt show total size", NULL) \
    PGS_ARG(PGS_ARG_FLAG, apparent, 'a', "apparent", "Show the logiacl size", NULL) \
    PGS_ARG(PGS_ARG_FLAG, count_stats, 'c', "count_stats", "Counts how many files are hidden, specifici size groups etc", NULL) \
    PGS_ARG(PGS_ARG_VALUE, size, 's', "size", "Only counts file for the specific size", NULL)


#define PGS_ARGS_FUNC_PREFIX scan_args
#define PGS_ARGS_IMPLEMENTATION
#include "third_party/pgs_args.h"

#ifndef GETDENTS64_CACHE_SIZE
#   define GETDENTS64_CACHE_SIZE 131072
#endif

#include "scan/scanner.h"
#include "third_party/pgs_log.h"
#include "utils/fog_timer.h"

scan_args_t scan_args;

typedef struct {
    int file_count;
    int dir_count;
    int notify_step;
    uint64_t total_size;
    FogTimer timer;
    bool scan_stats;
    int hidden_count;
    int empty_files;
    uint64_t file_size;
    uint64_t file_count_size;

    int below_4kib;
    int below_16kib;
    int below_64kib;
    int below_1mib;
    int below_16mib;
    int below_1gib;
    int above_1gib;

    uint64_t size_empty;
    uint64_t size_below_4kib;
    uint64_t size_below_16kib;
    uint64_t size_below_64kib;
    uint64_t size_below_1mib;
    uint64_t size_below_16mib;
    uint64_t size_below_1gib;
    uint64_t size_above_1gib;
} CmdScanData;

#define PCT(part, total) \
    ((total) > 0 ? (double)(part) * 100.0 / (double)(total) : 0.0)


void print_stats(CmdScanData stats, const char *scan_path, bool decimal)
{
    printf("\nScan Summary\n");
    printf("============\n");

    printf("Total files     : %'d\n", stats.file_count);

    if (stats.scan_stats) {
        if (stats.file_size > 0) {
            print_human_size(stats.file_size, decimal);
            printf("        : %zu Files", stats.file_count_size);
        } else if (stats.hidden_count) {
            double hidden_pct = (double)stats.hidden_count * 100.0 / stats.file_count;
            printf("Hidden files    : %'d (%.2f%%)\n", stats.hidden_count, hidden_pct);
        }

        if (stats.total_size > 0 && !stats.file_size) {
            printf("\nSize distribution:\n");

            struct {
                const char *range;
                uint64_t count;
                uint64_t size;
            } rows[] = {
                {"  0-4 KiB    ", stats.below_4kib,     stats.size_below_4kib},
                {"  4-16 KiB   ", stats.below_16kib,    stats.size_below_16kib},
                {" 16-64 KiB   ", stats.below_64kib,    stats.size_below_64kib},
                {" 64 KiB-1 MiB", stats.below_1mib,     stats.size_below_1mib},
                {"  1-16 MiB   ", stats.below_16mib,    stats.size_below_16mib},
                {" 16 MiB-1 GiB", stats.below_1gib,     stats.size_below_1gib},
                {"  > 1 GiB    ", stats.above_1gib,     stats.size_above_1gib},
            };

            for (int i = 0; i < 7; i++) {
                double file_pct = PCT(rows[i].count, stats.file_count);
                double size_pct = PCT(rows[i].size,  stats.total_size);

                printf("%-15s : %'12" PRIu64 " files (%5.2f%%)   ",
                       rows[i].range,
                       rows[i].count,
                       file_pct);

                print_human_size(rows[i].size, decimal);
                printf(" (%5.2f%%)\n", size_pct);
            }
        }
    }

    printf("\n");
    printf("Dir Count       : %'d\n", stats.dir_count);

    printf("Total Size      : ");
    print_human_size(stats.total_size, decimal);
    if (scan_args.ignore_file_size_present) {
        printf(" (ignored via ignore fs flag [-%c | --%s])",
               IGNORE_FS_SHORT_FLAG, IGNORE_FS_LONG_FLAG);
    }
    printf("\n");

#ifdef __linux__
    struct statvfs vfs;
    if (statvfs(scan_path, &vfs) == 0) {
        uint64_t total_capacity = (uint64_t)vfs.f_blocks * vfs.f_frsize;
        printf("Disk Size       : ");
        print_human_size(total_capacity, decimal);
        printf("\n");
    } else {
        perror("Warning: Could not get filesystem size");
    }
#elif defined(__WIN32)
    (void)scan_path;
    // TODO: Windows disk size
#endif

    printf("Time taken      : %'lu ms\n", timer_ms(stats.timer));
}

static void on_file(int dirfd, const char *name, const char *full_path,
                    struct STAT *info, void *userdata) {
    (void)dirfd;
    (void)name;
    (void)full_path;
    CmdScanData *d = userdata;
    d->file_count++;
    if (d->notify_step > 0 && d->file_count % d->notify_step == 0)
        fprintf(stderr, "  scanning... %d files found\r", d->file_count);

    if (info)
        d->total_size += get_filesize(*info, scan_args.apparent_present);


    if (d->scan_stats) {
        if (name[0] == '.')
            d->hidden_count += 1;
        if (info) {
            uint64_t size = info->st_size;
            if (d->file_size > 0) {
                if (size == d->file_size)
                    d->file_count_size += 1;
                return;
            }


            if (size == 0) {
                d->empty_files++;
                d->size_empty += 0;
            } else if (size < 4 * KIB) {
                d->below_4kib++;
                d->size_below_4kib += size;
            } else if (size < 16 * KIB) {
                d->below_16kib++;
                d->size_below_16kib += size;
            } else if (size < 64 * KIB) {
                d->below_64kib++;
                d->size_below_64kib += size;
            } else if (size < 1 * MIB) {
                d->below_1mib++;
                d->size_below_1mib += size;
            } else if (size < 16 * MIB) {
                d->below_16mib++;
                d->size_below_16mib += size;
            } else if (size < 1 * GIB) {
                d->below_1gib++;
                d->size_below_1gib += size;
            } else {
                d->above_1gib++;
                d->size_above_1gib += size;
            }
        }
    }
}

static bool on_dir(const char *full_path, void *userdata) {
    (void)full_path;
    CmdScanData *d = userdata;
    d->dir_count++;
    return true;
}

bool cmd_scan(int argc, char **argv) {
    if (!scan_args_parse(&scan_args, argc, argv, false)) {
        return false;
    }

    if (scan_args.help_present) {
        if (scan_args.help_value) {
            scan_args_print_help_specific_name(scan_args.help_value);
        } else {
            scan_args_print_help();
        }
        return true;
    }

    if (scan_args.positional_count < 1) {
        fprintf(stderr, "Usage: scan <path> [options]\n");
        return false;
    }

    const char *path = scan_args.positionals[0];

    if (scan_args.verbose_present) {
        pgs_log_minimal_log_level = PGS_LOG_DEBUG;
    } else {
        pgs_log_minimal_log_level = PGS_LOG_FATAL;
    }

    CmdScanData data = {0};

    data.notify_step = scan_args.notify_step_present
        ? atoi(scan_args.notify_step_value)
        : 0;

    data.scan_stats = scan_args.count_stats_present;

    data.file_size = scan_args.size_present
        ? pgs_args_parse_size(scan_args.size_value)
        : 0;

    if (data.file_size > 0)
        data.scan_stats = true;

    ScanOptions opts = {
        .userdata = &data,
        .skip_hidden = !scan_args.include_hidden_present,
        .skip_mnt = !scan_args.include_mount_present,
        .on_dir = on_dir,
        .on_file = on_file,
        .recursive = scan_args.recursive_present,
        .no_exclude = scan_args.noexluce_present,
        .ignore_file_size = scan_args.ignore_file_size_present,
        .path_arena = NULL
    };

    timer_start(&data.timer);
    if (!scan_path(path, &opts))
        return false;
    timer_end(&data.timer);

    print_stats(data, path, scan_args.decimal_present);

    return true;
}
