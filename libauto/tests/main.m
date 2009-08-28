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
/* this file #imported into test programs */

#include "lookfor.c"

int main(int argc, char *argv[]) {
    global = [[TestObject alloc] init];
    
    for (int i = 0; i < 101; ++i) [[TestObject alloc] init];
    dup2tmpfile(argv[0]);
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    if (localDidFinalize == 0) {
        printf("%s: Whoops, no locals were finalized!!\n", argv[0]);
        ++Errors;
    }
    else if (!lookfor(what)) {
        printf("%s: *** no resurrection warning for stack local garbage stored into %s\n", argv[0], where);
        ++Errors;
    }
    
    for (int i = 0; i < 101; ++i) [[[TestObject alloc] init] makeGlobal];
    dup2tmpfile(argv[0]);
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    if (globalDidFinalize == 0) {
        printf("%s: Whoops, no globals were finalized!!\n", argv[0]);
        ++Errors;
    }
    else if (!lookfor(what)) {
        printf("%s: *** no resurrection warning for isGlobal garbage stored into %s\n", argv[0], where);
        ++Errors;
    }

    if (Errors == 0) {
        printf("%s: Success!\n", argv[0]);
        unlink(tmpfilename);
    }
    else {
        printf("%s: look in %s for details\n", argv[0], tmpfilename);
    }
    return Errors;
    
}
