/*
	File     :	aod.h

	Contains :	Open directory routines for Apple Open Directory

	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

	Change History (most recent first):

	To Do:
*/


#ifndef __aod_h__
#define __aod_h__	1

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

#define	kMAX_GUID_LEN			128
#define	kONE_K_BUF				1024

typedef enum {
	eNoErr					=	  0,
	eWrongVersion			= -1010,
	eItemNotFound			= -1011,
	eInvalidDataType		= -1012
} eErrorType;


typedef enum {
	eUnknownAcctState	= 0,
	eAcctEnabled		= 1,
	eAcctDisabled		= 2,
	eAcctForwarded		= 3,
	eAcctIMAPLoginOK	= 4,
	eAcctPOP3LoginOK	= 5
} eMailAcctState;

struct od_user_opts
{
	char			fUserID[ kONE_K_BUF ];
	char			fRecName[ kONE_K_BUF ];
	char			fAutoFwdAddr[ kONE_K_BUF ];
	eMailAcctState	fAcctState;
	eMailAcctState	fPOP3Login;
	eMailAcctState	fIMAPLogin;
};

int aodGetUserOptions	( const char *inUserID, struct od_user_opts *inOutOpts );

#endif /* aod */