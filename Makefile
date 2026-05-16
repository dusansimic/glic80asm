CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2
SRCS    := src/main.c src/parser.c src/encoder.c src/expr.c \
           src/lexer.c src/symtab.c src/asm.c
OBJS    := $(SRCS:.c=.o)
BIN     := glic80asm

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN) tests/out.bin

test: $(BIN)
	./tests/run_tests.sh
