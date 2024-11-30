
%.o: %.c
	gcc -O3 -c $< 

test: test.o image.o
	gcc -o test test.o image.o

clean:
	rm -f *.o test

run: test
	./test
