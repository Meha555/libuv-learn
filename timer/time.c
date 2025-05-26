#include <uv.h>

void timeout_cb(uv_timer_t* handle) {
    static uint64_t last_time = 0;
    uint64_t current_time = uv_hrtime();
    uint64_t elapsed_ms = (current_time - last_time) / 1e6;

    if (last_time != 0) {
        printf("Elapsed time: %llu ms\n", (unsigned long long)elapsed_ms);
    } else {
        printf("First tick\n");
    }

    last_time = current_time;
}

int main() {
    uv_loop_t *loop = uv_default_loop();

    uint64_t start_time = uv_hrtime(); // 获取起始时间

    uv_timer_t timer;
    uv_timer_init(loop, &timer);

    uv_timer_start(&timer, timeout_cb, 0, 1000);

    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}