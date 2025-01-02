#CCFLAGS=-O3 -std=gnu17
CC=gcc
CCFLAGS=-ggdb -std=gnu17 # -Wall -Wextra

INC=-I./src
SOURCEDIR := src
SOURCES := $(filter-out src/main.c, $(wildcard $(SOURCEDIR)/*.c))
OBJDIR = obj
OBJECTS := $(patsubst $(SOURCEDIR)/%.c,$(OBJDIR)/%.o, $(SOURCES))

TEST_INC=-I./src -I./test
TESTDIR := test
TEST_SOURCES := $(wildcard $(TESTDIR)/*.c)
TEST_OBJDIR = test_obj
TEST_OBJECTS := $(patsubst $(TESTDIR)/%.c,$(TEST_OBJDIR)/%.o, $(TEST_SOURCES))

BINDIR := bin

all: $(BINDIR)/arga $(BINDIR)/test

arga: $(BINDIR)/arga
	$(BINDIR)/arga

run: $(BINDIR)/arga
	for file in $(shell $(BINDIR)/arga); do 	\
		echo $$file; 																			\
		$(BINDIR)/arga $$file; 														\
	done

test: $(BINDIR)/test
	$(BINDIR)/test

clean:
	rm -rf $(OBJDIR) $(TEST_OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(SOURCEDIR)/*.h Makefile | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INC) -c $< -o $@

$(TEST_OBJDIR):
	mkdir -p $(TEST_OBJDIR)

$(TEST_OBJDIR)/%.o: $(TESTDIR)/%.c $(SOURCEDIR)/*.h $(TESTDIR)/*.h Makefile | $(TEST_OBJDIR)
	$(CC) $(CCFLAGS) $(TEST_INC) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/arga: $(OBJECTS) obj/main.o | $(BINDIR)
	$(CC) -o $(BINDIR)/arga $(OBJECTS) obj/main.o -lcjson

$(BINDIR)/test: $(TEST_OBJECTS) $(OBJECTS) | $(BINDIR)
	$(CC) -o $(BINDIR)/test $(TEST_OBJECTS) $(OBJECTS) -lcjson
