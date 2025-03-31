//
//  trustd_objc_helpers.h
//  Security
//
//

#ifndef trustd_objc_helpers_h
#define trustd_objc_helpers_h

static inline bool isNSString(id nsType) {
    return nsType && [nsType isKindOfClass:[NSString class]];
}

static inline bool isNSNumber(id nsType) {
    return nsType && [nsType isKindOfClass:[NSNumber class]];
}

static inline bool isNSDate(id nsType) {
    return nsType && [nsType isKindOfClass:[NSDate class]];
}

static inline bool isNSData(id nsType) {
    return nsType && [nsType isKindOfClass:[NSData class]];
}

static inline bool isNSArray(id nsType) {
    return nsType && [nsType isKindOfClass:[NSArray class]];
}

static inline bool isNSDictionary(id nsType) {
    return nsType && [nsType isKindOfClass:[NSDictionary class]];
}

#endif /* trustd_objc_helpers_h */
