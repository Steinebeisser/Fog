#include "scan_cmd.h"
#include "utils/formatter.h"

#undef PGS_ARGS
#define PGS_ARGS \
    PGS_ARG(PGS_ARG_OPTIONAL, help, 'h', "help", "Show help message", NULL) \
    PGS_ARG(PGS_ARG_FLAG, verbose, 'v', "verbose", "Enable verbose output", NULL) \
    PGS_ARG(PGS_ARG_FLAG, recursive, 'r', "recurseive", "Scan recusively", NULL) \
    PGS_ARG(PGS_ARG_FLAG, noexluce, 'e', "no-exluce", "By default some directories are excluded, this scans them anyways, can lead to weird behavior", NULL) \
    PGS_ARG(PGS_ARG_FLAG, include_hidden, 'H', "include-hidden", "Include Hidden Files in the scan", NULL) \
    PGS_ARG(PGS_ARG_FLAG, include_mount, 'm', "include-mount", "Include Mount Files in the scan", NULL) \
    PGS_ARG(PGS_ARG_FLAG, decimal, 'd', "decimal", "Use decimal units (TB, GB, MB) instead of binary (TiB, GiB, MiB)", NULL) \
    PGS_ARG(PGS_ARG_FLAG, apparent, 'a', "apparent", "Show the logiacl size", NULL)


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
    uint64_t total_size;
    FogTimer timer;
} CmdScanData;


#include <sys/statvfs.h>


void print_stats(CmdScanData stats, const char *scan_path, bool decimal)
{
    printf("File Count : %d\n", stats.file_count);
    printf("Dir Count  : %d\n", stats.dir_count);

    printf("Total Size : ");
    print_human_size(stats.total_size, decimal);
    printf("\n");

    struct statvfs vfs;
    if (statvfs(scan_path, &vfs) == 0) {
        uint64_t total_capacity = (uint64_t)vfs.f_blocks * vfs.f_frsize;

        printf("Disk Size  : ");
        print_human_size(total_capacity, decimal);
        printf("\n");
    } else {
        perror("Warning: Could not get filesystem size");
    }

    printf("Time taken : %ld ms\n", timer_ms(stats.timer));
}

static void on_file(int dirfd, const char *name, const char *full_path,
                    struct STAT *info, void *userdata) {
    (void)dirfd;
    (void)name;
    (void)full_path;
    CmdScanData *d = userdata;
    d->file_count++;
    d->total_size += get_filesize(*info, scan_args.apparent_present);
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

    ScanOptions opts = {
        .userdata = &data,
        .skip_hidden = !scan_args.include_hidden_present,
        .skip_mnt = !scan_args.include_mount_present,
        .on_dir = on_dir,
        .on_file = on_file,
        .recursive = scan_args.recursive_present,
        .no_exclude = scan_args.noexluce_present
    };

    timer_start(&data.timer);
    if (!scan_path(path, &opts))
        return false;
    timer_end(&data.timer);

    print_stats(data, path, scan_args.decimal_present);

    return true;
}
