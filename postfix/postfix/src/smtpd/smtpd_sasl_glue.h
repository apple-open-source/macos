/*++
/* NAME
/*	smtpd_sasl_glue 3h
/* SUMMARY
/*	Postfix SMTP server, SASL support interface
/* SYNOPSIS
/*	#include "smtpd_sasl_glue.h"
/* DESCRIPTION
/* .nf

 /*
  * SASL protocol interface
  */
#ifdef __APPLE_OS_X_SERVER__
#define	ODA_NO_ERROR			0
#define	ODA_AUTH_FAILED			-2000
#define	ODA_AUTH_CANCEL			-2001
#define	ODA_PROTOCOL_ERROR		-2002

typedef enum {
	eAODNoErr				= 0,
	eAODParamErr			= -1,
	eAODOpenDSFailed		= -2,
	eAODOpenSearchFailed	= -3,
	eAODUserNotFound		= -4,
	eAODCantOpenUserNode	= -5,
	eAODAuthFailed			= -6,
	eAODAuthWarnNewPW		= -7,
	eAODAuthWarnExpirePW	= -8,
	eAOD					= 0xFF
} eAODError;

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

extern void smtpd_sasl_initialize(int);
extern char *smtpd_pw_server_authenticate(SMTPD_STATE *, const char *, const char *);
#else /* __APPLE_OS_X_SERVER__ */
extern void smtpd_sasl_initialize(void);
#endif /* __APPLE_OS_X_SERVER__ */
extern void smtpd_sasl_connect(SMTPD_STATE *, const char *, const char *);
extern void smtpd_sasl_disconnect(SMTPD_STATE *);
extern int smtpd_sasl_authenticate(SMTPD_STATE *, const char *, const char *);
extern void smtpd_sasl_logout(SMTPD_STATE *);
extern int permit_sasl_auth(SMTPD_STATE *, int, int);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Initial implementation by:
/*	Till Franke
/*	SuSE Rhein/Main AG
/*	65760 Eschborn, Germany
/*
/*	Adopted by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/
