#include <stdio.h>
#include <uv.h>

uv_mutex_t mu;
int res;

// 任务函数会在线程池中执行
void work_callback(uv_work_t* req) {
    int id = *(int*)req->data;
    // 模拟耗时操作，例如计算或 I/O 操作
    printf("Thread %d@%p running in thread pool...\n", id, uv_thread_self());
    uv_sleep(1000 + rand() % 500);
    uv_mutex_lock(&mu);
    res++;
    uv_mutex_unlock(&mu);
}

// 完成通知回调函数会在主线程（事件循环线程）中执行
void after_work(uv_work_t* req, int status) {
    int id = *(int*)req->data;
    if (status == UV_ECANCELED) {
        fprintf(stderr, "Task canceled.\n");
    } else {
        printf("Work complete, result: %d\n", res);
    }
    free(req);
}

int main() {
    uv_loop_t* loop = uv_default_loop();
    uv_mutex_init(&mu);


    for (int i = 0; i < 10; i++) {
        uv_work_t* work_req = (uv_work_t*)calloc(1, sizeof(uv_work_t));
        work_req->data = malloc(sizeof(int));
        *(int*)work_req->data = i;
        // 初始化工作请求并提交到线程池
        uv_queue_work(loop, work_req, work_callback, after_work);
    }

    // 运行事件循环以等待线程完成
    uv_run(loop, UV_RUN_DEFAULT);

    uv_mutex_destroy(&mu);
    return 0;
}