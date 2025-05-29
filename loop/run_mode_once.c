#include <stdio.h>
#include <uv.h>

uv_loop_t* loop = NULL;

void timer_callback(uv_timer_t* handle) {
    printf("Timer fired!\n");
}

void run_once_example() {
    uv_timer_t timer;
    uv_timer_init(loop, &timer);
    uv_timer_start(&timer, timer_callback, 1000, 1000);
    printf("Running in UV_RUN_ONCE mode. Timer will fire once.\n");
    int ret = uv_run(loop, UV_RUN_ONCE);
    printf("Return value: %d\n", ret); // 返回值是1，因为这个timer是多发定时器，而UV_RUN_ONCE只poll了第一次超时，还有下一次超时没有被poll。
    uv_timer_stop(&timer);
    uv_close((uv_handle_t*)&timer, NULL);
    uv_loop_close(loop);
}

int main() {
    loop = uv_default_loop();
    run_once_example();
    return 0;
}