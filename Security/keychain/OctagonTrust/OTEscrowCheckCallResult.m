/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import "OTEscrowTranslation.h"
#import "OTEscrowCheckCallResult.h"

@implementation OTEscrowCheckCallResult

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OTEscrowCheckCallResult:"
            " needsReenroll: %@,"
            " octagonTrusted: %@,"
            " moveRequest? %@,"
            " secureTermsNeeded? %@,"
            " repairReason: %ld,"
            " repairDisabled: %@>",
            self.needsReenroll ? @"YES" : @"NO",
            self.octagonTrusted ? @"YES" : @"NO",
            self.secureTermsNeeded ? @"YES" : @"NO",
            self.moveRequest,
            self.repairReason,
            self.repairDisabled ? @"YES" : @"NO"
    ];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _moveRequest = [coder decodeObjectOfClass:[OTEscrowMoveRequestContext class] forKey:@"moveRequest"];
        _needsReenroll = [coder decodeBoolForKey:@"needsReenroll"];
        _octagonTrusted = [coder decodeBoolForKey:@"octagonTrusted"];
        _secureTermsNeeded = [coder decodeBoolForKey:@"secureTermsNeeded"];
        _repairReason = [coder decodeIntegerForKey:@"repairReason"];
        _repairDisabled = [coder decodeBoolForKey:@"repairDisabled"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeBool:self.needsReenroll forKey:@"needsReenroll"];
    [coder encodeBool:self.octagonTrusted forKey:@"octagonTrusted"];
    [coder encodeBool:self.secureTermsNeeded forKey:@"secureTermsNeeded"];
    [coder encodeObject:self.moveRequest forKey:@"moveRequest"];
    [coder encodeInteger:self.repairReason forKey:@"repairReason"];
    [coder encodeBool:self.repairDisabled forKey:@"repairDisabled"];
}

- (NSDictionary*)dictionaryRepresentation {
    NSMutableDictionary *ret = [[NSMutableDictionary alloc] init];
    
    ret[@"needsReenroll"] = @(self.needsReenroll);
    ret[@"octagonTrusted"] = @(self.octagonTrusted);
    ret[@"secureTermsNeeded"] = @(self.secureTermsNeeded);
    if (self.moveRequest != nil) {
        ret[@"moveRequest"] = [self.moveRequest dictionaryRepresentation];
    }
    ret[@"repairReason"] = @(self.repairReason);
    ret[@"repairDisabled"] = @(self.repairDisabled);
    return ret;
}

@end
