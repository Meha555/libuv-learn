#include <stdio.h>
#include <uv.h>

uv_loop_t* loop = NULL;

void timer_callback(uv_timer_t* handle) {
    printf("Timer fired!\n");
}

void run_nowait_example() {
    uv_timer_t timer;
    uv_timer_init(loop, &timer);
    uv_timer_start(&timer, timer_callback, 1000, 1000);
    printf("Running in UV_RUN_NOWAIT mode. Timer may not fire.\n");
    int ret = uv_run(loop, UV_RUN_NOWAIT);
    printf("Return value: %d\n", ret); // 返回值是1，原因同UV_RUN_ONCE。没有执行timer_callback，因为uv_run不阻塞，导致程序退出了
    uv_timer_stop(&timer);
    uv_close((uv_handle_t*)&timer, NULL);
    uv_loop_close(loop);
}

int main() {
    loop = uv_default_loop();
    run_nowait_example();
    return 0;
}