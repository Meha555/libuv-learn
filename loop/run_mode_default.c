#include <stdio.h>
#include <uv.h>

uv_loop_t* loop = NULL;

void timer_callback(uv_timer_t* handle) {
    printf("Timer fired!\n");
}

void run_default_example() {
    uv_timer_t timer;
    uv_timer_init(loop, &timer);
    uv_timer_start(&timer, timer_callback, 1000, 1000);
    printf("Running in UV_RUN_DEFAULT mode. Press Ctrl + C to stop.\n");
    int ret = uv_run(loop, UV_RUN_DEFAULT);
    printf("Return value: %d\n", ret);
    uv_timer_stop(&timer);
    uv_close((uv_handle_t*)&timer, NULL);
    uv_loop_close(loop);
}

int main() {
    loop = uv_default_loop();
    run_default_example();
    return 0;
}