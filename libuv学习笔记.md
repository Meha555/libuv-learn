# libuv学习笔记

>  [libuv documentation](https://docs.libuv.org/en/v1.x/) 
>
>  [设计摘要 — libuv documentation](https://libuv-docs-chinese.readthedocs.io/zh/latest/design.html) 
>
>  [libuv高效编程__杰杰_的博客-CSDN博客](https://blog.csdn.net/jiejiemcu/category_9918060.html)
>
>   [libuv漫谈之线程 - 知乎](https://zhuanlan.zhihu.com/p/25973650) 

## libuv概述

>  [c 的网络I/O库总结（libevent,libuv,libev,libeio）-CSDN博客](https://blog.csdn.net/weixin_38597669/article/details/124918885) 

libuv实际上是一个抽象层：

- 对IO轮询机制：封装了底层的epoll、iocp等接口为 `uv__io_t`，对上层提供统一的接口
- 对举殡共和流等提供了高级别的抽象
- 对线程提供了跨平台的抽象

![image](https://img2024.cnblogs.com/blog/3077699/202504/3077699-20250416141557692-1708498635.png)

## 架构

libuv的实现是一个很经典生产者-消费者模型。libuv在整个生命周期中，每一次循环都执行每个阶段（phase）维护的任务队列。逐个执行节点里的回调，在回调中，不断生产新的任务，从而不断驱动libuv。

注意：**libuv 里多数传入回调函数的 API 是异步的，但不能绝对地认为所有这类 API 都是异步的，需要根据具体的 API 文档和实现来判断**。

## 句柄和请求

**句柄和请求在libuv里面既起到了抽象的作用，也起到了为事件循环统一事件源的作用**。

- **Handle**：一个句柄表示一个可用的资源（fd、socket、timer、进程），比如一个TCP连接。句柄的生命周期应当与所表示的资源相同。

- **Request**：一个请求表示一个操作的开始，并在得到应答时完成请求。因此请求的生命周期是短暂的，**通常只维持在一个回调函数的时间（所以应该在回调函数里面释放申请的内存）**。请求通常对应着在句柄上的IO操作（如在TCP连接中发送数据）或不在句柄上操作（获取本机地址）。

> 可以理解handle为xcb_xxx_t，而request为xcb_cookie_t

约定：以下本文将统称句柄和请求为句柄。两者不再区分。

```c
/* Handle types. */
typedef struct uv_loop_s uv_loop_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_dir_s uv_dir_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_udp_s uv_udp_t;
typedef struct uv_pipe_s uv_pipe_t;
typedef struct uv_tty_s uv_tty_t;
typedef struct uv_poll_s uv_poll_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_async_s uv_async_t;
typedef struct uv_process_s uv_process_t;
typedef struct uv_fs_event_s uv_fs_event_t;
typedef struct uv_fs_poll_s uv_fs_poll_t;
typedef struct uv_signal_s uv_signal_t;

/* Request types. */
typedef struct uv_req_s uv_req_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;
typedef struct uv_getnameinfo_s uv_getnameinfo_t;
typedef struct uv_shutdown_s uv_shutdown_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;
typedef struct uv_work_s uv_work_t;
typedef struct uv_random_s uv_random_t;
```

### 基类

`uv_handle_t` 是libuv所有句柄类型的抽象基类，其经过字节对齐使得所有其他的句柄类型都可以与这个类型强转（类似于 `xcb_abstract_event_t` ）。提供了所有句柄都共有的API。

类似的 `uv_req_t` 是libuv所有请求类型的抽象基类。

>  Libuv handles are not movable. Pointers to handle structures passed to functions must remain valid for the duration of the requested operation. Take care when using stack allocated handles. 

> 关于结构体类型强制转换：
>
> 在 C 语言中，结构体的内存布局是从第一个成员开始依次排列的。因此下面的强转是成立的：
>
> ```c
> typedef struct {
>     uv_write_t req;  // 第一个成员
>     uv_buf_t buf;    // 第二个成员
> } write_req_t;
> 
> write_req_t wr;
> uv_write_t *wq = (uv_write_t*)(&wr);
> ```
>
> 因为 `uv_write_t` 是 `write_req_t` 的第一个成员，因此首地址相同，因此按 `uv_write_t*` 位解释时能正确分割出第一个成员。
>
>  所以**在C语言中认为结构体第一个成员的类型是整个结构体类型的基类**

#### 句柄相关的基本操作

> https://docs.libuv.org/en/v1.x/handle.html

最常用的 `uv_close()` ：关闭句柄。必须在句柄的内存被释放前调用，然后句柄的内存应该在 `close_cb` 回调中释放（所以在没有资源需要释放的情况下 `close_cb` 可以填 `NULL`）。

注意：
- 官网文档有一句话：*对于"In-progress requests"如 `uv_connect_t`、`uv_write_t` 等，**请求会被取消并调用各自对应的回调，并携带错误 `UV_ECANCELED`**（注意这种情况不要重复释放内存）。*但事实上查阅 `uv_close()` 的实现，发现只有 `uv_tcp_t/uv_named_pipe_t/uv_tty_t/uv_udp_t/uv_poll_t/uv_timer_t/uv_prepare_t/uv_check_t/uv_idle_t/uv_async_t/uv_process_t/uv_fs_event_t/uv_fs_poll_t` 句柄才能使用这个函数，否则 abort 。
`
- 取消句柄的 `uv_cancel()` 函数只适用于 `uv_fs_t/uv_geneaddrinfo_t/uv_getnameinfo_t/uv_work_t/uv_random_t` 请求，否则返回 `UV_EINVAL` 。

#### 句柄的激活

句柄的*active*状态：

- 对于 `uv_async_t` 句柄，其初始化时就自动激活了，直到 `uv_close()` 被调用才取消激活。

- 对于 `uv_pipe_t\uv_tcp_t` 等于I/O相关的句柄，是读就绪/写就绪（读写数据、连接和接受连接）时激活。

- `uv_check_t\uv_idle_t\uv_timer_t` 等具有 `uv_xxx_start()` 函数的句柄，是在调用这个函数时才激活；调用 `uv_xxx_stop()` 时会取消激活。

#### 句柄引用计数

句柄有 `uv_ref()` 和 `uv_unref()` 的操作，并且这两个操作都是幂等的，重复引用和取消引用都只产生一次影响（**因此句柄的引用计数只会是0或1，这种不采用counter的设计就是为了保证幂等性**）。

句柄会在被*active*时引用，对应的事件触发或者stop时解引用。

默认情况下事件循环会一直运行，直到已经没有*active*和*referenced*的句柄才停止。开发者可以通过对句柄进行*unreferencing*来强制让事件循环提前终止，比如先调用 `uv_timer_start()` 后又调用 `uv_unref()` 。

### 流程句柄

*流程句柄是我起的名字，表示这类句柄是固定在事件循环的流程中的，且它们都不是在轮询(polling)那步处理的。*

作用是在事件循环中按顺序执行回调。有3种：`uv_idle_t`、`uv_prepare_t`、`uv_check_t`。

libuv中有一个有意思的实现，所有idle、prepare以及check句柄相关的函数都是通过宏生成的：见 `loop-watcher.c`

所有流程句柄都有3个操作：

- `uv_xxx_init()` ：初始化
- `uv_xxx_start()` ：开始监听（进入活跃状态，事件循环将会调用其绑定的回调）
- `uv_xxx_stop()` ：停止监听（进入停止状态，事件循环不会再调用其绑定的回调）

三种watcher句柄的区别：

- `uv_idle_t` 会在每次循环时运行一次绑定的回调，在 `uv_prepare_t` 之前；只要有一个 idle 句柄处于活跃状态，那么事件循环就不会进入阻塞状态（即零超时轮询）。所以 idle 句柄一般用于执行一些心跳、低优先级的任务。
- `uv_prepare_t` 会在每次循环时运行一次绑定的回调，在轮询 I/O 之前；相当于前处理钩子。
- `uv_check_t` 会在每次循环时运行一次绑定的回调，在轮询 I/O 之后；相当于后处理钩子。

### timer句柄

定时器句柄用于让回调函数在指定的时刻被事件循环调用。有单发和多发两种。

- `uv_timer_init()` ：初始化
- `uv_timer_start()` ：启动定时器。如果超时时间为0，则将在下个事件循环执行时执行绑定的回调（和Qt一样）。
- `uv_timer_again()` ：停止定时器，如果定时器正在重复运行，则使用 repeat 作为超时时间重新启动定时器（相当于未指定超时时间采用的默认值）。如果计时器从未启动过，则返回 `UV_EINVAL`。
- `uv_timer_set_repeat()` ：修改定时器的 repeat。

**注意**：

- 由于是事件循环负责维护时间戳和调用回调，因此 `uv_timer_t` 无法脱离 `uv_loop_t` 使用。
- `uv_loop_t` 在执行定时器回调之前和等待 I/O 唤醒之后，会立即更新其缓存的 `uv_loop_t::time` 当前时间戳。

### signal句柄

libuv对系统信号进行的封装。如果创建了signal句柄并且start了，那么当对应的信号到达时，libuv会调用对应的回调函数。

关于libuv的signal handle有几个点要知悉：

- 以编程方式调用 `raise()` 或 `abort()` 触发的信号不会被libuv检测到，所以这些信号不会对应的回调函数。
- `SIGKILL` 和 `SIGSTOP` 无法被捕捉。
- 通过libuv处理 `SIGBUS`、`SIGFPE`、`SIGILL` 或 `SIGSEGV` 会导致未定义的行为。
- libuv的信号与平台的信号基本上是一样的，也就是说信号可以从系统中其他进程发出。
- libuv的信号依赖管道，libuv会申请一个管道， 用于其他进程（libuv事件循环进程或fork出来的进程）和libuv事件循环进程通信 。 然后往libuv事件循环的的io观察者队列注册一个观察者，这其实就是观察这个管道是否可读，libuv在轮询I/O的阶段会把观察者加到`epoll`中。io观察者里保存了管道读端的文件描述符 `loop->signal_pipefd[0]` 和回调函数 `uv__signal_event`。 

### async句柄

async句柄用于提供异步唤醒的功能， 比如在用户线程中唤醒主事件循环线程，并且触发对应的回调函数。

从事件循环线程的处理过程可知，它在IO循环时会进入阻塞状态，而阻塞的具体时间则通过计算得到，那么在某些情况下，我们想要唤醒事件循环线程，就可以通过ansyc去操作，比如当线程池的线程处理完事件后，执行的结果是需要交给事件循环线程的，这时就需要唤醒事件循环线程，当然方法也是很简单，调用一下 `uv_async_send()` 函数通知事件循环线程即可。libuv线程池中的线程就是利用这个机制和主事件循环线程通讯。

#### 结构

```c
struct uv_async_s {
  UV_HANDLE_FIELDS
  UV_ASYNC_PRIVATE_FIELDS
};

#define UV_ASYNC_PRIVATE_FIELDS                                               \
  uv_async_cb async_cb;                                                       \
  struct uv__queue queue; // 作为队列节点插入 loop->async_handles                \
  int pending; // 标记此async_cb已经在被执行，其他被唤醒的线程不要执行这个回调（这里会使用无锁编程）           \
```

#### 实现

`uv_async_t` 只有2个暴露的API（没有 `uv_async_start()` 函数）：

（1）初始化并激活

```c
int uv_async_init(uv_loop_t* loop, uv_async_t* handle, uv_async_cb async_cb) {
  int err;
  // 创建一个管道，并且将管道注册到loop->async_io_watcher，并且start
  err = uv__async_start(loop);
  if (err)
    return err;
  // 设置UV_HANDLE_REF标记，并且将async handle 插入loop->handle_queue
  uv__handle_init(loop, (uv_handle_t*)handle, UV_ASYNC);
  handle->async_cb = async_cb;
  handle->pending = 0;
  handle->u.fd = 0; /* This will be used as a busy flag. */
  // 将句柄插入async_handles队列
  uv__queue_insert_tail(&loop->async_handles, &handle->queue);
  // start这个句柄
  uv__handle_start(handle);

  return 0;
}

static int uv__async_start(uv_loop_t* loop) {
  int pipefd[2];
  int err;
  // 避免重复初始化
  if (loop->async_io_watcher.fd != -1)
    return 0;
  // 创建一个用于事件通知的fd
  err = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (err < 0)
    return UV__ERR(errno);

  pipefd[0] = err;
  pipefd[1] = -1;
  // 用创建的用于事件通知的fd和回调函数uv__async_io初始化并启动async_io_watcher
  // 从而在线程池中的线程调用uv_async_send向loop线程发送消息时，loop在polling IO事件时执行uv__async_io来遍历async_handles队列，执行其中所有pending==1的uv_async_t句柄的回调函数
  uv__io_init(&loop->async_io_watcher, uv__async_io, pipefd[0]);
  uv__io_start(loop, &loop->async_io_watcher, POLLIN); // 监听管道读端fd的读就绪
  loop->async_wfd = pipefd[1]; // 保存管道写端的fd

  return 0;
}

static void uv__async_io(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  char buf[1024];
  ssize_t r;
  struct uv__queue queue;
  struct uv__queue* q;
  uv_async_t* h;
  _Atomic int *pending;

  assert(w == &loop->async_io_watcher);

  for (;;) {
    r = read(w->fd, buf, sizeof(buf));

    if (r == sizeof(buf))
      continue;

    if (r != -1)
      break;

    if (errno == EAGAIN || errno == EWOULDBLOCK)
      break;

    if (errno == EINTR)
      continue;

    abort();
  }

  uv__queue_move(&loop->async_handles, &queue);
  while (!uv__queue_empty(&queue)) {
    q = uv__queue_head(&queue);
    h = uv__queue_data(q, uv_async_t, queue);

    uv__queue_remove(q);
    uv__queue_insert_tail(&loop->async_handles, q);

    /* Atomically fetch and clear pending flag */
    pending = (_Atomic int*) &h->pending;
    if (atomic_exchange(pending, 0) == 0)
      continue;

    if (h->async_cb == NULL)
      continue;
	// 执行与uv_async_t绑定的回调函数
    h->async_cb(h);
  }
}
```

（2）发送消息唤醒处于轮询中的事件循环然后调用绑定的async回调。其实就是将消息写入管道中，让io观察者发现管道有数据从而唤醒事件循环线程，并随之处理这个async handle。 

```c
int uv_async_send(uv_async_t* handle) {
  _Atomic int* pending;
  _Atomic int* busy;
  // 以下使用了先标记后检查的策略：
  pending = (_Atomic int*) &handle->pending;
  busy = (_Atomic int*) &handle->u.fd;
  // 无锁的读，看看是不是已经有线程对这个pending字段+1了。如果 pending 的值不为 0，说明已经有其他线程发起了异步事件，当前线程无需重复操作，直接返回
  /* Do a cheap read first. */
  if (atomic_load_explicit(pending, memory_order_relaxed) != 0)
    return 0;
  // 如果无锁读通过，操作过这个pending字段，那么就原子+1
  /* Set the loop to busy. */
  atomic_fetch_add(busy, 1);
  // 使用 atomic_exchange 将 pending 的值设置为 1，并返回旧值。
  //    如果旧值为 0，说明当前线程是第一个发起异步事件的线程。在这种情况下，调用 uv__async_send 向事件循环发送信号，通知其执行回调。
  //    否则就直接进行最后的操作，即撤销这次的+1操作
  /* Wake up the other thread's event loop. */
  if (atomic_exchange(pending, 1) == 0)
    uv__async_send(handle->loop);

  /* Set the loop to not-busy. */
  atomic_fetch_add(busy, -1);

  return 0;
}

static void uv__async_send(uv_loop_t* loop) {
  const void* buf;
  ssize_t len;
  int fd;
  int r;

  buf = "";
  len = 1;
  fd = loop->async_wfd;

  if (fd == -1) {
    static const uint64_t val = 1;
    buf = &val;
    len = sizeof(val);
    fd = loop->async_io_watcher.fd;  /* eventfd */
  }

  do
    r = write(fd, buf, len); // 写入一个空字符串来让fd读就绪，这样io观察者就会polling到
  while (r == -1 && errno == EINTR);

  if (r == len)
    return;

  if (r == -1)
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return;

  abort();
}
```

### stream句柄

#### uv_stream_t

>  [uv_stream_t — Stream handle - libuv documentation](https://docs.libuv.org/en/v1.x/stream.html) 

`uv_stream_t` 句柄抽象了一个双工的通道，有3个实现类：`uv_tcp_t` 、`uv_pipe_t` 和 `uv_tty_t` 。

```c
struct uv_stream_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
};
#define UV_STREAM_FIELDS                                                      \
  /* 等待写的字节数 */                                                        \
  size_t write_queue_size;                                                    \
  /* 分配内存的函数 */                                                        \
  uv_alloc_cb alloc_cb;                                                       \
  /* 读取完成时候执行的回调函数 */                                            \
  uv_read_cb read_cb;                                                         \
  /* private */                                                               \
  UV_STREAM_PRIVATE_FIELDS
#define UV_STREAM_PRIVATE_FIELDS   // linux平台为例                             \
  uv_connect_t *connect_req;       // 建立连接的请求                             \
  uv_shutdown_t *shutdown_req;     // 关闭连接的请求                             \
  uv__io_t io_watcher;             // IO观察者                                 \
  void* write_queue[2];            // 待写入数据的队列                           \
  void* write_completed_queue[2];  // 已写入数据的队列                           \
  uv_connection_cb connection_cb;  // 有新连接时的回调函数                        \
  int delayed_error;               // 延时时的错误代码                           \
  int accepted_fd;                 // 接收连接后产生的对端fd                      \
  void* queued_fds;                // 排队中的fd队列                             \
  UV_STREAM_PRIVATE_PLATFORM_FIELDS     // 目前为空                             \

```

##### 常用函数

（1）`uv_shutdown`：关闭流的写端（读端未关闭），它会等待未完成的写操作，在关闭后调用回调。入参的 req 必须是没有 init 的 `uv_shutdown_t` 

（2）`uv_listen`：监听客户端请求。`uv_stream_t` 必须是 `uv_tcp_t` 或 `uv_pipe_t` 。连接后调用回调

（3）`uv_accept`：配合 `uv_listen` 来接收新来的连接。一般是在连接完成的回调中调用。没有回调

（4）`uv_read_start`：连接成功后，异步读取对端的内容（相当于 `boost::asio::read()` ，是读取一些字节），读到后调用回调

（5）`uv_read_stop` ：停止从对端读取数据，此后不再调用读完成回调。

（6）`uv_write`：按顺序写入数据到对端。还有一个 `uv_write2` 可以向管道写数据。

##### 实现

`uv__stream_io()` 函数是 stream handle 的事件处理函数，它在 `uv__io_init()` 函数中就被注册了，在调用 `uv__stream_io()` 函数时，传递了事件循环对象、io 观察者对象、事件类型等信息。我们来看看stream handle是如何处理可读写事件的：

- 通过 `container_of()` 宏获取 stream handle 的实例，其实是计算出来的。

- 如果 `stream->connect_req` 存在，说明该 stream handle 需要进行连接，于是调用 `uv__stream_connect()` 函数请求建立连接。

- 满足可读取数据的条件，调用 `uv__read()` 函数进行数据读取

- 如果满足流结束条件 调用 `uv__stream_eof()` 进行相关处理。

- 如果满足可写条件，调用 `uv__write()` 函数去写入数据，当然，待写入的数据会被放在 `stream->write_queue` 队列中。

- 在写完数据后，调用 `uv__write_callbacks()` 函数去清除队列的数据，并通知应用层已经写完了。

```c
static void uv__stream_io(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  uv_stream_t* stream;

  /* 获取 stream handle 的实例 */
  stream = container_of(w, uv_stream_t, io_watcher);

  /* 断言，判断是否满足类型 */
  assert(stream->type == UV_TCP ||
         stream->type == UV_NAMED_PIPE ||
         stream->type == UV_TTY);
  assert(!(stream->flags & UV_HANDLE_CLOSING));

  if (stream->connect_req) {
    /* 如果需要建立连接，则请求建立连接 */
    uv__stream_connect(stream);
    return;
  }

  /* 断言 */
  assert(uv__stream_fd(stream) >= 0);

  /* 满足读数据条件，进行数据读取 */
  if (events & (POLLIN | POLLERR | POLLHUP))
    uv__read(stream);

  /* read_cb 可能会关闭 stream，此处判断一下是否需要关闭fd */
  if (uv__stream_fd(stream) == -1)
    return;  /* read_cb closed stream. */

  /* 如果满足流结束条件 调用 uv__stream_eof() 进行相关处理。 */
  if ((events & POLLHUP) &&
      (stream->flags & UV_HANDLE_READING) &&
      (stream->flags & UV_HANDLE_READ_PARTIAL) &&
      !(stream->flags & UV_HANDLE_READ_EOF)) {
    uv_buf_t buf = { NULL, 0 };
    uv__stream_eof(stream, &buf);
  }

  if (uv__stream_fd(stream) == -1)
    return;  /* read_cb closed stream. */

  /* 如果有数据要写入，则调用uv__write()去写数据，写完了调用uv__write_callbacks()函数 */
  if (events & (POLLOUT | POLLERR | POLLHUP)) {
    uv__write(stream);
    uv__write_callbacks(stream);

    /* Write queue drained. */
    if (QUEUE_EMPTY(&stream->write_queue))
      uv__drain(stream);
  }
}
```

#### uv_tcp_t

```

```

#### uv_pipe_t



### 线程句柄和进程句柄

libuv为线程进行的跨平台封装（非常好，这样C语言使用线程就方便了）。线程运行的函数签名是 `void (*entry)(void *arg)` 。

### 线程池

libuv 提供了一个全局线程池（在所有 `uv_loop` 中共享），可用于运行用户代码并在循环线程中收到通知。此线程池在内部用于运行所有文件系统操作，以及 getaddrinfo 和 getnameinfo 请求。

> 事实上一个程序应该只有一个线程池（多了也没啥用），所以libuv的这个线程池暴露出来给开发者用。

使用以下数据结构和函数来操作：

- `uv_work_t` ：任务request类型
- `void (*uv_work_cb)(uv_work_t *req)` ：工作任务函数，在线程池中执行
- `void (*uv_after_work_cb)(uv_work_t *req, int status)` ：任务完成回调函数，在主线程（事件循环所在线程）执行
- `uv_queue_work()` ：向线程池中投递一个任务
- `uv_cancel()` ：取消任务执行，会使得任务完成回调携带 `UV_ECANCELED`

**注意**：

- 线程池是延迟初始化的（第一次使用它时才预分配并创建线程，这和spdlog的相反）
- 线程池中任务的执行需要自行保证线程安全

## 事件循环

libuv的事件循环用于对IO进行轮询，以便在IO就绪时执行相关的操作和回调。

**注意**：

- **是"one loop per thread"的，因此必须关联到单个线程上，且使用非阻塞套接字。【因此 `uv_loop_t` 相关的API都不是线程安全的，不应该跨线程使用 `uv_loop_t`】**

- libuv中文件操作是可以异步执行的（启动子线程），而网络IO总是同步的（单线程执行）
- 对于文件I/O的操作，由于平台并未对文件I/O提供轮询机制，libuv通过线程池的方式阻塞他们，每个I/O将对应产生一个线程，并在线程中进行阻塞，当有数据可操作的时候解除阻塞，进而进行回调处理，因此libuv将维护一个线程池，线程池中可能创建了多个线程并阻塞它们。（文件操作不会是高并发的，所以这么做没问题）

一次事件循环可以分为多个阶段：

![image](https://img2024.cnblogs.com/blog/3077699/202504/3077699-20250416142115350-140600487.png)

这张图很明确的表示了libuv中所有I/O的事件循环处理的过程，其实就是 `uv_run()` 函数执行的过程，它内部是一个while循环：

1. 首先判断循环是否是处于活动状态，它主要是通过当前是否存在处于alive活动状态的句柄，**如果句柄都没有了，那循环也就没有意义了，如果不存在则直接退出**（和`boost::asio::io_context::run()`一样）。

2. 开始倒计时，主要是维护所有句柄中的定时器。当某个句柄超时了，就会告知应用层已经超时了，就退出去或者重新加入循环中。

3. 调用待处理的回调函数，如果有待处理的回调函数，则去处理它，如果没有就接着往下运行【注意这里，**其实是上一轮事件循环处理的IO事件的回调deferred到下一轮事件循环执行（和Boost.Asio一样，回调触发时是IO操作已经完成了**】。

4. 执行空闲句柄的回调，反正它这个线程都默认会有空闲句柄的，这个空闲句柄会在每次循环中被执行。

5. 执行准备句柄的钩子。简单来说就是在某个I/O要阻塞前，有需要的话就去运行一下他的准备钩子，举个例子吧，比如我要从某个文件读取数据，如果我在读取数据进入阻塞状态之前想打印一个信息，那么就可以通过这个准备句柄的钩子去处理这个打印信息。

6. 计算轮询超时，在阻塞I/O之前，循环会计算阻塞的时间（保证能有唤醒时刻），并将这个I/O进入阻塞状态（如果可以的话，阻塞超时为0则表示不阻塞），这些是计算超时的规则：

   1. 如果使用 `UV_RUN_NOWAIT` 模式运行循环，则超时为0。
   2. 如果要停止循环（uv_stop()被调用），则超时为0。
   3. 如果没有活动的句柄或请求，则超时为0。
   4. 如果有任何空闲的句柄处于活动状态，则超时为0。
   5. 如果有任何要关闭的句柄，则超时为0。
   6. 如果以上情况均不匹配，则采用最接近的定时器超时，或者如果没有活动的定时器，则为无穷大。

   > 事件循环将会被阻塞在 I/O 循环上（例如 `epoll_pwait()` 调用），直到该套接字有 I/O 事件就绪时唤醒这个线程，调用关联的回调函数，然后便可以在 handles 上进行读、写或其他想要进行的操作 requests。这也直接避免了时间循环一直工作导致占用 CPU 的问题。

7. 执行检查句柄的钩子，其实当程序能执行到这一步，就表明I/O已经退出阻塞状态了，那么有可能是可读/写数据，也有可能超时了，此时libuv将在这里检查句柄的回调，如果有可读可写的操作就调用他们对应的回调，当超时了就调用超时的处理（就是一个dispatcher）。

8. 如果通过调用 `uv_close()` 函数关闭了句柄，则会调用close将这个I/O关闭。

9. 在超时后更新下一次的循环时间，前提是通过 `UV_RUN_DEFAULT` 模式去运行这个循环。一共有3种运行模式：

   - 默认模式 `UV_RUN_DEFAULT`：运行事件循环，直到不再有活动的和引用的句柄或请求为止。
   - 单次阻塞模式 `UV_RUN_ONCE`：轮询一次I/O，如果没有待处理的回调，则进入阻塞状态。完成处理后（不再有活动的和引用的句柄或请求为止）返回零，不继续运行事件循环。
   - 单次不阻塞模式 `UV_RUN_NOWAIT`：对I/O进行一次轮询，但如果没有待处理的回调，则不会阻塞。

### uv_loop_t

>  [uv_loop_t — Event loop - libuv documentation](https://docs.libuv.org/en/v1.x/loop.html) 

这是一个特殊的句柄，代表一个libuv维护的事件循环，因此是事件循环的入口，所有在事件循环上注册的Handles/Requests都会被注册到内部。

> IO 观察者：在 libuv 内部，对所有 I/O 操作进行了统一的抽象，在底层的操作系统 I/O 操作基础上，结合事件循环机制，实现了 IO 观察者，对应结构体 `uv__io_s`，通过它可以知道 I/O 相关的信息，比如可读、可写等，handle 通过内嵌组合 IO 观察者的方式获得 IO 的监测能力（C语言继承）。
>
> ```c
> struct uv__io_s {
>   uv__io_cb cb;
>   struct uv__queue pending_queue;
>   struct uv__queue watcher_queue;
>   unsigned int pevents; /* Pending event mask i.e. mask at next tick. */
>   unsigned int events;  /* Current event mask. */
>   int fd;
>   UV_IO_PRIVATE_PLATFORM_FIELDS
> };
> ```

```c
struct uv_loop_s {
  /* 用户数据-可以用于任何用途，libuv是不会触碰这个字段的数据的。 */
  void* data;
  /* 统计当前处于活跃状态的句柄的计数器 */
  unsigned int active_handles;
  /* handle队列是一个双向链表，存储所有的句柄 */
  struct uv__queue { // uv__queue是一个linux风格的双链表（作为循环双链表使用，和wl_list一样）
    struct uv__queue* next;
    struct uv__queue* prev;
  } handle_queue;

  union {
    /* 未使用，主要是防止uv_loop_s结构体大小被改变了 */
    void* unused;    
    /* 统计当前在线程池中调用的异步I/O请求个数 */
    unsigned int count; 
  } active_reqs;
  /* 用于后续扩展使用的预留字段 */
  void* internal_fields;
  /* 用于停止事件循环。*/
  unsigned int stop_flag;
  /* 这个宏定义在不同的平台有不一样的处理，具体看下面的定义 */
  UV_LOOP_PRIVATE_FIELDS    
};

#define UV_LOOP_PRIVATE_FIELDS                                                \
  unsigned long flags; // 运行时的一些标记                                                       \
  int backend_fd; // epoll的fd                                                            \
  struct uv__queue pending_queue; // pending阶段的队列                                            \
  struct uv__queue watcher_queue; // uv__io_t 的观察者链表                       \
  uv__io_t** watchers; // watcher_queue队列的节点有一个字段fd，watchers以fd为索引，记录对应的uv__io_t结构体                                                       \
  unsigned int nwatchers;       // watcher数量                                              \
  unsigned int nfds;          // watcher中的fd个数                                                \
  struct uv__queue wq;            // 线程池中的线程处理完任务后把对应的结构体插入到wq队列                                            \
  uv_mutex_t wq_mutex;            // 控制wq队列互斥访问的互斥锁                                            \
  uv_async_t wq_async;                   // 用于线程池和主线程通信的异步句柄                                     \
  uv_rwlock_t cloexec_lock;                                                   \
  uv_handle_t* closing_handles;        // closing阶段的队列，由uv_close()产生                                       \
  struct uv__queue process_handles;         // fork出来的进程队列                                  \
  struct uv__queue prepare_handles;         // loop的prepare句柄队列                                  \
  struct uv__queue check_handles;                              // loop的check句柄队列                                  \
  struct uv__queue idle_handles;                       // loop的idle句柄队列                       \
  struct uv__queue async_handles;                     // loop的async句柄队列                        \
  void (*async_unused)(void);  /* TODO(bnoordhuis) Remove in libuv v2. */     \
  uv__io_t async_io_watcher; // 专门用于监听async句柄的观察者                      \
  int async_wfd; // 用于loop线程和线程池中其他线程进行async写的写端fd                                             \
  struct {                                                                    \
    void* min;                                                                \
    unsigned int nelts;                                                       \
  } timer_heap;  // 二叉堆定时器                                                             \
  uint64_t timer_counter;  // 管理定时器节点的id，自增                                                   \
  uint64_t time;                // 当前时间戳                                              \
  int signal_pipefd[2];        // 用于fork出来的子进程和主进程通信的管道，用于子进程收到信号时通知主进程，然后由主进程执行子进程注册的回调                                               \
  uv__io_t signal_io_watcher;     // 保存读端fd，主进程监听它，在子进程收到信号时会写入这个管道，主进程就能读到信息，从而执行回调                                               \
  uv_signal_t child_watcher;                                                  \
  int emfile_fd;                                      // 备用的文件描述符                        
```

**注意**：libuv提供了一个全局静态的默认事件循环：

```c
static uv_loop_t default_loop_struct;
static uv_loop_t* default_loop_ptr;

uv_loop_t* uv_default_loop(void) {
  if (default_loop_ptr != NULL)
    return default_loop_ptr; // 保证了只会初始化一次这个结构体（由于事件循环是线程独占的，所以不必考虑初始化事件循环成员的线程安全问题）

  if (uv_loop_init(&default_loop_struct))
    return NULL;

  default_loop_ptr = &default_loop_struct;
  return default_loop_ptr;
}
```

## 关键代码分析

> [Meha555/libuv-learn: libuv系列教程的配套代码，从0到深度了解libuv的框架与使用。](https://github.com/Meha555/libuv-learn) 

（1）`uv_loop_init()` ：默认初始化 `uv_loop_t`

```c
int uv_loop_init(uv_loop_t* loop) {
  uv__loop_internal_fields_t* lfields;
  void* saved_data;
  int err;

  /* 清空数据 */
  saved_data = loop->data;
  memset(loop, 0, sizeof(*loop));
  loop->data = saved_data;

  lfields = (uv__loop_internal_fields_t*) uv__calloc(1, sizeof(*lfields));
  if (lfields == NULL)
    return UV_ENOMEM;
  loop->internal_fields = lfields;

  err = uv_mutex_init(&lfields->loop_metrics.lock);
  if (err)
    goto fail_metrics_mutex_init;
  memset(&lfields->loop_metrics.metrics,
         0,
         sizeof(lfields->loop_metrics.metrics));
  /* 初始化定时器堆，初始化工作队列、空闲队列、各种队列 */
  heap_init((struct heap*) &loop->timer_heap);
  uv__queue_init(&loop->wq);
  uv__queue_init(&loop->idle_handles);
  uv__queue_init(&loop->async_handles);
  uv__queue_init(&loop->check_handles);
  uv__queue_init(&loop->prepare_handles);
  uv__queue_init(&loop->handle_queue);  /* 这个队列很重要，对于libuv中其他的 handle 在初始化后都会被放到此队列中 */
  /* 初始化I/O观察者相关的内容，初始化处于活跃状态的观察者句柄计数、请求计数、文件描述符等为0 */
  loop->active_handles = 0;
  loop->active_reqs.count = 0;
  loop->nfds = 0;
  loop->watchers = NULL;
  loop->nwatchers = 0;
  /* 初始化挂起的I/O观察者队列，挂起的I/O观察者会被插入此队列延迟处理 */
  uv__queue_init(&loop->pending_queue);
  /* 初始化 I/O观察者队列，所有初始化后的I/O观察者都会被插入此队列 */
  uv__queue_init(&loop->watcher_queue);

  loop->closing_handles = NULL;
  /* 初始化时间，获取系统当前的时间 */
  uv__update_time(loop);
  loop->async_io_watcher.fd = -1;
  loop->async_wfd = -1;
  loop->signal_pipefd[0] = -1;
  loop->signal_pipefd[1] = -1;
  loop->backend_fd = -1;
  loop->emfile_fd = -1;

  loop->timer_counter = 0;
  loop->stop_flag = 0;
  /* 初始化平台、linux Windows等 */
  err = uv__platform_loop_init(loop);
  if (err)
    goto fail_platform_init;
  /* 初始化信号 */
  uv__signal_global_once_init();
  err = uv__process_init(loop);
  if (err)
    goto fail_signal_init;
  uv__queue_init(&loop->process_handles);
  /* 初始化线程读写锁 */
  err = uv_rwlock_init(&loop->cloexec_lock);
  if (err)
    goto fail_rwlock_init;
  /* 初始化线程互斥锁 */
  err = uv_mutex_init(&loop->wq_mutex);
  if (err)
    goto fail_mutex_init;

  err = uv_async_init(loop, &loop->wq_async, uv__work_done);
  if (err)
    goto fail_async_init;

  uv__handle_unref(&loop->wq_async);
  loop->wq_async.flags |= UV_HANDLE_INTERNAL;

  return 0;

fail_async_init:
  uv_mutex_destroy(&loop->wq_mutex);

fail_mutex_init:
  uv_rwlock_destroy(&loop->cloexec_lock);

fail_rwlock_init:
  uv__signal_loop_cleanup(loop);

fail_signal_init:
  uv__platform_loop_delete(loop);

fail_platform_init:
  uv_mutex_destroy(&lfields->loop_metrics.lock);

fail_metrics_mutex_init:
  uv__free(lfields);
  loop->internal_fields = NULL;

  uv__free(loop->watchers);
  loop->nwatchers = 0;
  return err;
}
```

（2）执行事件循环。这个函数不可重入，所以不要在回调中调用，防止同一线程内重复调用。

```c
int uv_run(uv_loop_t *loop, uv_run_mode mode) {
  DWORD timeout;
  int r;
  int can_sleep;
  /* handle保活处理 */
  r = uv__loop_alive(loop);
  if (!r)
    uv_update_time(loop); // 更新loop->time（Update the event loop’s concept of “now”.）

  /* Maintain backwards compatibility by processing timers before entering the
   * while loop for UV_RUN_DEFAULT. Otherwise timers only need to be executed
   * once, which should be done after polling in order to maintain proper
   * execution order of the conceptual event loop. */
  if (mode == UV_RUN_DEFAULT && r != 0 && loop->stop_flag == 0) {
    uv_update_time(loop);
    uv__run_timers(loop);
  }

  while (r != 0 && loop->stop_flag == 0) {
    can_sleep = loop->pending_reqs_tail == NULL && loop->idle_handles == NULL;

    uv__process_reqs(loop);
    uv__idle_invoke(loop);
    uv__prepare_invoke(loop);
	/* 计算要阻塞的时间 */
    timeout = 0;
    if ((mode == UV_RUN_ONCE && can_sleep) || mode == UV_RUN_DEFAULT)
      timeout = uv_backend_timeout(loop);

    uv__metrics_inc_loop_count(loop);
	/* 开始polling阻塞轮询 */
    uv__poll(loop, timeout);
    /* 程序执行到这里表示被唤醒了，被唤醒的原因可能是I/O可读可写、或者超时了，检查handle是否可以操作 */

    /* Process immediate callbacks (e.g. write_cb) a small fixed number of
     * times to avoid loop starvation.*/
    for (r = 0; r < 8 && loop->pending_reqs_tail != NULL; r++)
      uv__process_reqs(loop);

    /* Run one final update on the provider_idle_time in case uv__poll*
     * returned because the timeout expired, but no events were received. This
     * call will be ignored if the provider_entry_time was either never set (if
     * the timeout == 0) or was already updated b/c an event was received.
     */
    uv__metrics_update_idle_time(loop);

    uv__check_invoke(loop);
    uv__process_endgames(loop);

    uv_update_time(loop); // 更新loop->time（Update the event loop’s concept of “now”.）
    uv__run_timers(loop);

    r = uv__loop_alive(loop);
    if (mode == UV_RUN_ONCE || mode == UV_RUN_NOWAIT)
      break;
  }

  /* The if statement lets the compiler compile it to a conditional store.
   * Avoids dirtying a cache line.
   */
  if (loop->stop_flag != 0)
    loop->stop_flag = 0;

  return r;
}
```

（3）`uv_stop` 要求事件循环在下个循环执行前停止。

```c
void uv_stop(uv_loop_t* loop) {
  loop->stop_flag = 1; // 在uv_run中while条件就会检查这个stop_flag
}
```

（4）`uv_loop_close` 释放 `uv_loop_t` 的内部资源。只能在事件循环已经结束且所有handle和request都已经关闭时调用，否则返回 `UV_EBUSY` 。调用后用户可以释放为 `uv_loop_t` 分配的内存（比如 `user_data`、堆上分配的 `uv_loop_t` 自身）。

```c
int uv_loop_close(uv_loop_t* loop) {
  struct uv__queue* q;
  uv_handle_t* h;
#ifndef NDEBUG
  void* saved_data;
#endif

  if (uv__has_active_reqs(loop))
    return UV_EBUSY;

  uv__queue_foreach(q, &loop->handle_queue) {
    h = uv__queue_data(q, uv_handle_t, handle_queue);
    if (!(h->flags & UV_HANDLE_INTERNAL))
      return UV_EBUSY;
  }

  uv__loop_close(loop);

#ifndef NDEBUG
  saved_data = loop->data;
  memset(loop, -1, sizeof(*loop));
  loop->data = saved_data;
#endif
  if (loop == default_loop_ptr)
    default_loop_ptr = NULL;

  return 0;
}
```

（5）`uv_now` 通过事件循环获取当前的毫秒数。

```c
uint64_t uv_now(const uv_loop_t* loop) {
  return loop->time; // time成员在单次事件循环每次开始时就会更新
}
```

（6）`uv_update_time` 更新 `uv_loop_t::time` 成员。libuv 在事件循环开始时缓存当前时间戳，以减少与时间相关的系统调用次数。一般情况不需要调用这个函数，但是如果有回调执行了很长时间，那么就有必要调用一下来更新时间。