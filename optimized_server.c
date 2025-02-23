/* Optimized server.c */
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>

#define TIMEOUT 30
#define MAX_CLIENTS 100
#define NAME_LEN 32
#define BUFFER_SIZE 256

// Client structure
typedef struct {
    int socket;
    char name[NAME_LEN];
} Client;

static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static Client clients[MAX_CLIENTS];
static int client_count = 0;

// Function declarations
void broadcast_message(const char* message, int exclude_sock);
void* handle_client(void* arg);

// Broadcast messages to all clients
void broadcast_message(const char* message, int exclude_sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != exclude_sock) {
            if (send(clients[i].socket, message, strlen(message), 0) < 0) {
                fprintf(stderr, "Broadcast to client %d failed: %s\n", istrerror(errno));
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Handle individual client
void* handle_client(void* arg) {
    int csock = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char client_name[NAME_LEN];
    int bytes_read;

    if ((bytes_read = recv(csock, client_name, NAME_LEN, 0)) <= 0) {
        perror("Failed to receive client name");
        close(csock);
        return NULL;
    }

    client_name[bytes_read] = '\0';

    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        clients[client_count].socket = csock;
        strncpy(clients[client_count].name, client_name, NAME_LEN);
        client_count++;
    } else {
        printf("Max clients reached. Connection rejected.\n");
        close(csock);
        pthread_mutex_unlock(&clients_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&clients_mutex);

    printf("Client '%s' connected.\n", client_name);

    struct timeval timeout = {TIMEOUT, 0};
    setsockopt(csock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    while ((bytes_read = recv(csock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("Message from '%s': %s\n", client_name, buffer);

        char broadcast_buf[BUFFER_SIZE + NAME_LEN];
        snprintf(broadcast_buf, sizeof(broadcast_buf), "%s: %s", client_name, buffer);
        broadcast_message(broadcast_buf, csock);
    }

    if (bytes_read == 0) {
        printf("Client '%s' disconnected.\n", client_name);
    } else if (bytes_read < 0) {
        perror("Client receive failed or timed out");
    }

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == csock) {
            clients[i] = clients[--client_count];
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(csock);
    return NULL;
}

int main(void) {
    struct sockaddr_in server, client;
    int sock, *csock;
    pthread_t thread_id;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(5678);

    printf("Server is starting...\n");

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully.\n");

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Binding successful.\n");

    if (listen(sock, 5) < 0) {
        perror("Listen failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port 5678...\n");

    while (1) {
        socklen_t client_size = sizeof(client);
        csock = malloc(sizeof(int));
        if (!csock) {
            perror("Memory allocation failed");
            continue;
        }

        *csock = accept(sock, (struct sockaddr*)&client, &client_size);
        if (*csock < 0) {
            perror("Accept failed");
            free(csock);
            continue;
        }

        if (pthread_create(&thread_id, NULL, handle_client, csock) != 0) {
            perror("Thread creation failed");
            close(*csock);
            free(csock);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(sock);
    return 0;
}
