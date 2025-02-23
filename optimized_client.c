/* Optimized client_final.c */
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5678
#define BUFFER_SIZE 256

// Connection state
static int is_connected = 0;
static pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function declarations
void* receive_messages(void* arg);
int reconnect(int* sock);
void handle_user_input(int sock);

// Thread function to receive messages
void* receive_messages(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while (1) {
        bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("\n[Server]: %s\n", buffer);
            printf("Enter Message: ");
            fflush(stdout);
        } else if (bytes_read == 0) {
            printf("\nServer closed the connection.\n");
            pthread_mutex_lock(&conn_mutex);
            is_connected = 0;
            pthread_mutex_unlock(&conn_mutex);
            break;
        } else {
            perror("\nConnection lost");
            pthread_mutex_lock(&conn_mutex);
            is_connected = 0;
            pthread_mutex_unlock(&conn_mutex);
            break;
        }
    }

    close(sock);
    return NULL;
}

// Reconnect logic
int reconnect(int* sock) {
    struct sockaddr_in server;

    *sock = socket(PF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror("Socket creation failed");
        return 0;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(SERVER_PORT);

    if (connect(*sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Reconnection failed");
        close(*sock);
        return 0;
    }

    printf("\nReconnected to server successfully.\n");
    pthread_mutex_lock(&conn_mutex);
    is_connected = 1;
    pthread_mutex_unlock(&conn_mutex);
    return 1;
}

// Handle user input and send messages
void handle_user_input(int sock) {
    char buffer[BUFFER_SIZE];

    while (1) {
        printf("Enter Message: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            perror("Input failed");
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            pthread_mutex_lock(&conn_mutex);
            is_connected = 0;
            pthread_mutex_unlock(&conn_mutex);
            break;
        }
    }
}

int main(void) {
    int sock;
    char name[32];
    pthread_t recv_thread;

    while (1) {
        if (!is_connected) {
            printf("Connecting to server...\n");
            if (reconnect(&sock)) {
                printf("Enter your name: ");
                fgets(name, sizeof(name), stdin);
                name[strcspn(name, "\n")] = '\0';

                if (send(sock, name, strlen(name), 0) < 0) {
                    perror("Name send failed");
                    close(sock);
                    continue;
                }

                if (pthread_create(&recv_thread, NULL, receive_messages, &sock) != 0) {
                    perror("Thread creation failed");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
            } else {
                printf("Retrying connection in 5 seconds...\n");
                sleep(5);
                continue;
            }
        }

        handle_user_input(sock);
    }

    pthread_join(recv_thread, NULL);
    close(sock);
    return 0;
}
