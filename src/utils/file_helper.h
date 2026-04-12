#ifndef FOG_FILE_HELPER_H
#define FOG_FILE_HELPER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __linux__
    #include <sys/stat.h>
    #define STAT stat
#elif defined(_WIN32)
    #include <windows.h>
    #define STAT _stat
#endif

bool get_file_info(const char *path, struct STAT *st);
bool get_file_info_at(int dirfd, const char *name, struct STAT *st);

bool is_directory(struct STAT info);
bool is_file(struct STAT info);
size_t get_filesize(struct STAT stat);


#endif // FOG_FILE_HELPER_H
