#include <uv.h>
#include <stdio.h>

#define TIMEOUT_MS 1

// 添加连接取消标记
int connect_cancelled = 0;

uv_pipe_t client;
uv_connect_t connect_req;
uv_timer_t timer;

void on_connect(uv_connect_t* req, int status) {
    // 检查连接是否已取消
    if (connect_cancelled) {
        return;
    }
    if (status == 0) {
        printf("连接成功\n");
    } else {
        fprintf(stderr, "连接失败: %s\n", uv_strerror(status));
    }
}

void conn_cancel_cb(uv_handle_t* handle) {
    fprintf(stderr, "取消连接\n");
}

void on_timeout(uv_timer_t* handle) {
    printf("连接超时\n");
    // 标记连接已取消
    connect_cancelled = 1;
    uv_close((uv_handle_t*) &connect_req, conn_cancel_cb);
    uv_close((uv_handle_t*) &client, NULL);
    uv_timer_stop(&timer);
}

int main() {
    uv_loop_t* loop = uv_default_loop();

    uv_pipe_init(loop, &client, 0);
    uv_timer_init(loop, &timer);

    uv_pipe_connect(&connect_req, &client, "\\\\?\\pipe\\echo.sock", on_connect);
    uv_timer_start(&timer, on_timeout, TIMEOUT_MS, 0);

    return uv_run(loop, UV_RUN_DEFAULT);
}