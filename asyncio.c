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

static int epfd;
static ucontext_t ucp_main;
static ucontext_t *ucp_pending = NULL;
static ucontext_t *ucp_cleanup = NULL;
static size_t ucp_count = 0;

static inline void dispatch()
{
    if(ucp_pending)
    {
        ucontext_t *ucp = ucp_pending;
        ucp_pending = ucp->uc_link;
        setcontext(ucp);
    }
}

static inline void cleanup()
{
    if(ucp_cleanup)
    {
        free(ucp_cleanup);
        ucp_cleanup = NULL;
    }
}

static void startup(ucontext_t *ucp, void (*func)(void *args), void *args)
{
    cleanup();

    func(args);

    ucp_cleanup = ucp;
    if(--ucp_count == 0)
        setcontext(&ucp_main);

    dispatch();

    struct epoll_event event;
    while(epoll_wait(epfd, &event, 1, -1) != 1);
    setcontext(event.data.ptr);
}

void async_create(void (*func)(void *args), void *args)
{
    ucontext_t *ucp = malloc(STACK_SIZE);

    getcontext(ucp);
    ucp->uc_stack.ss_sp = (void*)ucp + sizeof(ucontext_t);
    ucp->uc_stack.ss_size = STACK_SIZE - sizeof(ucontext_t);
    ucp->uc_link = ucp_pending;
    ucp_pending = ucp;
    ucp_count++;

    makecontext(ucp, (void(*)())startup, 3, ucp, func, args);
}

void async_wait(int fd, int dir)
{
    volatile int flag = 0;

    ucontext_t ucp;
    getcontext(&ucp);
    if(flag)
    {
        cleanup();
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return;
    }

    struct epoll_event event = { dir ? EPOLLOUT : EPOLLIN, { &ucp } };
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);

    flag = 1;
    dispatch();

    while(epoll_wait(epfd, &event, 1, -1) != 1);
    setcontext(event.data.ptr);
}

void async_sleep(unsigned long usec)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, 0);

    struct itimerspec it = { { 0, 0 }, { usec / 1000000, usec % 1000000 * 1000 } };
    timerfd_settime(fd, 0, &it, NULL);

    async_wait(fd, ASYNC_READ);
    close(fd);
}

void async_main()
{
    epfd = epoll_create1(0);

    volatile int flag = 0;
    getcontext(&ucp_main);
    if(flag)
    {
        cleanup();
        return;
    }

    flag = 1;
    dispatch();
}
