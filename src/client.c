#include "../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

// 客户端全局状态
typedef struct {
    int sock_fd;          // 服务器连接fd
    char username[32];    // 登录的用户名
    int is_login;         // 是否已登录（0/1）
} ClientState;

ClientState client_state = {0}; // 初始化客户端状态

// 发送消息到服务器（序列化后发送）
int send_message_to_server(int fd, const Message *msg) {
    if (fd < 0 || !msg) return -1;

    // 序列化消息（格式：type|from|to|content|timestamp）
    char buf[MAX_MESSAGE_LEN * 2] = {0};
    int len = snprintf(buf, sizeof(buf), "%d|%s|%s|%s|%ld",
                      msg->type, msg->from, msg->to, msg->content, msg->timestamp);
    if (len <= 0 || len >= sizeof(buf)) {
        fprintf(stderr, "消息序列化失败\n");
        return -1;
    }

    // 发送消息
    ssize_t n = write(fd, buf, len);
    if (n != len) {
        perror("发送消息失败");
        return -1;
    }

    return 0;
}

// 处理服务器回复的消息（优化：显示发送者）
void handle_server_response(int fd) {
    char buf[MAX_MESSAGE_LEN * 2] = {0};
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) {
        fprintf(stderr, "服务器断开连接\n");
        close(fd);
        exit(1);
    }

    // 解析服务器消息
    Message msg;
    if (message_parse(buf, &msg) != 0) {
        fprintf(stderr, "解析服务器消息失败：%s\n", buf);
        return;
    }

    // 根据消息类型处理
    switch (msg.type) {
        case MSG_LOGIN:
            if (strcmp(msg.content, "登录成功") == 0) {
                client_state.is_login = 1;
                strcpy(client_state.username, msg.to);
                printf("✅ 登录成功！当前用户名：%s\n", client_state.username);
            } else {
                printf("❌ 登录失败：%s\n", msg.content);
            }
            break;

        case MSG_CHAT:
            // 区分：服务器回复 vs 其他用户发送的聊天消息
            if (strcmp(msg.from, "server") == 0) {
                // 服务器的发送成功提示
                printf("📩 服务器回复：%s\n", msg.content);
            } else {
                // 其他用户发送的消息（显示发送者）
                if (strcmp(msg.to, "all") == 0) {
                    printf("📢 %s 群聊：%s\n", msg.from, msg.content);
                } else {
                    printf("💬 %s 对你说：%s\n", msg.from, msg.content);
                }
            }
            break;

        // 新增：处理在线列表消息
        case MSG_ONLINE_LIST:
            printf("📋 %s\n", msg.content);
            break;

        case MSG_LOGOUT:
            printf("✅ 退出成功！\n");
            client_state.is_login = 0;
            memset(client_state.username, 0, sizeof(client_state.username));
            break;

        case MSG_ERROR:
            printf("❌ 错误：%s\n", msg.content);
            break;

        default:
            printf("📨 服务器消息：%s\n", msg.content);
            break;
    }
}

// 登录指令处理
void handle_login(const char *username, const char *password) {
    if (client_state.is_login) {
        printf("❌ 你已登录，无需重复登录\n");
        return;
    }

    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        printf("❌ 用户名/密码不能为空\n");
        return;
    }

    // 构造登录消息
    Message msg = {0};
    msg.type = MSG_LOGIN;
    strcpy(msg.from, username);
    strcpy(msg.to, "server");
    strcpy(msg.content, password); // 密码放在content字段
    msg.timestamp = time(NULL);

    // 发送登录消息
    if (send_message_to_server(client_state.sock_fd, &msg) == 0) {
        printf("🔑 正在登录：%s\n", username);
    }
}

// 发送聊天消息指令处理
void handle_chat(const char *to, const char *content) {
    if (!client_state.is_login) {
        printf("❌ 请先登录！（输入：login 用户名 密码）\n");
        return;
    }

    if (!to || !content || strlen(content) == 0) {
        printf("❌ 消息内容不能为空\n");
        return;
    }

    // 构造聊天消息
    Message msg = {0};
    msg.type = MSG_CHAT;
    strcpy(msg.from, client_state.username);
    strcpy(msg.to, to);
    strcpy(msg.content, content);
    msg.timestamp = time(NULL);

    // 发送聊天消息
    if (send_message_to_server(client_state.sock_fd, &msg) == 0) {
        if (strcmp(to, "all") == 0) {
            printf("📤 群聊消息：%s\n", content);
        } else {
            printf("📤 私聊<%s>：%s\n", to, content);
        }
    }
}

// 退出登录指令处理
void handle_logout() {
    if (!client_state.is_login) {
        printf("❌ 你还未登录\n");
        return;
    }

    // 构造退出消息
    Message msg = {0};
    msg.type = MSG_LOGOUT;
    strcpy(msg.from, client_state.username);
    strcpy(msg.to, "server");
    strcpy(msg.content, "logout");
    msg.timestamp = time(NULL);

    // 发送退出消息
    if (send_message_to_server(client_state.sock_fd, &msg) == 0) {
        printf("🚪 正在退出登录...\n");
    }
}

// 新增：获取在线列表
void handle_online_list() {
    if (!client_state.is_login) {
        printf("❌ 请先登录！（输入：login 用户名 密码）\n");
        return;
    }

    // 构造在线列表请求消息
    Message msg = {0};
    msg.type = MSG_ONLINE_LIST;
    strcpy(msg.from, client_state.username);
    strcpy(msg.to, "server");
    strcpy(msg.content, "get online list");
    msg.timestamp = time(NULL);

    if (send_message_to_server(client_state.sock_fd, &msg) == 0) {
        printf("🔍 正在获取在线用户列表...\n");
    }
}




// 解析用户输入的指令（新增list指令）
void parse_user_input(const char *input) {
    if (!input || strlen(input) == 0) return;

    char cmd[32] = {0};
    char arg1[32] = {0};
    char arg2[1024] = {0};
    sscanf(input, "%s %s %[^\n]", cmd, arg1, arg2);

    // 处理不同指令
    if (strcmp(cmd, "login") == 0) {
        handle_login(arg1, arg2);
    } else if (strcmp(cmd, "chat") == 0) {
        handle_chat(arg1, arg2);
    } else if (strcmp(cmd, "logout") == 0) {
        handle_logout();
    } else if (strcmp(cmd, "list") == 0) { // 新增list指令
        handle_online_list();
    } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        printf("👋 退出客户端...\n");
        if (client_state.sock_fd > 0) {
            close(client_state.sock_fd);
        }
        exit(0);
    } else if (strcmp(cmd, "help") == 0) {
        // 优化帮助信息
        printf("===== 指令帮助 =====\n");
        printf("login 用户名 密码   - 登录服务器\n");
        printf("chat 接收者 消息   - 发送消息（接收者填all为群聊）\n");
        printf("list                - 查看在线用户列表\n"); // 新增
        printf("logout             - 退出登录\n");
        printf("quit/exit          - 退出客户端\n");
        printf("help               - 查看帮助\n");
        printf("====================\n");
    } else {
        printf("❌ 未知指令！输入help查看帮助\n");
    }
}

// 客户端主循环（监听用户输入和服务器消息）
void client_main_loop() {
    fd_set read_fds;
    char input_buf[MAX_MESSAGE_LEN] = {0};

    printf("===== QQ客户端（连接服务器成功）=====\n");
    printf("💡 输入help查看所有指令\n");

    while (1) {
        // 初始化fd集合
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // 标准输入（用户输入）
        FD_SET(client_state.sock_fd, &read_fds); // 服务器连接

        // 监听fd（无超时，阻塞等待）
        int max_fd = (client_state.sock_fd > STDIN_FILENO) ? client_state.sock_fd : STDIN_FILENO;
        int n = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (n == -1) {
            perror("select失败");
            continue;
        }

        // 处理服务器消息
        if (FD_ISSET(client_state.sock_fd, &read_fds)) {
            handle_server_response(client_state.sock_fd);
        }

        // 处理用户输入
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(input_buf, 0, sizeof(input_buf));
            if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
                continue;
            }

            // 去掉换行符
            input_buf[strcspn(input_buf, "\n")] = '\0';
            // 解析并处理指令
            parse_user_input(input_buf);
        }
    }
}

// 客户端初始化（连接服务器）
int client_init(const char *server_ip, int server_port) {
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("创建socket失败");
        return -1;
    }

    // 服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("无效的服务器IP");
        close(sock_fd);
        return -1;
    }

    // 连接服务器
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("连接服务器失败");
        close(sock_fd);
        return -1;
    }

    printf("✅ 成功连接服务器：%s:%d\n", server_ip, server_port);
    client_state.sock_fd = sock_fd;
    return sock_fd;
}

// 主函数
int main(int argc, char *argv[]) {
    // 检查参数
    if (argc != 3) {
        fprintf(stderr, "用法：%s <服务器IP> <服务器端口>\n", argv[0]);
        fprintf(stderr, "示例：%s 127.0.0.1 8888\n", argv[0]);
        return -1;
    }

    // 初始化客户端（连接服务器）
    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    if (client_init(server_ip, server_port) < 0) {
        fprintf(stderr, "客户端初始化失败\n");
        return -1;
    }

    // 进入主循环
    client_main_loop();

    // 释放资源
    close(client_state.sock_fd);
    return 0;
}
