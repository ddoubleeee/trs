CC = gcc
CFLAGS = -Wall -Wextra
SRC = $(wildcard src/*.c)
OUT = bin/main

all: $(OUT)

$(OUT): $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)


clean:
	rm -rf bin
