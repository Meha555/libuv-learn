#include <uv.h>
#include <assert.h>

// NOTE uv_loop_t在没有待处理的fd时会自动退出。或者在其他回调中手动调用uv_stop来设置停止标志，从而不管是否还有活跃的fd
// 因此，uv_stop不是优雅退出，即uv_stop不是必须调用的。

void on_uv_close(uv_handle_t* handle) {
}

void on_uv_walk(uv_handle_t* handle, void* arg) {
  if (!uv_is_closing(handle)) { //FALSE, handle is closing
    uv_close(handle, on_uv_close);
  }
}

int main() {
  uv_timer_t timer;
  int r;
  r = uv_timer_init(uv_default_loop(), &timer); // insert a active handle into loop
  assert(r == 0);
  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  assert(r == 0);
  // It returns busy cause the timer handle is not closed.
  r = uv_loop_close(uv_default_loop());
  assert(r == UV_EBUSY);
  if (r != 0) {
    // Close pending handles
    uv_walk(uv_default_loop(), on_uv_walk, NULL);
    // run the loop until there are no pending callbacks
    do {
        r = uv_run(uv_default_loop(), UV_RUN_ONCE);
    } while(r != 0);
    // Now we're safe.
    r = uv_loop_close(uv_default_loop());
  }
  assert(r == 0);
}