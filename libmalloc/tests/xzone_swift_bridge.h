#ifndef __XZONE_SWIFT_BRIDGE_H__
#define __XZONE_SWIFT_BRIDGE_H__

#import <Foundation/Foundation.h>

#include <stdint.h>

void
validate_swift_bucketing(uint64_t ptrs);

void
validate_obj_array(NSArray *a);

#endif // __XZONE_SWIFT_BRIDGE_H__
