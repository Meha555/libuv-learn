#include <uv.h>

uv_barrier_t barrier;
void worker1(void *arg) {
  printf("worker1,thread id is:%p\n", uv_thread_self());
  uv_sleep(1000);
  uv_barrier_wait(&barrier);
}

void worker2(void *arg) {
  printf("worker2,thread id is:%p\n", uv_thread_self());
  uv_sleep(2000);
  uv_barrier_wait(&barrier);
}

void callback(void *arg){
  printf("waiting for worker\n");
  uv_barrier_wait(&barrier);
  printf("all thread done\n");
}

int main(int argc, char **argv) {
  uv_barrier_init(&barrier,3);
  uv_thread_t nthread1,nthread2,nthread3;
  uv_thread_create(&nthread1, worker1, NULL);
  uv_thread_create(&nthread2, worker2, NULL);
  uv_thread_create(&nthread3, callback, NULL);
  uv_thread_join(&nthread1);
  uv_thread_join(&nthread2);
  uv_thread_join(&nthread3);
}