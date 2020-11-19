NAME = MooseX-Declare
VERSION = 0.43

include ../Makefile.inc

emptyfiles = /usr/share/man/man3/MooseX::Declare::Context::WithOptions.3pm \
	/usr/share/man/man3/MooseX::Declare::StackItem.3pm \
	/usr/share/man/man3/MooseX::Declare::Syntax::MethodDeclaration::Parameterized.3pm

postinstall:
	@set -x && \
	for e in $(emptyfiles); do \
	    if [ ! -s $(DSTROOT)$$e ]; then \
		rm -f $(DSTROOT)$$e || exit 1; \
	    fi \
	done
