/*
 * Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 * Public domain.
 */

#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include "asyncio.h"

#define STACK_SIZE 0x800000

static int epfd = -1;
static ucontext_t ctx_main;
static ucontext_t *ctx_pending = NULL;
static ucontext_t *ctx_cleanup = NULL;
static size_t ctx_count = 0;

static inline void dispatch()
{
    if(ctx_pending)
    {
        ucontext_t *ctx = ctx_pending;
        ctx_pending = ctx->uc_link;
        setcontext(ctx);
    }
}

static inline void cleanup()
{
    if(ctx_cleanup)
    {
        free(ctx_cleanup);
        ctx_cleanup = NULL;
    }
}

static void startup(ucontext_t *ctx, void (*func)(void *args), void *args)
{
    cleanup();

    func(args);

    ctx_cleanup = ctx;
    if(--ctx_count == 0)
        setcontext(&ctx_main);

    dispatch();

    struct epoll_event event;
    while(epoll_wait(epfd, &event, 1, -1) != 1);
    setcontext(event.data.ptr);
}

void async_create(void (*func)(void *args), void *args)
{
    ucontext_t *ctx = malloc(STACK_SIZE);

    getcontext(ctx);
    ctx->uc_stack.ss_sp = (void*)ctx + sizeof(ucontext_t);
    ctx->uc_stack.ss_size = STACK_SIZE - sizeof(ucontext_t);
    ctx->uc_link = ctx_pending;
    ctx_pending = ctx;
    ctx_count++;

    makecontext(ctx, (void(*)())startup, 3, ctx, func, args);
}

void async_wait(int fd, int dir)
{
    if(epfd == -1) return;
    volatile int flag = 0;

    ucontext_t ctx;
    getcontext(&ctx);
    if(flag)
    {
        cleanup();
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return;
    }

    struct epoll_event event = { dir ? EPOLLOUT : EPOLLIN, { &ctx } };
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) != 0)
        return; // return if fd is waited upon by another context; caller should dup()

    flag = 1;
    dispatch();

    while(epoll_wait(epfd, &event, 1, -1) != 1);
    setcontext(event.data.ptr);
}

void async_sleep(unsigned long usec)
{
    if(epfd == -1) return;
    int fd = timerfd_create(CLOCK_MONOTONIC, 0);

    struct itimerspec it = { { 0, 0 }, { usec / 1000000, usec % 1000000 * 1000 } };
    timerfd_settime(fd, 0, &it, NULL);

    async_wait(fd, ASYNC_READ);
    close(fd);
}

void async_main()
{
    if(epfd != -1) return;
    epfd = epoll_create1(0);

    volatile int flag = 0;
    getcontext(&ctx_main);
    if(flag)
    {
        cleanup();
        goto out;
    }

    flag = 1;
    dispatch();

out:
    close(epfd);
    epfd = -1;
}
