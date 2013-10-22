/*
 * Copyright (c) 2012, 2013 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __API_SUPPORT__
#define __API_SUPPORT__

#include <CoreFoundation/CoreFoundation.h>
#include <net/pfkeyv2.h>
#include "racoon_types.h"
#include <sys/socket.h>
#include <SNIPSecIKEDefinitions.h>
#include <SNIPSecDBDefinitions.h>
#include <SNIPSecIKE.h>
#include <SNIPSecDB.h>

struct isakmp_cfg_state;
struct ikev2_traffic_selector;

#define kSNIPSecDBSrcRangeEndAddress        CFSTR("SrcRangeEndAddress")     /* CFString */
#define kSNIPSecDBDstRangeEndAddress        CFSTR("DstRangeEndAddress")     /* CFString */
#define kSNIPSecDBSrcRangeEndPort			CFSTR("SrcRangeEndPort") 		/* CFNumber */
#define kSNIPSecDBDstRangeEndPort			CFSTR("DstRangeEndPort") 		/* CFNumber */

#define kSNIPSecDBPolicyID                  CFSTR("PolicyID") 		/* CFNumber */

#define kSNIPSecDBPolicyType                CFSTR("PolicyType")     /* CFString */
#define kSNIPSecDBValPolicyTypeDiscard      CFSTR("Discard")
#define kSNIPSecDBValPolicyTypeNone         CFSTR("None")
#define kSNIPSecDBValPolicyTypeIPSec        CFSTR("IPSec")
#define kSNIPSecDBValPolicyTypeEntrust      CFSTR("Entrust")
#define kSNIPSecDBValPolicyTypeBypass       CFSTR("Bypass")
#define kSNIPSecDBValPolicyTypeGenerate     CFSTR("Generate")

#define kSNIPSecDBSACreateTime         CFSTR("CreateTime")
#define kSNIPSecDBSACurrentTime         CFSTR("CurrentTime")
#define kSNIPSecDBSADiffTime         CFSTR("DiffTime")
#define kSNIPSecDBSAHardLifetime         CFSTR("HardLifetime")
#define kSNIPSecDBSASoftLifetime         CFSTR("SoftLifetime")
#define kSNIPSecDBSALastUseTime         CFSTR("LastUseTime")
#define kSNIPSecDBSAHardUseTime         CFSTR("HardUseTime")
#define kSNIPSecDBSASoftUseTime         CFSTR("SoftUseTime")
#define kSNIPSecDBSACurrentBytes        CFSTR("CurrentBytes")
#define kSNIPSecDBSAHardBytes           CFSTR("HardBytes")
#define kSNIPSecDBSASoftBytes           CFSTR("SoftBytes")
#define kSNIPSecDBSACurrentAllocations  CFSTR("CurrentAllocations")
#define kSNIPSecDBSAHardAllocations     CFSTR("HardAllocations")
#define kSNIPSecDBSASoftAllocations     CFSTR("SoftAllocations")

#define kSNIPSecDBSAState              CFSTR("State")
#define kSNIPSecDBValSAStateLarval     CFSTR("Larval")
#define kSNIPSecDBValSAStateMature     CFSTR("Mature")
#define kSNIPSecDBValSAStateDying      CFSTR("Dying")
#define kSNIPSecDBValSAStateDead       CFSTR("Dead")

#define kSNIPSecIKEAssignedPCSCFIPv6Address CFSTR("AssignedPCSCFIPv6Address")

typedef uint32_t InternalSessionRef;
typedef uint32_t InternalItemRef;

/* IPSec DB API Types */
typedef InternalSessionRef InternalDBRef;
typedef InternalItemRef InternalDBSARef;
typedef InternalItemRef InternalDBPolicyRef;
typedef InternalItemRef InternalDBInterfaceRef;
#define kInternalDBRefInvalid 0
#define kInternalDBSARefInvalid 0
#define kInternalDBPolicyRefInvalid 0
#define kInternalDBInterfaceRefInvalid 0

/* IKE API Types */
typedef InternalSessionRef InternalIKESARef;
typedef InternalItemRef InternalChildSARef;
#define kInternalIKESARefInvalid 0
#define kInternalChildSARefInvalid 0

/* Internal support functions -- Dictionaries should be verified for required keys and valid types before calling these */
void ASSendXPCReply (InternalSessionRef sessionRef, InternalItemRef objRef, int callType, void *retVal, Boolean success);
void ASSendXPCMessage(uint32_t message, void *messageobj, uint32_t sessionID, uint32_t itemID);

/* IPSec DB API Functions */
InternalDBRef ASDBCreate (void);
InternalDBSARef ASDBGetSPI (InternalDBRef ref, CFDictionaryRef sadata);
InternalDBSARef ASDBCreateSA (InternalDBRef ref, CFDictionaryRef sadata);
Boolean ASDBUpdateSA (InternalDBRef ref, InternalDBSARef saref, CFDictionaryRef sadata);
Boolean ASDBDeleteSA (InternalDBRef ref, InternalDBSARef saref);
Boolean ASDBCopySA (InternalDBRef ref, InternalDBSARef saref);
Boolean ASDBFlushSA (InternalDBRef ref, Boolean *blockForResponse);
CFArrayRef ASDBCopySAIDs (InternalDBRef ref);
InternalDBPolicyRef ASDBAddPolicy (InternalDBRef ref, CFDictionaryRef spdata);
Boolean ASDBDeletePolicy (InternalDBRef ref, InternalDBPolicyRef policyref);
Boolean ASDBCopyPolicy (InternalDBRef ref, InternalDBPolicyRef policyref);
Boolean ASDBFlushPolicy (InternalDBRef ref, Boolean *blockForResponse);
CFArrayRef ASDBCopyPolicyIDs (InternalDBRef ref);
Boolean ASDBFlushAll (InternalDBRef ref, Boolean *blockForResponse);
Boolean ASDBDispose (InternalDBRef ref, Boolean *blockForResponse);

/* IPSec DB Interface Functions */
InternalDBInterfaceRef ASDBCreateIPSecInterface (InternalDBRef ref, struct sockaddr_storage *address, struct sockaddr_storage *netmask, struct sockaddr_storage *v6address, int v6prefix);
Boolean ASDBFlushInterfaces (InternalDBRef ref);

/* IKE API Functions */
InternalIKESARef ASIKECreate (CFDictionaryRef ikedata, CFDictionaryRef childData);
InternalChildSARef ASIKEStartConnection (InternalIKESARef ref);
Boolean ASIKEStopConnection (InternalIKESARef ref);
InternalChildSARef ASIKEStartChildSA (InternalIKESARef ref, CFDictionaryRef ikechilddata);
Boolean ASIKEStopChildSA (InternalIKESARef ref, InternalChildSARef childref);
SNIPSecIKEStatus ASIKEGetConnectionStatus (InternalIKESARef ref);
SNIPSecIKEStatus ASIKEGetChildStatus (InternalIKESARef ref, InternalChildSARef childref);
Boolean ASIKEDispose (InternalIKESARef ref, Boolean *blockForResponse);
Boolean ASIKEEnableAll (InternalIKESARef ref);
Boolean ASIKEDisableAll (InternalIKESARef ref);

/* Functions to support racoon */
InternalDBSARef ASDBGetSPIFromIKE (InternalDBRef ref, phase2_handle_t *phase2);
Boolean ASDBAddSAFromIKE (InternalDBRef ref, phase2_handle_t *phase2, Boolean update);
Boolean ASDBDeleteSAFromIKE (InternalDBRef ref, struct sockaddr_storage *dst, uint32_t spi, int ipsecProtocol);
Boolean ASDBFlushAllForIKEChildSA (InternalDBRef ref, InternalChildSARef childRef);
InternalDBPolicyRef ASDBAddPolicyFromIKE (InternalDBRef ref, phase2_handle_t *phase2);
Boolean ASDBReceivePFKeyMessage (caddr_t *message, int array_size); /* Returns TRUE if handled message */
Boolean ASDBGetIPSecInterfaceName (InternalDBRef ref, char *buf, int bufLen);
Boolean ASIKEConnectionAddChildSAFromIKE (InternalIKESARef ref, phase2_handle_t *childSA);
Boolean ASIKEConnectionSwapChildSAs (InternalIKESARef ref, InternalChildSARef oldChildSA, InternalChildSARef newChildSA);
void ASIKEConnectionExpireChildSAFromIKE (InternalIKESARef ref, InternalChildSARef childSARef);
Boolean ASHasValidSessions (void);
void ASIKEUpdateLocalAddressesFromIKE (void);
void ASIKEUpdateStatusFromIKE (InternalIKESARef ref, InternalChildSARef childRef, uint32_t status, uint32_t reason);
phase2_handle_t *ASIKEConnectionGetChildSAFromIKE (InternalIKESARef ref, InternalChildSARef childSARef);
void ASIKEUpdateConfigurationFromIKE (InternalIKESARef ref, struct isakmp_cfg_state *config);
void ASIKEUpdateTrafficSelectorsFromIKE (InternalIKESARef ref, InternalChildSARef childRef, struct ikev2_traffic_selector *local, struct ikev2_traffic_selector *remote);
void ASIKEStopConnectionFromIKE (InternalIKESARef ref);

#endif
