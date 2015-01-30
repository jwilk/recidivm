CFLAGS ?= -g -O2
CFLAGS += -Wall -Wextra -Wconversion
CFLAGS += $(shell getconf LFS_CFLAGS)

.PHONY: all
all: ppvm

.PHONY: clean
clean:
	rm -f ppvm

# vim:ts=4 sts=4 sw=4 noet
