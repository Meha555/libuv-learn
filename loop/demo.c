#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

int main() 
{
    uv_loop_t *loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_run(loop, UV_RUN_DEFAULT);

    printf("quit...\n");

    uv_loop_close(loop); // 如果loop是uv_default_loop()，也需要uv_loop_close，但是不要free
    free(loop);
    return 0;
}
