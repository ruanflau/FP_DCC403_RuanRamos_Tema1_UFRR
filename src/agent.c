#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common.h"
#include "net_io.h"
#include "tls.h"

#define SHELL_PATH "/bin/bash"

static SSL_CTX *g_ssl_ctx = NULL;

void sigchld_handler(int sig)
{
    (void)sig;
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
    }
    errno = saved;
}

static ssize_t timed_recv(Connection *conn, unsigned char *buf, size_t maxlen)
{
    if (conn->ssl)
    {
        fd_set rfd;
        struct timeval tv;
        FD_ZERO(&rfd);
        FD_SET(conn->sockfd, &rfd);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        int r = select(conn->sockfd + 1, &rfd, NULL, NULL, &tv);
        if (r <= 0 && SSL_pending(conn->ssl) == 0 && !SSL_has_pending(conn->ssl))
            return -2;
        return net_recv(conn, buf, maxlen);
    }
    return net_recv(conn, buf, maxlen);
}

void handle_client(int client_fd, AppConfig *cfg)
{
    Connection conn;
    conn.sockfd = client_fd;
    conn.cfg = cfg;
    conn.ssl = NULL;

    if (cfg->use_encrypt)
    {
        conn.ssl = SSL_new(g_ssl_ctx);
        SSL_set_fd(conn.ssl, client_fd);
        if (SSL_accept(conn.ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
            SSL_free(conn.ssl);
            close(client_fd);
            exit(1);
        }
    }

    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0)
    {
        perror("pipe");
        exit(1);
    }

    pid_t shell_pid = fork();
    if (shell_pid == 0)
    {
        setpgid(0, 0);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execl(SHELL_PATH, SHELL_PATH, (char *)NULL);
        perror("execl");
        exit(1);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    int shell_write_fd = in_pipe[1];
    int shell_read_fd = out_pipe[0];

    fd_set readfds;
    unsigned char buf[BUFFER_SIZE];
    unsigned char netbuf[BUFFER_SIZE * 2];
    struct timeval tv;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        FD_SET(shell_read_fd, &readfds);
        int maxfd = (client_fd > shell_read_fd) ? client_fd : shell_read_fd;

        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        /* Rede -> shell */
        int has_ssl_data = conn.ssl &&
                           (SSL_pending(conn.ssl) > 0 || SSL_has_pending(conn.ssl));

        if (has_ssl_data || (activity > 0 && FD_ISSET(client_fd, &readfds)))
        {
            do
            {
                ssize_t n = timed_recv(&conn, netbuf, sizeof(netbuf));
                if (n == -2)
                    break;
                if (n <= 0)
                    goto end_session;

                for (ssize_t i = 0; i < n; i++)
                {
                    if (netbuf[i] == CTRL_C)
                    {
                        killpg(shell_pid, SIGINT);
                    }
                    else if (netbuf[i] == CTRL_D)
                    {
                        if (shell_write_fd != -1)
                        {
                            close(shell_write_fd);
                            shell_write_fd = -1;
                        }
                    }
                    else if (shell_write_fd != -1)
                    {
                        write(shell_write_fd, &netbuf[i], 1);
                    }
                }
            } while (conn.ssl &&
                     (SSL_pending(conn.ssl) > 0 || SSL_has_pending(conn.ssl)));
        }

        /* Shell -> rede */
        if (activity > 0 && shell_read_fd != -1 && FD_ISSET(shell_read_fd, &readfds))
        {
            ssize_t n = read(shell_read_fd, buf, sizeof(buf));
            if (n <= 0)
                goto end_session;
            net_send(&conn, buf, n);
        }
    }

end_session:
    if (conn.ssl)
    {
        SSL_shutdown(conn.ssl);
        SSL_free(conn.ssl);
    }
    close(client_fd);
    if (shell_write_fd != -1)
        close(shell_write_fd);
    if (shell_read_fd != -1)
        close(shell_read_fd);
    waitpid(shell_pid, NULL, 0);
    exit(0);
}

int main(int argc, char *argv[])
{
    AppConfig cfg;
    if (parse_args(argc, argv, &cfg) != 0)
    {
        fprintf(stderr, "Uso: %s --port=NUMERO [--compress] [--encrypt]\n", argv[0]);
        return 1;
    }

    if (cfg.use_encrypt)
    {
        SSL_library_init();
        SSL_load_error_strings();
        g_ssl_ctx = tls_init_server_ctx("certs/server.crt", "certs/server.key");
        if (!g_ssl_ctx)
        {
            fprintf(stderr, "Falha ao inicializar TLS\n");
            return 1;
        }
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(cfg.port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("Industrial Edge Agent escutando na porta %d (compress=%d, encrypt=%d)\n",
           cfg.port, cfg.use_compress, cfg.use_encrypt);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            close(server_fd);
            handle_client(client_fd, &cfg);
        }
        else if (pid > 0)
        {
            close(client_fd);
        }
        else
        {
            perror("fork");
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}