//
//  SOSAccountTesting.h
//  sec
//
//  Created by Mitch Adler on 6/18/13.
//
//

#ifndef SEC_SOSAccountTesting_h
#define SEC_SOSAccountTesting_h

#include <CoreFoundation/CoreFoundation.h>
#include <SecureObjectSync/SOSAccount.h>

//
// Account comparison
//

#define kAccountsAgreeTestMin 9
#define kAccountsAgreeTestPerPeer 1
#define accountsAgree(x) (kAccountsAgreeTestMin + kAccountsAgreeTestPerPeer * (x))

static void unretired_peers_is_subset(const char* label, CFArrayRef peers, CFArrayRef allowed_peers)
{
    CFArrayForEach(peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        CFErrorRef leftError = NULL;
        CFErrorRef rightError = NULL;
        
        ok(SOSPeerInfoIsRetirementTicket(pi) || SOSPeerInfoIsCloudIdentity(pi) || CFArrayContainsValue(allowed_peers, CFRangeMake(0, CFArrayGetCount(allowed_peers)), pi), "Peer is allowed (%s) Peer: %@, Allowed %@", label, pi, allowed_peers);
        
        CFReleaseNull(leftError);
        CFReleaseNull(rightError);
    });
}

static void accounts_agree_internal(char *label, SOSAccountRef left, SOSAccountRef right, bool check_peers)
{
    CFErrorRef error = NULL;
    {
        CFArrayRef leftPeers = SOSAccountCopyActivePeers(left, &error);
        ok(leftPeers, "Left peers (%@) - %s", error, label);
        CFReleaseNull(error);

        CFArrayRef rightPeers = SOSAccountCopyActivePeers(right, &error);
        ok(rightPeers, "Right peers (%@) - %s", error, label);
        CFReleaseNull(error);

        ok(CFEqual(leftPeers, rightPeers), "Matching peers (%s) Left: %@, Right: %@", label, leftPeers, rightPeers);

        if (check_peers) {
            CFMutableArrayRef allowed_identities = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

            CFArrayRef leftIdentities = SOSAccountCopyAccountIdentityPeerInfos(left, kCFAllocatorDefault, &error);
            ok(leftIdentities, "Get identities (%@)", error);
            CFReleaseNull(error);
            
            CFArrayAppendArray(allowed_identities, leftIdentities, CFRangeMake(0, CFArrayGetCount(leftIdentities)));
            
            CFReleaseNull(leftIdentities);
            
            CFArrayRef rightIdentities = SOSAccountCopyAccountIdentityPeerInfos(right, kCFAllocatorDefault, &error);
            ok(rightIdentities, "Get identities (%@)", error);
            CFReleaseNull(error);
            
            CFArrayAppendArray(allowed_identities, rightIdentities, CFRangeMake(0, CFArrayGetCount(rightIdentities)));
            
            CFReleaseNull(rightIdentities);

            unretired_peers_is_subset(label, leftPeers, allowed_identities);
        }

        CFReleaseNull(leftPeers);
        CFReleaseNull(rightPeers);
    }
    {
        CFArrayRef leftConcurringPeers = SOSAccountCopyConcurringPeers(left, &error);
        ok(leftConcurringPeers, "Left peers (%@) - %s", error, label);

        CFArrayRef rightConcurringPeers = SOSAccountCopyConcurringPeers(right, &error);
        ok(rightConcurringPeers, "Right peers (%@) - %s", error, label);

        ok(CFEqual(leftConcurringPeers, rightConcurringPeers), "Matching concurring peers Left: %@, Right: %@", leftConcurringPeers, rightConcurringPeers);

        CFReleaseNull(leftConcurringPeers);
        CFReleaseNull(rightConcurringPeers);
    }
    {
        CFArrayRef leftApplicants = SOSAccountCopyApplicants(left, &error);
        ok(leftApplicants, "Left Applicants (%@) - %s", error, label);

        CFArrayRef rightApplicants = SOSAccountCopyApplicants(right, &error);
        ok(rightApplicants, "Left Applicants (%@) - %s", error, label);

        ok(CFEqual(leftApplicants, rightApplicants), "Matching applicants (%s) Left: %@, Right: %@", label, leftApplicants, rightApplicants);

        CFReleaseNull(leftApplicants);
        CFReleaseNull(rightApplicants);
    }
}

static inline void accounts_agree(char *label, SOSAccountRef left, SOSAccountRef right)
{
    accounts_agree_internal(label, left, right, true);
}


//
// Change handling
//

static CFMutableDictionaryRef ExtractPendingChanges(CFMutableDictionaryRef changes)
{
    CFMutableDictionaryRef extracted = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, changes);

    CFDictionaryRemoveAllValues(changes);

    return extracted;
}

#define kFeedChangesToMultieTestCountPer 1
static inline void FeedChangesToMulti(CFMutableDictionaryRef changes, ...)
{
    CFDictionaryRef changes_to_send = ExtractPendingChanges(changes);

    SOSAccountRef account;

    secerror("Change block: %@", changes_to_send);

    CFErrorRef error = NULL;
	va_list argp;
	va_start(argp, changes);
    while((account = va_arg(argp, SOSAccountRef)) != NULL) {
        ok(SOSAccountHandleUpdates(account, changes_to_send, &error), "SOSAccountHandleUpdates failed (%@)", error);
        CFReleaseNull(error);
    }
}

static inline void InjectChangeToMulti(CFStringRef changeKey, CFStringRef changeValue, ...)
{
    CFMutableDictionaryRef changes_to_send = CFDictionaryCreateMutable(NULL, 1, NULL, NULL);
    CFDictionaryAddValue(changes_to_send, changeKey, changeValue);
    
    SOSAccountRef account;
    
    secerror("Change block: %@", changes_to_send);
    
    CFErrorRef error = NULL;
	va_list argp;
	va_start(argp, changeValue);
    while((account = va_arg(argp, SOSAccountRef)) != NULL) {
        ok(SOSAccountHandleUpdates(account, changes_to_send, &error), "SOSAccountHandleUpdates failed (%@)", error);
        CFReleaseNull(error);
    }
    CFReleaseNull(changes_to_send);
}



#define kFeedChangesToTestCount 1
static inline void FeedChangesTo(CFMutableDictionaryRef changes, SOSAccountRef account)
{
    CFDictionaryRef changes_to_send = ExtractPendingChanges(changes);

    secerror("Change block: %@", changes_to_send);

    CFErrorRef error = NULL;
    ok(SOSAccountHandleUpdates(account, changes_to_send, &error), "SOSAccountHandleUpdates failed (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(changes_to_send);
}


static SOSAccountRef CreateAccountForLocalChanges(CFMutableDictionaryRef changes, CFStringRef name, CFStringRef data_source_name)
{
    SOSAccountKeyInterestBlock interest_block = ^(bool getNewKeysOnly, CFArrayRef alwaysKeys, CFArrayRef afterFirstUnlockKeys, CFArrayRef unlockedKeys) {};
    SOSAccountDataUpdateBlock update_block = ^ bool (CFDictionaryRef keys, CFErrorRef *error) {
        CFDictionaryForEach(keys, ^(const void *key, const void *value) {
            CFDictionarySetValue(changes, key, value);
        });
        return true;
    };

    SOSDataSourceFactoryRef factory = SOSTestDataSourceFactoryCreate();
    SOSTestDataSourceFactoryAddDataSource(factory, data_source_name, SOSTestDataSourceCreate());

    CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(name);

    SOSAccountRef result = SOSAccountCreate(kCFAllocatorDefault, gestalt, factory, interest_block, update_block);

    CFReleaseNull(gestalt);

    return result;
}


static inline int countPeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyPeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countActivePeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActivePeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countActiveValidPeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActiveValidPeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countApplicants(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    int retval = 0;
    
    if(applicants) retval = (int)CFArrayGetCount(applicants);
    CFReleaseNull(error);
    CFReleaseNull(applicants);
    return retval;
}


static inline void showActiveValidPeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActiveValidPeers(account, &error);
    CFArrayForEach(peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        ok(0, "Active Valid Peer %@", pi);
    });
    CFReleaseNull(peers);
}

#endif
