#include <uv.h>

#define BUF_SIZE 10

typedef struct {
  char *base;
  int cur;
} buf_t;
buf_t buf;
uv_sem_t empty;
uv_sem_t full;
uv_mutex_t mutex;

void do_produce(void *arg) {
  while(1) {
    uv_sem_wait(&empty);
    uv_mutex_lock(&mutex);
    char c = rand() % 26 + 'A';
    printf("buf.cur = %d, produce %c\n", buf.cur, c);
    buf.base[buf.cur++] = c;
    uv_mutex_unlock(&mutex);
    uv_sem_post(&full);
    uv_sleep(1000);
  }
}

void do_consume(void *arg) {
  while(1) {
    uv_sem_wait(&full);
    uv_mutex_lock(&mutex);
    char c = buf.base[--buf.cur];
    printf("buf.cur = %d, consume %c\n", buf.cur, c);
    uv_mutex_unlock(&mutex);
    uv_sem_post(&empty);
    uv_sleep(1000);
  }
}

int main(int argc, char **argv) {
  buf.base = calloc(1, BUF_SIZE);
  uv_mutex_init(&mutex);
  uv_sem_init(&empty, BUF_SIZE);
  uv_sem_init(&full, 0);
  uv_thread_t producer, consumer;
  uv_thread_create(&producer, do_produce, NULL);
  uv_thread_create(&consumer, do_consume, NULL);
  uv_thread_join(&producer);
  uv_thread_join(&producer);
}