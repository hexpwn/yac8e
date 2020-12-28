LDFLAGS=-lncurses
FLAGS=-g

all: 
	gcc src/main.c -o bin/main ${LDFLAGS} ${FLAGS}

clean: 
	rm -rf bin/*
