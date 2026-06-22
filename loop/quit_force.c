#include <stdio.h>
#include <uv.h>
#include <assert.h>

int main() {
    uv_loop_t *loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);

    uv_run(loop, UV_RUN_DEFAULT);

    printf("after uv_run\n");
    assert(uv_run(loop, UV_RUN_DEFAULT) == 0);
    // 我们可以确定此时没有活跃句柄，所以无需优雅退出
    free(loop);
    return 0;
}