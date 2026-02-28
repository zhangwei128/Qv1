#ifndef COMMON_H
#define COMMON_H
//系统文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>

// 常量定义（适配你的硬件：7800X3D 16线程、32G内存）
#define SERVER_PORT 8888                // 监听端口
#define MAX_CLIENTS 100000              // 最大客户端数（适配32G内存）
#define MAX_MESSAGE_LEN 1024            // 消息最大长度（防攻击）
#define EPOLL_MAX_EVENTS 1024           // epoll最大事件数
#define THREAD_POOL_SIZE 16             // 线程池大小（适配16线程CPU）
#define BACKLOG 1024                    // 监听队列大小

//客户端状态枚举
typedef enum{
    CLIENT_OFFLINE = 0, //离线
    CLIENT_ONLINE = 1,  //在线（未登录）
    CLIENT_LOGINED = 2, //已登录
} ClientStatus;

//客户端结构体
typedef struct {
    int fd;                 //客户端socket文件描述符（唯一标识）
    char username[32];      //用户名（登录后赋值）
    char ip[46];            //客户端ip地址
    ClientStatus status;    //客户端状态
    time_t connect_time;    //连接时间
} Client;

//消息类型枚举
typedef enum{
    MSG_LOGIN = 1,          //登录消息
    MSG_CHAT = 2,           //普通聊天消息（群聊/私聊）
    MSG_LOGOUT = 3,         //退出消息
    MSG_ERROR = 4,          //错误提示
    MSG_ONLINE_LIST = 5     //在线用户列表
} MessageType;

//消息结构体
typedef struct{
    MessageType type;       //消息类型
    char from[32];          //消息发送者
    char to[32];            //接收者（群聊填"all"）
    char content[MAX_MESSAGE_LEN];     //消息内容（限制1024）
    time_t timestamp;       //消息时间戳
} Message;

//线程池任务结构体
typedef struct{
    void (*func)(void *arg); //任务函数
    void *arg;              //任务参数
} ThreadTask;

//线程池结构体
typedef struct{
    pthread_t *threads;     //线程数组（修正笔误：现成→线程）
    ThreadTask *task_queue; //任务队列（环形队列）
    int queue_size;         //队列最大容量
    int front;              //队头
    int rear;               //队尾
    int count;              //当前任务数
    pthread_mutex_t mutex;  //互斥锁
    pthread_cond_t cond;    //条件变量
    int shutdown;           //是否关闭线程池
} ThreadPool;

//服务器结构体
typedef struct {
    int listen_fd;          //监听socket
    int epoll_fd;           //epoll文件描述符
    int port;               //监听端口
    int max_clients;        //最大支持客户端数
    int client_count;       //当前在线客户端数
    Client *clients;        //客户端数组（动态分配）
    int thread_pool_size;   //线程池大小
    ThreadPool *thread_pool;//线程池实例
} Server;

//函数声明
//server.c
Server* server_init(int port, int max_clients);
int server_start(Server *server);
void server_destroy(Server *server);

//client.c
int client_add(Server *server, int fd, const char *ip);
int client_remove(Server *server, int fd);
Client* client_find_by_fd(Server *server, int fd);
Client* client_find_by_username(Server *server, const char *username);

//message.c
int message_parse(const char *buf, Message *msg);
int message_send(int fd, const Message *msg);
int message_broadcast(Server *server, const Message *msg, const char *exclude);
int message_get_online_list(Server *server, char *list, int list_len);

//epoll.c
int epoll_create_fd();
int epoll_add_fd(int epoll_fd, int fd);
int epoll_handle_events(Server *server, int epoll_fd, struct epoll_event *events, int n);
void client_handle_msg(void *arg);

//threadpool.c
ThreadPool* thread_pool_create(int size);
int thread_pool_add_task(ThreadPool *pool, void (*func)(void *), void *arg);
void thread_pool_destroy(ThreadPool *pool);

#endif //COMMON_H
