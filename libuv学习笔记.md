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

libuv是一个高性能的，事件驱动的I/O库，并且提供了跨平台（如windows, linux）的API。

libuv**强制**使用异步的，事件驱动的编程风格。它的核心工作是提供一个event-loop，还有基于I/O和其它事件通知的回调函数。

libuv实际上是一个抽象层：

- 对IO轮询机制：封装了底层的epoll、iocp等接口为 `uv__io_t`，对上层提供统一的接口
- 对句柄和流等提供了高级别的抽象
- 对线程提供了跨平台的抽象

![image](https://img2024.cnblogs.com/blog/3077699/202504/3077699-20250416141557692-1708498635.png)

## 架构

libuv的实现是一个很经典的生产者-消费者模型。libuv在整个生命周期中，每一次循环都执行每个阶段（phase）维护的任务队列。逐个执行节点里的回调，在回调中，不断生产新的任务，从而不断驱动libuv。

注意：**libuv 里多数传入回调函数的 API 是异步的，但不能绝对地认为所有这类 API 都是异步的，需要根据具体的 API 文档和实现来判断**。

## 事件循环

libuv的事件循环用于对IO进行轮询，以便在IO就绪时执行相关的操作和回调。

**注意**：

- **使用 `uv_loop_init()` 接口初始化loop的线程和调用 `uv_run()` 的线程应保持一致，称为loop线程，并且对uvloop的所有非线程安全操作，均需保证与loop同线程。此外应当使用非阻塞套接字。【"one loop per thread"，因此 `uv_loop_t` 相关的API都不是线程安全的，不应该跨线程使用 `uv_loop_t`】**

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
   2. 如果要停止循环（`uv_stop()` 被调用），则超时为0。
   3. 如果没有活动的句柄或请求，则超时为0。
   4. 如果有任何空闲的句柄处于活动状态，则超时为0。
   5. 如果有任何要关闭的句柄，则超时为0。
   6. 如果以上情况均不匹配，则采用最接近的定时器超时，或者如果没有活动的定时器，则为无穷大。

   > 事件循环将会被阻塞在 I/O 循环上（例如 `epoll_pwait()` 调用），直到该套接字有 I/O 事件就绪时唤醒这个线程，调用关联的回调函数，然后便可以在 handles 上进行读、写或其他想要进行的操作 requests。这也直接避免了时间循环一直工作导致占用 CPU 的问题。

7. 执行检查句柄的钩子，其实当程序能执行到这一步，就表明I/O已经退出阻塞状态了，那么有可能是可读/写数据，也有可能超时了，此时libuv将在这里检查句柄的回调，如果有可读可写的操作就调用他们对应的回调，当超时了就调用超时的处理（就是一个dispatcher）。

8. 如果通过调用 `uv_close()` 函数关闭了句柄，则会调用close将这个I/O关闭。

9. 在超时后更新下一次的循环时间，前提是通过 `UV_RUN_DEFAULT` 模式去运行这个循环。


### uv_loop_t

>  [uv_loop_t — Event loop - libuv documentation](https://docs.libuv.org/en/v1.x/loop.html) 

这是一个特殊的句柄，代表一个libuv维护的事件循环，因此是事件循环的入口，所有在事件循环上注册的Handles/Requests都会被注册到内部。

> IO 观察者：在 libuv 内部，对所有 I/O 操作进行了统一的抽象，在底层的操作系统 I/O 操作基础上，结合事件循环机制，实现了 IO 观察者，对应结构体 `uv__io_s`，通过它可以知道 I/O 相关的信息，比如可读、可写等，handle 通过内嵌组合 IO 观察者的方式获得 IO 的监测能力（C语言继承）。
>
> ```c
> struct uv__io_s {
> uv__io_cb cb;
> struct uv__queue pending_queue;
> struct uv__queue watcher_queue;
> unsigned int pevents; /* Pending event mask i.e. mask at next tick. */
> unsigned int events;  /* Current event mask. */
> int fd;
> UV_IO_PRIVATE_PLATFORM_FIELDS
> };
> ```

- `uv_loop_init()` ：初始化 `uv_loop_t` 结构体

- `uv_loop_close()` ：释放所有事件循环相关的资源（不是事件循环结构体本身），而后可以析构事件循环结构体。必须在该事件循环已经停止以及相关的所有句柄被关闭后调用，否则报错 `UV_EBUSY` 。

- `uv_run()` ：运行事件循环。一共有3种运行模式：

  - 默认模式 `UV_RUN_DEFAULT`：运行事件循环，直到不再有活动的和引用的句柄或请求为止。**如果调用了 `uv_stop()`，且仍有活动句柄或请求，则返回非零。其他情况下返回 0**。
  - 单次阻塞模式 `UV_RUN_ONCE`：轮询一次I/O，如果没有待处理的回调则进入阻塞等待。完成处理后（不再有活动的和引用的句柄或请求为止）返回0，退出事件循环。【一定轮询一次，这个模式是认为loop中当前一定有事件发生】
  - 单次不阻塞模式 `UV_RUN_NOWAIT`：轮询一次I/O，但如果没有待处理的回调则不会阻塞，直接返回。【最多轮询1次】

- `uv_loop_alive()` ：返回非0表示存在活跃的句柄，或者正在关闭句柄

- `uv_stop()` ：强制停止事件循环（不是立即，但可以确保在下一次事件循环之前停止）

- `uv_now()` ：返回当前由事件循环计算出来的时间戳（毫秒）

- `uv_update_time()` ：手动让事件循环更新一下时间戳，从而让 `uv_now()` 的结果更精确。一般情况下不调用，只在你让libuv执行了一个很耗时的回调函数后，可以执行一下。

- `uv_walk(uv_loop_t *loop, uv_walk_cb walk_cb, void *arg)` ：对事件循环中的每个活跃句柄执行回调函数 `walk_cb`，并传递一个用户定义的参数 `arg` 。

- `uv_default_loop()` ：一个全局静态的默认事件循环。由于是默认预分配的事件循环，所以如果你引入的三方库中也用了libuv，则不要随便关闭这个默认的事件循环，因为可能三方库用了这个默认的事件循环。这个默认的事件循环也需要调用 `uv_loop_close()` 。

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
  uv_async_t wq_async;            // 用于线程池中的工作线程和主线程通信的异步句柄                                     \
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

## 句柄和请求

**句柄和请求在libuv里面既起到了抽象的作用，也起到了为事件循环统一事件源的作用**。

- **Handle**：一个句柄表示一个可用的资源（fd、socket、timer、进程），比如一个TCP连接。句柄的生命周期应当与所表示的资源相同。

- **Request**：一个请求表示一个操作的开始，并在得到应答时完成请求。因此请求的生命周期是短暂的，**通常只维持在一个回调函数的时间（所以应该在回调函数里面释放申请的内存）**。请求通常对应着在句柄上的IO操作（如在TCP连接中发送数据）或不在句柄上操作（获取本机地址）。

> 可以理解handle为xcb_xxx_t，而request为xcb_cookie_t

对于libuv中的requests，开发者需要确保在进行异步任务提交时，**通过动态申请的request，要在loop所在线程执行的complete回调函数中释放**。用uv_work_t举例，代码可参考如下：

```cpp
uv_work_t* work = new uv_work_t;
uv_queue_work(loop, work, [](uv_work_t* req) {
    // 异步业务操作
}, [](uv_work_t* req, int status) {
    // 回调中释放内存
    delete req;
});
```

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

最常用的 **`uv_close()` ：关闭句柄，使得句柄变为非激活状态**。**必须在句柄的内存被释放前调用，然后句柄的内存应该在 `close_cb` 回调中释放，或者在那之后（所以在没有资源需要释放的情况下 `close_cb` 可以填 `NULL`）**。

注意：
- **每个handle都必须调用 `uv_close()`**
- 官网文档有一句话：*对于"In-progress requests"如 `uv_connect_t`、`uv_write_t` 等，**请求会被取消并调用各自对应的回调，并携带错误 `UV_ECANCELED`**（注意这种情况不要重复释放内存）。*但事实上查阅 `uv_close()` 的实现，发现只有 `uv_tcp_t/uv_named_pipe_t/uv_tty_t/uv_udp_t/uv_poll_t/uv_timer_t/uv_prepare_t/uv_check_t/uv_idle_t/uv_async_t/uv_process_t/uv_fs_event_t/uv_fs_poll_t` 句柄才能使用这个函数，否则 abort 。
- 取消句柄的 `uv_cancel()` 函数只适用于 `uv_fs_t/uv_geneaddrinfo_t/uv_getnameinfo_t/uv_work_t/uv_random_t` 请求，否则返回 `UV_EINVAL` 。

**关于句柄关闭的坑**：
- https://blog.coderzh.com/2022/04/03/libuv/
- https://aheadsnail.github.io/2020/06/29/shi-yong-libuv-xiao-jie/#toc-heading-5

**句柄需要遵守的原则**：

1. 句柄的初始化工作应在事件循环的线程中进行。
2. 若由于业务问题，句柄需要在其他工作线程初始化，在使用之前用原子变量判断是否初始化完成。
3. 句柄在确定后续不再使用后，调用 `uv_close()` 将句柄从loop中摘除。

> 在这里需要特别说明一下 `uv_close()` ，它被用来关闭一个handle，但是关闭handle的动作是异步的。调用 `uv_close()` 后，首先将要关闭的handle挂载到loop的 `closing_handles` 队列上，然后等待loop所在线程运行 `uv__run_closing_handles()` 函数。最后回调函数close_cb将会在loop的下一次迭代中执行。因此，释放内存等操作应该在close_cb中进行。并且这种异步的关闭操作会带来多线程问题，开发者需要谨慎处理uv_close的时序问题，并且保证在close_cb执行之前就不在使用handles的数据。

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
- `uv_timer_start(uv_timer_t *handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat)` ：启动定时器。如果 `timeout` 为0，则将在下个事件循环执行时执行绑定的回调（和Qt一样）。如果 `repeat` 不为0，则先在 `timeout` 时间后执行一次回调，然后每隔 `repeat` 时间执行一次回调。时间单位是毫秒。
- `uv_timer_stop` ：停止定时器，不再调用超时回调。
- `uv_timer_again()` ：停止定时器，如果定时器正在重复运行，则使用 repeat 作为 timeout 重新启动定时器（相当于未指定 timeout 采用的默认值）。如果计时器从未启动过，则返回 `UV_EINVAL`。
- `uv_timer_set_repeat()` ：修改定时器的 repeat。

**注意**：

- 由于是事件循环负责维护时间戳和调用回调，因此 `uv_timer_t` 无法脱离 `uv_loop_t` 使用。
- `uv_loop_t` 在执行定时器回调之前和等待 I/O 唤醒之后，会立即更新其缓存的 `uv_loop_t::time` 当前时间戳。

- 不要在多个线程中使用libuv的接口（uv_timer_start、uv_timer_stop和uv_timer_again）同时操作同一个loop的timer heap，否则将导致崩溃。如因业务需求需要操作指定线程的定时器，请使用uv_async_send线程安全函数实现。

  ```cpp
  // 错误示例
  int main() {
      uv_loop_t* loop = uv_default_loop(); // 在主线程中获得了loop
      uv_timer_t* timer = new uv_timer_t;
  
      uv_timer_init(loop, timer); // timer所属的loop运行uv_run在主线程，所以timer属于主线程
      std::thread t1([&loop, &timer](){
          uv_timer_start(timer, [](uv_timer_t* timer){ // 但是直接在子线程中操作了timer，此时可能造成uv的事件循环线程和t1线程对timer的访问产生竞争
              uv_timer_stop(timer);
              uv_close((uv_handle_t*)timer, [](uv_handle_t* handle){
                  delete (uv_timer_t*)handle;
              });
          }, 1000, 0);
      });
      uv_run(loop, UV_RUN_DEFAULT);
      t1.join();
  }
  
  // 正确示例
  uv_async_t* async = new uv_async_t;
  
  static void async_cb(uv_async_t* handle) // 此函数始终在主线程执行
  {
      auto loop = handle->loop;
      uv_timer_t* timer = new uv_timer_t;
      uv_timer_init(loop, timer);
      uv_timer_start(timer, [](uv_timer_t* timer){
          uv_timer_stop(timer);
          // 关闭timer句柄
          uv_close((uv_handle_t*)timer, [](uv_handle_t* handle){ delete (uv_timer_t*)handle; });
      }, 1000, 0);
      // 关闭async句柄
      uv_close((uv_handle_t*)handle, [](uv_handle_t* handle){ delete (uv_async_t*)handle; });
  }
  int main() {
  	uv_loop_t* loop = uv_default_loop(); // 在主线程中获得了loop
      uv_async_init(loop, async, async_cb);
      std::thread t([](){
          uv_async_send(async);  // 在任意子线程中调用uv_async_send这个线程安全的接口，通知主线程调用与async绑定的timer_cb
      });
      uv_run(loop, UV_RUN_DEFAULT);
      t.join();
  }
  ```


### signal句柄

libuv对系统信号进行的封装。如果创建了signal句柄并且start了，那么当对应的信号到达时，libuv会调用对应的回调函数。

关于libuv的signal handle有几个点要知悉：

- 以编程方式调用 `raise()` 或 `abort()` 触发的信号不会被libuv检测到，所以这些信号不会对应的回调函数。
- `SIGKILL` 和 `SIGSTOP` 无法被捕捉。
- 通过libuv处理 `SIGBUS`、`SIGFPE`、`SIGILL` 或 `SIGSEGV` 会导致未定义的行为。
- libuv的信号与平台的信号基本上是一样的，也就是说信号可以从系统中其他进程发出。
- libuv的信号依赖管道，libuv会申请一个管道， 用于其他进程（libuv事件循环进程或fork出来的进程）和libuv事件循环进程通信 。 然后往libuv事件循环的的io观察者队列注册一个观察者，这其实就是观察这个管道是否可读，libuv在轮询I/O的阶段会把观察者加到`epoll`中。io观察者里保存了管道读端的文件描述符 `loop->signal_pipefd[0]` 和回调函数 `uv__signal_event`。 

### async句柄

async句柄用于提供异步唤醒的功能， 比如在用户线程中唤醒主事件循环线程，并且触发对应的回调函数【**即用作线程间通信的手段**】。

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

#### 线程间通信

如果因为业务需要，必须在其他线程往loop线程抛任务，请使用uv_async_send函数：即在async句柄初始化时，注册一个回调函数，并在该回调中实现相应的操作，当调用uv_async_send时，在主线程上执行该回调函数。

**主要用途就是唤醒事件循环线程让它做事，因为libuv不是线程安全的，不能在用户线程中发送数据（只允许在事件循环线程发送数据）**

1. `uv_async_t` 从调用 `uv_async_init()` 开始后就一直处于活跃状态，除非用 `uv_close()` 将其关闭。
2. `uv_async_t` 回调的执行顺序是严格按照 `uv_async_init()` 的初始化顺序来的，而非调用 `uv_async_send()` 的顺序。因此按照初始化的顺序来管理好时序问题是必要的。

![image](https://img2024.cnblogs.com/blog/3077699/202506/3077699-20250626160425216-1958805953.png)

#### 实现

`uv_async_t` 只有2个暴露的API（没有 `uv_async_start()` 函数）：

（1）`uv_async_init` 初始化并激活

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

（2）`uv_async_send` 发送消息唤醒处于轮询中的事件循环然后调用绑定的async回调。其实就是将消息写入管道中，让io观察者发现管道有数据从而唤醒事件循环线程，并随之处理这个`uv_async_t`句柄。不能对正在关闭或已经关闭的`uv_async_t`句柄调用

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

Pipe句柄在Unix上提供了对本地域套接字的抽象，在Windows上提供了命名管道。它是`uv_stream_t`的“子类”。管道的用途很多，可以用来读写文件，还可以用来做进程间的通信。

```c
int uv_pipe_init(uv_loop_t *loop, uv_pipe_t *handle, int ipc)
```

这里的第三个参数 `ipc` 是一个布尔值，为1表示这个pipe可以传递文件描述符，为0表示不能传递文件描述符。且只有客户端可以设置为1，服务端应始终设置为0。

**注意**：

- `uv_pipe_init()` 创建管道时，必须保证当前进程在管道文件所在目录具有可读写权限。否则会报错 `UV_EBUSY`
- 在 `uv_pipe_init()` 时不会创建管道，而是在 `uv_pipe_connect` 时才创建管道；
- 如果 `uv_pipe_connect()` 失败（从其回调的第二个参数可以看出），则应当 `uv_close()` 这个管道（自然是在 `uv_pipe_connect()` 的回调中调用），然后可以在 `uv_close()` 的回调中尝试重连（即 `uv_pipe_init()` + `uv_pipe_connect()` ）。此问题是因为 `uv_pipe_connect()` 在第一次调用时，会认为连接已成功，置 `UV_HANLDE_CONNECTION` 标志（Windows）。详见libuv-learn的echo_client_demo.c

### uv_poll_t

Poll句柄用于监视文件描述符的可读性、可写性和断开连接，类似于[`poll(2)`](http://linux.die.net/man/2/poll)的目的。

Poll句柄的目的是支持集成外部库，这些库依赖于事件循环来通知套接字状态的更改，比如`c-ares`或`libssh2`。不建议将 `uv_poll_t` 用于任何其他目的——因为像`uv_tcp_t`、`uv_udp_t`等提供了一个比`uv_poll_t`更快、更可伸缩的实现，尤其是在Windows上用的是iocp。

可能轮询处理偶尔会发出信号，表明文件描述符是可读或可写的，即使它不是。因此，当用户试图从fd读取或写入时，应该总是准备再次处理EAGAIN错误或类似的EAGAIN错误。

同一个套接字不能有多个活跃的Poll句柄，因为这可能会导致libuv出现`busyloop`或其他故障。

当活跃的Poll句柄轮询文件描述符时，用户不应关闭该文件描述符。否则可能导致句柄报告错误，但也可能开始轮询另一个套接字。但是可以在调用`uv_poll_stop()`或`uv_close()`之后立即安全地关闭fd。

下面罗列的是轮询的事件类型：

```c
enum uv_poll_event {
    UV_READABLE = 1,
    UV_WRITABLE = 2,
    UV_DISCONNECT = 4,
    UV_PRIORITIZED = 8
};
```

### 线程句柄和进程句柄

libuv为线程进行的跨平台封装（非常好，这样C语言使用线程就方便了）。线程运行的函数签名是 `void (*entry)(void *arg)` 。

### 线程池和异步任务

libuv 提供了一个全局线程池（在所有 `uv_loop` 中共享），可用于运行用户代码并在循环线程中收到通知。此线程池在内部用于运行所有文件系统操作，以及 getaddrinfo 和 getnameinfo 请求。

> 事实上一个程序应该只有一个线程池（多了也没啥用），所以libuv的这个线程池暴露出来给开发者用。

使用以下数据结构和函数来操作：

- `uv_work_t` ：任务request类型
- `void (*uv_work_cb)(uv_work_t *req)` ：工作任务函数，在线程池中执行
- `void (*uv_after_work_cb)(uv_work_t *req, int status)` ：任务完成回调函数，在主线程（事件循环所在线程）执行
- `uv_queue_work()` ：向线程池中投递一个任务，从而在子线程中执行耗时操作，然后将结果回调到主线程上进行处理
- `uv_cancel()` ：取消任务执行，会使得任务完成回调携带 `UV_ECANCELED`

**注意**：

- 线程池是延迟初始化的（第一次使用它时才预分配并创建线程，这和spdlog的相反）

- 线程池中任务的执行需要自行保证线程安全

- `work_cb` 与 `after_work_cb` 的执行有一个时序问题，只有 `work_cb` 执行完，libuv内部通过 `uv_async_send(loop->wq_async)` 触发fd事件，loop所在线程在下一次迭代中才会执行 `after_work_cb`。只有执行完 `after_work_cb` 时，与之相关的 `uv_work_t` 生命周期才算结束。 

- 对于一些特定场景，比如对内存开销敏感的场景中，同一个request可以重复使用，前提是保证同一类任务之间的顺序，并且要确保最后一次调用`uv_queue_work` 时在其 `after_work_cb` 回调中做好对该request的释放工作：

  ```cpp
  uv_work_t* work = new uv_work_t;
  uv_queue_work(loop, work,
      [](uv_work_t* work) {/*do something*/},
      [](uv_work_t* work, int status) {
          // do something
          uv_queue_work(loop, work,
              [](...) {/*do something*/},
              [](...) {
                  //do something
                  if (last_task) {  // 最后一个任务执行完以后，释放该request
                      delete work;
                  }
          });
      },
  );
  ```

- `uv_queue_work` 函数仅用于抛异步任务，**异步任务的execute回调被提交到线程池后会经过调度执行，因此并不保证多次提交的任务及其回调按照时序关系执行**。 

![image](https://img2024.cnblogs.com/blog/3077699/202506/3077699-20250626160441415-2044143660.png)

### 文件操作

与通过操作系统提供的非阻塞机制实现的socket操作不同，libuv的文件操作是使用自己实现的线程池来实现非阻塞的，调用的还是操作系统的阻塞api。

注意：
- 所有的文件操作函数都有同步和异步的版本，通过传入的 `callback` 参数来区分。如果 `callback` 为 `NULL`，则表示同步操作。此时函数返回值是错误码。而异步版本的函数返回值始终是0。

#### 读写文件



#### 文件系统操作


#### 缓冲和流


#### 监听文件系统变化

##### uv_fs_event_t

FS事件句柄允许用户监视一个给定的路径的更新事件，例如，如果文件被重命名或其中有一个通用更改。

##### uv_fs_poll_t

FS轮询句柄允许用户监视给定的更改路径。与`uv_fs_event_t`不同，fs poll句柄使用`stat`检测文件何时发生了更改，这样它们就可以在不支持fs事件句柄的文件系统上工作。

## 粘包/残包

libuv提供的 `uv_buf_t` 是 libuv 中发送的数据包的单位：

```c
typedef struct uv_buf_t {
  char* base;
  size_t len;
} uv_buf_t;
```

注意它只是用于libuv框架层的，使用 libuv 的 `uv_stream_t` 进行应用层数据收发时仍然可能粘包/残包。

## 坑点

### handle跨线程使用（线程安全问题）

线程安全函数：可以在非loop线程调用，其他的句柄函数都必须在其loop所在的线程调用（如在回调中调用）。

- `uv_async_send()`
- `uv_thread_create()`
- 锁相关的操作，如 `uv_mutex_lock()`、`uv_mutex_unlock()` 等等。

非线程安全函数：

- `uv_queue_work()`：投递异步任务
- `uv_os_unsetenv()`：删除环境变量
- `uv_os_setenv()`：设置环境变量
- `uv_os_getenv()`：获取环境变量
- `uv_os_environ()`：检索所有的环境变量
- `uv_os_tmpdir()`：获取临时目录
- `uv_os_homedir()`：获取家目录

**提示：所有形如 `uv_xxx_init()` 的函数，即使它是以线程安全的方式实现的，但使用时要注意避免多个线程同时调用 `uv_xxx_init()` 初始化同一个句柄，否则它依旧会引起多线程资源竞争的问题。最好的方式是每个线程使用自己创建的句柄，或者在事件循环线程中调用这种 `uv_xxx_init()` 函数。**

那么，想要跨线程触发一个动作，正确做法是loop线程持有一个 `uv_async_t` 在 `uv_async_send` 触发的回调函数中执行要需要执行的代码段。具体参考：https://blog.csdn.net/robinfoxnan/article/details/118364469

### 正确退出事件循环

libuv 的官方示例里，几乎都是正常的退出流程：各个 handle 都主动退出了，最后 uv_run 再退出。而实际遇到的情况可能是，handle 还处于有效的 ACTIVE 状态时需要退出，怎么办？调用 `uv_loop_close` ？不行，handle 还处于 ACTIVE 状态，`uv_loop_close` 会返回 `UV_EBUSY`。

正确做法是：

1. 应该通知各个模块把相关的 uv handle 都 close 掉。

2. 调用 `uv_stop()` 停止事件循环，不再往里塞任务。

3. 通过 `uv_walk()` 遍历所有 handle，强制关闭掉来兜底。

   ```c
   uv_stop(loop);
   
   void ensure_closing(uv_handle_t* handle, void*) {
     if (!uv_is_closing(handle)) {
       uv_close(handle, nullptr);
     }
   }
   
   uv_walk(loop, ensure_closing, nullptr);
   ```

4. 调用 `uv_loop_close` 关闭事件循环。

但是，问题又来了，这样的兜底 `uv_close` 并没有传 close 回调，那 handle 的内存又如何删除？假如有漏网之鱼，就让它泄露算了？好像也没有太好的办法，只能尽量做好 handle 管理，能主动进行 `uv_close`，而不是等到 `uv_walk` 里被兜底 close。

接着又有另外一个问题，前面说到 `uv_close` 只是加入 loop 的 close 队列，并没有触发真正的 close 和回调。这时要确保这些 handle 被 close，则需要再次触发 `uv_run` 直到所有 handle close 完毕。

```c
for (;;)
  if (uv_run(loop, UV_RUN_DEFAULT) == 0)
    break;
```

uv 已经要退出了，但为了执行前面的 `uv__run_closing_handles`，又触发了 uv_run 的执行。假设哪里处理不好，其他线程又往 uv 线程抛了新的 handle 任务，这时就是灾难，很可能 crash 就会扑面而来。所以这里一定要做好状态管理，一旦进入 `uv_stop` 流程，其他线程也不应该再往 uv 抛任务。

上面提到的问题不是只有新手才会犯，我们可以看看著名的 electron 项目，也是犯了同样的错误：

https://github.com/electron/electron/pull/25332

> This PR wraps the uv_async_t objects owned by NodeBindings and ElectronBindings inside a new UvHandle wrapper class which handles uv_handle_ts’ specific rules about destruction:
>
> [uv_close()] MUST be called on each handle before memory is released. Moreover, the memory can only be released in close_cb or after it has returned.
>
> The UvHandle wrapper class handles this close-delete twostep so that client code doesn’t have to think about it. Failure to finish closing before freeing is what caused the uv_walk() crash in #25248.

为了确保 handle 正确被释放，electron 对 uv 的 handle 做了一层包装，很好的方法，可以借鉴一下：

```cpp
template <typename T,
          typename std::enable_if<
              // these are the C-style 'subclasses' of uv_handle_t
              std::is_same<T, uv_async_t>::value ||
              std::is_same<T, uv_check_t>::value ||
              std::is_same<T, uv_fs_event_t>::value ||
              std::is_same<T, uv_fs_poll_t>::value ||
              std::is_same<T, uv_idle_t>::value ||
              std::is_same<T, uv_pipe_t>::value ||
              std::is_same<T, uv_poll_t>::value ||
              std::is_same<T, uv_prepare_t>::value ||
              std::is_same<T, uv_process_t>::value ||
              std::is_same<T, uv_signal_t>::value ||
              std::is_same<T, uv_stream_t>::value ||
              std::is_same<T, uv_tcp_t>::value ||
              std::is_same<T, uv_timer_t>::value ||
              std::is_same<T, uv_tty_t>::value ||
              std::is_same<T, uv_udp_t>::value>::type* = nullptr>
class UvHandle {
 public:
  UvHandle() : t_(new T) {}
  ~UvHandle() { reset(); }
  T* get() { return t_; }
  uv_handle_t* handle() { return reinterpret_cast<uv_handle_t*>(t_); }

  void reset() {
    auto* h = handle();
    if (h != nullptr) {
      DCHECK_EQ(0, uv_is_closing(h));
      uv_close(h, OnClosed);
      t_ = nullptr;
    }
  }

 private:
  static void OnClosed(uv_handle_t* handle) {
    delete reinterpret_cast<T*>(handle);
  }

  T* t_ = {};
};
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

（5）`uv_now` 通过事件循环获取当前的毫秒数，`uv_hrtime` 获取纳秒数。后者不需要传入loop参数

```c
uint64_t uv_now(const uv_loop_t* loop) {
  return loop->time; // time成员在单次事件循环每次开始时就会更新
}
```

（6）`uv_update_time` 更新 `uv_loop_t::time` 成员。libuv 在事件循环开始时缓存当前时间戳，以减少与时间相关的系统调用次数。一般情况不需要调用这个函数，但是如果有回调执行了很长时间，那么就有必要调用一下来更新时间。