/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2020-04-22 20:22:38
 * @LastEditTime: 2020-04-25 01:29:52
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <uv.h>

uv_loop_t *loop;

// uv_spawn 一次只能执行一个程序，需要分开执行不同的命令。
uv_process_t mkdir_req;
uv_process_t rm_req;
uv_process_options_t mkdir_options;
uv_process_options_t rm_options;

// mkdir 进程退出回调
void mkdir_on_exit(uv_process_t *req, int64_t exit_status, int term_signal) {
    printf("mkdir process exited with status %ld, signal %d\n", exit_status, term_signal);
    uv_close((uv_handle_t*) req, NULL);

    if (exit_status == 0) {
        // mkdir 成功，执行 rm 命令
        int r;
        if ((r = uv_spawn(loop, &rm_req, &rm_options))) {
            printf("%s\n", uv_strerror(r));
        } else {
            printf("Launched rm process with ID %d\n", rm_req.pid);
        }
    }
}

// rm 进程退出回调
void rm_on_exit(uv_process_t *req, int64_t exit_status, int term_signal) {
    printf("rm process exited with status %ld, signal %d\n", exit_status, term_signal);
    uv_close((uv_handle_t*) req, NULL);
    uv_loop_close(loop); // 关闭事件循环
}

int main() {
    int r;

    // mkdir 命令参数
    char* mkdir_args[] = {
        "mkdir",
        "-p",
        "dircreated",
        NULL
    };

    // rm 命令参数
    char* rm_args[] = {
        "rm",
        "-r",
        "dircreated",
        NULL
    };

    loop = uv_default_loop();

    // 配置 mkdir 选项
    mkdir_options.exit_cb = mkdir_on_exit;
    mkdir_options.file = "mkdir";
    mkdir_options.args = mkdir_args;

    // 配置 rm 选项
    rm_options.exit_cb = rm_on_exit;
    rm_options.file = "rm";
    rm_options.args = rm_args;

    // 启动 mkdir 进程
    if ((r = uv_spawn(loop, &mkdir_req, &mkdir_options))) {
        printf("%s\n", uv_strerror(r));
        return 1;
    } else {
        printf("Launched mkdir process with ID %d\n", mkdir_req.pid);
    }

    return uv_run(loop, UV_RUN_DEFAULT);
}