#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "server.h"
#include "http.h"
#include "utils.h"

#define MAX_EVENTS 64
#define MAX_CONNECTIONS 1024

int start_server(const char *dir, const char *addr_str, int port) {
    // Переходим в рабочую директорию
    if (chdir(dir) < 0) {
        perror("chdir");
        return -1;
    }

    // Создаем серверный сокет
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    // Настраиваем опции сокета
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    // Привязываем сокет к адресу и порту
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, addr_str, &addr.sin_addr);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    // Создаем epoll-инстанс
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        return -1;
    }

    // Добавляем серверный сокет в epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_fd);
        return -1;
    }

    // Выделяем память для клиентов
    struct client *clients = calloc(MAX_CONNECTIONS, sizeof(struct client));
    if (!clients) {
        perror("calloc");
        close(epoll_fd);
        close(server_fd);
        return -1;
    }

    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                // Принимаем новое соединение
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) perror("accept");
                    continue;
                }

                // Ищем свободный слот для клиента
                int slot = -1;
                for (int j = 0; j < MAX_CONNECTIONS; j++) {
                    if (clients[j].fd == 0) {
                        slot = j;
                        break;
                    }
                }
                if (slot == -1) {
                    close(client_fd);
                    continue;
                }

                // Инициализируем клиента
                set_nonblocking(client_fd);
                clients[slot].fd = client_fd;
                clients[slot].request_len = 0;
                clients[slot].response_len = 0;
                clients[slot].response_sent = 0;
                clients[slot].file_fd = -1;

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                    perror("epoll_ctl");
                    close(client_fd);
                    continue;
                }
            } else {
                // Обрабатываем событие клиента
                int slot = -1;
                for (int j = 0; j < MAX_CONNECTIONS; j++) {
                    if (clients[j].fd == fd) {
                        slot = j;
                        break;
                    }
                }
                if (slot == -1) continue;

                handle_client(epoll_fd, &clients[slot], events[i].events);
            }
        }
    }

    // Очищаем ресурсы (недостижимо в текущем цикле, но для полноты)
    free(clients);
    close(epoll_fd);
    close(server_fd);
    return 0;
}