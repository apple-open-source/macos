/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#include <stdlib.h>
#include <string.h>

#include "metrics.h"

#define MAX_WARNINGS_LOGGED 5
#define MAX_ERRORS_LOGGED 5
#define WARN_FIRST -1

#ifndef ROUNDUP
#define ROUNDUP(COUNT, MULTIPLE) ((((COUNT) + (MULTIPLE) - 1) / (MULTIPLE)) * (MULTIPLE))
#endif

typedef struct failure_info {
	int location;
	int code;
} failure_info_t;

typedef struct metrics {
	FILE *file;
	time_t start_time;
	int warning_count;
	failure_info_t warnings[MAX_WARNINGS_LOGGED];
	int error_count;
	failure_info_t errors[MAX_ERRORS_LOGGED];
	int last_error_location;
	char *path;
	int result;
} metrics_t;
metrics_t metrics = {};

void
set_metrics_file(FILE *file)
{
	metrics.file = file;
}

void
set_metric_start_time(time_t time)
{
	metrics.start_time = time;
}

void
set_metric_path(char *path)
{
	metrics.path = strdup(path);
}

void
mtree_record_failure(int location, int code)
{
	if (code <= WARN_FIRST) {
		if (metrics.warning_count < MAX_WARNINGS_LOGGED) {
			metrics.warning_count++;
		} else {
			// Shift up the warnings to make space for the latest one.
			for (int index = 0; index < MAX_ERRORS_LOGGED - 1; index++) {
				metrics.warnings[index] = metrics.warnings[index + 1];
			}
		}
		metrics.warnings[metrics.warning_count - 1].location = location;
		metrics.warnings[metrics.warning_count - 1].code = code;
	} else {
		int error_index = -1;
		if (metrics.error_count <= MAX_ERRORS_LOGGED) {
			if (metrics.error_count > 0) {
				// Log all but the last error which occured in the location and
				// code arrays. The last (location, error) is logged in
				// (metrics.last_error_location, metrics.error)
				error_index = metrics.error_count - 1;
			}
			metrics.error_count++;
		} else {
			// Shift up the errors to make space for the latest one.
			for (int index = 0; index < MAX_ERRORS_LOGGED - 1; index++) {
				metrics.errors[index] = metrics.errors[index + 1];
			}
			error_index = MAX_ERRORS_LOGGED - 1;
		}
		if (error_index >= 0) {
			metrics.errors[error_index].location = metrics.last_error_location;
			metrics.errors[error_index].code = metrics.result;
		}
		metrics.last_error_location = location;
		metrics.result = code;
	}
}
/*
 * Note on format of metric string
 * 1) dev points to the path
 * 2) result is the overall result code from mtree
 * 3) warnings and errors (upto 5 each) are printed in the format :
 *	w:(location1:code1),(location2:code2).... and
 *	e:(location1:code1),(location2:code2).... respectively.
 * 4) fl is the last failure location of the run which is 0 if there is no failure
 * 5) time is the total time taken for the run
 */
void
print_metrics_to_file(void)
{
	if (metrics.file == NULL) {
		return;
	}

	fprintf(metrics.file, "dev=%s result=%d ",
		metrics.path ? metrics.path : "", metrics.result);
	if (metrics.warning_count) {
		fprintf(metrics.file, "w:");
		for (int index = 0; index < metrics.warning_count; index++) {
			fprintf(metrics.file, "(%d:%d)",
				metrics.warnings[index].location, metrics.warnings[index].code);
		}
		fprintf(metrics.file, " ");
	}
	if (metrics.error_count > 1) {
		fprintf(metrics.file, "e:");
		for (int index = 0; index < metrics.error_count - 1; index++) {
			fprintf(metrics.file, "(%d:%d)",
				metrics.errors[index].location, metrics.errors[index].code);
		}
		fprintf(metrics.file, " ");
	}
	fprintf(metrics.file, "fl=%d time=%ld\n",
		metrics.last_error_location, ROUNDUP((time(NULL) - metrics.start_time), 60) / 60);

	fclose(metrics.file);
	if (metrics.path) {
		free(metrics.path);
		metrics.path = NULL;
	}
}
