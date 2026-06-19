#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

#define BUFFER_SIZE     4096
#define MAX_HOST_LEN    256
#define MAX_PATH_LEN    256

#define CTRL_C  0x03
#define CTRL_D  0x04

typedef struct {
    int  port;
    char host[MAX_HOST_LEN];
    int  use_compress;   /* 0 = off, 1 = on */
    int  use_encrypt;    /* 0 = off, 1 = on */
    char log_file[MAX_PATH_LEN];
    int  has_log;        /* indica se --log= foi passado */
} AppConfig;

void config_init_defaults(AppConfig *cfg);
int  parse_args(int argc, char *argv[], AppConfig *cfg);

#endif