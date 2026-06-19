#include <unistd.h>
#include <string.h>
#include "net_io.h"
#include "compress.h"

ssize_t net_send(Connection *conn, const unsigned char *data, size_t len)
{
    unsigned char compbuf[BUFFER_SIZE * 2];
    const unsigned char *out_data = data;
    size_t out_len = len;

    if (conn->cfg->use_compress)
    {
        size_t clen = sizeof(compbuf);
        if (compress_data(data, len, compbuf, &clen) == 0)
        {
            out_data = compbuf;
            out_len = clen;
        }
    }

    if (conn->ssl)
    {
        return SSL_write(conn->ssl, out_data, out_len);
    }
    return write(conn->sockfd, out_data, out_len);
}

ssize_t net_recv(Connection *conn, unsigned char *buf, size_t maxlen)
{
    unsigned char rawbuf[BUFFER_SIZE * 2];
    ssize_t n;

    if (conn->ssl)
    {
        n = SSL_read(conn->ssl, rawbuf, sizeof(rawbuf));
    }
    else
    {
        n = read(conn->sockfd, rawbuf, sizeof(rawbuf));
    }
    if (n <= 0)
        return n;

    if (conn->cfg->use_compress)
    {
        size_t dlen = maxlen;
        if (decompress_data(rawbuf, n, buf, &dlen) == 0)
            return dlen;
        return -1;
    }

    if ((size_t)n > maxlen)
        n = maxlen;
    memcpy(buf, rawbuf, n);
    return n;
}