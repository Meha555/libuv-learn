#include <stdio.h>
#include <uv.h>

uv_barrier_t blocker;
uv_rwlock_t numlock;
int shared_num;

void reader(void *n)
{
    int id = *(int *)n;
    int i;
    for (i = 0; i < 20; i++) {
        uv_rwlock_rdlock(&numlock);
        printf("Reader %d: acquired lock\n", id);
        printf("Reader %d: shared num = %d\n", id, shared_num);
        uv_rwlock_rdunlock(&numlock);
        printf("Reader %d: released lock\n", id);
    }
    uv_barrier_wait(&blocker);
}

void writer(void *n)
{
    int id = *(int *)n;
    int i;
    for (i = 0; i < 20; i++) {
        uv_rwlock_wrlock(&numlock);
        printf("Writer %d: acquired lock\n", id);
        shared_num++;
        printf("Writer %d: incremented shared num = %d\n", id, shared_num);
        uv_rwlock_wrunlock(&numlock);
        printf("Writer %d: released lock\n", id);
    }
    uv_barrier_wait(&blocker);
}

int main()
{
    uv_barrier_init(&blocker, 4);

    shared_num = 0;
    uv_rwlock_init(&numlock);

    uv_thread_t threads[3];

    int thread_ids[] = {1, 2, 3};
    uv_thread_create(&threads[0], reader, &thread_ids[0]);
    uv_thread_create(&threads[1], reader, &thread_ids[1]);

    uv_thread_create(&threads[2], writer, &thread_ids[2]);

    uv_barrier_wait(&blocker);
    uv_barrier_destroy(&blocker);

    uv_rwlock_destroy(&numlock);
    return 0;
}
