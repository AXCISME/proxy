#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 4096

typedef struct {
    int client_sock;
    int target_sock;
} proxy_session;

void *forward_data(void *arg) {
    int source_sock = ((int *)arg)[0];
    int dest_sock = ((int *)arg)[1];
    free(arg);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(source_sock, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t n = write(dest_sock, buffer + bytes_written, bytes_read - bytes_written);
            if (n <= 0) {
                perror("write failed");
                close(source_sock);
                close(dest_sock);
                return NULL;
            }
            bytes_written += n;
        }
    }

    if (bytes_read < 0) {
        perror("read failed");
    }

    shutdown(dest_sock, SHUT_WR);
    close(source_sock);
    close(dest_sock);
    return NULL;
}

void handle_connection(int client_sock, struct sockaddr_in *target_addr) {
    int target_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (target_sock == -1) {
        perror("socket failed");
        close(client_sock);
        return;
    }

    if (connect(target_sock, (struct sockaddr *)target_addr, sizeof(*target_addr)) == -1) {
        perror("connect failed");
        close(client_sock);
        close(target_sock);
        return;
    }

    int *args1 = malloc(2 * sizeof(int));
    int *args2 = malloc(2 * sizeof(int));
    *args1 = client_sock;
    *(args1 + 1) = target_sock;
    *args2 = target_sock;
    *(args2 + 1) = client_sock;

    pthread_t thread1, thread2;
    if (pthread_create(&thread1, NULL, forward_data, args1) != 0 ||
        pthread_create(&thread2, NULL, forward_data, args2) != 0) {
        perror("pthread_create failed");
        free(args1);
        free(args2);
        close(client_sock);
        close(target_sock);
        return;
    }

    pthread_detach(thread1);
    pthread_detach(thread2);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source_port> <target_host> <target_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int source_port = atoi(argv[1]);
    char *target_host = argv[2];
    int target_port = atoi(argv[3]);

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    if (inet_pton(AF_INET, target_host, &target_addr.sin_addr) <= 0) {
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(source_port);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, SOMAXCONN) == -1) {
        perror("listen failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d, forwarding to %s:%d\n", source_port, target_host, target_port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("accept failed");
            continue;
        }

        printf("Accepted connection from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        handle_connection(client_sock, &target_addr);
    }

    close(listen_sock);
    return 0;
}
