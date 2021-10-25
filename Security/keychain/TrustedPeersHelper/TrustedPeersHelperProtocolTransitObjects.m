
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocolTransitObjects.h"

@implementation TrustedPeersHelperIntendedTPPBSecureElementIdentity

- (instancetype)initWithSecureElementIdentity:(TPPBSecureElementIdentity* _Nullable)secureElementIdentity
{
    if((self = [super init])) {
        _secureElementIdentity = secureElementIdentity;
    }
    return self;
}

+ (TrustedPeersHelperIntendedTPPBSecureElementIdentity*)intendedEmptyIdentity
{
    return [[TrustedPeersHelperIntendedTPPBSecureElementIdentity alloc] initWithSecureElementIdentity:nil];
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<TrustedPeersHelperIntendedTPPBSecureElementIdentity: %@>", self.secureElementIdentity];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _secureElementIdentity = [coder decodeObjectOfClass:[TPPBSecureElementIdentity class] forKey:@"identity"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.secureElementIdentity forKey:@"identity"];
}
@end
