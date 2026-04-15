#include <stddef.h>
#define _GNU_SOURCE

#include "scanner.h"

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <syscall.h>
#include <dirent.h>

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


bool scan_directory_fd(int dirfd, const char *path, const ScanOptions *opts) {
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

            const char *path_to_use = NULL;
            if (!opts->ignore_full_path) {
                char full_path[PATH_MAX];
                size_t len = strlen(current_path);
                memcpy(full_path, path, len);
                full_path[len++] = '/';
                size_t name_len = entry->d_reclen - (offsetof(struct linux_dirent64, d_name)) - 1;
                memcpy(full_path + len, entry->d_name, name_len);
                len += name_len;
                full_path[len] = '\0';
                path_to_use = full_path;
            }



            if (entry->d_type == DT_REG) {
                if (opts->on_file) {
                    if (opts->ignore_file_size) {
                        opts->on_file(dirfd, entry->d_name, path_to_use, NULL, opts->userdata);
                    } else {
                        struct STAT info;
                        if (get_file_info_at(dirfd, entry->d_name, &info)) {
                            opts->on_file(dirfd, entry->d_name, path_to_use, &info, opts->userdata);
                        }
                    }
                }
            } else if (entry->d_type == DT_DIR && opts->recursive) {

                if (opts->on_dir)
                    if (!opts->on_dir(path_to_use, opts->userdata))
                        continue;

                if (opts->recursive) {
                    int fd = openat(dirfd, entry->d_name, O_RDONLY | O_DIRECTORY);
                    if (fd < 0) continue;

                    scan_directory_fd(fd, path_to_use, opts);
                    close(fd);
                }
            } else if (entry->d_type == DT_UNKNOWN) {
                struct STAT info;
                if (!get_file_info_at(dirfd, entry->d_name, &info))
                    continue;

                if (is_directory(info) && opts->recursive) {
                    if (opts->on_dir) opts->on_dir(path_to_use, opts->userdata);
                    scan_directory_fd(dirfd, path_to_use, opts);
                } else if (is_file(info)) {
                    if (opts->on_file)
                        opts->on_file(dirfd, entry->d_name, path_to_use, &info, opts->userdata);
                }
            }
        }
    }
    return true;
}


bool scan_directory(const char *path, const ScanOptions *opts) {
    int root_fd = open(path, O_RDONLY | O_DIRECTORY);
    if (root_fd < 0)
        return false;

    bool ret = scan_directory_fd(root_fd, path, opts);
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
