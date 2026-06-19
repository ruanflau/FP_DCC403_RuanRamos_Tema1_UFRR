/* tls.h */
#ifndef TLS_H
#define TLS_H

#include <openssl/ssl.h>

SSL_CTX *tls_init_server_ctx(const char *cert_file, const char *key_file);
SSL_CTX *tls_init_client_ctx(void);

#endif