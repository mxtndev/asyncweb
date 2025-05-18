#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>
#include "http.h"
#include "utils.h"

void send_error(int fd, int status, const char *message) {
    // Формируем и отправляем HTTP-ответ с ошибкой
    char buf[512];
    snprintf(buf, sizeof(buf),
             "HTTP/1.1 %d %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n"
             "%s", status, message, strlen(message), message);
    write(fd, buf, strlen(buf));
}

void handle_client(int epoll_fd, struct client *client, uint32_t events) {
    // Обрабатываем события клиента
    struct epoll_event ev;

    if (events & EPOLLIN) {
        // Читаем запрос
        while (1) {
            ssize_t n = read(client->fd, client->request + client->request_len,
                            MAX_REQUEST_SIZE - client->request_len);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                goto close_client;
            }
            if (n == 0) goto close_client;
            client->request_len += n;
            if (client->request_len >= MAX_REQUEST_SIZE) {
                send_error(client->fd, 400, "Bad Request");
                goto close_client;
            }

            // Проверяем завершение запроса
            if (strstr(client->request, "\r\n\r\n")) {
                // Парсим запрос
                char *method = strtok(client->request, " ");
                char *path = strtok(NULL, " ");
                if (!method || !path || strcmp(method, "GET") != 0) {
                    send_error(client->fd, 400, "Bad Request");
                    goto close_client;
                }

                // Санитизируем путь
                char filepath[MAX_PATH];
                snprintf(filepath, sizeof(filepath), ".%s", path);
                if (strstr(filepath, "..")) {
                    send_error(client->fd, 403, "Forbidden");
                    goto close_client;
                }

                // Открываем файл
                client->file_fd = open(filepath, O_RDONLY);
                if (client->file_fd < 0) {
                    if (errno == ENOENT) send_error(client->fd, 404, "Not Found");
                    else send_error(client->fd, 403, "Forbidden");
                    goto close_client;
                }

                // Получаем размер файла
                struct stat st;
                if (fstat(client->file_fd, &st) < 0) {
                    send_error(client->fd, 500, "Internal Server Error");
                    close(client->file_fd);
                    goto close_client;
                }
                client->file_size = st.st_size;

                // Формируем заголовки ответа
                client->response_len = snprintf(client->response, MAX_RESPONSE,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %ld\r\n"
                    "Connection: keep-alive\r\n\r\n",
                    client->file_size);

                // Переключаемся на отправку данных
                ev.events = EPOLLOUT | EPOLLET;
                ev.data.fd = client->fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev) < 0) {
                    perror("epoll_ctl");
                    goto close_client;
                }
                break;
            }
        }
    }

    if (events & EPOLLOUT) {
        // Отправляем заголовки ответа
        while (client->response_sent < client->response_len) {
            ssize_t n = write(client->fd, client->response + client->response_sent,
                             client->response_len - client->response_sent);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                goto close_client;
            }
            client->response_sent += n;
        }

        // Отправляем файл
        while (client->response_sent < client->response_len + client->file_size) {
            off_t offset = client->response_sent - client->response_len;
            ssize_t n = sendfile(client->fd, client->file_fd, &offset,
                                client->file_size - offset);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                goto close_client;
            }
            client->response_sent += n;
        }

        if (client->response_sent >= client->response_len + client->file_size) {
            // Сбрасываем состояние клиента
            close(client->file_fd);
            client->file_fd = -1;
            client->response_sent = 0;
            client->response_len = 0;
            client->request_len = 0;

            // Переключаемся на чтение нового запроса
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = client->fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev) < 0) {
                perror("epoll_ctl");
                goto close_client;
            }
        }
    }

    if (events & (EPOLLERR | EPOLLHUP)) {
close_client:
        cleanup_client(epoll_fd, client);
    }
}