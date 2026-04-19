#define _GNU_SOURCE

#include "scanner.h"

#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/types.h>
#include <linux/limits.h>

#include "utils/arena.h"
#include "utils/panic.h"

#ifndef GETDENTS64_CACHE_SIZE
#   define GETDENTS64_CACHE_SIZE 131072
#endif

static bool is_excluded_dir(const char *name, bool no_mnt) {
    switch (name[0]) {
        case 'p': return name[1] == 'r' && name[2] == 'o' && name[3] == 'c' && name[4] == '\0'; // proc
        case 's': return name[1] == 'y' && name[2] == 's' && name[3] == '\0';                   // sys
        case 'd': return name[1] == 'e' && name[2] == 'v' && name[3] == '\0';                   // dev
        case 'r': return name[1] == 'u' && name[2] == 'n' && name[3] == '\0';                   // run
        default:  break;
    }

    if (no_mnt) {
        switch (name[0]) {
            case 'm': return name[1] == 'n' && name[2] == 't' && name[3] == '\0';
        }
    }

    return false;
}
struct linux_dirent64 {
   ino64_t        d_ino;    /* 64-bit inode number */
   off64_t        d_off;    /* Not an offset; see getdents() */
   unsigned short d_reclen; /* Size of this dirent */
   unsigned char  d_type;   /* File type */
   char           d_name[]; /* Filename (null-terminated) */
};


bool scan_directory_fd(int dirfd, const char *path, size_t path_len, const ScanOptions *opts) {
    char buf[GETDENTS64_CACHE_SIZE];
    int nread;

    const char *current_path = path;

    while ((nread = syscall(SYS_getdents64, dirfd, buf, sizeof(buf))) > 0) {
        int offset = 0;
        while (offset < nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(buf + offset);
            offset += entry->d_reclen;

            if (entry->d_name[0] == '.') {
                if (entry->d_name[1] == '\0' ||
                    (entry->d_name[1] == '.' && entry->d_name[2] == '\0')) {
                    continue;
                }

                if (opts->skip_hidden)
                    continue;
            }

            if (entry->d_type == DT_DIR && !opts->no_exclude && is_excluded_dir(entry->d_name, opts->skip_mnt))
                continue;

            Arena *a = NULL;
            if (opts->path_arena) {
                a = opts->path_arena;
                if (!a->base) {
                    if (!arena_init(a, 1 << 29))
                        PANIC("Couldnt Allocate Arena Memory\n");
                }
            }



            if (entry->d_type == DT_REG) {
                if (opts->on_file) {
                    char *full = NULL;

                    if (a) {
                        size_t base_len = path_len;
                        size_t name_len = strlen(entry->d_name);
                        size_t total_len = base_len + name_len + 2; // add '\0'



                        full = arena_alloc(a, total_len);
                        if (!full)
                            PANIC("FAILED ALLOCATION, NEED RESIZE\n");
                        memcpy(full, current_path, base_len);

                        full[base_len] = '/';
                        memcpy(full + base_len + 1, entry->d_name, name_len);
                        full[total_len - 1] = '\0';
                        // PANIC("FULL: %s\nCURRENT PATH: %s\n", full, current_path);
                    }
                    if (opts->ignore_file_size) {
                        opts->on_file(dirfd, entry->d_name, full, NULL, opts->userdata);
                    } else {
                        struct STAT info;
                        if (get_file_info_at(dirfd, entry->d_name, &info)) {
                            opts->on_file(dirfd, entry->d_name, full, &info, opts->userdata);
                        }
                    }
                }
            } else if (entry->d_type == DT_DIR && opts->recursive) {
                // size_t saved_pos = 0;
                char *full = NULL;
                size_t total_len = 0;


                if (a) {
                    // saved_pos = a->pos;
                    size_t base_len = path_len;
                    size_t name_len = strlen(entry->d_name);
                    total_len = base_len + name_len + 2; // add '\0'


                    full = arena_alloc(a, total_len);
                    if (!full)
                        PANIC("FAILED ALLOCATION, NEED RESIZE\n");

                    memcpy(full, current_path, base_len);
                    full[base_len] = '/';
                    memcpy(full + base_len + 1, entry->d_name, name_len + 1);
                    full[total_len - 1] = '\0';
                }



                if (opts->on_dir)
                    if (!opts->on_dir(full, opts->userdata))
                        continue;

                if (opts->recursive) {
                    int fd = openat(dirfd, entry->d_name, O_RDONLY | O_DIRECTORY);
                    if (fd < 0) continue;

                    scan_directory_fd(fd, full, total_len - 1, opts);
                    close(fd);

                    // if (a)
                    //     a->pos = saved_pos;
                    // PANIC("");
                }

            } else if (entry->d_type == DT_UNKNOWN) {
                PANIC("UNSUPPORTED FILE SYSTEM\n");
                // struct STAT info;
                // if (!get_file_info_at(dirfd, entry->d_name, &info))
                //     continue;
                //
                // if (is_directory(info) && opts->recursive) {
                //     if (opts->on_dir) opts->on_dir(path_to_use, opts->userdata);
                //     scan_directory_fd(dirfd, path_to_use, opts);
                // } else if (is_file(info)) {
                //     if (opts->on_file)
                //         opts->on_file(dirfd, entry->d_name, path_to_use, &info, opts->userdata);
                // }
            }
        }
    }
    return true;
}


bool scan_directory(const char *path, const ScanOptions *opts) {
    int root_fd = open(path, O_RDONLY | O_DIRECTORY);
    if (root_fd < 0)
        return false;

    size_t path_len = 0;
    if (path)
        path_len = strlen(path);

    // PANIC("PATH LEN: %zu\nPath: %s\n", path_len, path);

    char to_use_path[PATH_MAX];
    memcpy(to_use_path, path, path_len);

    if (to_use_path[path_len - 1] == '/')
        to_use_path[path_len-- - 1] = '\0';
    else
        to_use_path[path_len] = '\0';

    bool ret = scan_directory_fd(root_fd, to_use_path, path_len, opts);
    close(root_fd);
    return ret;
}

bool scan_path(const char *path, const ScanOptions *opts) {
    struct STAT info;
    if (!get_file_info(path, &info)) return false;

    if (is_directory(info))
        return scan_directory(path, opts);
    else if (is_file(info)) {
        if (opts->on_file)
            opts->on_file(-1, path, path, &info, opts->userdata);
        return true;
    }
    return false;
}
