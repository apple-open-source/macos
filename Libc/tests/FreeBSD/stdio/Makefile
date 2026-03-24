FreeBSD_stdio_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_stdio_DIR}
FreeBSD_stdio_TESTS = \
	flushexit_test \
	flushlbuf_test

$(FreeBSD_stdio_TESTS):: OTHER_LDFLAGS += -latf-c
