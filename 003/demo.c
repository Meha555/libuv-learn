#include <stdio.h>
#include <uv.h>

int64_t num = 0;

// NOTE 传递给回调函数的handle指针必须保持有效，本例中handle虽然是栈上分配的，但是其生命周期足够长，所以不会出现问题
void my_idle_cb(uv_idle_t* handle)
{
    num++;
    printf("idle callback\n");
    if (num >= 5) {
        printf("idle stop, num = %ld\n", num);
        uv_stop(handle->data);
    }
}

int main()
{
    uv_idle_t idler;
    // 获取事件循环结构体
    uv_loop_t* loop = uv_loop_new();
    idler.data = loop;
    // 初始化一个idler
    uv_idle_init(loop, &idler);
    // 王事件循环的idle节点添加一个任务
    uv_idle_start(&idler, my_idle_cb);
    // 启动事件循环
    uv_run(loop, UV_RUN_DEFAULT);
    // 销毁事件循环
    uv_loop_close(loop);
    // 释放内存
    free(loop);
    return 0;
}