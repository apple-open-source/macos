//
//  main.c
//  copyfile_test
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/mount.h>
#include <getopt.h>
#include <removefile.h>

#include "test_utils.h"

static tests_t copyfile_tests;

void register_test(test_t *t) {
	LIST_INSERT_HEAD(&copyfile_tests.t_tests, t, t_list);
	copyfile_tests.t_num++;
}

__attribute__((noreturn)) static void
usage(const char *progname, bool was_error) {
	printf("%s: a tool to test libcopyfile(3)\n", progname);
	printf("Usage: %s [ --test <testname> | --list | -h ]\n", progname);

	printf("%s --test (`-t`): Run a specific test.\n", progname);
	printf("%s --list (`-l`): List all supported tests.\n", progname);
	printf("%s --help (`-h`): Print this usage message.\n", progname);
	printf("If no arguments are provided, all tests will be run.\n");

	exit(was_error ? EX_USAGE : EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
	test_t *test;
	char *test_name = NULL;
	bool suite_failed = false;
	uid_t starting_euid = geteuid();
	uid_t current_euid = starting_euid;
	uint32_t bsize = 0;
	int arg;

	struct option long_options[] = {
		{ "test", 	required_argument,	NULL,	't' },
		{ "list",	no_argument,		NULL,	'l' },
		{ "help",	no_argument,		NULL,	'h' },
		{ NULL, 0, NULL, 0 },
	};

	// Parse arguments.
	while ((arg = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		switch (arg) {
			case 't':
				test_name = optarg;
				break;
			case 'l':
				sort_tests(&copyfile_tests);
				LIST_FOREACH(test, &copyfile_tests.t_tests, t_list) {
					printf("%s\n", test->t_name);
				}
				return EXIT_SUCCESS;
			default:
				printf("Invalid option %c specified.\n", (char)arg);
				OS_FALLTHROUGH;
			case 'h':
				usage(getprogname(), (arg != 'h'));
		}
	}

	// Do some housekeeping.
	if (!test_name) {
		sort_tests(&copyfile_tests);
	}
	setlinebuf(stdout);
	sranddev();

	// Run the specified test (or all tests).
	LIST_FOREACH(test, &copyfile_tests.t_tests, t_list) {
		if (test_name && strcmp(test->t_name, test_name)) {
			// We were asked to run a specific test,
			// which is not this one - keep going.
			continue;
		}

		set_up_test_dir((bsize == 0) ? &bsize : NULL);

		printf("[TEST] %s\n[BEGIN]\n", test->t_name);
		fflush(stdout);

		bool skip_test = false;
		if (test->t_asroot && current_euid) {
			if (seteuid(0)) {
				printf("[SKIP] %s\n", test->t_name);
				skip_test = true;
				if (test_name) {
					// Fail execution if the only test
					// that was requested was skipped.
					suite_failed = true;
				}
			} else {
				current_euid = 0;
			}
		} else if (!current_euid && starting_euid) {
			assert_no_err(seteuid(starting_euid));
		}

		if (!skip_test) {
			bool test_failed = test->t_func(TEST_DIR, bsize);
			if (test_failed) {
				printf("[FAIL] %s\n", test->t_name);
				suite_failed = true;
			} else {
				printf("[PASS] %s\n", test->t_name);
			}
		}

		if (test_name && !strcmp(test->t_name, test_name)) {
			test_name = NULL;
			break;
		}
	}

	if (test_name != NULL) {
		printf("No test named %s was found.\n", test_name);
		suite_failed = true;
	}

	// Remove our test directory at the end.
	(void)removefile(TEST_DIR, NULL, REMOVEFILE_RECURSIVE);

	return suite_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
