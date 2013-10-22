//
//  comparison.h
//  utilities
//
//  Created by Mitch Adler on 6/14/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef utilities_comparison_h
#define utilities_comparison_h

#define MIN(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a <= _b ? _a : _b; })
#define MAX(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a >= _b ? _a : _b; })

uint64_t constant_memcmp(const uint8_t *first, const uint8_t *second, size_t count);

#endif
