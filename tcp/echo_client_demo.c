#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define DEFAULT_PORT 6666
#define DEFAULT_HOST "127.0.0.1"

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

uv_loop_t *client_loop;
uv_tcp_t client;

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

void on_write(uv_write_t* req, int status);

void do_echo() {
    char* message = (char*)calloc(1, 125);
    scanf("%s", message);
    write_req_t *wr = (write_req_t*)malloc(sizeof(write_req_t));
    wr->buf = uv_buf_init(message, 125);
    // char message[125];
    // memset(message, 0, sizeof(message));
    // uv_buf_t buf = uv_buf_init((char *)message, strlen(message));

    // The memory pointed to by the buffers must remain valid until the callback gets called.
    uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    uv_write((uv_write_t*)wr, (uv_stream_t *)&client, &wr->buf, 1, on_write);
}

// 连接成功后的回调函数
void on_connect(uv_connect_t *req, int status) {
    free(req);
    if (status < 0) {
        fprintf(stderr, "Connection error: %s\n", uv_strerror(status));
        return;
    }
    printf("Connected to server\n");
    do_echo();
}

// 分配缓冲区的回调函数
void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

// 数据读完成后的回调函数
void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        printf("Received from server: %.*s\n", (int)nread, buf->base);
        do_echo();
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "Read error: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t *)stream, NULL);
    }
    free(buf->base);
}

// 数据写完成后的回调函数
void on_write(uv_write_t* req, int status) {
    if (status == 0) {
        uv_read_start((uv_stream_t*)&client, alloc_cb, on_read);
    } else {
        if (status != UV_EOF) {
            fprintf(stderr, "Write error: %s\n", uv_strerror(status));
        }
    }
    free_write_req(req);
}

int main() {
    struct sockaddr_in addr;

    client_loop = uv_default_loop();

    // 初始化 TCP 客户端
    uv_tcp_init(client_loop, &client);

    // 解析服务器地址
    uv_ip4_addr(DEFAULT_HOST, DEFAULT_PORT, &addr);

    // 创建连接请求
    uv_connect_t *connect_req = (uv_connect_t*)malloc(sizeof(uv_connect_t));
    int r = uv_tcp_connect(connect_req, &client, (const struct sockaddr *)&addr, on_connect);
    if (r) {
        fprintf(stderr, "Connect error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(client_loop, UV_RUN_DEFAULT);
}