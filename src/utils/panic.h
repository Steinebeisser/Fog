#ifndef FOG_PANIC
#define FOG_PANIC

#include <stddef.h>

#define STR(x) #x

#define PANIC(fmt, ...) \
    panic(__FILE__, __LINE__, fmt, ##__VA_ARGS__);

void panic(const char *file, int line, const char *fmt, ...);

#endif // FOG_PANIC
