#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

// 任务结构体：每个任务对应一个待处理的客户端请求
typedef struct Task {
    void (*function)(void *arg);  // 任务处理函数
    void *arg;                    // 函数参数
    struct Task *next;            // 下一个任务（链表）
} Task;

// 线程池结构体
typedef struct ThreadPool {
    pthread_mutex_t mutex;        // 互斥锁：保护任务队列
    pthread_cond_t cond;          // 条件变量：唤醒空闲线程
    Task *task_queue;             // 任务队列（链表）
    pthread_t *threads;           // 线程数组
    int thread_count;             // 线程数量
    int queue_size;               // 任务队列大小
    int is_running;               // 线程池是否运行
} ThreadPool;

// 线程池初始化
ThreadPool *threadpool_create(int thread_count);

// 添加任务到线程池
int threadpool_add_task(ThreadPool *pool, void (*func)(void *), void *arg);

// 销毁线程池
void threadpool_destroy(ThreadPool *pool);

#endif
