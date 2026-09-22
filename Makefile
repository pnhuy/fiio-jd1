CC = clang
CPPFLAGS += -Isrc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c11
FRAMEWORKS = -framework IOKit -framework CoreFoundation

.PHONY: all test clean
all: build/jd1ctl

build:
	mkdir -p $@

build/jd1ctl: src/jd1ctl.c src/cli.c src/cli.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/jd1ctl.c src/cli.c $(LDFLAGS) $(FRAMEWORKS) -o $@

build/test_cli: tests/test_cli.c src/cli.c src/cli.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_cli.c src/cli.c $(LDFLAGS) -o $@

test: build/test_cli
	./build/test_cli

clean:
	rm -rf build
