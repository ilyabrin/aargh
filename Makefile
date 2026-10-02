# Makefile for argh.h
# Works with GCC, Clang and MinGW on Linux, macOS and Windows.
#
#   make            build tests and examples
#   make test       build and run tests, a build without stdio, the fuzz corpus
#   make fuzz       fuzz argh_parse with libFuzzer (clang), FUZZ_TIME seconds
#   make examples   build the examples in examples/
#   make smoke      run the examples and check their output
#   make bench      run benchmarks (speed and code size)
#   make size-arm   flash added to bare-metal ARM firmware (needs arm-none-eabi-gcc)
#   make cxx        check that argh.h compiles as C++
#   make CC=clang   use a different compiler

CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -O2

ifeq ($(OS),Windows_NT)
    EXE = .exe
    # cmd's del wants backslashes
    RM  = cmd /C del /Q
    fixpath = $(subst /,\,$(1))
    # MinGW ships gcc but no "cc"
    ifeq ($(origin CC),default)
        CC = gcc
    endif
else
    EXE =
    RM  = rm -f
    fixpath = $(1)
endif

EXAMPLES = examples/wc$(EXE) examples/logship$(EXE) examples/pkg/pkg$(EXE)
PKG_SRC  = examples/pkg/main.c examples/pkg/install.c examples/pkg/remote.c examples/pkg/exec.c

.PHONY: all test examples smoke bench size-arm fuzz cxx clean

all: test_argh$(EXE) $(EXAMPLES)

test: test_argh$(EXE) nostdio_check$(EXE) fuzz_replay$(EXE) static_check$(EXE)
	./test_argh$(EXE)
	./nostdio_check$(EXE)
	./static_check$(EXE) -v
	./fuzz_replay$(EXE) tests/fuzz/*
	@if $(CC) $(CFLAGS) -o settings_mismatch$(EXE) tests/settings_main.c tests/settings_impl.c >/dev/null 2>&1; \
	then echo "FAIL: files with different settings linked"; exit 1; \
	else echo "files with different settings: rejected at link time"; fi

test_argh$(EXE): tests/test_argh.c argh.h
	$(CC) $(CFLAGS) -o $@ tests/test_argh.c $(LDFLAGS)

# argh.h built without <stdio.h>, as firmware would use it
nostdio_check$(EXE): tests/nostdio_check.c argh.h
	$(CC) $(CFLAGS) -o $@ tests/nostdio_check.c $(LDFLAGS)

# Two private copies of argh.h with ARGH_STATIC in one program
static_check$(EXE): tests/static_check.c tests/static_check_other.c argh.h
	$(CC) $(CFLAGS) -o $@ tests/static_check.c tests/static_check_other.c $(LDFLAGS)

# The fuzz target run once on each saved input, with any compiler
fuzz_replay$(EXE): tests/fuzz_argh.c argh.h
	$(CC) $(CFLAGS) -DFUZZ_REPLAY -o $@ tests/fuzz_argh.c $(LDFLAGS)

# New inputs go to fuzz_corpus/; tests/fuzz holds the seeds
FUZZ_CC   ?= clang
FUZZ_TIME ?= 60
fuzz: tests/fuzz_argh.c argh.h
	$(FUZZ_CC) -std=c99 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -o fuzz_argh tests/fuzz_argh.c
	mkdir -p fuzz_corpus
	./fuzz_argh -max_total_time=$(FUZZ_TIME) -max_len=1024 fuzz_corpus tests/fuzz

examples: $(EXAMPLES)

examples/wc$(EXE): examples/wc.c argh.h
	$(CC) $(CFLAGS) -o $@ examples/wc.c $(LDFLAGS)

examples/logship$(EXE): examples/logship.c argh.h
	$(CC) $(CFLAGS) -o $@ examples/logship.c $(LDFLAGS)

examples/pkg/pkg$(EXE): $(PKG_SRC) examples/pkg/pkg.h argh.h
	$(CC) $(CFLAGS) -o $@ $(PKG_SRC) $(LDFLAGS)

smoke: $(EXAMPLES)
	sh examples/smoke.sh

# Benchmarks measure a release build: C11 for timespec_get, -O2, NDEBUG
bench: bench_parse$(EXE)
	./bench_parse$(EXE)
	sh bench/size.sh $(CC)

bench_parse$(EXE): bench/bench_parse.c argh.h
	$(CC) -std=c11 -O2 -DNDEBUG -Wall -Wextra -o $@ bench/bench_parse.c

size-arm:
	sh bench/size_arm.sh

cxx: tests/cxx_check.cpp argh.h
	$(CXX) -std=c++11 -Wall -Wextra -Wpedantic -Werror -o cxx_check$(EXE) tests/cxx_check.cpp

clean:
	-$(RM) $(call fixpath,test_argh$(EXE) nostdio_check$(EXE) fuzz_replay$(EXE) static_check$(EXE) bench_parse$(EXE) cxx_check$(EXE) $(EXAMPLES))
