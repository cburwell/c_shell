#!/bin/bash

CC=gcc
CFLAG=-Wall -g

shell: shell_v0.c
	$(CC) -o shell $(CFLAGS) shell_v0.c
test: shell
	./run
clean:
	$(RM) shell
