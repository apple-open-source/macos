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

#ifndef SecureNetworking_ipsec_xpc_H
#define SecureNetworking_ipsec_xpc_H

#define SN_ENTITLEMENT_IPSEC_IKE    CFSTR("com.apple.private.SecureNetworking.ipsec_ike")
#define SN_ENTITLEMENT_IPSEC_DB     CFSTR("com.apple.private.SecureNetworking.ipsec_db")

#define IPSEC_HELPER    "com.apple.SecureNetworking.IPSec"

/* IKE */
#define	IPSECOPCODE         "ipsecopcode"
#define	IPSECOPIKEDICT      "ipsecikedict"
#define	IPSECOPCHILDDICT    "ipsecchilddict"
#define	IPSECOBJREF         "ipsecobjectref"
#define IPSECIKEID          "ipsecikeid"
#define IPSECCHILDID        "ipsecchildid"
#define IPSECIKESTATUS      "ipsecikestatus"
#define IPSECCHILDSTATUS    "ipsecchildstatus"


/* DB SA */
#define IPSECSASESSIONID    "ipsecsasessionid"
#define IPSECSAID           "ipsecsaid"
#define IPSECSADICT         "ipsecsadict"
#define IPSECSASPI          "ipsecsaspi"
#define IPSECSAIDARRAY      "ipsecsaidarray"
#define IPSECPOLICYID       "ipsecpolicyid"
#define	IPSECPOLICYDICT     "ipsecpolicydict"
#define IPSECPOLICYIDARRAY  "ipsecpolicyidarray"

/* message */
#define IPSECMESSAGE        "ipsecmessage"
#define IPSECITEMID         "ipsecitemid"
#define IPSECITEMDICT       "ipsecitemdict"

#define SERVERREPLY         "reply"

#define REPLYOFFSET         0x1000

#define kSNIPSecDBInvalidSPI        0

enum {
	IPSECIKE_CREATE         = 0x0001,
	IPSECIKE_START,
	IPSECIKE_STOP,
	IPSECIKE_GETSTATUS,
	IPSECIKE_INVALIDATE,
	IPSECIKE_START_CHILD,
	IPSECIKE_STOP_CHILD,
	IPSECIKE_ENABLE_CHILD,
	IPSECIKE_DISABLE_CHILD,
	IPSECIKE_GETSTATUS_CHILD
};


enum {
    IPSECDB_CREATESESSION  = 0x0101,
	IPSECDB_GETSPI,
	IPSECDB_ADDSA,
    IPSECDB_UPDATESA,
	IPSECDB_DELETESA,
	IPSECDB_COPYSA,
	IPSECDB_FLUSHSA,
	IPSECDB_ADDPOLICY,
	IPSECDB_DELETEPOLICY,
	IPSECDB_COPYPOLICY,
	IPSECDB_FLUSHPOLICIES,
	IPSECDB_FLUSHALL,
	IPSECDB_INVALIDATE,
    IPSECDB_COPYSAIDS,
    IPSECDB_COPYPOLICYIDS
};

enum {
	SERVER_REPLY_OK		= 0x0000,
	SERVER_FAILED
};

#endif
