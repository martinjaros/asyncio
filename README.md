asyncio
=======

Stop using threads for IO, start using **asyncio**.

Example:

```c
#include <stdio.h>
#include <unistd.h>
#include "asyncio.h"

#define read(fd, buf, n) ({ async_wait(fd, ASYNC_READ); read(fd, buf, n); })
#define write(fd, buf, n) ({ async_wait(fd, ASYNC_WRITE); write(fd, buf, n); })
#define usleep(n) async_sleep(n)

static void reader(void *arg)
{
    int fd = *(int*)arg;

    int i;
    while(read(fd, &i, sizeof(i)) > 0)
        printf("read %d\n", i);

    close(fd);
}

static void writer(void *arg)
{
    int fd = *(int*)arg;

    int i;
    for(i = 0; i < 3; i++)
    {
        printf("write %d\n", i);
        write(fd, &i, sizeof(i));
        usleep(500000);
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
```
