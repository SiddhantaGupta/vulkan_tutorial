CC = gcc
CFLAGS = -std=c23 -Wall -g
LDFLAGS =
LIBS = -lvulkan -lglfw -lm

SRCS = main.c

all: run

build: build/shaders
	mkdir -p build
	$(CC) $(SRCS) -o build/vk_tut $(CFLAGS) $(LDFLAGS) $(LIBS)

build/shaders:
	cd ./shaders && ./compile.sh

run: build
	./build/vk_tut

clean:
	rm -rf build/

.PHONY: all build run clean build/shaders


