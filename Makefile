CFLAGS ?= -g -O2
CFLAGS += -Wall -Wextra -Wconversion
CFLAGS += $(shell getconf LFS_CFLAGS)
LDFLAGS += $(shell getconf LFS_LDFLAGS)
LDLIBS += $(shell getconf LFS_LIBS)

.PHONY: all
all: recidivm

.PHONY: install
install: recidivm
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m755 $(<) $(DESTDIR)$(PREFIX)/bin/$(<)
ifeq "$(wildcard .git doc/recidivm.1)" ".git"
	# run "$(MAKE) -C doc" to build the manpage
else
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m644 doc/recidivm.1 $(DESTDIR)$(PREFIX)/share/man/man1/recidivm.1
endif

.PHONY: clean
clean:
	rm -f recidivm

# vim:ts=4 sts=4 sw=4 noet
