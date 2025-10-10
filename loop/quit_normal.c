#include <stdio.h>
#include <uv.h>

void idle_cb(uv_idle_t *idle) {
    printf("idle callback\n");
    static int count = 0;
    if (++count >= 5) {
        // 停掉这个句柄后，如果loop没有其他活跃的句柄了，则loop会停止
        uv_idle_stop(idle);
    }
}

void timer_cb(uv_timer_t *timer) {
    printf("timer callback\n");
    static int count = 0;
    if (++count >= 3) {
        // 停掉这个句柄后，如果loop没有其他活跃的句柄了，则loop会停止
        uv_timer_stop(timer);
        uv_close((uv_handle_t *)timer, NULL);
    }
}

int main() {
    uv_loop_t *loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_idle_t idle;
    uv_idle_init(loop, &idle);
    uv_idle_start(&idle, idle_cb);

    uv_timer_t timer;
    uv_timer_init(loop, &timer);
    // NOTE 如果这里是单发定时器，则其超时回调中不调用uv_timer_stop也是可以的
    uv_timer_start(&timer, timer_cb, 1000, 1000);

    uv_run(loop, UV_RUN_DEFAULT);

    printf("after uv_run\n");
    uv_loop_close(loop);
    free(loop);
    return 0;
}