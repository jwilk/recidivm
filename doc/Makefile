version = $(shell head -n1 changelog | cut -d ' ' -f2 | tr -d '()')

.PHONY: all
all: ppvm.1

ppvm.1: manpage.pod
	sed -e 's/L<\([a-z_-]\+\)(\([0-9]\+\))>/B<\1>(\2)/' $(<) \
	| pod2man --utf8 -c '' -n PPVM -r 'ppvm $(version)' \
	> $(@).tmp
	mv $(@).tmp $(@)

.PHONY: clean
clean:
	rm -f *.1 *.tmp

# vim:ts=4 sts=4 sw=4 noet
