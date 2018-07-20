version = $(shell head -n1 changelog | cut -d ' ' -f2 | tr -d '()')

.PHONY: all
all: recidivm.1

recidivm.1: manpage.pod
	perl -p -e 's/L<([a-z_-]+)[(]([0-9]+)[)]>/B<\1>(\2)/g' $(<) \
	| pod2man --utf8 -c '' -n RECIDIVM -r 'recidivm $(version)' \
	> $(@).tmp
	mv $(@).tmp $(@)

.PHONY: clean
clean:
	rm -f *.1 *.tmp

# vim:ts=4 sts=4 sw=4 noet
