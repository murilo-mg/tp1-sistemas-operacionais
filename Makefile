CC = gcc
CFLAGS = -std=c11 -O0 -Wall -Wextra -pthread
PROGRAMAS = q1_v1 q1_v2 q1_v3 q2_v1 q2_v2 q2_v3
.PHONY: all clean
all: $(PROGRAMAS)
%: %.c
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(PROGRAMAS)
