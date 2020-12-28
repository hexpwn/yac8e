LDFLAGS=-lncurses
FLAGS=-g

all: 
	gcc src/main.c -o bin/main ${LDFLAGS} ${FLAGS}

test:
	gcc src/test.c -o bin/test ${LDFLAGS} ${FLAGS}

clean: 
	rm -rf bin/*
