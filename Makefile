all: example

example: example.c asyncio.c asyncio.h
	gcc -g -O2 -Wall -Werror -Wno-unused-result -o $@ example.c asyncio.c
