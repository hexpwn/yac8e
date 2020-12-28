LDFLAGS=-lncurses
FLAGS=-g

all: 
	gcc src/yac8e.c -o bin/yac8e ${LDFLAGS} ${FLAGS}

test:
	gcc src/test.c -o bin/test ${LDFLAGS} ${FLAGS}

clean: 
	rm -rf bin/*
