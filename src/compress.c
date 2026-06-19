/* compress.c */
#include <zlib.h>
#include <string.h>
#include "compress.h"

int compress_data(const unsigned char *src, size_t src_len,
                  unsigned char *dst, size_t *dst_len)
{
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK)
        return -1;

    strm.next_in = (unsigned char *)src;
    strm.avail_in = src_len;
    strm.next_out = dst;
    strm.avail_out = *dst_len;

    int ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        deflateEnd(&strm);
        return -1;
    }

    *dst_len = strm.total_out;
    deflateEnd(&strm);
    return 0;
}

int decompress_data(const unsigned char *src, size_t src_len,
                    unsigned char *dst, size_t *dst_len)
{
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    if (inflateInit(&strm) != Z_OK)
        return -1;

    strm.next_in = (unsigned char *)src;
    strm.avail_in = src_len;
    strm.next_out = dst;
    strm.avail_out = *dst_len;

    int ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        inflateEnd(&strm);
        return -1;
    }

    *dst_len = strm.total_out;
    inflateEnd(&strm);
    return 0;
}