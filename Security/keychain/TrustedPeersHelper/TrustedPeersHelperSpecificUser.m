
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"

@implementation TPSpecificUser

- (instancetype)initWithCloudkitContainerName:(NSString*)cloudkitContainerName
                             octagonContextID:(NSString*)octagonContextID
                               appleAccountID:(NSString*)appleAccountID
                                      altDSID:(NSString*)altDSID
                             isPrimaryPersona:(BOOL)isPrimaryPersona
                          personaUniqueString:(NSString* _Nullable)personaUniqueString
{
    if((self = [super init])) {
        _cloudkitContainerName = cloudkitContainerName;

        // This extra check indicates that this decision is being made at the wrong layer. This should be refactored.
        if(isPrimaryPersona || [octagonContextID hasSuffix:altDSID]) {
            _octagonContextID = octagonContextID;
        } else {
            _octagonContextID = [NSString stringWithFormat:@"%@_%@", octagonContextID, altDSID];
        }
        _appleAccountID = appleAccountID;
        _altDSID = altDSID;
        _isPrimaryAccount = isPrimaryPersona;
        _personaUniqueString = personaUniqueString;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<TPSpecificUser: altDSID:%@ o:%@ ck:%@ p:%@/%@ aaID:%@>",
            self.altDSID,
            self.octagonContextID,
            self.cloudkitContainerName,
            self.personaUniqueString,
            self.isPrimaryAccount ? @"primary" : @"secondary",
            self.appleAccountID];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _cloudkitContainerName = [coder decodeObjectOfClass:[NSString class] forKey:@"cloudkit"];
        _octagonContextID = [coder decodeObjectOfClass:[NSString class] forKey:@"octagon"];
        _appleAccountID = [coder decodeObjectOfClass:[NSString class] forKey:@"aaID"];
        _altDSID = [coder decodeObjectOfClass:[NSString class] forKey:@"altDSID"];
        _isPrimaryAccount = [coder decodeBoolForKey:@"isPrimary"];
        _personaUniqueString = [coder decodeObjectOfClass:[NSString class] forKey:@"persona"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.cloudkitContainerName forKey:@"cloudkit"];
    [coder encodeObject:self.octagonContextID forKey:@"octagon"];
    [coder encodeObject:self.appleAccountID forKey:@"aaID"];
    [coder encodeObject:self.altDSID forKey:@"altDSID"];
    [coder encodeBool:self.isPrimaryAccount forKey:@"isPrimary"];
    [coder encodeObject:self.personaUniqueString forKey:@"persona"];
}

- (BOOL)isEqual:(id)object
{
    if(![object isKindOfClass:[TPSpecificUser class]]) {
        return NO;
    }

    TPSpecificUser* obj = (TPSpecificUser*)object;

    return ([self.cloudkitContainerName isEqualToString:obj.cloudkitContainerName] &&
            [self.octagonContextID isEqualToString:obj.octagonContextID] &&
            [self.appleAccountID isEqualToString:obj.appleAccountID] &&
            [self.altDSID isEqualToString:obj.altDSID] &&
            self.isPrimaryAccount == obj.isPrimaryAccount &&
            ((self.personaUniqueString == nil && obj.personaUniqueString == nil) || [self.personaUniqueString isEqualToString:obj.personaUniqueString]));
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone {
    return [[TPSpecificUser allocWithZone:zone] initWithCloudkitContainerName:self.cloudkitContainerName
                                                             octagonContextID:self.octagonContextID
                                                               appleAccountID:self.appleAccountID
                                                                      altDSID:self.altDSID
                                                             isPrimaryPersona:self.isPrimaryAccount
                                                          personaUniqueString:self.personaUniqueString];
}

- (NSUInteger)hash {
    return self.cloudkitContainerName.hash
        ^ self.octagonContextID.hash
        ^ self.appleAccountID.hash
        ^ self.altDSID.hash
        ^ self.personaUniqueString.hash;
}

- (CKContainer *)makeCKContainer
{
    CKContainerID *containerID = [CKContainer containerIDForContainerIdentifier:self.cloudkitContainerName];
    CKContainerOptions* containerOptions = [[CKContainerOptions alloc] init];
    containerOptions.bypassPCSEncryption = YES;

    if(!self.isPrimaryAccount) {
        containerOptions.accountOverrideInfo = [[CKAccountOverrideInfo alloc] initWithAltDSID:self.altDSID];
    }
    return [[CKContainer alloc] initWithContainerID:containerID options:containerOptions];
}

@end

#endif // OCTAGON
