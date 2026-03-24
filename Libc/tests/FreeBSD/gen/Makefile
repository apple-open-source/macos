FreeBSD_gen_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_gen_DIR}
FreeBSD_gen_TESTS = \
	fnmatch_test 	\
	glob_blocks_test\
	glob2_test	\
	opendir_test	\
	popen_test	\
	scandir_test	\
	scandir_blocks_test \
	t_alarm		\
	t_basedirname	\
	t_dir		\
	t_ftok		\
	t_fnmatch

$(FreeBSD_gen_TESTS):: OTHER_LDFLAGS += -latf-c
