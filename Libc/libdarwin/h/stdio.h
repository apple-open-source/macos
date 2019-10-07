/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

/*!
 * @header
 * Non-standard, Darwin-specific additions for the stdio(3) family of APIs.
 */
#ifndef __DARWIN_STDIO_H
#define __DARWIN_STDIO_H

#include <os/base.h>
#include <os/api.h>
#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

// TAPI and the compiler don't agree about header search paths, so if TAPI found
// our header in the SDK, help it out.
#if DARWIN_TAPI && DARWIN_API_VERSION < 20180727
#define DARWIN_API_AVAILABLE_20180727
#endif

__BEGIN_DECLS;

/*!
 * @typedef os_fd_t
 * A type alias for a file descriptor.
 */
typedef int os_fd_t;

/*!
 * @function os_fd_valid
 * Returns whether the given integer is a valid file descriptor number.
 *
 * @param fd
 * The integer to check.
 *
 * @result
 * A Boolean indicating whether the integer is a valid file descriptor number,
 * that is, greater than or equal to zero.
 */
DARWIN_API_AVAILABLE_20180727
OS_ALWAYS_INLINE OS_WARN_RESULT
static inline bool
os_fd_valid(os_fd_t fd)
{
	return (fd >= STDIN_FILENO);
}

/*!
 * @function fcheck_np
 * Checks the status of an fread(3) or fwrite(3) operation to a FILE.
 *
 * @param f
 * The file on which the operation was performed.
 *
 * @param n
 * The return value of the operation.
 *
 * @param expected
 * The expected return value of the operation.
 *
 * @result
 * One of the following integers:
 *
 *     0     The operation succeeded
 *     EOF   The operation encountered the end of the FILE stream before it
 *           could complete
 *     1     There was an error
 */
DARWIN_API_AVAILABLE_20180727
OS_EXPORT OS_WARN_RESULT OS_NONNULL1
int
fcheck_np(FILE *f, size_t n, size_t expected);

/*!
 * @function dup_np
 * Variant of dup(2) that guarantees the dup(2) operation will either succeed or
 * not return.
 *
 * @param fd
 * The descriptor to dup(2).
 *
 * @result
 * A new file descriptor number that is functionally equivalent to what the
 * caller passed.
 *
 * @discussion
 * The implementation will retry if the operation was interrupted by a signal.
 * If the operation failed for any other reason, the implementation will
 * terminate the caller.
 */
DARWIN_API_AVAILABLE_20180727
OS_EXPORT OS_WARN_RESULT
os_fd_t
dup_np(os_fd_t fd);

/*!
 * @function zsnprintf_np
 * snprintf(3) variant which returns the numnber of bytes written less the null
 * terminator.
 *
 * @param buff
 * The buffer in which to write the string.
 *
 * @param len
 * The length of the buffer.
 *
 * @param fmt
 * The printf(3)-like format string.
 *
 * @param ...
 * The arguments corresponding to the format string.
 *
 * @result
 * The number of bytes written into the buffer, less the null terminator. This
 * routine is useful for successive string printing that may be lossy, as it
 * will simply return zero when there is no space left in the destination
 * buffer, i.e. enables the following pattern:
 *
 * char *cur = buff;
 * size_t left = sizeof(buff);
 * for (i = 0; i < n_strings; i++) {
 *     size_t n_written = zsnprintf_np(buff, left, "%s", strings[i]);
 *     cur += n_written;
 *     left -= n_written;
 * }
 *
 * This loop will safely terminate without any special care since, as soon as
 * the buffer's space is exhausted, all further calls to zsnprintf_np() will
 * write nothing and return zero.
 */
DARWIN_API_AVAILABLE_20170407
OS_EXPORT OS_WARN_RESULT OS_NONNULL1 OS_NONNULL3 OS_FORMAT_PRINTF(3, 4)
size_t
zsnprintf_np(char *buff, size_t len, const char *fmt, ...);

/*!
 * @function crfprintf_np
 * fprintf(3) variant that appends a new line character to the output.
 *
 * @param f
 * The file to which the output should be written.
 *
 * @param fmt
 * The printf(3)-like format string.
 *
 * @param ...
 * The arguments corresponding to the format string.
 */
DARWIN_API_AVAILABLE_20181020
OS_EXPORT OS_NONNULL1 OS_NONNULL2 OS_FORMAT_PRINTF(2, 3)
void
crfprintf_np(FILE *f, const char *fmt, ...);

/*!
 * @function vcrfprintf_np
 * vfprintf(3) variant that appends a new line character to the output.
 *
 * @param f
 * The file to which the output should be written.
 *
 * @param fmt
 * The printf(3)-like format string.
 *
 * @param ap
 * The argument list corresponding to the format string.
 */
DARWIN_API_AVAILABLE_20181020
OS_EXPORT OS_NONNULL1 OS_NONNULL2 OS_NONNULL3
void
vcrfprintf_np(FILE *f, const char *fmt, va_list ap);

/*!
 * @function wfprintf_np
 * fprintf(3) variant which wraps the output to the specified column width,
 * inserting new lines as necessary. Output will be word-wrapped with a trivial
 * algorithm.
 *
 * @param f
 * The file to which the output should be written.
 *
 * @param initpad
 * The number of spaces that should be inserted prior to the first line of
 * output. If a negative value is given, the implementation will assume that an
 * amount of spaces equal to the absolute value of the parameter has already
 * been written, and therefore it will only use the parameter to compute line-
 * wrapping information and not insert any additional spaces on the first line
 * of output.
 *
 * @param pad
 * The number of spaces that should be inserted prior to every line of output
 * except the first line.
 *
 * @param width
 * The maximum number of columns of each line of output. Pass zero to indicate
 * that there is no maximum.
 *
 * @param fmt
 * The printf(3)-like format string.
 *
 * @param ...
 * The arguments corresponding to the format string.
 *
 * @discussion
 * This routine will silently fail to print to the desired output stream if
 * there was a failure to allocate heap memory.
 */
DARWIN_API_AVAILABLE_20181020
OS_EXPORT OS_NONNULL1 OS_NONNULL5 OS_NONNULL6
void
wfprintf_np(FILE *f, ssize_t initpad, size_t pad, size_t width,
		const char *fmt, ...);

/*!
 * @function vwfprintf_np
 * vfprintf(3) variant which wraps the output to the specified column width,
 * inserting new lines as necessary. Output will be word-wrapped with a trivial
 * algorithm.
 *
 * @param f
 * The file to which the output should be written.
 *
 * @param initpad
 * The number of spaces that should be inserted prior to the first line of
 * output. If a negative value is given, the implementation will assume that an
 * amount of spaces equal to the absolute value of the parameter has already
 * been written, and therefore it will only use the parameter to compute line-
 * wrapping information and not insert any additional spaces on the first line
 * of output.
 *
 * @param pad
 * The number of spaces that should be inserted prior to every line of output
 * except the first line.
 *
 * @param width
 * The maximum number of columns of each line of output. Pass zero to indicate
 * that there is no maximum.
 *
 * @param fmt
 * The printf(3)-like format string.
 *
 * @param ap
 * The argument list corresponding to the format string.
 *
 * @discussion
 * This routine will silently fail to print to the desired output stream if
 * there was a failure to allocate heap memory.
 */
DARWIN_API_AVAILABLE_20181020
OS_EXPORT OS_NONNULL1 OS_NONNULL5 OS_NONNULL6
void
vwfprintf_np(FILE *f, ssize_t initpad, size_t pad, size_t width,
		const char *fmt, va_list ap);

__END_DECLS;

#endif // __DARWIN_STDIO_H
