/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#if OCTAGON

#import "keychain/ckks/CloudKitCategories.h"

@implementation CKOperationGroup (CKKS)
+(instancetype) CKKSGroupWithName:(NSString*)name {
    CKOperationGroup* operationGroup = [[CKOperationGroup alloc] init];
    operationGroup.expectedSendSize = CKOperationGroupTransferSizeKilobytes;
    operationGroup.expectedReceiveSize = CKOperationGroupTransferSizeKilobytes;
    operationGroup.name = name;
    return operationGroup;
}
@end

@implementation NSError (CKKS)

-(bool) ckksIsCKErrorRecordChangedError {
    NSDictionary<CKRecordID*,NSError*>* partialErrors = self.userInfo[CKPartialErrorsByItemIDKey];
    if([self.domain isEqualToString:CKErrorDomain] && self.code == CKErrorPartialFailure && partialErrors) {
        // Check if this error was "you're out of date"

        for(NSError* error in partialErrors.objectEnumerator) {
            if((![error.domain isEqualToString:CKErrorDomain]) || (error.code != CKErrorBatchRequestFailed && error.code != CKErrorServerRecordChanged && error.code != CKErrorUnknownItem)) {
                // There's an error in there that isn't CKErrorServerRecordChanged, CKErrorBatchRequestFailed, or CKErrorUnknownItem. Don't handle nicely...
                return false;
            }
        }

        return true;
    }
    return false;
}

- (BOOL)isCuttlefishError:(CuttlefishErrorCode)cuttlefishErrorCode
{
    NSError *error = self;

    if ([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorServerRejectedRequest) {
        NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];

        if([underlyingError.domain isEqualToString:CKInternalErrorDomain] && underlyingError.code == CKErrorInternalPluginError) {
            NSError* cuttlefishError = underlyingError.userInfo[NSUnderlyingErrorKey];

            if([cuttlefishError.domain isEqualToString:CuttlefishErrorDomain] && cuttlefishError.code == cuttlefishErrorCode) {
                return YES;
            }
        }
    }
    return NO;
}

- (NSTimeInterval)cuttlefishRetryAfter {
    NSError *error = self;

    if ([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorServerRejectedRequest) {
        NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];

        if([underlyingError.domain isEqualToString:CKInternalErrorDomain] && underlyingError.code == CKErrorInternalPluginError) {
            NSError* cuttlefishError = underlyingError.userInfo[NSUnderlyingErrorKey];

            if([cuttlefishError.domain isEqualToString:CuttlefishErrorDomain]) {
                NSNumber* val = cuttlefishError.userInfo[CuttlefishErrorRetryAfterKey];
                if (val) {
                    return (NSTimeInterval)val.doubleValue;
                }
            }
        }
    }
    return 0;
}

@end

@implementation CKAccountInfo (CKKS)
// Ugly, and might break if CloudKit changes how they print objects. Sorry, CloudKit!
- (NSString*)description {
    NSString* ckprop = [self CKPropertiesDescription];
    NSString* description =  [NSString stringWithFormat: @"<CKAccountInfo: %@>", ckprop];
    return description;
}
@end

#endif //OCTAGON
