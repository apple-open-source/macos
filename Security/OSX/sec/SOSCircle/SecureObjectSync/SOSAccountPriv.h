//
//  SOSAccountPriv.h
//  Security
//

#ifndef SOSAccountPriv_h
#define SOSAccountPriv_h

#import <Foundation/Foundation.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecAKSWrappers.h>

#include <Security/SecKeyPriv.h>

#include <Security/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>

#include <AssertMacros.h>
#include <assert.h>

#import <notify.h>

#include <Security/SecureObjectSync/SOSInternal.h>

#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCircleV2.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <securityd/SOSCloudCircleServer.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>

#include <utilities/iCloudKeychainTrace.h>

#include <Security/SecItemPriv.h>


extern const CFStringRef kSOSUnsyncedViewsKey;
extern const CFStringRef kSOSPendingEnableViewsToBeSetKey;
extern const CFStringRef kSOSPendingDisableViewsToBeSetKey;
extern const CFStringRef kSOSRecoveryKey;
extern const CFStringRef kSOSAccountUUID;
extern const CFStringRef kSOSAccountPeerNegotiationTimeouts;
extern const CFStringRef kSOSRecoveryRing;
extern const CFStringRef kSOSEscrowRecord;
extern const CFStringRef kSOSTestV2Settings;
extern const CFStringRef kSOSRateLimitingCounters;
extern const CFStringRef kSOSAccountPeerLastSentTimestamp;
extern const CFStringRef kSOSAccountRenegotiationRetryCount;
extern const CFStringRef kSOSInitialSyncTimeoutV0;

#define kSecServerPeerInfoAvailable "com.apple.security.fpiAvailable"

typedef void (^SOSAccountSaveBlock)(CFDataRef flattenedAccount, CFErrorRef flattenFailError);

@class SOSMessageIDS;
@class SOSMessageKVS;
@class CKKeyParameter;
@class SOSAccountTrustClassic;
@class SOSKVSCircleStorageTransport;
@class SOSCircleStorageTransport;
@class SOSCKCircleStorage;

@interface SOSAccount : NSObject <SOSControlProtocol>

@property   (nonatomic, retain)     NSDictionary                *gestalt;
@property   (nonatomic, retain)     NSData                      *backup_key;
@property   (nonatomic, retain)     NSString                    *deviceID;

@property   (nonatomic, retain)     SOSAccountTrustClassic      *trust;

@property   (nonatomic, retain)     dispatch_queue_t            queue;
@property   (nonatomic, retain)     dispatch_source_t           user_private_timer;
@property   (nonatomic)             SecKeyRef                   accountPrivateKey;

@property   (nonatomic)             SOSDataSourceFactoryRef     factory;

@property   (nonatomic, retain)     NSData                      *_password_tmp;
@property   (nonatomic, assign)     BOOL                        isListeningForSync;
@property   (nonatomic, assign)     int                         lock_notification_token;
@property   (nonatomic, retain)     CKKeyParameter*             key_transport;
@property   (nonatomic)             SOSKVSCircleStorageTransport*  circle_transport;
@property   (nonatomic, retain)     SOSMessageKVS*              kvs_message_transport;
@property   (nonatomic, retain)     SOSMessageIDS*              ids_message_transport;
@property   (nonatomic, retain)     SOSCKCircleStorage*         ck_storage;


@property   (nonatomic, assign)     BOOL                        circle_rings_retirements_need_attention;
@property   (nonatomic, assign)     BOOL                        engine_peer_state_needs_repair;
@property   (nonatomic, assign)     BOOL                        key_interests_need_updating;

@property   (nonatomic, retain)     NSMutableArray               *change_blocks;

@property   (nonatomic, retain)     NSMutableDictionary          *waitForInitialSync_blocks;
@property   (nonatomic, assign)     BOOL                        isInitialSyncing;

@property   (nonatomic)             NSData*                     accountKeyDerivationParamters;

@property   (nonatomic, assign)     BOOL                        accountKeyIsTrusted;
@property   (nonatomic)             SecKeyRef                   accountKey;
@property   (nonatomic)             SecKeyRef                   previousAccountKey;

@property   (copy)                  SOSAccountSaveBlock         saveBlock;


// Identity access properties, all delegated to the trust object
@property   (readonly, nonatomic)   BOOL                        hasPeerInfo;
@property   (readonly, nonatomic)   SOSPeerInfoRef              peerInfo;
@property   (readonly, nonatomic)   SOSFullPeerInfoRef          fullPeerInfo;
@property   (readonly, nonatomic)   NSString*                   peerID;


-(id) init;
-(id) initWithGestalt:(CFDictionaryRef)gestalt factory:(SOSDataSourceFactoryRef)factory;

void SOSAccountAddSyncablePeerBlock(SOSAccount*  a,
                                    CFStringRef ds_name,
                                    SOSAccountSyncablePeersBlock changeBlock);

-(bool) ensureFactoryCircles;
-(void) ensureOctagonPeerKeys;

-(void) flattenToSaveBlock;

void SOSAccountSetToNew(SOSAccount*  a);

bool SOSAccountIsMyPeerActive(SOSAccount* account, CFErrorRef* error);

// MARK: In Sync checking
typedef bool (^SOSAccountWaitForInitialSyncBlock)(SOSAccount*  account);

CF_RETURNS_RETAINED CFStringRef SOSAccountCallWhenInSync(SOSAccount* account, SOSAccountWaitForInitialSyncBlock syncBlock);
bool SOSAccountUnregisterCallWhenInSync(SOSAccount* account, CFStringRef id);

bool SOSAccountHandleOutOfSyncUpdate(SOSAccount* account, CFSetRef oldOOSViews, CFSetRef newOOSViews);

void SOSAccountEnsureSyncChecking(SOSAccount* account);
void SOSAccountCancelSyncChecking(SOSAccount* account);

CFMutableSetRef SOSAccountCopyOutstandingViews(SOSAccount* account);
void SOSAccountNotifyEngines(SOSAccount* account);
CFMutableSetRef SOSAccountCopyOutstandingViews(SOSAccount* account);
bool SOSAccountIsViewOutstanding(SOSAccount* account, CFStringRef view);
CFMutableSetRef SOSAccountCopyIntersectionWithOustanding(SOSAccount* account, CFSetRef inSet);
bool SOSAccountIntersectsWithOutstanding(SOSAccount* account, CFSetRef views);
bool SOSAccountHasOustandingViews(SOSAccount* account);
bool SOSAccountHasCompletedInitialSync(SOSAccount* account);
bool SOSAccountHasCompletedRequiredBackupSync(SOSAccount* account);
CFMutableSetRef SOSAccountCopyOutstandingViews(SOSAccount* account);
bool SOSAccountSyncingV0(SOSAccount* account);

// MARK: DER Stuff


size_t der_sizeof_fullpeer_or_null(SOSFullPeerInfoRef data, CFErrorRef* error);

uint8_t* der_encode_fullpeer_or_null(SOSFullPeerInfoRef data, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

const uint8_t* der_decode_fullpeer_or_null(CFAllocatorRef allocator, SOSFullPeerInfoRef* data,
                                           CFErrorRef* error,
                                           const uint8_t* der, const uint8_t* der_end);


size_t der_sizeof_public_bytes(SecKeyRef publicKey, CFErrorRef* error);

uint8_t* der_encode_public_bytes(SecKeyRef publicKey, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

const uint8_t* der_decode_public_bytes(CFAllocatorRef allocator, CFIndex algorithmID, SecKeyRef* publicKey, CFErrorRef* error, const uint8_t* der, const uint8_t* der_end);


// Update
-(SOSCCStatus) getCircleStatus:(CFErrorRef*) error;

bool SOSAccountHandleCircleMessage(SOSAccount* account,
                                   CFStringRef circleName, CFDataRef encodedCircleMessage, CFErrorRef *error);

CF_RETURNS_RETAINED
CFDictionaryRef SOSAccountHandleRetirementMessages(SOSAccount* account, CFDictionaryRef circle_retirement_messages, CFErrorRef *error);

void SOSAccountRecordRetiredPeersInCircle(SOSAccount* account);

bool SOSAccountHandleUpdateCircle(SOSAccount* account,
                                  SOSCircleRef prospective_circle,
                                  bool writeUpdate,
                                  CFErrorRef *error);


// My Peer
bool SOSAccountHasFullPeerInfo(SOSAccount* account, CFErrorRef* error);

bool SOSAccountIsMyPeerInBackupAndCurrentInView(SOSAccount* account, CFStringRef viewname);
bool SOSAccountUpdateOurPeerInBackup(SOSAccount* account, SOSRingRef oldRing, CFErrorRef *error);
bool SOSAccountIsPeerInBackupAndCurrentInView(SOSAccount* account, SOSPeerInfoRef testPeer, CFStringRef viewname);
bool SOSDeleteV0Keybag(CFErrorRef *error);
void SOSAccountForEachBackupView(SOSAccount* account,  void (^operation)(const void *value));
bool SOSAccountUpdatePeerInfo(SOSAccount* account, CFStringRef updateDescription, CFErrorRef *error, bool (^update)(SOSFullPeerInfoRef fpi, CFErrorRef *error));

// Currently permitted backup rings.
void SOSAccountForEachBackupRingName(SOSAccount* account, void (^operation)(CFStringRef value));
void SOSAccountForEachRingName(SOSAccount* account, void (^operation)(CFStringRef value));

// My Circle
bool SOSAccountHasCircle(SOSAccount* account, CFErrorRef* error);
SOSCircleRef CF_RETURNS_RETAINED SOSAccountEnsureCircle(SOSAccount* a, CFStringRef name, CFErrorRef *error);

void AppendCircleKeyName(CFMutableArrayRef array, CFStringRef name);

CFStringRef SOSInterestListCopyDescription(CFArrayRef interests);


// FullPeerInfos - including Cloud Identity
SOSFullPeerInfoRef CopyCloudKeychainIdentity(SOSPeerInfoRef cloudPeer, CFErrorRef *error);

bool SOSAccountIsAccountIdentity(SOSAccount* account, SOSPeerInfoRef peer_info, CFErrorRef *error);
bool SOSAccountFullPeerInfoVerify(SOSAccount* account, SecKeyRef privKey, CFErrorRef *error);
CF_RETURNS_RETAINED SOSPeerInfoRef GenerateNewCloudIdentityPeerInfo(CFErrorRef *error);

// Credentials
bool SOSAccountHasPublicKey(SOSAccount* account, CFErrorRef* error);
bool SOSAccountPublishCloudParameters(SOSAccount* account, CFErrorRef* error);
bool SOSAccountRetrieveCloudParameters(SOSAccount* account, SecKeyRef *newKey,
                                       CFDataRef derparms,
                                       CFDataRef *newParameters, CFErrorRef* error);

//DSID
void SOSAccountAssertDSID(SOSAccount* account, CFStringRef dsid);

//
// Key extraction
//

SecKeyRef SOSAccountCopyDeviceKey(SOSAccount* account, CFErrorRef *error);
SecKeyRef CF_RETURNS_RETAINED GeneratePermanentFullECKey(int keySize, CFStringRef name, CFErrorRef* error);

// Testing
void SOSAccountSetLastDepartureReason(SOSAccount* account, enum DepartureReason reason);
void SOSAccountSetUserPublicTrustedForTesting(SOSAccount* account);

void SOSAccountPurgeIdentity(SOSAccount*);
bool sosAccountLeaveCircle(SOSAccount* account, SOSCircleRef circle, CFErrorRef* error);
bool sosAccountLeaveRing(SOSAccount*  account, SOSRingRef ring, CFErrorRef* error);
bool SOSAccountForEachRing(SOSAccount* account, SOSRingRef (^action)(CFStringRef name, SOSRingRef ring));
bool SOSAccountUpdateBackUp(SOSAccount* account, CFStringRef viewname, CFErrorRef *error);
void SOSAccountEnsureRecoveryRing(SOSAccount* account);
bool SOSAccountEnsureInBackupRings(SOSAccount* account);

bool SOSAccountEnsurePeerRegistration(SOSAccount* account, CFErrorRef *error);

extern const CFStringRef kSOSUnsyncedViewsKey;
extern const CFStringRef kSOSPendingEnableViewsToBeSetKey;
extern const CFStringRef kSOSPendingDisableViewsToBeSetKey;
extern const CFStringRef kSOSRecoveryKey;

typedef enum{
    kSOSTransportNone = 0,
    kSOSTransportIDS = 1,
    kSOSTransportKVS = 2,
    kSOSTransportFuture = 3,
    kSOSTransportPresent = 4
}TransportType;

SOSPeerInfoRef SOSAccountCopyPeerWithID(SOSAccount* account, CFStringRef peerid, CFErrorRef *error);

bool SOSAccountSetValue(SOSAccount* account, CFStringRef key, CFTypeRef value, CFErrorRef *error);
bool SOSAccountClearValue(SOSAccount* account, CFStringRef key, CFErrorRef *error);
CFTypeRef SOSAccountGetValue(SOSAccount* account, CFStringRef key, CFErrorRef *error);

bool SOSAccountAddEscrowToPeerInfo(SOSAccount* account, SOSFullPeerInfoRef myPeer, CFErrorRef *error);
bool SOSAccountAddEscrowRecords(SOSAccount* account, CFStringRef dsid, CFDictionaryRef record, CFErrorRef *error);
void SOSAccountRemoveRing(SOSAccount* a, CFStringRef ringName);
SOSRingRef SOSAccountCopyRingNamed(SOSAccount* a, CFStringRef ringName, CFErrorRef *error);
SOSRingRef SOSAccountRingCreateForName(SOSAccount* a, CFStringRef ringName, CFErrorRef *error);
bool SOSAccountUpdateRingFromRemote(SOSAccount* account, SOSRingRef newRing, CFErrorRef *error);
bool SOSAccountUpdateRing(SOSAccount* account, SOSRingRef newRing, CFErrorRef *error);
bool SOSAccountRemoveBackupPeers(SOSAccount* account, CFArrayRef peerIDs, CFErrorRef *error);
bool SOSAccountResetRing(SOSAccount* account, CFStringRef ringName, CFErrorRef *error);
bool SOSAccountCheckPeerAvailability(SOSAccount* account, CFErrorRef *error);
bool SOSAccountUpdateNamedRing(SOSAccount* account, CFStringRef ringName, CFErrorRef *error,
                               SOSRingRef (^create)(CFStringRef ringName, CFErrorRef *error),
                               SOSRingRef (^copyModified)(SOSRingRef existing, CFErrorRef *error));

//
// MARK: Backup translation functions
//

CFStringRef SOSBackupCopyRingNameForView(CFStringRef viewName);

//
// Security tool test/debug functions
//
bool SOSAccountPostDebugScope(SOSAccount*  account, CFTypeRef scope, CFErrorRef *error);
CFDataRef SOSAccountCopyAccountStateFromKeychain(CFErrorRef *error);
bool SOSAccountDeleteAccountStateFromKeychain(CFErrorRef *error);
CFDataRef SOSAccountCopyEngineStateFromKeychain(CFErrorRef *error);
bool SOSAccountDeleteEngineStateFromKeychain(CFErrorRef *error);

bool SOSAccountIsNew(SOSAccount* account, CFErrorRef *error);
bool SOSAccountCheckForAlwaysOnViews(SOSAccount* account);
// UUID, no setter just getter and ensuring value.
void SOSAccountEnsureUUID(SOSAccount* account);
CFStringRef CF_RETURNS_RETAINED SOSAccountCopyUUID(SOSAccount* account);
const uint8_t* der_decode_cloud_parameters(CFAllocatorRef allocator,
                                           CFIndex algorithmID, SecKeyRef* publicKey,
                                           CFDataRef *parameters,
                                           CFErrorRef* error,
                                           const uint8_t* der, const uint8_t* der_end);

/*
 * HSA2/piggybacking
 */

CFDataRef SOSPiggyBackBlobCopyEncodedData(SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, CFErrorRef *error);

#if __OBJC__
NSData *SOSPiggyCreateInitialSyncData(NSArray<NSData*> *identities, NSArray<NSDictionary *>* tlks);
NSDictionary * SOSPiggyCopyInitialSyncData(const uint8_t** der, const uint8_t *der_end);
NSArray<NSDictionary*>* SOSAccountSortTLKS(NSArray<NSDictionary*>* tlks);
#endif

bool SOSAccountCleanupAllKVSKeys(SOSAccount* account, CFErrorRef* error);
bool SOSAccountPopulateKVSWithBadKeys(SOSAccount*  account, CFErrorRef* error);

@end

@interface SOSAccount (Persistence)

+(instancetype) accountFromData: (NSData*) data
                        factory: (SOSDataSourceFactoryRef) factory
                          error: (NSError**) error;
+(instancetype) accountFromDER: (const uint8_t**) der
                           end: (const uint8_t*) der_end
                       factory: (SOSDataSourceFactoryRef) factory
                         error: (NSError**) error;

-(NSData*) encodedData: (NSError**) error;
@end

#endif /* SOSAccount_h */
