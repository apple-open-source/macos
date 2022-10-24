
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

    NSString* altDSID = self.deps.activeAccount.altDSID;

    if(altDSID == nil) {
        secnotice("octagon", "iCloud account is not present or not configured: %@", self.deps.activeAccount);

        NSError* accountError = nil;
        TPSpecificUser* activeAccount = [self.deps.accountsAdapter findAccountForCurrentThread:self.deps.personaAdapter
                                                                               optionalAltDSID:nil
                                                                         cloudkitContainerName:self.deps.containerName
                                                                              octagonContextID:self.deps.contextID
                                                                                         error:&accountError];
        if(activeAccount == nil || activeAccount.altDSID == nil || accountError != nil) {
            secerror("octagon-account: unable to determine active account(%@); assuming no account is present: %@", self.deps.contextID, accountError);

            NSError *accountStateSaveError = nil;
            [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
                metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
                metadata.altDSID = nil;
                return metadata;
            } error:&accountStateSaveError];

            if(accountStateSaveError) {
                secerror("octagon: unable to save new account state: %@", accountStateSaveError);
            }

            self.nextState = self.stateIfNoAccount;

            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        // Ideally, we'd set this for the entire OTCuttlefishContext. But, we can't reach it from here.
        altDSID = activeAccount.altDSID;
    }

    secnotice("octagon", "iCloud account(altDSID %@) is configured; checking HSA2 status", altDSID);
    bool hsa2 = [self.deps.authKitAdapter accountIsHSA2ByAltDSID:altDSID];
    secnotice("octagon", "HSA2 is %@", hsa2 ? @"enabled" : @"disabled");

    NSError *accountStateSaveError = nil;
    [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
        if(hsa2) {
            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE;
        } else {
            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
        }
        metadata.altDSID = altDSID;
        return metadata;
    } error:&accountStateSaveError];

    // If there's an HSA2 account, return to 'initializing' here, as we want to centralize decisions on what to do next
    if(hsa2) {
        self.nextState = self.intendedState;
    } else {
        self.nextState = self.stateIfNotHSA2;
    }
    if(accountStateSaveError) {
        secerror("octagon: unable to save new account state: %@", accountStateSaveError);
    }

    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON
