# Makefile for DTrace analysis tools
# Copyright (c) 2024 Cognu Mach Contributors

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 

# Default target
all: dtrace-analyze

# Build the analysis tool
dtrace-analyze: dtrace-analyze.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean up
clean:
	rm -f dtrace-analyze *.o

# Install (copy to /usr/local/bin)
install: dtrace-analyze
	install -m 755 dtrace-analyze /usr/local/bin/
	install -m 755 dtrace-visualize.py /usr/local/bin/ || true

# Test the tool
test: dtrace-analyze
	./dtrace-analyze -v

.PHONY: all clean install test