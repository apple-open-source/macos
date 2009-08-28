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
//
//  BlockLifetime.h
//  auto
//
//  Created by Josh Behnke on 5/19/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "AutoTestScript.h"

/*
 BlockLifetime implements the following test scenario:
 Allocate a test block.
 Perform full collection.
 Verify test block was scanned exactly once.
 Verify test block remained local.
 Assign test block to a global variable.
 Verify test block became global.
 Clear global variable, assign test block to ivar.
 Loop:
    run generational collection
    verify block is scanned exactly once and age decrements by 1
 until test block age becomes 0.
 Run a full collection (to clear write barriers).
 Verify block is scanned exactly once.
 Run a generational collection.
 Verify block *not* scanned.
 Clear test block ivar.
 Run generational collection.
 Verify block *not* scanned and *not* collected.
 Run a full collection.
 Verify block was collected.
 */

@interface BlockLifetime : AutoTestScript {
    BOOL _becameGlobal;
    BOOL _scanned;
    BOOL _collected;
    BOOL _shouldCollect;
    uint32_t _age;
    id _testBlock;
    vm_address_t _disguisedTestBlock;
    AutoTestSynchronizer *_testThreadSynchronizer;
}

@end
