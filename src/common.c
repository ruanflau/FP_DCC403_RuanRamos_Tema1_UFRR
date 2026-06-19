#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"

void config_init_defaults(AppConfig *cfg)
{
    memset(cfg, 0, sizeof(AppConfig));
    cfg->port = 9095;
    strncpy(cfg->host, "127.0.0.1", MAX_HOST_LEN - 1);
}

int parse_args(int argc, char *argv[], AppConfig *cfg)
{
    config_init_defaults(cfg);

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--port=", 7) == 0)
        {
            cfg->port = atoi(argv[i] + 7);
        }
        else if (strncmp(argv[i], "--host=", 7) == 0)
        {
            strncpy(cfg->host, argv[i] + 7, MAX_HOST_LEN - 1);
        }
        else if (strcmp(argv[i], "--compress") == 0)
        {
            cfg->use_compress = 1;
        }
        else if (strcmp(argv[i], "--encrypt") == 0)
        {
            cfg->use_encrypt = 1;
        }
        else if (strncmp(argv[i], "--log=", 6) == 0)
        {
            strncpy(cfg->log_file, argv[i] + 6, MAX_PATH_LEN - 1);
            cfg->has_log = 1;
        }
        else
        {
            fprintf(stderr, "Argumento desconhecido: %s\n", argv[i]);
            return -1;
        }
    }

    if (cfg->port <= 0 || cfg->port > 65535)
    {
        fprintf(stderr, "Porta invalida: %d\n", cfg->port);
        return -1;
    }

    return 0;
}