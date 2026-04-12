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

static bool is_excluded_dir(const char *name, bool no_exclude, bool no_mnt) {
    if (no_exclude)
        return false;

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


bool scan_directory(const char *path, const ScanOptions *opts) {
    char buf[GETDENTS64_CACHE_SIZE];
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    int nread;
    while ((nread = syscall(SYS_getdents64, fd, buf, sizeof(buf))) > 0)
    {
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

            if (entry->d_type == DT_DIR && !opts->no_exclude && is_excluded_dir(entry->d_name, opts->no_exclude, opts->skip_mnt))
                continue;

            char full_path[PATH_MAX];
            size_t len = strlen(path);
            memcpy(full_path, path, len);

            full_path[len++] = '/';

            size_t name_len = strlen(entry->d_name);
            memcpy(full_path + len, entry->d_name, name_len);
            len += name_len;

            full_path[len] = '\0';

            if (entry->d_type == DT_REG) {
                if (opts->on_file) {
                    struct STAT info;
                    if (get_file_info_at(fd, entry->d_name, &info)) {
                        opts->on_file(fd, entry->d_name, full_path, &info, opts->userdata);
                    }
                }
            } else if (entry->d_type == DT_DIR && opts->recursive) {

                if (opts->on_dir)
                    if (!opts->on_dir(full_path, opts->userdata))
                        continue;
                scan_directory(full_path, opts);
            } else if (entry->d_type == DT_UNKNOWN) {
                struct STAT info;
                if (!get_file_info_at(fd, entry->d_name, &info))
                    continue;

                if (is_directory(info) && opts->recursive) {
                    if (opts->on_dir) opts->on_dir(full_path, opts->userdata);
                    scan_directory(full_path, opts);
                } else if (is_file(info)) {
                    if (opts->on_file)
                        opts->on_file(fd, entry->d_name, full_path, &info, opts->userdata);
                }
            }
        }
    }

    close(fd);
    return true;
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
