#CCFLAGS=-O3 -std=gnu17
CCFLAGS=-ggdb -std=gnu17

%.o: %.c *.h
	gcc ${CCFLAGS} -c $< 

all: test

test: test.o image.o filter.o transform.o
	gcc -o test test.o image.o transform.o

clean:
	rm -f *.o test

run: test
	./test
