CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
SRCS    := src/main.c src/parser.c src/encoder.c src/expr.c \
           src/lexer.c src/symtab.c src/asm.c
OBJS    := $(SRCS:.c=.o)
BIN     := glic80asm

# STATIC=1 links libc statically (Linux/Windows-MinGW only). Produces a
# binary with no shared-lib dependency, so it runs regardless of the host
# glibc version. NOT supported on macOS -- Apple only allows going through
# libSystem.dylib, so leave STATIC unset there. Static linking does not
# make one binary cross-OS; each OS still needs its own build.
ifeq ($(STATIC),1)
LDFLAGS += -static
endif

PREFIX  ?= $(HOME)/.local
BINDIR  ?= $(PREFIX)/bin

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -m 755 tools/compile.sh $(DESTDIR)$(BINDIR)/glic80compile

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN) $(DESTDIR)$(BINDIR)/glic80compile
