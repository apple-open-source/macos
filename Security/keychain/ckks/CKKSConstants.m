
#import <Foundation/Foundation.h>
#include <sys/sysctl.h>

#import "keychain/ckks/CKKS.h"

const SecCKKSItemEncryptionVersion currentCKKSItemEncryptionVersion = CKKSItemEncryptionVersion2;

NSString* const SecCKKSActionAdd = @"add";
NSString* const SecCKKSActionDelete = @"delete";
NSString* const SecCKKSActionModify = @"modify";

CKKSItemState* const SecCKKSStateNew = (CKKSItemState*) @"new";
CKKSItemState* const SecCKKSStateUnauthenticated = (CKKSItemState*) @"unauthenticated";
CKKSItemState* const SecCKKSStateInFlight = (CKKSItemState*) @"inflight";
CKKSItemState* const SecCKKSStateReencrypt = (CKKSItemState*) @"reencrypt";
CKKSItemState* const SecCKKSStateError = (CKKSItemState*) @"error";
CKKSItemState* const SecCKKSStateDeleted = (CKKSItemState*) @"deleted";

CKKSProcessedState* const SecCKKSProcessedStateLocal = (CKKSProcessedState*) @"local";
CKKSProcessedState* const SecCKKSProcessedStateRemote = (CKKSProcessedState*) @"remote";

CKKSKeyClass* const SecCKKSKeyClassTLK = (CKKSKeyClass*) @"tlk";
CKKSKeyClass* const SecCKKSKeyClassA = (CKKSKeyClass*) @"classA";
CKKSKeyClass* const SecCKKSKeyClassC = (CKKSKeyClass*) @"classC";

NSString* SecCKKSContainerName = @"com.apple.security.keychain";
bool SecCKKSContainerUsePCS = false;

NSString* const SecCKKSSubscriptionID = @"keychain-changes";
NSString* const SecCKKSAPSNamedPort = @"com.apple.securityd.aps";

NSString* const SecCKRecordItemType = @"item";
NSString* const SecCKRecordHostOSVersionKey = @"uploadver";
NSString* const SecCKRecordEncryptionVersionKey = @"encver";
NSString* const SecCKRecordDataKey = @"data";
NSString* const SecCKRecordParentKeyRefKey = @"parentkeyref";
NSString* const SecCKRecordWrappedKeyKey = @"wrappedkey";
NSString* const SecCKRecordGenerationCountKey = @"gen";

NSString* const SecCKRecordPCSServiceIdentifier = @"pcsservice";
NSString* const SecCKRecordPCSPublicKey = @"pcspublickey";
NSString* const SecCKRecordPCSPublicIdentity = @"pcspublicidentity";
NSString* const SecCKRecordServerWasCurrent = @"server_wascurrent";

NSString* const SecCKRecordIntermediateKeyType = @"synckey";
NSString* const SecCKRecordKeyClassKey = @"class";

NSString* const SecCKRecordTLKShareType = @"tlkshare";
NSString* const SecCKRecordSenderPeerID = @"sender";
NSString* const SecCKRecordReceiverPeerID = @"receiver";
NSString* const SecCKRecordReceiverPublicEncryptionKey = @"receiverPublicEncryptionKey";
NSString* const SecCKRecordCurve = @"curve";
NSString* const SecCKRecordEpoch = @"epoch";
NSString* const SecCKRecordPoisoned = @"poisoned";
NSString* const SecCKRecordSignature = @"signature";
NSString* const SecCKRecordVersion = @"version";

NSString* const SecCKRecordCurrentKeyType = @"currentkey";

NSString* const SecCKRecordCurrentItemType = @"currentitem";
NSString* const SecCKRecordItemRefKey = @"item";

NSString* const SecCKRecordDeviceStateType = @"devicestate";
NSString* const SecCKRecordOctagonPeerID = @"octagonpeerid";
NSString* const SecCKRecordOctagonStatus = @"octagonstatus";
NSString* const SecCKRecordCirclePeerID = @"peerid";
NSString* const SecCKRecordCircleStatus = @"circle";
NSString* const SecCKRecordKeyState = @"keystate";
NSString* const SecCKRecordCurrentTLK = @"currentTLK";
NSString* const SecCKRecordCurrentClassA = @"currentClassA";
NSString* const SecCKRecordCurrentClassC = @"currentClassC";
NSString* const SecCKSRecordLastUnlockTime = @"lastunlock";
NSString* const SecCKSRecordOSVersionKey = @"osver";

NSString* const SecCKRecordManifestType = @"manifest";
NSString* const SecCKRecordManifestDigestValueKey = @"digest_value";
NSString* const SecCKRecordManifestGenerationCountKey = @"generation_count";
NSString* const SecCKRecordManifestLeafRecordIDsKey = @"leaf_records";
NSString* const SecCKRecordManifestPeerManifestRecordIDsKey = @"peer_manifests";
NSString* const SecCKRecordManifestCurrentItemsKey = @"current_items";
NSString* const SecCKRecordManifestSignaturesKey = @"signatures";
NSString* const SecCKRecordManifestSignerIDKey = @"signer_id";
NSString* const SecCKRecordManifestSchemaKey = @"schema";

NSString* const SecCKRecordManifestLeafType = @"manifest_leaf";
NSString* const SecCKRecordManifestLeafDERKey = @"der";
NSString* const SecCKRecordManifestLeafDigestKey = @"digest";

CKKSZoneKeyState* const SecCKKSZoneKeyStateReady = (CKKSZoneKeyState*) @"ready";
CKKSZoneKeyState* const SecCKKSZoneKeyStateReadyPendingUnlock = (CKKSZoneKeyState*) @"readypendingunlock";
CKKSZoneKeyState* const SecCKKSZoneKeyStateError = (CKKSZoneKeyState*) @"error";
CKKSZoneKeyState* const SecCKKSZoneKeyStateCancelled = (CKKSZoneKeyState*) @"cancelled";

CKKSZoneKeyState* const SecCKKSZoneKeyStateInitializing = (CKKSZoneKeyState*) @"initializing";
CKKSZoneKeyState* const SecCKKSZoneKeyStateInitialized = (CKKSZoneKeyState*) @"initialized";
CKKSZoneKeyState* const SecCKKSZoneKeyStateFetch = (CKKSZoneKeyState*) @"fetching";
CKKSZoneKeyState* const SecCKKSZoneKeyStateFetchComplete = (CKKSZoneKeyState*) @"fetchcomplete";
CKKSZoneKeyState* const SecCKKSZoneKeyStateNeedFullRefetch = (CKKSZoneKeyState*) @"needrefetch";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLK = (CKKSZoneKeyState*) @"waitfortlk";

CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLKCreation = (CKKSZoneKeyState*) @"waitfortlkcreation";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTLKUpload = (CKKSZoneKeyState*) @"waitfortlkupload";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForUnlock = (CKKSZoneKeyState*) @"waitforunlock";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForTrust = (CKKSZoneKeyState*) @"waitfortrust";
CKKSZoneKeyState* const SecCKKSZoneKeyStateUnhealthy = (CKKSZoneKeyState*) @"unhealthy";
CKKSZoneKeyState* const SecCKKSZoneKeyStateBadCurrentPointers = (CKKSZoneKeyState*) @"badcurrentpointers";
CKKSZoneKeyState* const SecCKKSZoneKeyStateNewTLKsFailed = (CKKSZoneKeyState*) @"newtlksfailed";
CKKSZoneKeyState* const SecCKKSZoneKeyStateHealTLKShares = (CKKSZoneKeyState*) @"healtlkshares";
CKKSZoneKeyState* const SecCKKSZoneKeyStateHealTLKSharesFailed = (CKKSZoneKeyState*) @"healtlksharesfailed";
CKKSZoneKeyState* const SecCKKSZoneKeyStateWaitForFixupOperation = (CKKSZoneKeyState*) @"waitforfixupoperation";
CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingZone = (CKKSZoneKeyState*) @"resetzone";
CKKSZoneKeyState* const SecCKKSZoneKeyStateResettingLocalData = (CKKSZoneKeyState*) @"resetlocal";
CKKSZoneKeyState* const SecCKKSZoneKeyStateLoggedOut = (CKKSZoneKeyState*) @"loggedout";
CKKSZoneKeyState* const SecCKKSZoneKeyStateZoneCreationFailed = (CKKSZoneKeyState*) @"zonecreationfailed";
CKKSZoneKeyState* const SecCKKSZoneKeyStateProcess = (CKKSZoneKeyState*) @"process";

NSString* const CKKSErrorDomain = @"CKKSErrorDomain";
NSString* const CKKSServerExtensionErrorDomain = @"CKKSServerExtensionErrorDomain";

const NSUInteger SecCKKSItemPaddingBlockSize = 20;

NSString* const SecCKKSAggdPropagationDelay   = @"com.apple.security.ckks.propagationdelay";
NSString* const SecCKKSAggdPrimaryKeyConflict = @"com.apple.security.ckks.pkconflict";
NSString* const SecCKKSAggdViewKeyCount = @"com.apple.security.ckks.keycount";
NSString* const SecCKKSAggdItemReencryption = @"com.apple.security.ckks.reencrypt";

NSString* const SecCKKSUserDefaultsSuite = @"com.apple.security.ckks";

NSString* SecCKKSHostOSVersion()
{
#ifdef PLATFORM
    // Use complicated macro magic to get the string value passed in as preprocessor define PLATFORM.
#define PLATFORM_VALUE(f) #f
#define PLATFORM_OBJCSTR(f) @PLATFORM_VALUE(f)
    NSString* platform = (PLATFORM_OBJCSTR(PLATFORM));
#undef PLATFORM_OBJCSTR
#undef PLATFORM_VALUE
#else
    NSString* platform = "unknown";
#warning No PLATFORM defined; why?
#endif

    NSString* osversion = nil;

    // If we can get the build information from sysctl, use it.
    char release[256];
    size_t releasesize = sizeof(release);
    bool haveSysctlInfo = true;
    haveSysctlInfo &= (0 == sysctlbyname("kern.osrelease", release, &releasesize, NULL, 0));

    char version[256];
    size_t versionsize = sizeof(version);
    haveSysctlInfo &= (0 == sysctlbyname("kern.osversion", version, &versionsize, NULL, 0));

    if(haveSysctlInfo) {
        // Null-terminate for extra safety
        release[sizeof(release)-1] = '\0';
        version[sizeof(version)-1] = '\0';
        osversion = [NSString stringWithFormat:@"%s (%s)", release, version];
    }

    if(!osversion) {
        //  Otherwise, use the not-really-supported fallback.
        osversion = [[NSProcessInfo processInfo] operatingSystemVersionString];

        // subtly improve osversion (but it's okay if that does nothing)
        osversion = [osversion stringByReplacingOccurrencesOfString:@"Version" withString:@""];
    }

    return [NSString stringWithFormat:@"%@ %@", platform, osversion];
}
