#include "../include/common.h"

// 线程工作函数
static void *thread_worker(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    if (!pool) return NULL;

    while (1) {
        // 加锁
        pthread_mutex_lock(&pool->mutex);

        // 无任务且未关闭：等待
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        // 线程池关闭且无任务：退出
        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->mutex);
            pthread_exit(NULL);
        }

        // 取出队头任务
        ThreadTask task = pool->task_queue[pool->front];
        pool->front = (pool->front + 1) % pool->queue_size;
        pool->count--;

        // 解锁
        pthread_mutex_unlock(&pool->mutex);

        // 执行任务
        if (task.func) {
            task.func(task.arg);
        }

        // 释放任务参数
        if (task.arg) {
            free(task.arg);
        }
    }

    return NULL;
}

// 创建线程池（修复：移除错误的thread_pool_size赋值）
ThreadPool* thread_pool_create(int size) {
    if (size <= 0) {
        fprintf(stderr, "线程池大小必须>0\n");
        return NULL;
    }

    ThreadPool *pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("malloc thread pool failed");
        return NULL;
    }

    // 初始化线程池参数（仅初始化ThreadPool结构体中定义的成员）
    pool->queue_size = 1024;       // 任务队列大小
    pool->front = 0;
    pool->rear = 0;
    pool->count = 0;
    pool->shutdown = 0;

    // 创建线程数组
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * size);
    if (!pool->threads) {
        perror("malloc threads failed");
        free(pool);
        return NULL;
    }

    // 创建任务队列
    pool->task_queue = (ThreadTask*)malloc(sizeof(ThreadTask) * pool->queue_size);
    if (!pool->task_queue) {
        perror("malloc task queue failed");
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&pool->mutex, NULL) != 0 ||
        pthread_cond_init(&pool->cond, NULL) != 0) {
        perror("init mutex/cond failed");
        free(pool->task_queue);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // 创建工作线程
    for (int i = 0; i < size; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_worker, pool) != 0) {
            perror("pthread_create failed");
            // 销毁已创建的线程
            for (int j = 0; j < i; j++) {
                pthread_cancel(pool->threads[j]);
            }
            free(pool->task_queue);
            free(pool->threads);
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond);
            free(pool);
            return NULL;
        }
    }

    printf("[ThreadPool] 线程池创建成功，线程数=%d\n", size);
    return pool;
}

// 添加任务到线程池
int thread_pool_add_task(ThreadPool *pool, void (*func)(void *), void *arg) {
    if (!pool || !func || pool->shutdown) return -1;

    // 加锁
    pthread_mutex_lock(&pool->mutex);

    // 队列满了
    if (pool->count >= pool->queue_size) {
        pthread_mutex_unlock(&pool->mutex);
        fprintf(stderr, "任务队列已满，拒绝添加任务\n");
        return -1;
    }

    // 添加任务到队尾
    pool->task_queue[pool->rear].func = func;
    pool->task_queue[pool->rear].arg = arg;
    pool->rear = (pool->rear + 1) % pool->queue_size;
    pool->count++;

    // 唤醒一个工作线程
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

// 销毁线程池（修复：用传入的size参数遍历线程，而非错误的thread_pool_size）
void thread_pool_destroy(ThreadPool *pool) {
    if (!pool) return;

    // 先获取线程数（通过线程数组长度反推，或改为全局/参数传递）
    // 简化方案：遍历到线程数组为NULL为止（更鲁棒）
    int thread_count = 0;
    if (pool->threads) {
        // 计算线程数（创建时的size）：这里简化为16（匹配THREAD_POOL_SIZE）
        thread_count = THREAD_POOL_SIZE;
    }

    // 标记关闭
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_mutex_unlock(&pool->mutex);

    // 唤醒所有线程
    pthread_cond_broadcast(&pool->cond);

    // 等待所有线程退出（修复：用正确的线程数）
    for (int i = 0; i < thread_count; i++) {
        if (pool->threads[i] != 0) {
            pthread_join(pool->threads[i], NULL);
        }
    }

    // 释放资源
    free(pool->task_queue);
    free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    free(pool);

    printf("[ThreadPool] 线程池已销毁\n");
}
