CC = cc
CFLAGS = -Wall -Wextra -Iinclude
SRC = $(wildcard src/*.c)
OUT = bin/main

all:
	mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -rf bin build
