NAME = MooseX-Types-Structured
VERSION = 0.30

include ../Makefile.inc

emptyfiles = /usr/share/man/man3/MooseX::Meta::TypeCoercion::Structured::Optional.3pm \
	     /usr/share/man/man3/MooseX::Meta::TypeConstraint::Structured::Optional.3pm \
	     /usr/share/man/man3/MooseX::Types::Structured::MessageStack.3pm \
	     /usr/share/man/man3/MooseX::Types::Structured::OverflowHandler.3pm
postinstall:
	@set -x && \
	for e in $(emptyfiles); do \
	    if [ ! -s $(DSTROOT)$$e ]; then \
		rm -f $(DSTROOT)$$e || exit 1; \
	    fi \
	done
