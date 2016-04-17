/*
 * Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 * Public domain.
 */

#ifndef __ASYNCIO_H__
#define __ASYNCIO_H__

#define ASYNC_READ  0
#define ASYNC_WRITE 1

// Creates an asynchronous task.
void async_create(void (*func)(void *args), void *args);

// Waits for a file descriptor to become ready for reading or writing.
// Do NOT wait for a same descriptor from multiple tasks, use dup() instead.
void async_wait(int fd, int dir);

// Suspends current task for the specified amount of microseconds.
void async_sleep(unsigned long usec);

// Waits for all tasks to finish.
void async_main();

#endif /* __ASYNCIO_H__ */
