/*
 * Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 * Public domain.
 */

#include <stdio.h>
#include <unistd.h>
#include "asyncio.h"

static void reader(void *arg)
{
    int i, fd = *(int*)arg;

    while(1)
    {
        async_wait(fd, ASYNC_READ);

        if(read(fd, &i, sizeof(i)) == 0)
        {
            close(fd);
            return;
        }

        printf("read %d\n", i);
    }
}

static void writer(void *arg)
{
    int i, fd = *(int*)arg;

    for(i = 0; i < 3; i++)
    {
        printf("write %d\n", i);
        write(fd, &i, sizeof(i));

        async_sleep(500000); // 0.5s
    }

    close(fd);
}

int main()
{
    int fds[2];
    pipe(fds);

    async_create(reader, &fds[0]);
    async_create(writer, &fds[1]);
    async_main();

    printf("done\n");
    return 0;
}
