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

// 追蹤連線狀態
int is_connected = 0;
pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;

// 接收訊息線程函數
void* receive_messages(void* arg) {
    int sock = *(int*)arg;
    char buf[256];
    int readSize;

    while (1) {
        // 接收伺服器訊息
        readSize = recv(sock, buf, sizeof(buf) - 1, 0);
        if (readSize > 0) {
            buf[readSize] = '\0';  // 確保字串結束
            printf("\n[Server]: %s\n", buf);
            printf("Enter Message: "); // 重新提示用戶輸入
            fflush(stdout);
        } else if (readSize == 0) {
            printf("\nServer closed the connection.\n");
            pthread_mutex_lock(&conn_mutex);
            is_connected = 0;  // 標記連線中斷
            pthread_mutex_unlock(&conn_mutex);
            break;
        } else {
            perror("\nConnection lost");
            pthread_mutex_lock(&conn_mutex);
            is_connected = 0;  // 標記連線中斷
            pthread_mutex_unlock(&conn_mutex);
            break;
        }
    }

    close(sock);
    return NULL;
}

// 重連邏輯
int reconnect(int* sock) {
    struct sockaddr_in server;

    // 建立新 Socket
    *sock = socket(PF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror("Socket creation failed");
        return 0;
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(SERVER_PORT);

    // 嘗試連線
    if (connect(*sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Reconnection failed");
        close(*sock);
        return 0;
    }

    printf("\nReconnected to server successfully.\n");
    pthread_mutex_lock(&conn_mutex);
    is_connected = 1;  // 標記重新連線成功
    pthread_mutex_unlock(&conn_mutex);
    return 1;
}

int main(void) {
    int sock;
    char buf[256];
    char name[32];
    pthread_t recv_thread;

    while (1) {
        if (!is_connected) {
            // 嘗試建立初始連線或重連
            printf("Connecting to server...\n");
            if (reconnect(&sock)) {
                printf("Enter your name: ");
                fgets(name, sizeof(name), stdin);
                name[strcspn(name, "\n")] = '\0';  // 去除換行符

                // 傳送名字給server
                if (send(sock, name, strlen(name), 0) < 0) {
                    perror("Name send failed");
                    close(sock);
                    continue;
                }

                // 啟動接收訊息線程
                if (pthread_create(&recv_thread, NULL, receive_messages, &sock) != 0) {
                    perror("Thread creation failed");
                    close(sock);
                    exit(1);
                }
            } else {
                printf("Retrying connection in 5 seconds...\n");
                sleep(5);  // 延遲再試
                continue;
            }
        }

        // 發送訊息給伺服器
        printf("Enter Message: ");
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            perror("Input failed");
            break;
        }

        buf[strcspn(buf, "\n")] = '\0';  // 去除換行符
        if (send(sock, buf, strlen(buf), 0) < 0) {
            perror("Send failed");
            pthread_mutex_lock(&conn_mutex);
            is_connected = 0;  // 標記連線中斷
            pthread_mutex_unlock(&conn_mutex);
            continue;
        }
    }

    pthread_join(recv_thread, NULL);
    close(sock);
    return 0;
}
