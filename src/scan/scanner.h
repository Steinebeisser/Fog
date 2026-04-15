#ifndef FOG_SCANNER_H
#define FOG_SCANNER_H

#include <stdbool.h>
#include "utils/file_helper.h"

typedef struct {
    bool recursive;
    bool no_exclude;
    bool skip_hidden;
    bool skip_mnt;
    void (*on_file)(int dirfd, const char *name, const char *full_path,
                    struct STAT *info, void *userdata);
    bool (*on_dir)(const char *full_path, void *userdata);
    bool ignore_file_size;
    bool ignore_full_path;
    void *userdata;
} ScanOptions;

bool scan_path(const char *path, const ScanOptions *opts);

#endif // FOG_SCANNER_H
