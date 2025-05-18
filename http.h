#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

#define MAX_REQUEST_SIZE 4096
#define MAX_PATH 256
#define MAX_RESPONSE 8192

struct client {
    int fd;                 // Сокет клиента
    char request[MAX_REQUEST_SIZE]; // Буфер запроса
    size_t request_len;     // Длина запроса
    char response[MAX_RESPONSE]; // Буфер ответа
    size_t response_len;    // Длина ответа
    size_t response_sent;   // Отправлено байт
    int file_fd;            // Дескриптор файла
    off_t file_size;        // Размер файла
};

// Отправляет HTTP-ошибку
void send_error(int fd, int status, const char *message);

// Обрабатывает события клиента
void handle_client(int epoll_fd, struct client *client, uint32_t events);

#endif