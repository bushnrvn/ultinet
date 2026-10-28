# Host tests, and a check that the driver builds for the C64 (needs cc65).
CC ?= cc

test: build/test
	build/test

build/test: tests/test.c tests/uci_sim.c src/uci.c src/linestream.c src/uci.h src/linestream.h
	@mkdir -p build
	$(CC) -Wall -Wextra -o $@ tests/test.c tests/uci_sim.c src/uci.c src/linestream.c -Isrc -Itests

c64:
	@mkdir -p build
	cl65 -t c64 -Os -I src -o build/example.prg examples/read_lines.c src/uci.c src/linestream.c

clean:
	rm -rf build

.PHONY: test c64 clean
