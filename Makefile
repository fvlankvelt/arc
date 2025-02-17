CC=gcc
CCFLAGS=-Og -ggdb -std=gnu17 -Wall -Wextra
#CCFLAGS=-ggdb -std=gnu17 -Wall -Wextra
CXXFLAGS=-Og -ggdb  -DUSE_C10D_GLOO -DUSE_C10D_NCCL -DUSE_DISTRIBUTED -DUSE_RPC -DUSE_TENSORPIPE -isystem /opt/libtorch/include -isystem /opt/libtorch/include/torch/csrc/api/include -isystem /usr/local/cuda/include -std=gnu++17 -D_GLIBCXX_USE_CXX11_ABI=1

LINKER=/usr/bin/c++ -rdynamic -L/lib/intel64  -L/lib/intel64_win  -L/lib/win-x64  -Wl,-rpath,/lib/intel64:/lib/intel64_win:/lib/win-x64:/opt/libtorch/lib:/usr/local/cuda/lib64 /opt/libtorch/lib/libtorch.so /opt/libtorch/lib/libc10.so /opt/libtorch/lib/libkineto.a /usr/local/cuda/lib64/libnvrtc.so /opt/libtorch/lib/libc10_cuda.so -Wl,--no-as-needed,"/opt/libtorch/lib/libtorch_cpu.so" -Wl,--as-needed -Wl,--no-as-needed,"/opt/libtorch/lib/libtorch_cuda.so" -Wl,--as-needed /opt/libtorch/lib/libc10_cuda.so /opt/libtorch/lib/libc10.so /usr/local/cuda/lib64/libcudart.so -Wl,--no-as-needed,"/opt/libtorch/lib/libtorch.so" -Wl,--as-needed /usr/local/cuda/lib64/libnvToolsExt.so

INC=-I./src
SOURCEDIR := src
CSOURCES := $(filter-out src/main.c, $(wildcard $(SOURCEDIR)/*.c))
OBJDIR = obj
COBJECTS := $(patsubst $(SOURCEDIR)/%.c,$(OBJDIR)/%.o, $(CSOURCES))
CXXOBJECTS := $(patsubst $(SOURCEDIR)/%.cpp,$(OBJDIR)/%.o, $(wildcard $(SOURCEDIR)/*.cpp))

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
		echo $$file; 														\
		time $(BINDIR)/arga $$file 2>& 1; 			\
	done

test: $(BINDIR)/test
	$(BINDIR)/test

clean:
	rm -rf $(OBJDIR) $(TEST_OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(SOURCEDIR)/*.h Makefile | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INC) -c $< -o $@
$(OBJDIR)/%.o: $(SOURCEDIR)/%.cpp $(SOURCEDIR)/*.h Makefile | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(TEST_OBJDIR):
	mkdir -p $(TEST_OBJDIR)

$(TEST_OBJDIR)/%.o: $(TESTDIR)/%.c $(SOURCEDIR)/*.h $(TESTDIR)/*.h Makefile | $(TEST_OBJDIR)
	$(CC) $(CCFLAGS) $(TEST_INC) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/arga: $(COBJECTS) $(CXXOBJECTS) obj/main.o | $(BINDIR)
	$(LINKER) -o $(BINDIR)/arga $(COBJECTS) $(CXXOBJECTS) obj/main.o -lcjson

$(BINDIR)/test: $(TEST_OBJECTS) $(COBJECTS) $(CXXOBJECTS) | $(BINDIR)
	$(LINKER) -o $(BINDIR)/test $(TEST_OBJECTS) $(COBJECTS) $(CXXOBJECTS) -lcjson
