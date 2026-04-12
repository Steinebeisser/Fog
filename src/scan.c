#define _GNU_SOURCE
#include "third_party/pgs_log.h"
#include "utils/file_helper.h"
#include "utils/fog_timer.h"
#include <fcntl.h>
#include <sys/syscall.h>
#include <stddef.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#define PGS_ARGS \
    PGS_ARG(PGS_ARG_FLAG, verbose, 'v', "verbose", "Enable verbose output", NULL) \
    PGS_ARG(PGS_ARG_FLAG, recursive, 'r', "recurseive", "Scan recusively", NULL) \
    PGS_ARG(PGS_ARG_FLAG, noexluce, 'e', "no-exluce", "By default some directories are excluded, this scans them anyways, can lead to weird behavior", NULL)

#define PGS_ARGS_IMPLEMENTATION
#include "third_party/pgs_args.h"

#ifndef GETDENTS64_CACHE_SIZE
#   define GETDENTS64_CACHE_SIZE 131072
#endif

PgsArgs scan_args;

typedef struct {
    int file_count;
    int dir_count;
    uint64_t total_size;
    FogTimer timer;
} ScanStats;

bool analyse_file(const char *path, ScanStats *stats, struct STAT stat) {
    PGS_LOG_DEBUG("Analysing file: %s", path);

    stats->file_count += 1;
    stats->total_size += get_filesize(stat);


    return true;
}

static bool is_excluded_dir(const char *name) {
    switch (name[0]) {
        case 'p': return name[1] == 'r' && name[2] == 'o' && name[3] == 'c' && name[4] == '\0'; // proc
        case 's': return name[1] == 'y' && name[2] == 's' && name[3] == '\0';                   // sys
        case 'd': return name[1] == 'e' && name[2] == 'v' && name[3] == '\0';                   // dev
        case 'r': return name[1] == 'u' && name[2] == 'n' && name[3] == '\0';                   // run
        default:  return false;
    }
}
struct linux_dirent64 {
   ino64_t        d_ino;    /* 64-bit inode number */
   off64_t        d_off;    /* Not an offset; see getdents() */
   unsigned short d_reclen; /* Size of this dirent */
   unsigned char  d_type;   /* File type */
   char           d_name[]; /* Filename (null-terminated) */
};


bool analyse_directory(const char *path, bool recursive, ScanStats *stats) {
    PGS_LOG_DEBUG("Analysing directory: %s", path);
    stats->dir_count += 1;

    char buf[GETDENTS64_CACHE_SIZE];
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    int nread;
    while ((nread = syscall(SYS_getdents64, fd, buf, sizeof(buf))) > 0)
    {
        int offset = 0;
        while (offset < nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buf + offset);
            offset += entry->d_reclen;

            PGS_LOG_DEBUG("Got Path %s in dir %s", entry->d_name, path);
            if (entry->d_name[0] == '.' || (entry->d_name[0] == '0' && entry->d_name[1] == '0'))
                continue;

            if (entry->d_type == DT_DIR && !scan_args.noexluce_present && is_excluded_dir(entry->d_name))
                continue;

            char full_path[PATH_MAX];
            size_t len = strlen(path);
            memcpy(full_path, path, len);

            full_path[len++] = '/';

            size_t name_len = strlen(entry->d_name);
            memcpy(full_path + len, entry->d_name, name_len);
            len += name_len;

            full_path[len] = '\0';

            if (entry->d_type == DT_DIR) {
                if (recursive)
                    analyse_directory(full_path, true, stats);
            } else if (entry->d_type == DT_REG) {
                struct STAT info;
                if (!get_file_info_at(fd, entry->d_name, &info)) {
                    PGS_LOG_ERROR("Failed to get filestats for file %s", full_path);
                    continue;
                }
                analyse_file(full_path, stats, info);
            } else if (entry->d_type == DT_UNKNOWN) {
                struct STAT info;
                if (!get_file_info_at(fd, entry->d_name, &info)) {
                    PGS_LOG_ERROR("Failed to get filestats for file %s", full_path);
                    continue;
                }
                if (is_directory(info) && recursive)
                    analyse_directory(full_path, true, stats);
                else if (is_file(info))
                    analyse_file(full_path, stats, info);
            }
        }
    }

    close(fd);

    return true;
}

#define KILOBYTE 1024ULL
#define MEGABYTE (KILOBYTE * 1024LL)
#define GiGABYTE (MEGABYTE * 1024LL)
#define TERABYTE (GiGABYTE * 1024LL)

void print_stats(ScanStats stats) {
    printf("File Count: %d\n", stats.file_count);
    printf("Dir Count: %d\n", stats.dir_count);

    if (stats.total_size > TERABYTE) {
        printf("Total Size: %llu TiB\n", stats.total_size / TERABYTE);
        printf("Debug size %lu\n", stats.total_size);
    } else if (stats.total_size > GiGABYTE) {
        printf("Total Size: %llu GiB\n", stats.total_size / GiGABYTE);
    } else if (stats.total_size > MEGABYTE) {
        printf("Total Size: %llu MiB\n", stats.total_size / MEGABYTE);
    } else if (stats.total_size > KILOBYTE) {
        printf("Total Size: %llu KiB\n", stats.total_size / KILOBYTE);
    } else {
        printf("Total Size: %zu Bytes\n", stats.total_size);
    }

    printf("Took %ld ms\n", timer_ms(stats.timer));
}


bool cmd_scan(int argc, char **argv) {
    if (!pgs_args_parse(&scan_args, argc, argv)) {
        return false;
    }

    if (scan_args.positional_count < 1) {
        fprintf(stderr, "Usage: scan <path>\n");
        return false;
    }

    const char *path = scan_args.positionals[0];

    if (scan_args.verbose_present) {
        pgs_log_minimal_log_level = PGS_LOG_DEBUG;
    } else {
        pgs_log_minimal_log_level = PGS_LOG_FATAL;
    }

    PGS_LOG_DEBUG("Path: %s", path);
    PGS_LOG_DEBUG("Recursive: %d", scan_args.recursive_present);
    PGS_LOG_DEBUG("Verbose: %d", scan_args.verbose_present);

    ScanStats stats = {0};
    timer_start(&stats.timer);

    struct STAT info;
    get_file_info(path, &info);

    if (is_directory(info)) {
        if (!analyse_directory(path, scan_args.recursive_present, &stats))
            return false;
    } else if (is_file(info)) {
        if (!analyse_file(path, &stats, info))
            return false;
    } else {
        fprintf(stderr, "Path is neither a directory nor file\n");
        return false;
    }

    timer_end(&stats.timer);


    print_stats(stats);

    return true;
}

