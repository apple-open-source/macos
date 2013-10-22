//
//  der_date.h
//  utilities
//
//  Created by Michael Brouwer on 7/9/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _UTILITIES_DER_DATE_H_
#define _UTILITIES_DER_DATE_H_

#include <CoreFoundation/CoreFoundation.h>

const uint8_t* der_decode_generalizedtime_body(CFAbsoluteTime *at, CFErrorRef *error,
                                               const uint8_t* der, const uint8_t *der_end);
const uint8_t* der_decode_universaltime_body(CFAbsoluteTime *at, CFErrorRef *error,
                                             const uint8_t* der, const uint8_t *der_end);

size_t der_sizeof_generalizedtime(CFAbsoluteTime at, CFErrorRef *error);
uint8_t* der_encode_generalizedtime(CFAbsoluteTime at, CFErrorRef *error,
                                    const uint8_t *der, uint8_t *der_end);

size_t der_sizeof_generalizedtime_body(CFAbsoluteTime at, CFErrorRef *error);
uint8_t* der_encode_generalizedtime_body(CFAbsoluteTime at, CFErrorRef *error,
                                         const uint8_t *der, uint8_t *der_end);

#endif /* _UTILITIES_DER_DATE_H_ */
