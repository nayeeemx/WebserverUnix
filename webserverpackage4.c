#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8081
#define MAX_CLIENTS 3
#define WEB_ROOT "./www"

pthread_mutex_t fileLock; //Mutex for synchronization
pthread_mutex_t threadLock; //Mutex for managing threads

void send_response(int client_fd, const char *response) {
    send(client_fd, response, strlen(response), 0);
}

void *handle_client(void *client_socket) {
    int client_fd = *(int *)client_socket;
    char buffer[1024];
    ssize_t bytes_read;

    //Read the HTTP request from the client
    bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        perror("Error reading request");
        close(client_fd);
        pthread_mutex_lock(&threadLock);
        pthread_exit(NULL);
    }

    
    buffer[bytes_read] = '\0';
    char *file_path = strtok(buffer, " ");
    if (file_path == NULL || strcmp(file_path, "GET") != 0) {
        perror("Invalid request");
        send_response(client_fd, "HTTP/1.1 400 Bad Request\r\n\r\n<h1>400 Bad Request</h1>");
        close(client_fd);
        pthread_mutex_lock(&threadLock);
        pthread_exit(NULL);
    }

    char *requested_file = strtok(NULL, " ");
    if (requested_file == NULL) {
        perror("Invalid request");
        send_response(client_fd, "HTTP/1.1 400 Bad Request\r\n\r\n<h1>400 Bad Request</h1>");
        close(client_fd);
        pthread_mutex_lock(&threadLock);
        pthread_exit(NULL);
    }

    //Constructing the full file path by prepending the web root
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, requested_file);

    //Checking if the requested file exists
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd != -1) {
        char response[] = "HTTP/1.1 200 OK\r\n\r\n";
        send_response(client_fd, response);

        //Lock the mutex before reading from the file
        pthread_mutex_lock(&fileLock);
        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
            send(client_fd, buffer, bytes_read, 0);
        }
        //Unlock the mutex after reading from the file
        pthread_mutex_unlock(&fileLock);

        close(file_fd);
    } else {
        perror("File not found");
        send_response(client_fd, "HTTP/1.1 404 Not Found\r\n\r\n<h1>404 Not Found</h1>");
    }

    close(client_fd);
    pthread_mutex_lock(&threadLock);
    pthread_exit(NULL);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t threads[MAX_CLIENTS];
    int i = 0;

 
    pthread_mutex_init(&fileLock, NULL);
    pthread_mutex_init(&threadLock, NULL);

    // Creating socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(1);
    }

    //Listening for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        exit(1);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Acceptance failed");
            continue;
        }

        if (pthread_create(&threads[i], NULL, handle_client, &client_fd) != 0) {
            perror("Thread creation failed");
        }

        i++;
    }

    close(server_fd);
    return 0;
}

