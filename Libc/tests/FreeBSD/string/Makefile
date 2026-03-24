FreeBSD_string_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_string_DIR}
FreeBSD_string_TESTS = \
	t_strchrnul

$(FreeBSD_string_TESTS):: OTHER_LDFLAGS += -latf-c
