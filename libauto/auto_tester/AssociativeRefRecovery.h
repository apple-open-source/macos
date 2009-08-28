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
//  AssociativeRefRecovery.h
//  auto
//
//  Created by Josh Behnke on 8/28/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "AutoTestScript.h"

/*
 This test verifies that associatied blocks are reclaimed along with the main block.
 It tests a Foundation object, bridged CF object, and a non-object block.
 */

@interface AssociativeRefRecovery : AutoTestScript {
    vm_address_t _foundation_ref;
    bool _foundation_ref_collected;
    vm_address_t _cf_bridged_ref;
    bool _cf_bridged_ref_collected;
    vm_address_t _non_object;
    bool _non_object_ref_collected;
    AutoTestSynchronizer *_sync;
}

@end
