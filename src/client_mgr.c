#include "../include/common.h"

// 添加客户端到数组
int client_add(Server *server, int fd, const char *ip) {
    if (!server || fd < 0 || !ip) return -1;

    // 找空位置
    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].fd == 0) {
            server->clients[i].fd = fd;
            strncpy(server->clients[i].ip, ip, sizeof(server->clients[i].ip)-1);
            server->clients[i].status = CLIENT_ONLINE; // 未登录，仅在线
            server->clients[i].connect_time = time(NULL);
            memset(server->clients[i].username, 0, sizeof(server->clients[i].username));
            server->client_count++;
            return 0;
        }
    }

    return -1;
}

// 从数组移除客户端
int client_remove(Server *server, int fd) {
    if (!server || fd < 0) return -1;

    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].fd == fd) {
            server->clients[i].fd = 0;
            memset(server->clients[i].ip, 0, sizeof(server->clients[i].ip));
            memset(server->clients[i].username, 0, sizeof(server->clients[i].username));
            server->clients[i].status = CLIENT_OFFLINE;
            server->client_count--;
            return 0;
        }
    }

    return -1;
}

// 根据fd查找客户端
Client* client_find_by_fd(Server *server, int fd) {
    if (!server || fd < 0) return NULL;

    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].fd == fd) {
            return &server->clients[i];
        }
    }

    return NULL;
}

// 根据用户名查找客户端
Client* client_find_by_username(Server *server, const char *username) {
    if (!server || !username) return NULL;

    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].status == CLIENT_LOGINED &&
            strcmp(server->clients[i].username, username) == 0) {
            return &server->clients[i];
        }
    }

    return NULL;
}
