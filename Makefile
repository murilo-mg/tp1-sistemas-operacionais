CC = gcc
CFLAGS = -std=c11 -O0 -Wall -Wextra -pthread
PYTHON ?= python3
PROGRAMAS = q1_v1 q1_v2 q1_v3 q2_v1 q2_v2 q2_v3

.PHONY: all test clean
all: $(PROGRAMAS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

test: all
	$(PYTHON) tests/testar.py

clean:
	rm -f $(PROGRAMAS)
