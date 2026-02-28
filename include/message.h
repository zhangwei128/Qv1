#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 消息类型枚举
typedef enum {
    MSG_LOGIN,    // 登录消息
    MSG_CHAT,     // 聊天消息
    MSG_LOGOUT,   // 退出消息
    MSG_ERROR     // 错误消息
} MsgType;

// 客户端/服务端通信的消息结构体
typedef struct {
    MsgType type;     // 消息类型
    char from[32];    // 发送者账号
    char to[32];      // 接收者账号（群聊填"all"）
    char content[256];// 消息内容
} Message;

// 序列化：将Message转为字符串（用于网络传输）
int msg_serialize(const Message *msg, char *buf, int buf_len);

// 反序列化：将字符串转为Message
int msg_deserialize(const char *buf, int buf_len, Message *msg);

// 转发消息给目标客户端（需结合客户端管理逻辑实现）
int msg_forward(const Message *msg, void *client_manager);

#endif
