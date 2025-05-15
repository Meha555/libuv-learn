#include <uv.h>

#define WORKERS 3

typedef struct {
    int size;
    int pos;
    int res;
} slice_t;

int data[WORKERS * 2] = {1, 2, 3, 4, 5, 6};
slice_t slice[WORKERS];

uv_thread_t workers[WORKERS];
uv_thread_t collector;

uv_barrier_t barrier;

void do_work(void* arg)
{
    slice_t* slice = (slice_t*)arg;
    int tmp = 0;
    for (int i = 0; i < slice->size; i++) {
        tmp += data[slice->pos + i];
    }
    slice->res = tmp;

    // 工作完成后等待屏障
    uv_barrier_wait(&barrier);
}

void do_collect(void* arg)
{
    int* sum = (int*)arg;
    // 等待所有worker线程完成工作（确保WORKERS+1个线程都到达了屏障点）
    uv_barrier_wait(&barrier);

    for (int i = 0; i < WORKERS; i++) {
        *sum += slice[i].res;
    }
}

int main() {
    uv_barrier_init(&barrier, WORKERS + 1); // 包括collector线程

    int sum = 0;

    uv_thread_create(&collector, do_collect, &sum);
    for (int i = 0; i < WORKERS; i++) {
        slice[i].size = WORKERS * 2 / WORKERS;
        slice[i].pos = i * slice[i].size;
        uv_thread_create(&workers[i], do_work, &slice[i]);
    }

    for (int i = 0; i < WORKERS; i++) {
        uv_thread_join(&workers[i]);
    }

    uv_thread_join(&collector);

    printf("Sum: %d\n", sum);

    return 0;
}