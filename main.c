#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"

int main(int argc, char *argv[]) {
    // Проверяем количество аргументов
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <директория> <адрес:порт>\n", argv[0]);
        return 1;
    }

    // Парсим адрес и порт
    char *addr_str = strtok(argv[2], ":");
    char *port_str = strtok(NULL, ":");
    if (!addr_str || !port_str) {
        fprintf(stderr, "Неверный формат адрес:порт\n");
        return 1;
    }

    // Проверяем корректность порта
    char *endptr;
    long port = strtol(port_str, &endptr, 10);
    if (*endptr != '\0' || port < 1 || port > 65535) {
        fprintf(stderr, "Неверный номер порта\n");
        return 1;
    }

    // Запускаем сервер
    if (start_server(argv[1], addr_str, (int)port) < 0) {
        return 1;
    }

    return 0;
}