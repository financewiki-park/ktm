CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?= -ldl
TARGET_PLATFORM ?= host

BIN := build/ktm

.PHONY: all clean test package

all: $(BIN)

build:
	mkdir -p build

$(BIN): src/tgbridge.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/tgbridge.c $(LDFLAGS) $(LDLIBS)

test: $(BIN)
	sh tests/test_host.sh ./$(BIN)
	sh tests/test_mock_flow.sh ./$(BIN)

package: $(BIN)
	sh tools/build-kpkg.sh ./$(BIN) "$(TARGET_PLATFORM)"

clean:
	rm -rf build dist
