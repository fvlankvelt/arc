#CCFLAGS=-O3 -std=gnu17
CCFLAGS=-ggdb -std=gnu17

%.o: %.c *.h
	gcc ${CCFLAGS} -c $< 

all: test_graph test_transform

test_graph: test_graph.o image.o filter.o transform.o
	gcc -o test_graph test_graph.o image.o transform.o

test_transform: test_transform.o image.o filter.o transform.o
	gcc -o test_transform test_transform.o image.o transform.o

clean:
	rm -f *.o test_graph test_transform

run: test_graph test_transform
	./test_graph && ./test_transform
