#include "formatter.h"
#include <stdio.h>

#define KILOBYTE 1024ULL
#define MEGABYTE (KILOBYTE * 1024ULL)
#define GIGABYTE (MEGABYTE * 1024ULL)
#define TERABYTE (GIGABYTE * 1024ULL)

void print_human_size(uint64_t bytes, bool decimal)
{
    if (decimal) {
        if (bytes >= 1000000000000ULL) {
            printf("%llu.%02llu TB", 
                   bytes / 1000000000000ULL,
                   (bytes % 1000000000000ULL) * 100ULL / 1000000000000ULL);
        }
        else if (bytes >= 1000000000ULL) {
            printf("%llu.%02llu GB", 
                   bytes / 1000000000ULL,
                   (bytes % 1000000000ULL) * 100ULL / 1000000000ULL);
        }
        else if (bytes >= 1000000ULL) {
            printf("%llu.%02llu MB", 
                   bytes / 1000000ULL,
                   (bytes % 1000000ULL) * 100ULL / 1000000ULL);
        }
        else if (bytes >= 1000ULL) {
            printf("%llu.%02llu KB", 
                   bytes / 1000ULL,
                   (bytes % 1000ULL) * 100ULL / 1000ULL);
        }
        else {
            printf("%lu Bytes", bytes);
        }
    } 
    else {
        if (bytes >= TERABYTE) {
            printf("%llu.%02llu TiB", bytes / TERABYTE,
                   (bytes % TERABYTE) * 100ULL / TERABYTE);
        }
        else if (bytes >= GIGABYTE) {
            printf("%llu.%02llu GiB", bytes / GIGABYTE,
                   (bytes % GIGABYTE) * 100ULL / GIGABYTE);
        }
        else if (bytes >= MEGABYTE) {
            printf("%llu.%02llu MiB", bytes / MEGABYTE,
                   (bytes % MEGABYTE) * 100ULL / MEGABYTE);
        }
        else if (bytes >= KILOBYTE) {
            printf("%llu.%02llu KiB", bytes / KILOBYTE,
                   (bytes % KILOBYTE) * 100ULL / KILOBYTE);
        }
        else {
            printf("%lu Bytes", bytes);
        }
    }
}
