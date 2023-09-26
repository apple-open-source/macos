
#if OCTAGON

#import "utilities/debugging.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTDetermineCDPCapableAccountStatusOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"

#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTDetermineCDPCapableAccountStatusOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* stateIfNotCDPCapable;
@property OctagonState* stateIfNoAccount;
@property NSOperation* finishedOp;
@end

@implementation OTDetermineCDPCapableAccountStatusOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                   stateIfCDPCapable:(OctagonState*)stateIfCDPCapable
                stateIfNotCDPCapable:(OctagonState*)stateIfNotCDPCapable
                    stateIfNoAccount:(OctagonState*)stateIfNoAccount
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;
        
        _intendedState = stateIfCDPCapable;
        _stateIfNotCDPCapable = stateIfNotCDPCapable;
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

    secnotice("octagon", "iCloud account(altDSID %@) is configured; checking if account is CDP capable", altDSID);
    bool cdpCapable = [self.deps.authKitAdapter accountIsCDPCapableByAltDSID:altDSID];
    secnotice("octagon", "Account is %@", cdpCapable ? @"capable" : @"not capable");

    NSError *accountStateSaveError = nil;
    [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
        if(cdpCapable) {
            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE;
        } else {
            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
        }
        metadata.altDSID = altDSID;
        return metadata;
    } error:&accountStateSaveError];

    // If there's an HSA2/Managed account, return to 'initializing' here, as we want to centralize decisions on what to do next
    if(cdpCapable) {
        self.nextState = self.intendedState;
    } else {
        self.nextState = self.stateIfNotCDPCapable;
    }
    if(accountStateSaveError) {
        secerror("octagon: unable to save new account state: %@", accountStateSaveError);
    }

    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON
