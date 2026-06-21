#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
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

/* 3.2 - Restaura o terminal ao estado original na saida do programa */
void restore_terminal(void)
{
    if (g_raw_mode_active)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
    }
}

/* 3.2 - Coloca o terminal em modo raw para capturar Ctrl+C/Ctrl+D como bytes */
void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(restore_terminal);
    g_raw_mode_active = 1;

    struct termios raw = g_orig_termios;
    /* Desliga echo local, modo canonico e geracao de sinais (ISIG) */
    raw.c_lflag &= ~(ICANON | ISIG | IEXTEN);         /* ECHO permanece ligado, para feedback visual */
    raw.c_iflag &= ~(IXON | BRKINT | INPCK | ISTRIP); /* ICRNL permanece ligado, traduz Enter (\r) em \n */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    /* c_oflag permanece intacto para manter a formatacao normal de \n */

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

int main(int argc, char *argv[])
{
    AppConfig cfg;
    if (parse_args(argc, argv, &cfg) != 0)
    {
        fprintf(stderr, "Uso: %s --host=IP --port=NUMERO [--compress] [--encrypt] [--log=arquivo.log]\n", argv[0]);
        return 1;
    }

    /* 3.5 - Abertura do arquivo de auditoria, se solicitado */
    FILE *log_fp = NULL;
    if (cfg.has_log)
    {
        log_fp = logger_open(cfg.log_file);
        if (!log_fp)
        {
            perror("Erro ao abrir arquivo de log");
            return 1;
        }
    }

    /* 3.1 - Criacao do socket TCP e conexao com o servidor */
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

    /* 3.6 - Handshake TLS no lado cliente, se ativo */
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
            fprintf(stderr, "Falha ao inicializar contexto TLS\n");
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

    /* 3.2 - Ativa modo raw apos imprimir as mensagens iniciais */
    enable_raw_mode();

    /* 3.3 - Loop de multiplexacao: teclado <-> socket */
    unsigned char keybuf[BUFFER_SIZE];
    unsigned char netbuf[BUFFER_SIZE * 2];
    fd_set readfds;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);
        int maxfd = (STDIN_FILENO > sockfd) ? STDIN_FILENO : sockfd;

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        /* Teclado -> servidor (aqui chegam os bytes 0x03 e 0x04 crus, */
        /* pois ISIG/ICANON estao desligados no modo raw) */
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            ssize_t n = read(STDIN_FILENO, keybuf, sizeof(keybuf));
            if (n <= 0)
                break;

            net_send(&conn, keybuf, n);
            if (log_fp)
                logger_write(log_fp, "SENT", keybuf, n);

            /* 3.4 - Encerramento local elegante ao digitar Ctrl+D */
            for (ssize_t i = 0; i < n; i++)
            {
                if (keybuf[i] == CTRL_D)
                    goto end_session;
            }
        }

        /* Servidor -> tela local */
        if (FD_ISSET(sockfd, &readfds))
        {
            ssize_t n = net_recv(&conn, netbuf, sizeof(netbuf));
            if (n <= 0)
            {
                printf("\nConexao encerrada pelo servidor.\n");
                break;
            }
            write(STDOUT_FILENO, netbuf, n);
            if (log_fp)
                logger_write(log_fp, "RECEIVED", netbuf, n);
        }
    }

end_session:
    restore_terminal();
    if (conn.ssl)
    {
        SSL_shutdown(conn.ssl);
        SSL_free(conn.ssl);
    }
    close(sockfd);
    if (log_fp)
        logger_close(log_fp);

    return 0;
}