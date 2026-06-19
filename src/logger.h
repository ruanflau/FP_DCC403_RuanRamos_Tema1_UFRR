/* logger.h */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stddef.h>

FILE *logger_open(const char *path);
void logger_write(FILE *fp, const char *direction,
                   const unsigned char *data, size_t len);
void logger_close(FILE *fp);

#endif