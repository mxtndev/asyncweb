#ifndef SERVER_H
#define SERVER_H

#include "http.h"

// Запускает сервер
int start_server(const char *dir, const char *addr_str, int port);

#endif