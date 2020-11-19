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

#ifndef _METRICS_H_
#define _METRICS_H_

#include <sys/time.h>
#include <stdio.h>

// mtree error logging
enum mtree_result {
	SUCCESS = 0,
	WARN_TIME = -1,
	WARN_USAGE = -2,
	WARN_CHECKSUM = -3,
	WARN_MISMATCH = -4,
	WARN_UNAME = -5,
	/* Could also be a POSIX errno value */
};

void set_metrics_file(FILE *file);
void set_metric_start_time(time_t time);
void set_metric_path(char *path);
#define RECORD_FAILURE(location, error) mtree_record_failure(location, error)
void mtree_record_failure(int location, int code);
void print_metrics_to_file(void);

#endif /* _METRICS_H_ */
