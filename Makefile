# Copyright © 2015-2019 Jakub Wilk <jwilk@jwilk.net>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the “Software”), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

CFLAGS ?= -g -O2
CFLAGS += -Wall -Wextra -Wconversion
CFLAGS += -D_FILE_OFFSET_BITS=64

PREFIX = /usr/local
DESTDIR =

bindir = $(PREFIX)/bin
mandir = $(PREFIX)/share/man

.PHONY: all
all: recidivm

.PHONY: install
install: recidivm
	install -d $(DESTDIR)$(bindir)
	install -m755 $(<) $(DESTDIR)$(bindir)/
ifeq "$(wildcard doc/recidivm.1)" ""
	# run "$(MAKE) -C doc" to build the manpage
else
	install -d $(DESTDIR)$(mandir)/man1
	install -m644 doc/$(<).1 $(DESTDIR)$(mandir)/man1/
endif

.PHONY: test
test: recidivm
	prove -v

.PHONY: test-installed
test-installed: $(or $(shell command -v recidivm;),$(bindir)/recidivm)
	RECIDIVM_TEST_TARGET=recidivm prove -v

.PHONY: clean
clean:
	rm -f recidivm

.error = GNU make is required

# vim:ts=4 sts=4 sw=4 noet
