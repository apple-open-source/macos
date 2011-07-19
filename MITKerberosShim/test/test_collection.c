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

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "test_collection.h"

void vtest_failed(test_collection_t*, const char*, const char*, va_list);

test_collection_t *
test_collection_init(const char *name)
{
	test_collection_t *tt;
	
	tt = malloc(sizeof(test_collection_t));
	assert(NULL != tt);
	bzero(tt, sizeof(test_collection_t));

	tests_set_name(tt, name);
	tt->flags = TC_FLAG_DEFAULTS;

	return tt;
}

void
test_collection_free(test_collection_t *tt)
{
	if (NULL != tt)
	{
		if (NULL != tt->name)
			free(tt->name);
		free(tt);
	}
}

size_t
tests_set_total_count_hint(test_collection_t *tt, size_t total)
{
	assert(NULL != tt);
	return (tt->total_count_hint = total);
}

size_t
tests_get_total_count_hint(const test_collection_t *tt)
{
	assert(NULL != tt);
	return tt->total_count_hint;
}

uint32_t
tests_get_flags(const test_collection_t *tt)
{
	assert(NULL != tt);
	return tt->flags;
}

uint32_t
tests_set_flags(test_collection_t *tt, uint32_t flags)
{
	assert(NULL != tt);
	return (tt->flags |= flags);
}

uint32_t
tests_unset_flags(test_collection_t *tt, uint32_t flags)
{
	assert(NULL != tt);
	return (tt->flags &= ~flags);
}

char *
tests_set_name(test_collection_t *tt, const char *name)
{
	assert(NULL != tt);
	if (NULL != name)
	{
		size_t len = strnlen(name, TC_NAME_MAX_LENGTH)+1;
		tt->name = calloc(len, sizeof(char));
		strlcpy(tt->name, name, len);
	}

	return tt->name;
}

char *
tests_get_name(const test_collection_t *tt)
{
	assert(NULL != tt);
	return tt->name;
}

test_collection_t *
tests_init_and_start(const char *name)
{
	test_collection_t *tt;
	tt = test_collection_init(name);
	assert(NULL != tt);

	tests_start_timer(tt);

	return tt;
}

int
tests_stop_and_free(test_collection_t *tt)
{
	int ret = TC_TESTS_FAILED;
	assert(NULL != tt);

	tests_stop_timer(tt);	
	ret = tests_return_value(tt);
	test_collection_free(tt);

	return ret;
}

time_t
tests_start_timer(test_collection_t *tt)
{
	assert(NULL != tt);

	tt->start_time = time(NULL);
	return tt->start_time;
}

time_t
tests_set_stop_time(test_collection_t *tt, time_t time)
{
	assert(NULL != tt);
	return (tt->stop_time = time);
}


time_t
tests_get_stop_time(test_collection_t *tt)
{
	assert(NULL != tt);
	return tt->stop_time;
}

time_t
tests_get_start_time(test_collection_t *tt)
{
	assert(NULL != tt);
	return tt->start_time;
}
time_t
tests_set_start_time(test_collection_t *tt, time_t time)
{
	assert(NULL != tt);
	return (tt->start_time = time);
}

time_t
tests_stop_timer(test_collection_t *tt)
{
	assert(NULL != tt);

	tt->stop_time = time(NULL);

	if (tt->flags&TC_FLAG_SUMMARY_ON_STOP)
	{
		int attempted = tt->passed_count + tt->failed_count;
		fprintf(stdout, "\nTotal passed: %zd\n", tt->passed_count);
		fprintf(stdout,   "Total FAILED: %zd\n", tt->failed_count);
		if ( attempted < tt->total_count_hint )
		{
			fprintf(stdout, "Total unattempted tests: %zd\n", 
				(tt->total_count_hint - attempted));
		}
		fprintf(stdout,   "Total time duration: %f\n\n", tests_duration(tt));
	}
	return tt->stop_time;
}

double
tests_duration(test_collection_t *tt)
{
	assert(NULL != tt);
	return difftime(tt->start_time, tt->stop_time);
}

int
tests_return_value(const test_collection_t *tt)
{
	assert(tt != NULL);
	return tt->failed_count > 0 ? TC_TESTS_FAILED : TC_TESTS_PASSED;
}

int
test_evaluate(test_collection_t *tt, const char *tname, int result)
{
	assert(NULL != tt);

	if (!result)
	{
		test_passed(tt, tname);
	} else {
		test_failed(tt, tname, "result = \"%d\"", result);
	}
	return result;
}

int
vtest_evaluate(test_collection_t *tt, const char *tname, int result, const char *format, ...)
{
	assert(NULL != tt);

	if (!result)
	{
		test_passed(tt, tname);
	} else {
		va_list args;
		va_start(args, format);
		vtest_failed(tt, tname, format, args);
		va_end(args);
	}
	return result;
}

void
test_passed(test_collection_t *tt, const char *tname)
{
	assert(NULL != tt);
	
	++(tt->passed_count);
	fprintf(stdout, "%s - %s: passed\n", tt->name, tname);
}

void
test_failed(test_collection_t *tt, const char *tname, const char *format, ...)
{
	va_list args;
	assert(NULL != tt);

	va_start(args, format);
	vtest_failed(tt, tname, format, args);
	va_end(args);
}

void
vtest_failed(test_collection_t *tt, const char *tname, const char *format, va_list args)
{
	assert(NULL != tt);

	++(tt->failed_count);

	fprintf(stderr, "%s - %s: FAILED ", tt->name, tname);
	vfprintf(stderr, format, args);
	if (tt->flags&TC_FLAG_EXIT_ON_FAILURE)
	{
		tests_stop_and_free(tt);
		exit(1);
	}
}

