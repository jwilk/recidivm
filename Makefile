CFLAGS ?= -g -O2
CFLAGS += -Wall -Wextra -Wconversion
CFLAGS += $(shell getconf LFS_CFLAGS)
LDFLAGS += $(shell getconf LFS_LDFLAGS)
LDLIBS += $(shell getconf LFS_LIBS)

.PHONY: all
all: recidivm

.PHONY: clean
clean:
	rm -f recidivm

# vim:ts=4 sts=4 sw=4 noet
