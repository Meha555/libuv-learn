#include <stdio.h>
#include <uv.h>

void hare_entry(void *arg) 
{
    int track_len = *((int *) arg);
    while (track_len) {
        track_len--;
        uv_sleep(1000);
        printf("hare ran another step\n");
    }
    printf("hare done running!\n");
}

void tortoise_entry(void *arg) 
{
    int track_len = *((int *) arg);
    while (track_len) 
    {
        track_len--;
        printf("tortoise ran another step\n");
        uv_sleep(3000);
    }
    printf("tortoise done running!\n");
}

int main() {
    int track_len = 10;
    uv_thread_t hare;
    uv_thread_t tortoise;
    uv_thread_create(&hare, hare_entry, &track_len);
    uv_thread_create(&tortoise, tortoise_entry, &track_len);

    uv_thread_join(&hare);
    uv_thread_join(&tortoise);
    return 0;
}
