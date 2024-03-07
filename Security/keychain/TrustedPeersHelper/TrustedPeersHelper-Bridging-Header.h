//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"
#import <TrustedPeers/TrustedPeers.h>
#import <TrustedPeers/TPDictionaryMatchingRules.h>
#import <TrustedPeers/TPPBCustodianRecoveryKey.h>
#import <TrustedPeers/TPPBHelpfulConstructors.h>
#import <TrustedPeers/TPPBHelpfulConstructors.h>
#import <TrustedPeers/TPPBPolicyCategoriesByView.h>
#import <TrustedPeers/TPPBPolicyDocument.h>
#import <TrustedPeers/TPPBPolicyIntroducersByCategory.h>
#import <TrustedPeers/TPPBPolicyModelToCategory.h>
#import <TrustedPeers/TPPBDisposition.h>
#import <TrustedPeers/TPPBDispositionEntry.h>

#import <TrustedPeers/SFKey+TPKey.h>
#import <TrustedPeers/TPECPublicKeyFactory.h>

#import "utilities/SecFileLocations.h"
#import <Security/SecTapToRadar.h>

#import <SecurityFoundation/SFKeychain.h>
#import <SecurityFoundation/SFIdentity.h>
#import <SecurityFoundation/SFAccessPolicy.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>

#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTPersonaAdapter.h"
#import "keychain/ot/OTManagedConfigurationAdapter.h"
#import "keychain/ot/ErrorUtils.h"
#import "keychain/ot/proto/generated_source/OTEscrowRecord.h"
#import "keychain/ot/proto/generated_source/OTEscrowRecordMetadata.h"
#import "keychain/ot/proto/generated_source/OTEscrowRecordMetadataClientMetadata.h"
#import "keychain/ot/proto/generated_source/OTEscrowMoveRequestContext.h"
#import "keychain/OctagonTrust/OTNotifications.h"

#import "keychain/TrustedPeersHelper/TPHObjcTranslation.h"
#import "keychain/TrustedPeersHelper/proto/generated_source/OTBottleContents.h"
#import "keychain/TrustedPeersHelper/proto/generated_source/OTBottle.h"
#import "keychain/TrustedPeersHelper/proto/generated_source/OTRecovery.h"
#import "keychain/TrustedPeersHelper/proto/generated_source/OTPrivateKey.h"
#import "keychain/TrustedPeersHelper/proto/generated_source/OTAuthenticatedCiphertext.h"
#import "keychain/TrustedPeersHelper/categories/OTPrivateKey+SF.h"
#import "keychain/TrustedPeersHelper/categories/OTAuthenticatedCiphertext+SF.h"

#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSNotifier.h"

#import "utilities/debugging.h"
#import "utilities/SecCFWrappers.h"

#import <CoreData/CoreDataErrors.h>

#import "keychain/ot/proto/generated_source/OTAccountSettings.h"
#import "keychain/ot/proto/generated_source/OTWalrus.h"
#import "keychain/ot/proto/generated_source/OTWebAccess.h"

#if TARGET_OS_OSX
#include <sandbox.h>
#endif

#import <Security/SecABC.h>
#import "OSX/utilities/SecInternalReleasePriv.h"

#import "keychain/analytics/SecurityAnalyticsConstants.h"
#import "keychain/analytics/SecurityAnalyticsReporterRTC.h"
#import "keychain/analytics/AAFAnalyticsEvent+Security.h"

#import <SoftLinking/SoftLinking.h>
#import "KeychainCircle/MetricsOverrideForTests.h"

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, KeychainCircle);
SOFT_LINK_FUNCTION(KeychainCircle, MetricsEnable, soft_MetricsEnable, bool, (void), ());
SOFT_LINK_FUNCTION(KeychainCircle, MetricsDisable, soft_MetricsDisable, bool, (void), ());
SOFT_LINK_FUNCTION(KeychainCircle, MetricsOverrideTestsAreEnabled, soft_MetricsOverrideTestsAreEnabled, bool, (void), ());
