#ifndef FOG_BLAKE2B_H
#define FOG_BLAKE2B_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t input_buffer[128];
    uint64_t chained_state[8];
    uint64_t total_num_bytes[2];
    size_t pointer_for_input_buffer;
    size_t outlen;
} Blake2bState;

void blake2b(const void *data, size_t len, void *out, size_t out_len);

void blake2b_init   (Blake2bState *s, size_t out_len);
void blake2b_update (Blake2bState *s, const void *data, size_t len);
void blake2b_final  (Blake2bState *s, void *out);

bool blake2b_file(const char *path, void *out, size_t out_len);

#endif // FOG_BLAKE2B_H
