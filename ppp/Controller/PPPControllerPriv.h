/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __PPPCONTROLLERPRIV_H__
#define __PPPCONTROLLERPRIV_H__

/*
 * OnDemand keys 
 */

/*
 * Keys passed by the Application or by CFNetwork 
 */

#define kSCPropNetPPPOnDemandHostName			CFSTR("OnDemandHostName")
#define kSCPropNetPPPOnDemandPriority			CFSTR("OnDemandPriority")
#define kSCValNetPPPOnDemandPriorityHigh		CFSTR("High")
#define kSCValNetPPPOnDemandPriorityLow			CFSTR("Low")
#define kSCValNetPPPOnDemandPriorityDefault		CFSTR("Default")

/*
 * Keys set in the Preferences 
 */

#define kSCPropNetPPPOnDemandEnabled			CFSTR("OnDemandEnabled")
#define kSCPropNetPPPOnDemandDomains			CFSTR("OnDemandDomains")
#define kSCPropNetPPPOnDemandMode				CFSTR("OnDemandMode")

#define kSCValNetPPPOnDemandModeAggressive		CFSTR("Aggressive")
#define kSCValNetPPPOnDemandModeConservative	CFSTR("Conservative")
#define kSCValNetPPPOnDemandModeCompatible		CFSTR("Compatible")


/*
 * IPSec keys 
 */

#define kSCEntNetIPSec							CFSTR("IPSec")

#define kSCPropNetIPSecSharedSecret				CFSTR("SharedSecret")
#define kSCPropNetIPSecSharedSecretEncryption	CFSTR("SharedSecretEncryption")
#define kSCValNetIPSecSharedSecretEncryptionKeychain	CFSTR("Keychain")


#endif


