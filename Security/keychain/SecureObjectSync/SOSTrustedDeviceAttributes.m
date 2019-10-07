//
//  SOSTrustedDeviceAttributes.m
//  Security
//
//

#import "SOSTrustedDeviceAttributes.h"


@implementation SOSTrustedDeviceAttributes

+ (BOOL)supportsSecureCoding { return YES; }

- (instancetype)initWithCoder:(NSCoder *)aDecoder {
    if(self = [super init]) {
        _machineID = [aDecoder decodeObjectOfClass:[NSString class] forKey:MACHINEID];
        _serialNumber = [aDecoder decodeObjectOfClass:[NSString class] forKey:SERIALNUMBER];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_machineID forKey:MACHINEID];
    [coder encodeObject:_serialNumber forKey:SERIALNUMBER];
}

@end
