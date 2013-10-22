//
//  comparison.c
//  utilities
//
//  Created by Keith Henrickson on 7/1/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include "comparison.h"

uint64_t constant_memcmp(const uint8_t *first, const uint8_t *second, size_t count) {
    uint64_t error_counter = 0;
    for (size_t counter = 0; counter < count; counter++) {
        error_counter |= first[counter] ^ second[counter];
    }
    return error_counter;
}
