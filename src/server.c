#include "../include/common.h"

// 客户端消息处理函数（线程池执行）
void client_handle_msg(void *arg) {
    int fd = *(int*)arg;
    Server *server = NULL;
    
    // 全局服务器指针
    extern Server *global_server;
    server = global_server;
    
    if (!server) return;

    // 查找客户端
    Client *client = client_find_by_fd(server, fd);
    if (!client) {
        fprintf(stderr, "[Msg] 找不到fd=%d的客户端\n", fd);
        close(fd);
        return;
    }

    // 读取消息
    char buf[MAX_MESSAGE_LEN * 2] = {0};
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) {
        printf("[Msg] 客户端fd=%d（%s）断开连接\n", fd, client->ip);
        client_remove(server, fd);
        close(fd);
        return;
    }

    // 解析消息
    Message msg;
    if (message_parse(buf, &msg) != 0) {
        fprintf(stderr, "[Msg] fd=%d 消息解析失败：%s\n", fd, buf);
        // 修复：结构体初始化+字符串赋值
        Message err_msg = {0}; // 初始化所有字段为0
        err_msg.type = MSG_ERROR;
        strcpy(err_msg.from, "server");
        strcpy(err_msg.to, client->username); // 用strcpy赋值字符串
        strcpy(err_msg.content, "消息格式错误");
        err_msg.timestamp = time(NULL);
        message_send(fd, &err_msg);
        return;
    }

    // 处理不同类型消息
    switch (msg.type) {
        case MSG_LOGIN: {
            printf("[Login] IP=%s(fd=%d) 尝试登录：%s\n", client->ip, fd, msg.from);
            
            // 简单登录验证
            if (strlen(msg.from) < 3 || strlen(msg.content) < 6) {
                Message err_msg = {0};
                err_msg.type = MSG_ERROR;
                strcpy(err_msg.from, "server");
                strcpy(err_msg.to, msg.from); // 修复字符串赋值
                strcpy(err_msg.content, "用户名/密码长度不足");
                err_msg.timestamp = time(NULL);
                message_send(fd, &err_msg);
                break;
            }

            // 检查用户名是否已登录
            if (client_find_by_username(server, msg.from)) {
                Message err_msg = {0};
                err_msg.type = MSG_ERROR;
                strcpy(err_msg.from, "server");
                strcpy(err_msg.to, msg.from); // 修复字符串赋值
                strcpy(err_msg.content, "用户名已登录");
                err_msg.timestamp = time(NULL);
                message_send(fd, &err_msg);
                break;
            }

            // 登录成功（修复strncpy截断警告：手动加终止符）
            strncpy(client->username, msg.from, sizeof(client->username)-1);
            client->username[sizeof(client->username)-1] = '\0'; // 补充终止符
            client->status = CLIENT_LOGINED;
            
            Message ok_msg = {0};
            ok_msg.type = MSG_LOGIN;
            strcpy(ok_msg.from, "server");
            strcpy(ok_msg.to, msg.from); // 修复字符串赋值
            strcpy(ok_msg.content, "登录成功");
            ok_msg.timestamp = time(NULL);
            message_send(fd, &ok_msg);
            printf("[Login] IP=%s(fd=%d) 登录成功：%s\n", client->ip, fd, msg.from);
            break;
        }

        case MSG_CHAT: {
            if (client->status != CLIENT_LOGINED) {
                Message err_msg = {0};
                err_msg.type = MSG_ERROR;
                strcpy(err_msg.from, "server");
                strcpy(err_msg.to, client->username);
                strcpy(err_msg.content, "未登录，无法发送消息");
                err_msg.timestamp = time(NULL);
                message_send(fd, &err_msg);
                break;
            }

            // 群聊
            if (strcmp(msg.to, "all") == 0) {
                int count = message_broadcast(server, &msg, client->username);
                printf("[Chat] %s(fd=%d) 群聊：%s（接收者=%d）\n",
                       client->username, fd, msg.content, count);
                
                // 回复发送成功
                Message ok_msg = {0};
                ok_msg.type = MSG_CHAT;
                strcpy(ok_msg.from, "server");
                strcpy(ok_msg.to, client->username); // 修复字符串赋值
                strcpy(ok_msg.content, "群消息发送成功");
                ok_msg.timestamp = time(NULL);
                message_send(fd, &ok_msg);
            }
            // 私聊
            else {
                Client *target = client_find_by_username(server, msg.to);
                if (!target) {
                    Message err_msg = {0};
                    err_msg.type = MSG_ERROR;
                    strcpy(err_msg.from, "server");
                    strcpy(err_msg.to, client->username); // 修复字符串赋值
                    strcpy(err_msg.content, "接收者不存在或未登录");
                    err_msg.timestamp = time(NULL);
                    message_send(fd, &err_msg);
                    break;
                }

                // 发送私聊消息
                if (message_send(target->fd, &msg) == 0) {
                    printf("[Chat] %s -> %s: %s\n", client->username, msg.to, msg.content);
                    Message ok_msg = {0};
                    ok_msg.type = MSG_CHAT;
                    strcpy(ok_msg.from, "server");
                    strcpy(ok_msg.to, client->username); // 修复字符串赋值
                    strcpy(ok_msg.content, "私聊消息发送成功");
                    ok_msg.timestamp = time(NULL);
                    message_send(fd, &ok_msg);
                } else {
                    Message err_msg = {0};
                    err_msg.type = MSG_ERROR;
                    strcpy(err_msg.from, "server");
                    strcpy(err_msg.to, client->username); // 修复字符串赋值
                    strcpy(err_msg.content, "私聊消息发送失败");
                    err_msg.timestamp = time(NULL);
                    message_send(fd, &err_msg);
                }
            }
            break;
        }

        case MSG_LOGOUT: {
            printf("[Logout] %s(fd=%d) 退出登录\n", client->username, fd);
            client->status = CLIENT_ONLINE;
            memset(client->username, 0, sizeof(client->username));
            
            Message ok_msg = {0};
            ok_msg.type = MSG_LOGOUT;
            strcpy(ok_msg.from, "server");
            strcpy(ok_msg.to, msg.from); // 修复字符串赋值
            strcpy(ok_msg.content, "退出成功");
            ok_msg.timestamp = time(NULL);
            message_send(fd, &ok_msg);
            break;
        }
        
        case MSG_ONLINE_LIST: {
            char online_list[900] = {0};
            int user_count = message_get_online_list(server, online_list, sizeof(online_list));
    
            Message list_msg = {0};
            list_msg.type = MSG_ONLINE_LIST;
    
            // 用snprintf替代strncpy，自动加终止符，消除警告
            snprintf(list_msg.from, sizeof(list_msg.from), "server");
            snprintf(list_msg.to, sizeof(list_msg.to), "%s", client->username);
    
            // 拼接在线列表（snprintf自动处理终止符）
             snprintf(list_msg.content, sizeof(list_msg.content),
                    "在线用户(%d)：%s", user_count, online_list);
    
            list_msg.timestamp = time(NULL);
            message_send(fd, &list_msg);
            printf("[OnlineList] %s 请求在线列表，当前在线%d人\n", 
                    client->username, user_count);
            break;
        } 
        default:
            fprintf(stderr, "[Msg] 未知消息类型：%d\n", msg.type);
            break;
    }
}


            

// 全局服务器指针（简化线程池参数传递）
Server *global_server = NULL;

// 服务器初始化
Server* server_init(int port, int max_clients) {
    // 分配内存
    Server *server = (Server*)malloc(sizeof(Server));
    if (!server) {
        perror("malloc server failed");
        return NULL;
    }
    memset(server, 0, sizeof(Server));

    // 初始化参数
    server->port = port;
    server->max_clients = max_clients;
    server->client_count = 0;
    server->thread_pool_size = THREAD_POOL_SIZE;

    // 创建监听socket
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd == -1) {
        perror("socket failed");
        free(server);
        return NULL;
    }

    // 端口复用
    int opt = 1;
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server->listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // 监听
    if (listen(server->listen_fd, BACKLOG) == -1) {
        perror("listen failed");
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // 创建epoll
    server->epoll_fd = epoll_create_fd();
    if (server->epoll_fd == -1) {
        perror("epoll create failed");
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // 添加监听fd到epoll
    if (epoll_add_fd(server->epoll_fd, server->listen_fd) == -1) {
        perror("epoll add listen fd failed");
        close(server->epoll_fd);
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // 分配客户端数组
    server->clients = (Client*)malloc(sizeof(Client) * max_clients);
    if (!server->clients) {
        perror("malloc clients failed");
        close(server->epoll_fd);
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    memset(server->clients, 0, sizeof(Client) * max_clients);

    // 创建线程池
    server->thread_pool = thread_pool_create(THREAD_POOL_SIZE);
    if (!server->thread_pool) {
        perror("thread pool create failed");
        free(server->clients);
        close(server->epoll_fd);
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // 设置全局服务器指针
    global_server = server;

    printf("[Server] 初始化完成：端口=%d，最大客户端=%d，线程池=%d\n",
           port, max_clients, THREAD_POOL_SIZE);
    return server;
}

// 服务器启动
int server_start(Server *server) {
    if (!server) return -1;

    struct epoll_event events[EPOLL_MAX_EVENTS];
    printf("[Server] 启动成功，监听0.0.0.0:%d...\n", server->port);

    while (1) {
        // 等待epoll事件
        int n = epoll_wait(server->epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait failed");
            continue;
        }

        // 处理事件
        epoll_handle_events(server, server->epoll_fd, events, n);
    }

    return 0;
}

// 销毁服务器
void server_destroy(Server *server) {
    if (!server) return;

    printf("[Server] 开始销毁资源...\n");

    // 关闭所有客户端
    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].fd > 0) {
            close(server->clients[i].fd);
        }
    }

    // 销毁线程池
    if (server->thread_pool) {
        thread_pool_destroy(server->thread_pool);
    }

    // 关闭监听fd和epoll
    if (server->listen_fd > 0) close(server->listen_fd);
    if (server->epoll_fd > 0) close(server->epoll_fd);

    // 释放内存
    if (server->clients) free(server->clients);
    free(server);
    global_server = NULL;

    printf("[Server] 所有资源已释放\n");
}

// 主函数（测试用）
int main() {
    Server *server = server_init(SERVER_PORT, MAX_CLIENTS);
    if (!server) {
        fprintf(stderr, "服务器初始化失败\n");
        return -1;
    }

    server_start(server);
    server_destroy(server);
    return 0;
}
