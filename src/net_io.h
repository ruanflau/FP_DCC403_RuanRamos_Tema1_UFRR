/* net_io.h */
#ifndef NET_IO_H
#define NET_IO_H

#include <openssl/ssl.h>
#include "common.h"

typedef struct {
    int      sockfd;
    SSL     *ssl;       /* NULL se --encrypt nao estiver ativo */
    AppConfig *cfg;
} Connection;

ssize_t net_send(Connection *conn, const unsigned char *data, size_t len);
ssize_t net_recv(Connection *conn, unsigned char *buf, size_t maxlen);

#endif