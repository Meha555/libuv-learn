#include <uv.h>
#include <stdio.h>

// 递归锁通常比普通锁略慢，因为需要维护额外的状态（如持有线程 ID 和递归次数）
uv_mutex_t recursive_mutex;

void recursive_function(int depth) {
    if (depth <= 0) return;

    uv_mutex_lock(&recursive_mutex);
    printf("Locked at depth %d\n", depth);

    recursive_function(depth - 1);

    uv_mutex_unlock(&recursive_mutex);
    printf("Unlocked at depth %d\n", depth);
}

int main() {
    // 初始化递归互斥锁
    if (uv_mutex_init_recursive(&recursive_mutex) != 0) {
        fprintf(stderr, "Failed to initialize recursive mutex\n");
        return 1;
    }

    // 递归调用，同一线程多次获取锁
    recursive_function(3);

    // 销毁互斥锁
    uv_mutex_destroy(&recursive_mutex);
    return 0;
}