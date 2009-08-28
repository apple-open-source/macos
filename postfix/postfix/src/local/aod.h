/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


#ifndef __aod_h__
#define __aod_h__	1

/* Account state */
#define	kXMLKeyAcctState				"kMailAccountState"
	#define	kXMLValueAcctEnabled			"Enabled"
	#define	kXMLValueAcctDisabled			"Off"
	#define	kXMLValueAcctFwd				"Forward"

/* Auto forward key (has no specific value) */
#define	kXMLKeyAutoFwd					"kAutoForwardValue"
#define	kXMLKeyAltAcctLoc				"kAltMailStoreLoc"

typedef enum {
	eUnknownAcctState	= 0,
	eAcctEnabled		= 1,
	eAcctDisabled		= 2,
	eAcctForwarded		= 3,
} eMailAcctState;

struct od_user_opts
{
	char			fUserID[ 512 ];
	char			fRecName[ 512 ];
	char			fAutoFwdAddr[ 215 ];
	eMailAcctState	fAcctState;
};

int aod_get_user_options	( const char *inUserID, struct od_user_opts *inOutOpts );
int ads_get_user_options	( const char *inUserID, struct od_user_opts *inOutOpts );

#endif /* aod */
