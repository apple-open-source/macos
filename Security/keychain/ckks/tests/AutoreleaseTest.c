/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dispatch/dispatch.h>
#include <objc/objc-internal.h>

#include "AutoreleaseTest.h"

static void
read_releases_pending(int fd, void (^handler)(ssize_t))
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        ssize_t result = -1;

        FILE *fp = fdopen(fd, "r");

        char *line = NULL;
        size_t linecap = 0;
        while (getline(&line, &linecap, fp) > 0) {
            ssize_t pending;

            if (sscanf(line, "objc[%*d]: %ld releases pending", &pending) == 1) {
                result = pending;
                break;
            }
        }
        free(line);

        fclose(fp);

        handler(result);
    });
}

ssize_t
pending_autorelease_count(void)
{
    __block ssize_t result = -1;
    dispatch_semaphore_t sema;
    int fds[2];
    int saved_stderr;

    // stderr replacement pipe
    pipe(fds);
    fcntl(fds[1], F_SETNOSIGPIPE, 1);

    // sead asynchronously - takes ownership of fds[0]
    sema = dispatch_semaphore_create(0);
    read_releases_pending(fds[0], ^(ssize_t pending) {
        result = pending;
        dispatch_semaphore_signal(sema);
    });

    // save and replace stderr
    saved_stderr = dup(STDERR_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);

    // make objc print the current autorelease pool
    _objc_autoreleasePoolPrint();

    // restore stderr
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);

    // wait for the reader
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
#if !__has_feature(objc_arc)
    dispatch_release(sema);
#endif

    return result;
}
