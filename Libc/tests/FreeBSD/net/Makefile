FreeBSD_net_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_net_DIR}
FreeBSD_net_TESTS = \
	sourcefilter_test

$(FreeBSD_net_TESTS):: OTHER_LDFLAGS += -latf-c
