#include "TribecaXPC.h"
#ifdef TRIBECA_XPC_HAS_AUTOERASE
#if TRIBECA_XPC_HAS_AUTOERASE

static NSString*
tribecaStateToString(TribecaState state){
	switch (state) {
		case kSecuritydTribecaStateUnknown:
			return @"Unknown";
		case kSecuritydTribecaStateDisabledForCurrentBoot:
			return @"DisabledForCurrentBoot";
		case kSecuritydTribecaStateEnabledLastRestore:
			return @"EnabledLastRestore";
		case kSecuritydTribecaStateEnabledAEToken:
			return @"EnabledAEToken";
		case kSecuritydTribecaStateEnabledLastRestoreInactiveUseChecking:
			return @"EnabledLastRestoreInactiveUseCheck";
	}
}

@implementation TribecaStatus

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@> State: %@ scheduledAutoerase: %@ lastRestore: %@ lastESCAuth: %@ aeTokenExpiry: %@ aeTokenIssuedAt: %@ inactiveUseExpiry: %@",
        NSStringFromClass([self class]),
        tribecaStateToString(self.state),
        self.scheduledAutoerase.description,
        self.lastRestore.description,
        self.lastESCAuth == NULL ? @"null" : self.lastESCAuth.description,
        self.aeTokenExpiry == NULL ? @"null" : self.aeTokenExpiry.description,
        self.aeTokenIssuedAt == NULL ? @"null" : self.aeTokenIssuedAt.description,
        self.inactiveUseExpiry == NULL ? @"null" : self.inactiveUseExpiry.description
    ];
}

- (instancetype _Nonnull)
    initWithState:(TribecaState)state
    scheduledAutoerase:(NSDate * _Nonnull)scheduledAutoerase
    lastRestore:(NSDate * _Nonnull)lastRestore
    inactiveUseExpiry:(NSDate * _Nullable)inactiveUseExpiry
    lastESCAuth:(NSDate * _Nullable)lastESCAuth
    aeTokenExpiry:(NSDate * _Nullable)aeTokenExpiry
    aeTokenIssuedAt:(NSDate * _Nullable)aeTokenIssuedAt
{
	if ((self = [super init])) {
        _state = state;
        _scheduledAutoerase = scheduledAutoerase;
        _lastRestore = lastRestore;
        _inactiveUseExpiry = inactiveUseExpiry;
        _lastESCAuth = lastESCAuth;
        _aeTokenExpiry = aeTokenExpiry;
        _aeTokenIssuedAt = aeTokenIssuedAt;
    }
    return self;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    [coder encodeInt:self.state forKey:@"state"];
    [coder encodeObject:self.scheduledAutoerase forKey:@"scheduledAutoerase"];
    [coder encodeObject:self.lastRestore forKey:@"lastRestore"];
    [coder encodeObject:self.inactiveUseExpiry forKey:@"inactiveUseExpiry"];
    [coder encodeObject:self.lastESCAuth forKey:@"lastESCAuth"];
    [coder encodeObject:self.aeTokenExpiry forKey:@"aeTokenExpiry"];
    [coder encodeObject:self.aeTokenIssuedAt forKey:@"aeTokenIssuedAt"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if (self = [super init]) {
        _state = [coder decodeIntForKey:@"state"];
        _scheduledAutoerase = [coder decodeObjectOfClass:[NSDate class] forKey:@"scheduledAutoerase"];
        _lastRestore = [coder decodeObjectOfClass:[NSDate class] forKey:@"lastRestore"];
        _inactiveUseExpiry = [coder decodeObjectOfClass:[NSDate class] forKey:@"inactiveUseExpiry"];
        _lastESCAuth = [coder decodeObjectOfClass:[NSDate class] forKey:@"lastESCAuth"];
        _aeTokenExpiry = [coder decodeObjectOfClass:[NSDate class] forKey:@"aeTokenExpiry"];
        _aeTokenIssuedAt = [coder decodeObjectOfClass:[NSDate class] forKey:@"aeTokenIssuedAt"];
    }
    return self;
}

@end

#endif /* if TRIBECA_XPC_HAS_AUTOERASE */
#endif /* ifdef TRIBECA_XPC_HAS_AUTOERASE */
