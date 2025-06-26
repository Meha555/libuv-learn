#include <uv.h>
#include <thread>
#include <cassert>

uv_async_t *async = new uv_async_t;
volatile bool should_stop_timer = false;
volatile bool should_close_async = false;

static int stop_loop(uv_loop_t *loop) {
    // 停止当前的loop，这一步会关闭掉一部分的句柄。但是还可能有处于ACTIVE状态的句柄没有被关闭。
    uv_stop(loop);
    static auto const ensure_close = [](uv_handle_t *handle, void *) {
        if (uv_is_closing(handle)) {
            return;
        } else {
            uv_close(handle, nullptr);
        }
    };
    // 遍历所有句柄, 如果handle处于ACTIVE状态, 调用ensure_close来关闭掉它
    uv_walk(loop, ensure_close, nullptr);
    // 临时运行一个uv_run, 直到loop中不存在ACTIVE状态的句柄和请求为止
    while (true) {
        if (uv_run(loop, UV_RUN_DEFAULT) == 0) {
            break;
        }
    }

    // 最后检查loop中是否还有ACTIVE状态的句柄
    if (uv_loop_alive(loop) != 0) {
        return -1;
    }
    return 0;
}

// 执行创建定时器操作
static void async_cb(uv_async_t *handle) {
    auto loop = handle->loop;
    uv_timer_t *timer = new uv_timer_t;
    uv_timer_init(loop, timer);

    uv_timer_start(
        timer,
        [](uv_timer_t *timer) {
            // do something
            // 业务决定此时要停掉timer
            if (should_stop_timer)
                uv_timer_stop(timer);
            // 关闭async句柄
            uv_close((uv_handle_t *)timer, [](uv_handle_t *handle) { delete (uv_timer_t *)handle; });
        },
        100, 100);
    // 业务决定此时要关闭async句柄
    if (should_close_async) {
        uv_close((uv_handle_t *)handle, [](uv_handle_t *handle) { delete (uv_async_t *)handle; });
    }
}

int main() {
    uv_loop_t *loop = new uv_loop_t;
    uv_loop_init(loop);
    uv_async_init(loop, async, async_cb);
    // 在另一个线程上调用uv_async_send函数
    std::thread t([]() {
        uv_async_send(async); // 通知loop线程调用与async句柄绑定的timer_cb
        uv_sleep(1000); // 模拟业务耗时
        should_stop_timer = true;
        should_close_async = true;
        uv_async_send(async); // 通知loop线程调用与async句柄绑定的timer_cb
    });
    uv_run(loop, UV_RUN_DEFAULT);
    // 清理所有的handle
    assert(stop_loop(loop) == 0);
    uv_loop_close(loop);
    delete loop;
    t.join();
    return 0;
}