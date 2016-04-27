/*
 * Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 * Public domain.
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include "asyncio.h"

#include <valgrind/valgrind.h>

#define STACK_SIZE 0x800000

static int epfd = -1; // global epoll instance
static unsigned long task_count = 0; // number of allocated tasks
static void *trash = NULL; // memory chunk of a previously terminated task
static ucontext_t *ucp_main = NULL; // main context
static ucontext_t *ucp_next = NULL; // list of pending contexts

static inline void clear(void **ptr)
{
    if(*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

static void startup(void *stack_top, unsigned long long stack_id, void (*func)(void *args), void *args)
{
    clear(&trash);
    func(args);
    trash = stack_top;

#ifdef __VALGRIND_H
    VALGRIND_STACK_DEREGISTER(stack_id);
#endif

    if((--task_count == 0) && ucp_main)
        setcontext(ucp_main);

    if(ucp_next)
    {
        ucontext_t *ucp = ucp_next;
        ucp_next = ucp->uc_link;
        setcontext(ucp);
    }

    struct epoll_event event;
    while(epoll_wait(epfd, &event, 1, -1) != 1);
    setcontext(event.data.ptr);
}

void async_create(void (*func)(void *args), void *args)
{
    void *stack_top = malloc(STACK_SIZE);
    unsigned long long stack_id = 0;

#ifdef __VALGRIND_H
    stack_id = VALGRIND_STACK_REGISTER(stack_top, stack_top + STACK_SIZE - 1);
#endif

    ucontext_t *ucp = stack_top + STACK_SIZE - sizeof(ucontext_t);

    getcontext(ucp);
    ucp->uc_stack.ss_sp = stack_top;
    ucp->uc_stack.ss_size = STACK_SIZE;
    makecontext(ucp, (void(*)())startup, 4, stack_top, stack_id, func, args);

    ucp->uc_link = ucp_next;
    ucp_next = ucp;
    task_count++;
}

void async_wait(int fd, int dir)
{
    if(epfd == -1) epfd = epoll_create1(EPOLL_CLOEXEC);
    if(epfd == -1) return;

    ucontext_t ctx;
    struct epoll_event event = { dir ? EPOLLOUT : EPOLLIN, { &ctx } };
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) != 0)
    {
        if(errno == EEXIST)
        {
            int fd2 = dup(fd);
            async_wait(fd2, dir);
            close(fd2);
        }

        return;
    }

    if(ucp_next)
    {
        ucontext_t *ucp = ucp_next;
        ucp_next = ucp->uc_link;
        swapcontext(&ctx, ucp);
    }
    else
    {
        struct epoll_event event;
        while(epoll_wait(epfd, &event, 1, -1) != 1);
        if(&ctx != event.data.ptr)
            swapcontext(&ctx, event.data.ptr);
    }

    clear(&trash);
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

void async_sleep(unsigned long usec)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if(fd == -1) return;

    struct itimerspec it = { { 0, 0 }, { usec / 1000000, usec % 1000000 * 1000 } };
    timerfd_settime(fd, 0, &it, NULL);

    async_wait(fd, ASYNC_READ);
    close(fd);
}

void async_main()
{
    if(ucp_main) return;

    if(ucp_next)
    {
        ucontext_t ctx;
        ucp_main = &ctx;

        ucontext_t *ucp = ucp_next;
        ucp_next = ucp->uc_link;
        swapcontext(&ctx, ucp);

        clear(&trash);
        ucp_main = NULL;
    }

    if(epfd != -1)
    {
        close(epfd);
        epfd = -1;
    }
}
