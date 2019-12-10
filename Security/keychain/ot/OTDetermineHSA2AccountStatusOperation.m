
#if OCTAGON

#import "utilities/debugging.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTDetermineHSA2AccountStatusOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"

#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTDetermineHSA2AccountStatusOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* stateIfNotHSA2;
@property OctagonState* stateIfNoAccount;
@property NSOperation* finishedOp;
@end

@implementation OTDetermineHSA2AccountStatusOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                         stateIfHSA2:(OctagonState*)stateIfHSA2
                      stateIfNotHSA2:(OctagonState*)stateIfNotHSA2
                    stateIfNoAccount:(OctagonState*)stateIfNoAccount
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = stateIfHSA2;
        _stateIfNotHSA2 = stateIfNotHSA2;
        _stateIfNoAccount = stateIfNoAccount;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSError *error = nil;
    NSString* primaryAccountAltDSID = [self.deps.authKitAdapter primaryiCloudAccountAltDSID:&error];


    if(primaryAccountAltDSID != nil) {
        secnotice("octagon", "iCloud account is present; checking HSA2 status");

        bool hsa2 = [self.deps.authKitAdapter accountIsHSA2ByAltDSID:primaryAccountAltDSID];
        secnotice("octagon", "HSA2 is %@", hsa2 ? @"enabled" : @"disabled");

        [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
            if(hsa2) {
                metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE;
            } else {
                metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
            }
            metadata.altDSID = primaryAccountAltDSID;
            return metadata;
        } error:&error];

        // If there's an HSA2 account, return to 'initializing' here, as we want to centralize decisions on what to do next
        if(hsa2) {
            self.nextState = self.intendedState;
        } else {
            //[self.deps.accountStateTracker setHSA2iCloudAccountStatus:CKKSAccountStatusNoAccount];
            self.nextState = self.stateIfNotHSA2;
        }

    } else {
        secnotice("octagon", "iCloud account is not present: %@", error);

        [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
            metadata.altDSID = nil;
            return metadata;
        } error:&error];

        self.nextState = self.stateIfNoAccount;
    }

    if(error) {
        secerror("octagon: unable to save new account state: %@", error);
    }

    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON
