/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
// this file is #included by others

#include <libgen.h>

char tmpfilename[512];

void dup2tmpfile(const char *name) {
    if (tmpfilename[0] == 0) {
        if (strlen(name) > 500) {
            printf("Whoops, name too long!!!\n");
            exit(1);
        }
        sprintf(tmpfilename, "/private/tmp/%d-%s", getpid(), basename((char *)name));
    }
    close(2);
    int result = creat(tmpfilename, 0777);
    if (result != 2) {
        printf("Whoops, didn't takeover error fd!!!, got %d when creating '%s'\n", result, tmpfilename);
        exit(1);
    }
}

bool lookfor(const char *errormsg) {
    char buffer[512];
    buffer[0] = 0;
    int fd = open(tmpfilename, 0);
    if (fd < 0) {
        printf("Whoops, can't open for reading %s\n", tmpfilename);
        exit(1);
    }
    int count = read(fd, buffer, 512);
    if (count < 0) {
        printf("Whoops, can't read tmpfilename\n");
        exit(1);
    }
    close(fd);
    if (strlen(errormsg) == 0) 
        return strlen(buffer) == 0;
    if (strstr(buffer, errormsg)) return true;
    printf("didn't find '%s' in <<<%s>>>\n", errormsg, buffer);
    return false;
}
