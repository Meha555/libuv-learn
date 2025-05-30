#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#ifdef _WIN32
#define PIPENAME "\\\\.\\pipe\\echo.sock" // Windows下命名管道必须以\\.\pipe\为前缀
#else
#define PIPENAME "/tmp/echo.sock"
#endif

typedef struct{
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

uv_loop_t *loop;
uv_pipe_t client;

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t *)req;
    free(wr->buf.base);
    free(wr);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_write(uv_write_t *req, int status);

void do_echo() {
    char *buf = (char*)calloc(1, 125);
    scanf("%s", buf);
    write_req_t *wr = (write_req_t*)malloc(sizeof(write_req_t));
    wr->buf = uv_buf_init(buf, 125);
    uv_write(&wr->req, (uv_stream_t *)&client, &wr->buf, 1, on_write);
}

void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        printf("read: %s\n", buf->base);
        do_echo();
    } else if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "read error: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t*)&client, NULL);
    }
    free(buf->base);
}

void on_write(uv_write_t *req, int status) {
    if (status == 0) {
        uv_read_start((uv_stream_t*)&client, alloc_cb, on_read);
    } else {
        if (status != UV_EOF) {
            fprintf(stderr, "write error: %s\n", uv_strerror(status));
        }
    }
    free_write_req(req);
}

void on_connect(uv_connect_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "connect error: %s\n", uv_strerror(status));
        return;
    }
    printf("connect to server\n");
    do_echo();
}

int main() {
    loop = uv_default_loop();

    uv_pipe_init(loop, &client, 0);

    uv_connect_t connect_req;
    uv_pipe_connect(&connect_req, &client, PIPENAME, on_connect);
    return uv_run(loop, UV_RUN_DEFAULT);
}