#include <stdio.h>
#include <uv.h>



int main() {
    uv_loop_t *loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_run(loop, UV_RUN_DEFAULT);

    printf("after uv_run\n");
    uv_loop_close(loop);
    free(loop);
    return 0;
}