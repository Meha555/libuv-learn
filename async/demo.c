/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2020-04-22 20:22:38
 * @LastEditTime: 2020-04-22 21:34:25
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#include <stdio.h>

#include <uv.h>

void my_async_cb(uv_async_t* handle)
{
    printf("my async running!\n");
}

void async_closed_cb(uv_handle_t* handle)
{
    printf("async closed!\n");
    uv_stop(uv_default_loop());
}

void wake_entry(void *arg)
{
    uv_sleep(5000);

    printf("wake_entry running, wake async!\n");

    uv_async_send((uv_async_t*)arg);

    // uv_stop(uv_default_loop());
    uv_close((uv_handle_t*)arg, async_closed_cb);
}


int main() 
{
    uv_thread_t wake;
    uv_async_t async;

    uv_async_init(uv_default_loop(), &async, my_async_cb);

    uv_thread_create(&wake, wake_entry, &async);

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    uv_thread_join(&wake);

    return 0;
}
