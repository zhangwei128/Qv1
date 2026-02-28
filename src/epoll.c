#include "../include/common.h"

// 创建epoll实例
int epoll_create_fd() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        return -1;
    }
    return epoll_fd;
}

// 将fd加入epoll监听（边缘触发+非阻塞）
int epoll_add_fd(int epoll_fd, int fd) {
    if (epoll_fd == -1 || fd == -1) return -1;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // 边缘触发+读事件
    ev.data.fd = fd;

    // 添加到epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl add failed");
        return -1;
    }

    // 设置fd为非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get flags failed");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set nonblock failed");
        return -1;
    }

    return 0;
}

// 处理epoll事件（核心）
int epoll_handle_events(Server *server, int epoll_fd, struct epoll_event *events, int n) {
    if (!server || !events || n <= 0) return -1;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char client_ip[46];

    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;
        uint32_t revents = events[i].events;

        // 处理错误事件
        if (revents & (EPOLLERR | EPOLLHUP)) {
            printf("[Epoll] 客户端fd=%d 异常断开\n", fd);
            client_remove(server, fd);
            close(fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            continue;
        }

        // 处理读事件
        if (revents & EPOLLIN) {
            // 新客户端连接（监听fd）
            if (fd == server->listen_fd) {
                int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_addr_len);
                if (client_fd == -1) {
                    perror("accept failed");
                    continue;
                }

                // 限制最大连接数
                if (server->client_count >= server->max_clients) {
                    printf("[Epoll] 达到最大连接数，拒绝fd=%d\n", client_fd);
                    close(client_fd);
                    continue;
                }

                // 获取客户端IP
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

                // 添加客户端到数组
                if (client_add(server, client_fd, client_ip) == 0) {
                    // 将客户端fd加入epoll
                    if (epoll_add_fd(epoll_fd, client_fd) == 0) {
                        printf("[Epoll] 新客户端连接：IP=%s, fd=%d, 在线数=%d\n",
                               client_ip, client_fd, server->client_count);
                    } else {
                        client_remove(server, client_fd);
                        close(client_fd);
                        printf("[Epoll] 添加fd=%d到epoll失败\n", client_fd);
                    }
                } else {
                    close(client_fd);
                    printf("[Epoll] 添加客户端IP=%s失败\n", client_ip);
                }
            }
            // 客户端消息（普通fd）
            else {
                // 封装任务参数
                int *fd_ptr = (int*)malloc(sizeof(int));
                *fd_ptr = fd;
                // 将消息处理任务加入线程池
                thread_pool_add_task(server->thread_pool, (void(*)(void*))client_handle_msg, fd_ptr);
            }
        }
    }
    return 0;
}
