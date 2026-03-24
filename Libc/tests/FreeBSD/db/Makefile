FreeBSD_db_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_db_DIR}
FreeBSD_db_TESTS = \
	dbm_nextkey_test \
	dbm_open_test	 \
	dbm_perm_test

$(FreeBSD_db_TESTS):: OTHER_LDFLAGS += -latf-c
