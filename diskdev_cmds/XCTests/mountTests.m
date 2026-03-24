/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
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

#import <XCTest/XCTest.h>

#include "mount_flags.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

#define BUFFER_SIZE 4096
int mount_main(int argc, const char *argv[]);

@interface mountTests : XCTestCase

@end

@implementation mountTests

static int
runMountWithOptions(int argc, const char *argv[], char *buffer, size_t buffer_size)
{
    pid_t pid;
    int pipefd[2];
    ssize_t bytes_read;
    size_t total_bytes_read = 0;
    
    if (pipe(pipefd) == -1) {
        XCTFail("pipe failed");
        return -1;
    }
    
    if ((pid = fork()) == -1) {
        XCTFail("fork failed");
        return -1;
    }
    
    if (pid == 0) { /* Child Process */
        /* The child will only write to the pipe */
        close(pipefd[0]);
        
        /* Redirect stdout to the write end of the pipe */
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            XCTFail("dup2 failed in child");
            exit(EXIT_FAILURE);
        }
        
        /* Close the original write end of the pipe */
        close(pipefd[1]);
        
        mount_main(argc, argv);
        exit(EXIT_SUCCESS);
    } else {  /* Parent Process */
        /* The parent will only read from the pipe */
        close(pipefd[1]);
        
        /* Read from the pipe until EOF */
        while ((bytes_read = read(pipefd[0], buffer + total_bytes_read, buffer_size - 1 - total_bytes_read)) > 0) {
            total_bytes_read += bytes_read;
            if (total_bytes_read >= buffer_size - 1) {
                XCTFail("buffer full");
                return -1;
            }
        }
        
        /* Check for read error */
        if (bytes_read == -1) {
            XCTFail("read failed");
            return -1;
        }
        
        /* Null-terminate the buffer to treat it as a string */
        buffer[total_bytes_read] = '\0';
        
        /* Close the read end of the pipe in the parent */
        close(pipefd[0]);
        
        /* Wait for the child process to terminate */
        int status;
        while ((waitpid(pid, &status, 0) == -1) && (errno == EINTR)) {
            usleep(1000);
        }
        
        /* Check child process exit status */
        if (WIFSIGNALED(status)) {
            XCTFail("aborted by signal %d", WTERMSIG(status));
            return 1;
        } else if (WIFSTOPPED(status)) {
            XCTFail("stopped by signal %d ?", WSTOPSIG(status));
            return 1;
        } else if (WEXITSTATUS(status)) {
            XCTFail("exited with status %d", WEXITSTATUS(status));
            return 1;
        }
    }
    
    return 0;
}

- (void)testMountOptionsNoFollow {
    int error;
    char buffer[BUFFER_SIZE];
    const char *argv[8] = { "mount", "-t", "nfs", "-o", "nofollow", "-d", "/private/tmp", "/private/nosuchdir"};
    
    error = runMountWithOptions(ARRAY_SIZE(argv), argv, buffer, sizeof(buffer));
    if (error) {
        XCTFail("runMountWithOptions failed");
        return;
    }
    
    XCTAssertNotEqual(strstr(buffer, "-o nofollow"), NULL, "Checking for the 'nofollow' option");
}

- (void)testMountOptionNoFollow {
    int error;
    char buffer[BUFFER_SIZE];
    const char *argv[7] = { "mount", "-t", "nfs", "-k", "-d", "/private/tmp", "/private/nosuchdir"};
    
    error = runMountWithOptions(ARRAY_SIZE(argv), argv, buffer, sizeof(buffer));
    if (error) {
        XCTFail("runMountWithOptions failed");
        return;
    }
    
    XCTAssertNotEqual(strstr(buffer, "-o nofollow"), NULL, "Checking for the 'nofollow' option");
}

@end
