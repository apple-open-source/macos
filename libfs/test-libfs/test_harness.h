#ifndef test_harness_c
#define test_harness_c

#include <sys/types.h>
#include <stdio.h>

int maketest(char *answer, size_t block_size,
			 off_t start_addr, size_t length);

#endif /* test_harness_c */
