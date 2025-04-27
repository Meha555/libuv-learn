/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2020-04-17 19:44:48
 * @LastEditTime: 2020-04-20 21:20:52
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#include <stdio.h>
#include <uv.h>

typedef struct my_time{
    int64_t now;
    uv_loop_t* loop;
} my_time_t;

void my_timer_cb(uv_timer_t* handle)
{
    my_time_t* update_time;

    update_time = (my_time_t*)handle->data;

    printf("timer callback running, time = %lld ...\n", update_time->now);

    update_time->now = uv_now(update_time->loop);
}

void signal_cb(int sig)
{
    printf("received sig = %d, stopping event loop\n", sig);
    uv_stop(uv_default_loop());
}

int main()
{
    signal(SIGINT, signal_cb);

    my_time_t time;
    time.now = uv_now(uv_default_loop());
    time.loop = uv_default_loop();

    uv_timer_t timer;
    timer.data = (void*)&time;

    uv_timer_init(uv_default_loop(), &timer);

    uv_timer_start(&timer, my_timer_cb, 0, 1000);

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
