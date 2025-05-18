#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "utils.h"

void set_nonblocking(int fd) {
    // Устанавливаем неблокирующий режим для дескриптора
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl get");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl set");
    }
}

void cleanup_client(int epoll_fd, struct client *client) {
    // Очищаем ресурсы клиента
    if (client->file_fd >= 0) {
        close(client->file_fd);
    }
    if (client->fd > 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
    }
    memset(client, 0, sizeof(struct client));
}