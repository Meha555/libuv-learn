#include <uv.h>

uv_key_t key;
uv_barrier_t barrier;
int num = 0;

void worker(void *arg) {
    // 子线程启动后，会产生一份TLS变量的副本
    num++;
    uv_key_set(&key, &num);
    int num_local_cpy = *((int*)uv_key_get(&key));
    uv_barrier_wait(&barrier);
    printf("num is %d, num_local_cpy is %d\n", num, num_local_cpy);
}

int main(int argc, char **argv) {
    uv_key_create(&key);
    uv_barrier_init(&barrier, 3);
    uv_thread_t nthread1, nthread2, nthread3;
    uv_thread_create(&nthread1, worker, NULL);
    uv_thread_create(&nthread2, worker, NULL);
    uv_thread_create(&nthread3, worker, NULL);
    uv_thread_join(&nthread1);
    uv_thread_join(&nthread2);
    uv_thread_join(&nthread3);
    uv_key_delete(&key);
}