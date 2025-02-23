#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define TIMEOUT 30       // 設定每個客戶端的接收超時時間，單位為秒
#define MAX_CLIENTS 100  // 支援的最大同時連線客戶端數
#define NAME_LEN 32      // 每個客戶端名稱的最大長度
#define BUFFER_SIZE 256  // 每次接收訊息的緩衝區大小

// 定義 Client 結構，包含每個客戶端的 socket 和 name。
typedef struct {
    int socket;
    char name[NAME_LEN];
} Client;

// 使用 pthread_mutex_t 來保護對 clients 陣列的存取，避免多線程競爭條件。
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
Client clients[MAX_CLIENTS];
int client_count = 0;

//將訊息廣播給所有連線的客戶端（排除訊息的來源客戶端 exclude_sock）。
void broadcast_message(const char* message, int exclude_sock) {
    // lock: 保護清單，避免多線程同時存取。
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != exclude_sock) {
            if (send(clients[i].socket, message, strlen(message), 0) < 0) {
                perror("Broadcast failed");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    int csock = *(int*)arg; // 取得客戶端的 socket arg
    free(arg);

    char buf[BUFFER_SIZE];
    char client_name[NAME_LEN];
    int readSize;

    // 接收客戶端名字
    if ((readSize = recv(csock, client_name, NAME_LEN, 0)) <= 0) {
        perror("Failed to get client name");
        close(csock);
        return NULL;
    }
    client_name[readSize] = '\0';

    // 新增到客戶端列表
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

    // 設置接收超時
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;  // 微秒數
    // 設定 Socket 的屬性, 新增timeout
    // (csock：指定的客戶端 Socket, SOL_SOCKET：表示修改通用 Socket 層的選項, SO_RCVTIMEO：設置接收操作的超時時間, (char*)&timeout：超時時間的結構體, sizeof(timeout)：結構體大小)
    setsockopt(csock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    // 處理訊息
    while ((readSize = recv(csock, buf, sizeof(buf)-1, 0)) > 0) {
        
        buf[readSize] = '\0';
        printf("Message from '%s': %s\n", client_name, buf);

        // 廣播訊息給所有客戶端
        char broadcast_buf[BUFFER_SIZE + NAME_LEN];
        snprintf(broadcast_buf, sizeof(broadcast_buf), "%s: %s", client_name, buf);
        broadcast_message(broadcast_buf, csock);  // 廣播給所有客戶端
    } // 若接收訊息失敗或連線超時，則退出循環。

    if (readSize == 0) {
        printf("Client '%s' disconnected.\n", client_name);
    } else if (readSize < 0) {
        perror("Client receive failed or timed out");
    }

    // 移除客戶端
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
    struct sockaddr_in server, client;  // 定義服務器和客戶端的地址結構
    int sock, *csock;
    pthread_t thread_id;

    bzero(&server, sizeof(server));  // 清空服務器地址結構，避免殘留數據

    // 設置服務器的地址屬性
    server.sin_family = AF_INET;  // 指定地址族為 IPv4
    server.sin_addr.s_addr = INADDR_ANY;  
    server.sin_port = htons(5678);  // 指定端口，使用 htons 將主機字節序轉為網絡字節序

    printf("Server is starting...\n");

    // creating socket
    sock = socket(PF_INET, SOCK_STREAM, 0);  // 使用 PF_INET (IPv4) 和 SOCK_STREAM (TCP)
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Socket created successfully.\n");

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // binding...
    // 將 Socket 綁定到指定的 IP 地址和端口
    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(1);
    }
    printf("Binding successful.\n");

    //  listening...
    // 啟動服務器並設置允許的最大等待連接數
    if (listen(sock, 5) < 0) {
        perror("Listen failed");
        close(sock);
        exit(1);
    }
    printf("Server is listening on port 5678...\n");

    while (1) {
        // 接受客戶端連接，返回一個新的 Socket 用於與該客戶端通信
        socklen_t addressSize = sizeof(client);
        csock = malloc(sizeof(int));
        if (!csock) {
            perror("Memory allocation failed");
            continue;
        }

        *csock = accept(sock, (struct sockaddr*)&client, &addressSize);
        if (*csock < 0) {
            perror("Accept failed");
            free(csock);
            continue;
        }
        // 進入通信循環，處理來自客戶端的消息
        if (pthread_create(&thread_id, NULL, handle_client, (void*)csock) != 0) {
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