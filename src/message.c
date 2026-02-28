#include "../include/common.h"

// 解析消息（格式：type|from|to|content|timestamp）
int message_parse(const char *buf, Message *msg) {
    if (!buf || !msg) return -1;

    char temp[MAX_MESSAGE_LEN * 2] = {0};
    strncpy(temp, buf, sizeof(temp)-1);

    // 拆分字段
    char *token = strtok(temp, "|");
    if (!token) return -1;
    msg->type = atoi(token);

    token = strtok(NULL, "|");
    if (!token) return -1;
    strncpy(msg->from, token, sizeof(msg->from)-1);

    token = strtok(NULL, "|");
    if (!token) return -1;
    strncpy(msg->to, token, sizeof(msg->to)-1);

    token = strtok(NULL, "|");
    if (!token) return -1;
    strncpy(msg->content, token, sizeof(msg->content)-1);

    token = strtok(NULL, "|");
    if (!token) return -1;
    msg->timestamp = atol(token);

    return 0;
}

// 发送消息到指定fd
int message_send(int fd, const Message *msg) {
    if (fd < 0 || !msg) return -1;

    // 序列化消息
    char buf[MAX_MESSAGE_LEN * 2] = {0};
    int len = snprintf(buf, sizeof(buf), "%d|%s|%s|%s|%ld",
                      msg->type, msg->from, msg->to, msg->content, msg->timestamp);
    if (len <= 0 || len >= sizeof(buf)) return -1;

    // 发送
    ssize_t n = write(fd, buf, len);
    return (n == len) ? 0 : -1;
}

// 广播消息（排除指定用户）
int message_broadcast(Server *server, const Message *msg, const char *exclude) {
    if (!server || !msg) return -1;

    int count = 0;
    for (int i = 0; i < server->max_clients; i++) {
        Client *client = &server->clients[i];
        // 已登录且不是排除用户
        if (client->status == CLIENT_LOGINED &&
            (exclude == NULL || strcmp(client->username, exclude) != 0)) {
            if (message_send(client->fd, msg) == 0) {
                count++;
            }
        }
    }

    return count;
}

// 获取在线用户列表（拼接为逗号分隔的字符串）

int message_get_online_list(Server *server, char *list, int list_len) {
    if (!server || !list || list_len <= 0) return -1;

    memset(list, 0, list_len);
    int count = 0;
    int current_pos = 0; // 当前拼接位置

    for (int i = 0; i < server->max_clients; i++) {
        Client *client = &server->clients[i];
        if (client->status == CLIENT_LOGINED && strlen(client->username) > 0) {
            // 计算当前需要的格式：逗号（非第一个）+ 用户名
            const char *fmt = (count > 0) ? ",%s" : "%s";
            // 用snprintf拼接，自动处理长度和终止符
            int ret = snprintf(&list[current_pos], list_len - current_pos,
                              fmt, client->username);
            
            // 检查是否拼接成功（ret < 0 或 超出长度则停止）
            if (ret < 0 || ret >= list_len - current_pos) {
                break;
            }
            
            current_pos += ret;
            count++;
        }
    }

    return count;
}


