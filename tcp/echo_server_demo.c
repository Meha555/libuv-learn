#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define DEFAULT_PORT 6666
#define DEFAULT_BACKLOG 128

uv_loop_t *server_loop;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*) malloc(suggested_size); // 这个suggested_size挺好，不用自己预估一个缓冲区大小了
    buf->len = suggested_size;
}

void echo_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free_write_req(req);
}

void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(buf->base, nread);
        uv_write((uv_write_t*) req, client, &req->buf, 1, echo_write); // write_req_t*类型可以转换为uv_write_t*类型，是因为uv_write_t作为了write_req_t的第一个成员
        return;
    }
    // The uv_read_cb callback will be made several times until there is no more data to read or uv_read_stop() is called.
    // nread might be 0, which does not indicate an error or EOF. This is equivalent to EAGAIN or EWOULDBLOCK under read(2).
    // if (nread == 0) {
    //     printf("Read again...\n");
    //     uv_read_start((uv_stream_t*)client, alloc_buffer, echo_read);
    //     return;
    // }
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, NULL);
    }

    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));

    // server and client must be handles running on the same loop.
    uv_tcp_init(server_loop, client);
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        struct sockaddr_in addr;
        int len = sizeof(addr);
        uv_tcp_getpeername(client, (struct sockaddr*) &addr, &len);
        printf("client (%s:%d) connected success ...\n", inet_ntoa(addr.sin_addr), addr.sin_port);
        uv_read_start((uv_stream_t*) client, alloc_buffer, echo_read);
    }
    else {
        uv_close((uv_handle_t*) client, NULL);
    }
}

int main() {
    struct sockaddr_in addr;

    server_loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(server_loop, &server);

    uv_ip4_addr("0.0.0.0", DEFAULT_PORT, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*) &server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(server_loop, UV_RUN_DEFAULT);
}