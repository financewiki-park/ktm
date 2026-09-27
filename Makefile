CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?= -ldl
TARGET_PLATFORM ?= host

BIN := build/ktm
TERMINAL := build/ktm-input

.PHONY: all clean test package

all: $(BIN) $(TERMINAL)

build:
	mkdir -p build

$(BIN): src/tgbridge.c src/run_command.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/tgbridge.c $(LDFLAGS) $(LDLIBS)

$(TERMINAL): src/terminal.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/terminal.c $(LDFLAGS)

test: $(BIN) $(TERMINAL)
	sh tests/test_host.sh ./$(BIN)
	sh tests/test_mock_flow.sh ./$(BIN)
	python3 tests/test_terminal.py ./$(TERMINAL)
	python3 tests/test_launcher.py
	python3 tests/test_run_command.py ./$(BIN)

package: $(BIN) $(TERMINAL)
	sh tools/build-kpkg.sh ./$(BIN) "$(TARGET_PLATFORM)" ./$(TERMINAL)

clean:
	rm -rf build dist
