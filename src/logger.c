/* logger.c */
#include <stdio.h>
#include "logger.h"

FILE *logger_open(const char *path)
{
    return fopen(path, "a");
}

void logger_write(FILE *fp, const char *direction,
                  const unsigned char *data, size_t len)
{
    if (!fp)
        return;

    fprintf(fp, "%s %zu bytes: ", direction, len);
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] >= 32 && data[i] <= 126)
            fputc(data[i], fp);
        else
            fprintf(fp, "\\x%02x", data[i]);
    }
    fputc('\n', fp);
    fflush(fp);
}

void logger_close(FILE *fp)
{
    if (fp)
        fclose(fp);
}