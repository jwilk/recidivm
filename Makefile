CFLAGS = -g -O2 -Wall
CFLAGS += $(shell getconf LFS_CFLAGS)

.PHONY: all
all: pkvm

.PHONY: clean
clean:
	rm -f pkvm

# vim:ts=4 sts=4 sw=4 noet
