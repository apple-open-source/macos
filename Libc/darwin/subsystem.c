/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <subsystem.h>
#include <sys/errno.h>
#include <sys/syslimits.h>
#include <_simple.h>

#define SUBSYSTEM_ROOT_PATH_KEY "subsystem_root_path"
#define SUBSYSTEM_ROOT_PATH_DELIMETER ':'
#define MAX_SUBSYSTEM_ROOT_PATHS 8

void _subsystem_init(const char *apple[]);

static const char * subsystem_root_path = NULL;
static size_t subsystem_root_path_len = 0;

/*
 * Takes the apple array, and initializes subsystem
 * support in Libc.
 */
void
_subsystem_init(const char **apple)
{
	const char * subsystem_root_path_string = _simple_getenv(apple, SUBSYSTEM_ROOT_PATH_KEY);
	if (subsystem_root_path_string) {
		subsystem_root_path = subsystem_root_path_string;
		subsystem_root_path_len = strnlen(subsystem_root_path, PATH_MAX);
	}
}

/*
 * Takes a buffer containing a subsystem root path and a file path, and constructs the
 * subsystem path for the given file path.  Assumes that the subsystem root
 * path will be "/" terminated.
 */
static bool
append_path_to_subsystem_root(char * buf, size_t buf_size, const char * file_path)
{
	size_t concatenated_length = strlcat(buf, file_path, buf_size);

	if (concatenated_length >= buf_size) {
		return false;
	}

	return true;
}

/*
 * Takes a pointer to a part of the subsystem root path string and extracts
 * the next viable path (from the start point to the NULL terminator or next ':'
 * character). Populates this path into the provided buffer. Returns a pointer to
 * the next root path if there is a following one, NULL if this was the last path.
 *
 * Note: It's not possible to use straight strsep(3) or similar because the subsystem
 *       root path is immutable.
 */
static const char *
extract_next_subsystem_root_path(char *buf, size_t buf_size, const char *root_path_begin)
{
	bool found_delimeter = false;
	size_t root_path_len = strlen(root_path_begin);

	if (buf_size == 0) {
		return root_path_begin;
	}

	const char *next_delimeter = memchr(root_path_begin, (int)SUBSYSTEM_ROOT_PATH_DELIMETER,
										root_path_len);
	if (next_delimeter != NULL) {
		root_path_len = (size_t)(next_delimeter - root_path_begin);
		found_delimeter = true;
	}
	if (root_path_len > (buf_size - 1)) {
		// We either found a sub-path that was too long or there was only one
		// path in the string and that was too long.
		return NULL;
	}
	memcpy(buf, root_path_begin, root_path_len);
	buf[root_path_len] = '\0';

	// If we found a delimeter, this indicates there is another path
	// after the one we just extracted. Return a pointer to the beginning
	// of this path (which is the character after the delimeter). Otherwise
	// that was the last component in the paths.
	if (found_delimeter) {
		return (next_delimeter + 1);
	} else {
		return NULL;
	}
}

int
open_with_subsystem(const char * path, int oflag)
{
	/* Don't support file creation. */
	if (oflag & O_CREAT){
		errno = EINVAL;
		return -1;
	}

	int result;

	result = open(path, oflag);

	if ((result < 0) && (errno == ENOENT) && (subsystem_root_path)) {
		/*
		 * If the file doesn't exist relative to root, search
		 * for it relative to the provided subsystem root paths.
		 */
		const char *next_subsystem_root_path_begin = subsystem_root_path;
		do {
			char constructed_path[PATH_MAX];
			next_subsystem_root_path_begin = extract_next_subsystem_root_path(constructed_path, sizeof(constructed_path), next_subsystem_root_path_begin);

			if (append_path_to_subsystem_root(constructed_path, sizeof(constructed_path), path)) {
				result = open(constructed_path, oflag);
				if (result >= 0) {
					break;
				} else if (errno == ENOENT) {
					continue;
				}
			} else {
				errno = ENAMETOOLONG;
				break;
			}
		} while (next_subsystem_root_path_begin != NULL);
	}

	return result;
}

int
stat_with_subsystem(const char *restrict path, struct stat *restrict buf)
{
	int result;

	result = stat(path, buf);

	if ((result < 0) && (errno == ENOENT) && (subsystem_root_path)) {
		/*
		 * If the file doesn't exist relative to root, search
		 * for it relative to the provided subsystem root paths.
		 */
		const char *next_subsystem_root_path_begin = subsystem_root_path;
		do {
			char constructed_path[PATH_MAX];
			next_subsystem_root_path_begin = extract_next_subsystem_root_path(constructed_path, sizeof(constructed_path), next_subsystem_root_path_begin);

			if (append_path_to_subsystem_root(constructed_path, sizeof(constructed_path), path)) {
				result = stat(constructed_path, buf);
				if (result >= 0) {
					break;
				} else if (errno == ENOENT) {
					continue;
				}
			} else {
				errno = ENAMETOOLONG;
				break;
			}
		} while (next_subsystem_root_path_begin != NULL);
	}

	return result;
}

