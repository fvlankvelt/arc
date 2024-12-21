#CCFLAGS=-O3 -std=gnu17
CC=gcc
CCFLAGS=-ggdb -std=gnu17 # -Wall -Wextra

INC=-I./src
SOURCEDIR := src
SOURCES := $(wildcard $(SOURCEDIR)/*.c)
OBJDIR = obj
OBJECTS := $(patsubst $(SOURCEDIR)/%.c,$(OBJDIR)/%.o, $(SOURCES))

TEST_INC=-I./src -I./test
TESTDIR := test
TEST_SOURCES := $(wildcard $(TESTDIR)/*.c)
TEST_OBJDIR = test_obj
TEST_OBJECTS := $(patsubst $(TESTDIR)/%.c,$(TEST_OBJDIR)/%.o, $(TEST_SOURCES))

BINDIR := bin

test: $(BINDIR)/test
	$(BINDIR)/test

clean:
	rm -rf $(OBJDIR) $(TEST_OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SOURCEDIR)/%.c Makefile | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INC) -c $< -o $@

$(TEST_OBJDIR):
	mkdir -p $(TEST_OBJDIR)

$(TEST_OBJDIR)/%.o: $(TESTDIR)/%.c Makefile | $(TEST_OBJDIR)
	$(CC) $(CCFLAGS) $(TEST_INC) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/test: $(TEST_OBJECTS) $(OBJECTS) | $(BINDIR)
	$(CC) -o $(BINDIR)/test $(TEST_OBJECTS) $(OBJECTS)

# 
# out/%.o: src/%.c *.h
# 	gcc ${CCFLAGS} -c $< 
# 
# all: test_graph test_transform test_filter
# 
# test_graph: test_graph.o image.o filter.o transform.o
# 	gcc -o test_graph test_graph.o image.o transform.o filter.o
# 
# test_transform: test_transform.o image.o filter.o transform.o
# 	gcc -o test_transform test_transform.o image.o transform.o filter.o
# 
# test_filter: test_filter.o image.o filter.o transform.o
# 	gcc -o test_filter test_filter.o image.o transform.o filter.o
# 
# clean:
# 	rm -f *.o test_graph test_transform test_filter
# 
# run: test_graph test_transform test_filter
# 	./test_graph && ./test_transform && ./test_filter
