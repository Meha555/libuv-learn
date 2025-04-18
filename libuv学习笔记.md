# libuv学习笔记

>  [libuv documentation](https://docs.libuv.org/en/v1.x/) 
>
>  [设计摘要 — libuv documentation](https://libuv-docs-chinese.readthedocs.io/zh/latest/design.html) 
>
>  [libuv高效编程__杰杰_的博客-CSDN博客](https://blog.csdn.net/jiejiemcu/category_9918060.html) 

## libuv概述

>  [c 的网络I/O库总结（libevent,libuv,libev,libeio）-CSDN博客](https://blog.csdn.net/weixin_38597669/article/details/124918885) 

libuv实际上是一个抽象层：

- 对IO轮询机制：封装了底层的epoll、iocp等接口为 `uv__io_t`，对上层提供统一的接口
- 对举殡共和流等提供了高级别的抽象
- 对线程提供了跨平台的抽象

![image](https://img2024.cnblogs.com/blog/3077699/202504/3077699-20250416141557692-1708498635.png)

## 架构

libuv的实现是一个很经典生产者-消费者模型。libuv在整个生命周期中，每一次循环都执行每个阶段（phase）维护的任务队列。逐个执行节点里的回调，在回调中，不断生产新的任务，从而不断驱动libuv。

## 句柄和请求

**句柄和请求在libuv里面既起到了抽象的作用，也起到了为事件循环统一事件源的作用**。

Handle：一个句柄表示一个可用的资源（fd、socket、timer、进程），比如一个TCP连接。句柄的生命周期应当与所表示的资源相同。

Request：一个请求表示一个操作的开始，并在得到应答时完成请求。因此请求的生命周期是短暂的，通常只维持在一个回调函数的时间。请求通常对应着在句柄上的IO操作（如在TCP连接中发送数据）或不在句柄上操作（获取本机地址）。

> 可以理解handle为xcb_xxx_t，而request为xcb_cookie_t

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

从事件循环线程的处理过程可知，它在io循环时会进入阻塞状态，而阻塞的具体时间则通过计算得到，那么在某些情况下，我们想要唤醒事件循环线程，就可以通过ansyc去操作，比如当线程池的线程处理完事件后，执行的结果是需要交给事件循环线程的，这时就需要用到唤醒事件循环线程，当然方法也是很简单，调用一下 `uv_async_send()` 函数通知事件循环线程即可。libuv线程池中的线程就是利用这个机制和主事件循环线程通讯。

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

`uv_async_t` 只有2个暴露的API：

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

  if (loop->async_io_watcher.fd != -1)
    return 0;
  // 创建一个用于事件通知的fd
  err = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (err < 0)
    return UV__ERR(errno);

  pipefd[0] = err;
  pipefd[1] = -1;
  // 用创建的用于事件通知的fd和回调函数uv__async_io初始化并启动async_io_watcher
  // 从而在线程池中的线程调用uv_async_send向loop线程发送消息时，loop在poll io时执行uv__async_io来遍历async_handles队列，执行其中所有pending==1的uv_async_t句柄的回调函数
  uv__io_init(&loop->async_io_watcher, uv__async_io, pipefd[0]);
  uv__io_start(loop, &loop->async_io_watcher, POLLIN); // 监听读就绪
  loop->async_wfd = pipefd[1];

  return 0;
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
```

### 线程句柄和进程句柄

libuv为线程进行的跨平台封装。线程运行的函数签名是 `void (*entry)(void *arg)` 。

## 事件循环

libuv的事件循环用于对IO进行轮询，以便在IO就绪时执行相关的操作和回调。

**注意**：

- **是"one loop per thread"的，因此必须关联到单个线程上，且使用非阻塞套接字。**

- libuv中文件操作是可以异步执行的（启动子线程），而网络IO总是同步的（单线程执行）
- 对于文件I/O的操作，由于平台并未对文件I/O提供轮询机制，libuv通过线程池的方式阻塞他们，每个I/O将对应产生一个线程，并在线程中进行阻塞，当有数据可操作的时候解除阻塞，进而进行回调处理，因此libuv将维护一个线程池，线程池中可能创建了多个线程并阻塞它们。（文件操作不会是高并发的，所以这么做没问题）

一次事件循环可以分为多个阶段：

![image](https://img2024.cnblogs.com/blog/3077699/202504/3077699-20250416142115350-140600487.png)

这张图很明确的表示了libuv中所有I/O的事件循环处理的过程，其实就是 `uv_run()` 函数执行的过程，它内部是一个while循环：

1. 首先判断循环是否是处于活动状态，它主要是通过当前是否存在处于alive活动状态的句柄，如果句柄都没有了，那循环也就没有意义了，如果不存在则直接退出。

2. 开始倒计时，主要是维护所有句柄中的定时器。当某个句柄超时了，就会告知应用层已经超时了，就退出去或者重新加入循环中。

3. 调用待处理的回调函数，如果有待处理的回调函数，则去处理它，如果没有就接着往下运行。

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
   - 单次阻塞模式 `UV_RUN_ONCE`：轮询一次I/O，如果没有待处理的回调，则进入阻塞状态，完成处理后返回零，不继续运行事件循环。
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



```c
int uv_run(uv_loop_t *loop, uv_run_mode mode) {
  DWORD timeout;
  int r;
  int can_sleep;

  r = uv__loop_alive(loop);
  if (!r)
    uv_update_time(loop);

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

    timeout = 0;
    if ((mode == UV_RUN_ONCE && can_sleep) || mode == UV_RUN_DEFAULT)
      timeout = uv_backend_timeout(loop);

    uv__metrics_inc_loop_count(loop);

    uv__poll(loop, timeout);

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

    uv_update_time(loop);
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

