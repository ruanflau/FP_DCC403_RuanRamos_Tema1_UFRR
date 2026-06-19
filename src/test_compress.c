#include <stdio.h>
#include <string.h>
#include "compress.h"

int main()
{
    const char *msg = "Telemetria de Borda - Canal ESG Conecta - dados repetidos repetidos repetidos";
    unsigned char compressed[BUFSIZ], decompressed[BUFSIZ];
    size_t clen = sizeof(compressed), dlen = sizeof(decompressed);

    compress_data((unsigned char *)msg, strlen(msg), compressed, &clen);
    printf("Original: %zu bytes | Comprimido: %zu bytes\n", strlen(msg), clen);

    decompress_data(compressed, clen, decompressed, &dlen);
    decompressed[dlen] = '\0';
    printf("Descomprimido: %s\n", decompressed);

    return 0;
}