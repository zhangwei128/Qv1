CC = gcc
CFLAGS = -Wall -g -Iinclude -O2
LDFLAGS = -lpthread -lm

# 服务器源文件
SERVER_SRCS = src/server.c src/epoll.c src/threadpool.c src/client_mgr.c src/message.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
SERVER_TARGET = qq_server

# 客户端源文件
CLIENT_SRCS = src/client.c src/message.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
CLIENT_TARGET = qq_client

# 默认编译服务器+客户端
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# 编译服务器
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(LDFLAGS)

# 编译客户端
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(LDFLAGS)

# 清理
clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_TARGET) $(CLIENT_TARGET)

# 运行服务器
run_server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

# 运行客户端（示例：连接本地服务器）
run_client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET) 127.0.0.1 8888

.PHONY: all clean run_server run_client
