/* tls.c */
#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "tls.h"

SSL_CTX *tls_init_server_ctx(const char *cert_file, const char *key_file)
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

SSL_CTX *tls_init_client_ctx(void)
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    /* Para certificados autoassinados em ambiente de teste,
       a verificacao estrita fica desativada aqui.
       Documentar essa decisao no relatorio. */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    return ctx;
}