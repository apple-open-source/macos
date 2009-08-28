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
/* test_bitmaps.cpp */

#include "AutoBitmap.h"
#include <stdio.h>
#include <assert.h>

using namespace Auto;

int main() {
    char bits[Bitmap::bytes_needed(64)];
    Bitmap bitmap(64, bits);
    bitmap.clear();
    
    assert(bitmap.bits_are_clear(0, bitmap.size_in_bits()));
    
    bitmap.set_bits(0, 16);
    bitmap.set_bits(48, 16);

    assert(!bitmap.bits_are_clear(0, 16));
    assert(bitmap.bits_are_clear(16, 32));
    assert(!bitmap.bits_are_clear(48, 16));
    
    return 0;
}
