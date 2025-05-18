#ifndef UTILS_H
#define UTILS_H

#include "http.h"

// Устанавливает неблокирующий режим
void set_nonblocking(int fd);

// Очищает ресурсы клиента
void cleanup_client(int epoll_fd, struct client *client);

#endif