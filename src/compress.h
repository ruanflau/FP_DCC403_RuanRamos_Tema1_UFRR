/* compress.h */
#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>

int compress_data(const unsigned char *src, size_t src_len,
                   unsigned char *dst, size_t *dst_len);

int decompress_data(const unsigned char *src, size_t src_len,
                     unsigned char *dst, size_t *dst_len);

#endif