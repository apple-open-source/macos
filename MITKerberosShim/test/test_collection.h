/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * 1) Create an test abstraction layer for integration into other systems.
 * 2) Create a simplified way to collect and report test results.
 * 3) Low impact in the test source.
 *
 *
 * Convenience functions:
 * ----------------------
 * test_collection_t *tests_init_and_start(const char*);
 * int tests_stop_and_free(test_collection_t*);
 * time_t tests_start_timer(test_collection_t*);
 * time_t tests_stop_timer(test_collection_t*);
 *
 * Test status functions:
 * ----------------------
 * void test_passed(test_collection_t*, const char*);
 * void test_failed(test_collection_t*, const char*, const char*, ...)
 * int test_evaluate(test_collection_t*, const char*, int, const char*, ...)
 *
 * Setting library options:
 * ------------------------
 * uint32_t tests_set_flags(test_collection_t*, uint32_t);
 * uint32_t tests_unset_flags(test_collection_t*, uint32_t);
 *
 * Other important functions:
 * --------------------------
 * size_t tests_set_total_count_hint(test_collection_t*, size_t);
 * int tests_return_value(const test_collection_t*);
 * double tests_duration(test_collection_t*);
 *
 *
 */

#include <inttypes.h>
#include <stdarg.h>
#include <time.h>

#if !defined(_TEST_COLLECTION_H_)
#define _TEST_COLLECTION_H_

/* Possible test statuses.  */
enum test_collection_return_values {
	TC_TESTS_PASSED = 0,
	TC_TESTS_FAILED
};

/* Maximum string length for name. */
#define TC_NAME_MAX_LENGTH 512

/* Available flags. */
#define TC_FLAG_NONE                0u
#define TC_FLAG_EXIT_ON_FAILURE (1u<<1)
#define TC_FLAG_SUMMARY_ON_STOP (1u<<2)

#define TC_FLAG_DEFAULTS (TC_FLAG_SUMMARY_ON_STOP)

/* Structure representing a collections of tests. */
typedef struct _struct_test_collection_t {
	char *name;
	size_t failed_count;
	size_t passed_count;
	size_t total_count_hint;
	time_t start_time;
	time_t stop_time;
	uint32_t flags;
} test_collection_t;

test_collection_t *tests_init_and_start(const char*);
int tests_stop_and_free(test_collection_t*);

void test_passed(test_collection_t*, const char*);
void test_failed(test_collection_t*, const char*, const char*, ...)
	__attribute__((format(printf, 2, 4)));
int test_evaluate(test_collection_t*, const char*, int);
int vtest_evaluate(test_collection_t*, const char*, int, const char*, ...)
	__attribute__((format(printf, 4, 5)));


test_collection_t *test_collection_init(const char*);
void test_collection_free(test_collection_t*);

int tests_return_value(const test_collection_t*);

time_t tests_start_timer(test_collection_t*);
time_t tests_stop_timer(test_collection_t*);
time_t tests_get_start_time(test_collection_t*);
time_t tests_set_start_time(test_collection_t*, time_t);
time_t tests_get_stop_time(test_collection_t*);
time_t tests_set_stop_time(test_collection_t*, time_t);
double tests_duration(test_collection_t*);

uint32_t tests_get_flags(const test_collection_t*);
uint32_t tests_set_flags(test_collection_t*, uint32_t);
uint32_t tests_unset_flags(test_collection_t*, uint32_t);

size_t tests_get_total_count_hint(const test_collection_t*);
size_t tests_set_total_count_hint(test_collection_t*, size_t);

char *tests_get_name(const test_collection_t*);
char *tests_set_name(test_collection_t*, const char*);

#endif /* _TEST_COLLECTION_H_ */

