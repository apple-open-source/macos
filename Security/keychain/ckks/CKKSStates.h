
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachine.h"

NS_ASSUME_NONNULL_BEGIN

// Flag initialization
typedef OctagonFlag CKKSFlag;

// The set of trusted peers has changed
extern CKKSFlag* const CKKSFlagTrustedPeersSetChanged;

// A client has requested that TLKs be created
extern CKKSFlag* const CKKSFlagTLKCreationRequested;
// We were waiting for a TLK upload, and one has occurred
extern CKKSFlag* const CKKSFlagKeyStateTLKsUploaded;

extern CKKSFlag* const CKKSFlagCloudKitLoggedIn;
extern CKKSFlag* const CKKSFlagCloudKitLoggedOut;

extern CKKSFlag* const CKKSFlagBeginTrustedOperation;
extern CKKSFlag* const CKKSFlagEndTrustedOperation;

extern CKKSFlag* const CKKSFlagChangeTokenExpired;
extern CKKSFlag* const CKKSFlagCloudKitZoneMissing;

extern CKKSFlag* const CKKSFlagDeviceUnlocked;

extern CKKSFlag* const CKKSFlagFetchRequested;
// Added when a key hierarchy fetch completes.
extern CKKSFlag* const CKKSFlagFetchComplete;

extern CKKSFlag* const CKKSFlagKeyStateProcessRequested;
extern CKKSFlag* const CKKSFlagKeySetRequested;

extern CKKSFlag* const CKKSFlagProcessIncomingQueue;
extern CKKSFlag* const CKKSFlagProcessOutgoingQueue;
extern CKKSFlag* const CKKSFlagScanLocalItems;
extern CKKSFlag* const CKKSFlagItemReencryptionNeeded;

extern CKKSFlag* const CKKSFlag24hrNotification;

NSSet<CKKSFlag*>* CKKSAllStateFlags(void);

NS_ASSUME_NONNULL_END

#endif
