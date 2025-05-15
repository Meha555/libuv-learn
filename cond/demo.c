#include <uv.h>

uv_mutex_t mutex;
uv_cond_t cond;
int num = 0;

void worker1(void *arg) {
  while(1){
    uv_mutex_lock(&mutex);
    printf("waiting for num > 2\n");
    while (num <= 2) {
      uv_cond_wait(&cond, &mutex);
    }
    printf("num is %d\n",num);
    num--;
    uv_mutex_unlock(&mutex);
    uv_sleep(1000);
  }
}

void worker2(void *arg) {
  while(1){
    uv_mutex_lock(&mutex);
    num++;
    if(num > 0){
      uv_cond_signal(&cond);
    }
    uv_mutex_unlock(&mutex);
    uv_sleep(1000);
  }
}

int main(int argc, char **argv) {
  uv_mutex_init(&mutex);
  uv_cond_init(&cond);
  uv_thread_t nthread1, nthread2;
  uv_thread_create(&nthread1, worker1, NULL);
  uv_thread_create(&nthread2, worker2, NULL);
  uv_thread_join(&nthread1);
  uv_thread_join(&nthread2);
  return 0;
}