CC=gcc
LDFLAGS=-lncurses
FLAGS=-g -Wall
BIN=bin/yac8e

all: yac8e

yac8e: src/yac8e.c
	$(CC) -o $(BIN) $^ $(LDFLAGS) $(FLAGS)

test: src/test.c
	$(CC) -o bin/test $^ $(LDFLAGS) $(FLAGS)

clean: 
	rm -rf bin/*
