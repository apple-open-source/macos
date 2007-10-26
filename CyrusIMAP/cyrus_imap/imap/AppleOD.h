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

#include <uuid/uuid.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include "prot.h"

/* Mail user attribute version */
#define	kXMLKeyAttrVersion				"kAttributeVersion"
	#define	kXMLValueVersion				"Apple Mail 1.0"

/* Account state */
#define	kXMLKeyAcctState				"kMailAccountState"
	#define	kXMLValueAcctEnabled			"Enabled"
	#define	kXMLValueAcctDisabled			"Off"
	#define	kXMLValueAcctFwd				"Forward"

/* Auto forward key (has no specific value) */
#define	kXMLKeyAutoFwd					"kAutoForwardValue"

/* IMAP login state */
#define	kXMLKeykIMAPLoginState			"kIMAPLoginState"
	#define	kXMLValueIMAPLoginOK			"IMAPAllowed"
	#define	kXMLValueIMAPLogInNotOK			"IMAPDeny"

/* POP3 login state */
#define	kXMLKeyPOP3LoginState			"kPOP3LoginState"
	#define	kXMLValuePOP3LoginOK			"POP3Allowed"
	#define	kXMLValuePOP3LoginNotOK			"POP3Deny"

/* Account location key (has no specific value) */
#define	kXMLKeyAcctLoc					"kMailAccountLocation"

/* Account location key (has no specific value) */
#define	kXMLKeyAltDataStoreLoc			"kAltMailStoreLoc"

/* Disk Quota  (has no specific value) */
#define	kXMLKeyDiskQuota				"kUserDiskQuota"

/* Server Principals */
#define	kXMLDictionary					"cyrus"
#define	kXMLSMTP_Principal				"smtp_principal"
#define	kXMLIMAP_Principal				"imap_principal"
#define	kXMLPOP3_Principal				"pop_principal"

#define	kPlistFilePath					"/etc/MailServicesOther.plist"

#define	kMAX_GUID_LEN			128
#define	kONE_K_BUF				1024

#define MAX_USER_NAME_SIZE		256
#define	MAX_USER_BUF_SIZE		512
#define	MAX_CHAL_BUF_SIZE		2048
#define	MAX_IO_BUF_SIZE			21848


typedef enum {
	eAODNoErr				=   0,
	eAODParamErr			=  -1,
	eAODOpenDSFailed		=  -2,
	eAODOpenSearchFailed	=  -3,
	eAODUserNotFound		=  -4,
	eAODCantOpenUserNode	=  -5,
	eAODAuthFailed			=  -6,
	eAODAuthWarnNewPW		=  -7,
	eAODAuthWarnExpirePW	=  -8,
	eAODAllocError			=  -9,
	eAODConfigError			= -10,
	eAODMethodNotEnabled	= -11,
	eAODAuthCanceled		= -12,
	eAODProtocolError		= -13,
	eAODInvalidDataType		= -14,
	eAODItemNotFound		= -15,
	eAODWrongVersion		= -16,
	eAODLostConnection		= -17,
	eAODNullbuffer			= -18,
	eAOD					= 0xFF
} eAODError;

typedef enum {
	eUnknownState			= 0x00000000,
	eUnknownUser			= 0x00000001,
	eAccountEnabled			= 0x00000002,
	eIMAPEnabled			= 0x00000004,
	ePOPEnabled				= 0x00000008,
	eAutoForwardedEnabled	= 0x00000010,
	eACLNotMember			= 0x00000020
} eAccountState;

struct od_user_opts
{
	tDirReference		fDirRef;
	tDirNodeReference	fSearchNodeRef;
	eAccountState		fAccountState;
	int					fDiskQuota;
	char			   *fUserIDPtr;
	uuid_t			   *fUserUUID;
	char			   *fAuthIDNamePtr;
	char			   *fRecNamePtr;
	char			   *fAccountLocPtr;
	char			   *fAltDataLocPtr;
	char			   *fAutoFwdPtr;
	char			   *fUserLocPtr;
	char			   *fClientIPString;
	char			   *fClientPortString;
	char			   *fHostPortString;
};

typedef enum
{
	kSGSSSuccess		= 0,
	kSGSSBufferSizeErr	= -70001,
	kSGSSImportNameErr	= -70002,
	kSGSSAquireCredErr	= -70003,
	kSGSSInquireCredErr	= -70004,
	kSGSSAuthFailed		= -70005,
	kGSSErrUnknownType	= -70010,
	kUnknownErr			= -70010
} eGSSError;

int	 odGetUserOpts		( const char *inUserID, struct od_user_opts *inOutOpts );
void odFreeUserOpts		( struct od_user_opts *inOutOpts, int inFreeOD );
int  odCheckPass		( const char *inPasswd, struct od_user_opts *inOutOpts );
int  odCheckAPOP		( const char *inChallenge, const char *inResponse, struct od_user_opts *inOutOpts );
int  odCRAM_MD5			( const char *inChallenge, const char *inResponse, struct od_user_opts *inOutOpts );
int  odDoAuthenticate	( const char *inMethod, const char *inDigest, const char *inCont,
						  const char *inProtocol, struct protstream *inStreamIn,
						  struct protstream *inStreamOut, struct od_user_opts *inOutOpts );

typedef struct
{
	int		len;
	char	key[ FILENAME_MAX ];
	int		reserved;
} CallbackUserData;

int apple_password_callback ( char *inBuf, int inSize, int in_rwflag, void *inUserData );


#endif /* aod */

