#define _GNU_SOURCE
#include "file_helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>


bool get_file_info(const char *path, struct stat *st) {
    return STAT(path, st) == 0;
}

bool get_file_info_at(int dirfd, const char *name, struct STAT *st) {
    return fstatat(dirfd, name, st, AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT) == 0;
}

bool is_directory(struct STAT stat)
{
#ifdef __linux__
    return (stat.st_mode & S_IFDIR);
#elif defined(__WIN32)

#else
#error "Unsupported Platform"
#endif
    return true;
}

bool is_file(struct STAT stat) {
#ifdef __linux__
    return (stat.st_mode & S_IFREG);
#elif defined(__WIN32)

#else
#error "Unsupported Platform"
#endif
    return true;
}

size_t get_filesize(struct STAT stat, bool apparent) {
    if (apparent)
        return stat.st_size;
    return stat.st_blocks * 512ULL;
}
