#include "blake2b.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define R1 32
#define R2 24
#define R3 16
#define R4 63

#ifndef ROTR64
#define ROTR64(x, y)  (((x) >> (y)) ^ ((x) << (64 - (y))))
#endif


static const uint64_t blake2b_iv[8] = {
   0x6A09E667F3BCC908, 0xBB67AE8584CAA73B,
   0x3C6EF372FE94F82B, 0xA54FF53A5F1D36F1,
   0x510E527FADE682D1, 0x9B05688C2B3E6C1F,
   0x1F83D9ABFB41BD6B, 0x5BE0CD19137E2179
};

static const uint8_t SIGMA[12][16] = {
    {  0 ,  1 ,  2 ,  3 ,  4 ,  5 ,  6 ,  7 ,  8 ,  9 ,  10,  11,  12,  13,  14,  15 },
    {  14,  10,  4 ,  8 ,  9 ,  15,  13,  6 ,  1 ,  12,  0 ,  2 ,  11,  7 ,  5 ,  3  },
    {  11,   8,  12,   0,   5,   2,  15,  13,  10,  14,   3,   6,   7,   1,   9,   4 },
    {  7 ,   9,   3,   1,  13,  12,  11,  14,   2,   6,   5,  10,   4,   0,  15,   8 },
    {   9,   0,   5,   7,   2,   4,  10,  15,  14,   1,  11,  12,   6,   8,   3,  13 },
    { 2  ,  12,   6,  10,   0,  11,   8,   3,   4,  13,   7,   5,  15,  14,   1,   9 },
    { 12 ,   5,   1,  15,  14,  13,   4,  10,   0,   7,   6,   3,   9,   2,   8,  11 },
    { 13 ,  11,   7,  14,  12,   1,   3,   9,   5,   0,  15,   4,   8,   6,   2,  10 },
    { 6  ,  15,  14,   9,  11,   3,   0,   8,  12,   2,  13,   7,   1,   4,  10,   5 },
    { 10 ,   2,   8,   4,   7,   6,   1,   5,  15,  11,   9,  14,   3,  12,  13,   0 },
    {  0 ,  1 ,  2 ,  3 ,  4 ,  5 ,  6 ,  7 ,  8 ,  9 ,  10,  11,  12,  13,  14,  15 },
    {  14,  10,  4 ,  8 ,  9 ,  15,  13,  6 ,  1 ,  12,  0 ,  2 ,  11,  7 ,  5 ,  3  }
};

#define B2B_G(a, b, c, d, x, y) {   \
   v[a] = v[a] + v[b] + x;         \
   v[d] = ROTR64(v[d] ^ v[a], 32); \
   v[c] = v[c] + v[d];             \
   v[b] = ROTR64(v[b] ^ v[c], 24); \
   v[a] = v[a] + v[b] + y;         \
   v[d] = ROTR64(v[d] ^ v[a], 16); \
   v[c] = v[c] + v[d];             \
   v[b] = ROTR64(v[b] ^ v[c], 63); }


#define B2B_GET64(p)                            \
   (((uint64_t) ((uint8_t *) (p))[0]) ^        \
   (((uint64_t) ((uint8_t *) (p))[1]) << 8) ^  \
   (((uint64_t) ((uint8_t *) (p))[2]) << 16) ^ \
   (((uint64_t) ((uint8_t *) (p))[3]) << 24) ^ \
   (((uint64_t) ((uint8_t *) (p))[4]) << 32) ^ \
   (((uint64_t) ((uint8_t *) (p))[5]) << 40) ^ \
   (((uint64_t) ((uint8_t *) (p))[6]) << 48) ^ \
   (((uint64_t) ((uint8_t *) (p))[7]) << 56))

static void blake2b_compress(Blake2bState *s, int last) {
        int i;
        uint64_t v[16], m[16];

        for (i = 0; i < 8; i++) {           // init work variables
            v[i] = s->chained_state[i];
            v[i + 8] = blake2b_iv[i];
        }

        v[12] ^= s->total_num_bytes[0];                 // low 64 bits of offset
        v[13] ^= s->total_num_bytes[1];                 // high 64 bits
        if (last)                           // last block flag set ?
            v[14] = ~v[14];

        for (i = 0; i < 16; i++)            // get little-endian words
            m[i] = B2B_GET64(&s->input_buffer[8 * i]);

        for (i = 0; i < 12; i++) {          // twelve rounds
            B2B_G( 0, 4,  8, 12, m[SIGMA[i][ 0]], m[SIGMA[i][ 1]]);
            B2B_G( 1, 5,  9, 13, m[SIGMA[i][ 2]], m[SIGMA[i][ 3]]);
            B2B_G( 2, 6, 10, 14, m[SIGMA[i][ 4]], m[SIGMA[i][ 5]]);
            B2B_G( 3, 7, 11, 15, m[SIGMA[i][ 6]], m[SIGMA[i][ 7]]);
            B2B_G( 0, 5, 10, 15, m[SIGMA[i][ 8]], m[SIGMA[i][ 9]]);
            B2B_G( 1, 6, 11, 12, m[SIGMA[i][10]], m[SIGMA[i][11]]);
            B2B_G( 2, 7,  8, 13, m[SIGMA[i][12]], m[SIGMA[i][13]]);
            B2B_G( 3, 4,  9, 14, m[SIGMA[i][14]], m[SIGMA[i][15]]);
        }

        for( i = 0; i < 8; ++i )
            s->chained_state[i] ^= v[i] ^ v[i + 8];
}


void blake2b(const void *data, size_t len, void *out, size_t out_len) {
    Blake2bState s = {0};
    blake2b_init(&s, out_len);
    blake2b_update(&s, data, len);
    blake2b_final(&s, out);
}

void blake2b_init   (Blake2bState *s, size_t out_len) {
    size_t i;
    for (i = 0; i < 8; i++)             // state, "param block"
        s->chained_state[i] = blake2b_iv[i];
    s->chained_state[0] ^= 0x01010000 ^ (0 << 8) ^ out_len;

    s->total_num_bytes[0] = 0;                      // input count low word
    s->total_num_bytes[1] = 0;                      // input count high word
    s->pointer_for_input_buffer = 0;                         // pointer within buffer
    s->outlen = out_len;
}
void blake2b_update (Blake2bState *s, const void *data, size_t len) {
    size_t i;

    for (i = 0; i < len; ++i) {
        if (s->pointer_for_input_buffer == 128) {
            s->total_num_bytes[0] += s->pointer_for_input_buffer;
            if (s->total_num_bytes[0] < s->pointer_for_input_buffer)
                s->total_num_bytes[1] += 1;
            blake2b_compress(s, 0);
            s->pointer_for_input_buffer = 0;
        }
        s->input_buffer[s->pointer_for_input_buffer++] = ((const uint8_t *) data)[i];
    }

}
void blake2b_final  (Blake2bState *s, void *out) {
    size_t i;

    s->total_num_bytes[0] += s->pointer_for_input_buffer;
    if (s->total_num_bytes[0] < s->pointer_for_input_buffer)
        s->total_num_bytes[1] += 1;

    while (s->pointer_for_input_buffer < 128)
        s->input_buffer[s->pointer_for_input_buffer++] = 0;
    blake2b_compress(s, 1);

    for (i = 0; i < s->outlen; ++i) {
        (( uint8_t * ) out)[i] = (s->chained_state[i >> 3] >> (8* (i & 7))) & 0xFF;
    }
}

bool blake2b_file(const char *path, void *out, size_t out_len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);

    Blake2bState s = {0};
    blake2b_init(&s, out_len);

    uint8_t buf[65536];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        blake2b_update(&s, buf, (size_t)n);

    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);

    if (n < 0) return false;

    blake2b_final(&s, out);
    return true;
}
