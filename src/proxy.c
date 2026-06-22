#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common.h"
#include "net_io.h"
#include "tls.h"
#include "logger.h"

static struct termios g_orig_termios;
static int g_raw_mode_active = 0;
static volatile int g_running = 1;

void restore_terminal(void)
{
    if (g_raw_mode_active)
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(restore_terminal);
    g_raw_mode_active = 1;

    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/* Estrutura passada para a thread de leitura */
typedef struct
{
    Connection *conn;
    FILE *log_fp;
} RecvArgs;

/* Thread dedicada a ler do servidor e escrever na tela */
void *recv_thread(void *arg)
{
    RecvArgs *ra = (RecvArgs *)arg;
    Connection *conn = ra->conn;
    FILE *log_fp = ra->log_fp;
    unsigned char buf[BUFFER_SIZE * 2];

    while (g_running)
    {
        ssize_t n = net_recv(conn, buf, sizeof(buf));
        if (n <= 0)
        {
            /* Conexao encerrada ou erro */
            restore_terminal();
            printf("\nConexao encerrada pelo servidor.\n");
            g_running = 0;
            break;
        }
        write(STDOUT_FILENO, buf, n);
        if (log_fp)
            logger_write(log_fp, "RECEIVED", buf, n);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    AppConfig cfg;
    if (parse_args(argc, argv, &cfg) != 0)
    {
        fprintf(stderr, "Uso: %s --host=IP --port=NUMERO [--compress] [--encrypt] [--log=arquivo.log]\n", argv[0]);
        return 1;
    }

    FILE *log_fp = NULL;
    if (cfg.has_log)
    {
        log_fp = logger_open(cfg.log_file);
        if (!log_fp)
        {
            perror("log");
            return 1;
        }
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(cfg.port);
    if (inet_pton(AF_INET, cfg.host, &server_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Endereco invalido: %s\n", cfg.host);
        return 1;
    }
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    Connection conn;
    conn.sockfd = sockfd;
    conn.cfg = &cfg;
    conn.ssl = NULL;

    if (cfg.use_encrypt)
    {
        SSL_library_init();
        SSL_load_error_strings();
        SSL_CTX *ctx = tls_init_client_ctx();
        if (!ctx)
        {
            fprintf(stderr, "Falha TLS\n");
            return 1;
        }
        conn.ssl = SSL_new(ctx);
        SSL_set_fd(conn.ssl, sockfd);
        if (SSL_connect(conn.ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
            return 1;
        }
    }

    printf("Conectado a %s:%d (compress=%d, encrypt=%d)\n",
           cfg.host, cfg.port, cfg.use_compress, cfg.use_encrypt);
    printf("Pressione Ctrl+D para encerrar a sessao.\n\n");

    enable_raw_mode();

    /* Inicia thread de leitura do servidor */
    RecvArgs ra = {&conn, log_fp};
    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, &ra);

    /* Loop principal: le do teclado e envia ao servidor */
    unsigned char keybuf[BUFFER_SIZE];
    while (g_running)
    {
        ssize_t n = read(STDIN_FILENO, keybuf, sizeof(keybuf));
        if (n <= 0)
            break;

        net_send(&conn, keybuf, n);
        if (log_fp)
            logger_write(log_fp, "SENT", keybuf, n);

        for (ssize_t i = 0; i < n; i++)
        {
            if (keybuf[i] == CTRL_D)
            {
                g_running = 0;
                break;
            }
        }
    }

    g_running = 0;
    restore_terminal();

    if (conn.ssl)
    {
        SSL_shutdown(conn.ssl);
        SSL_free(conn.ssl);
    }
    close(sockfd);

    pthread_join(tid, NULL);

    if (log_fp)
        logger_close(log_fp);
    return 0;
}