/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

/*
 *  checkpw.c
 *  utility to authenticate users using crypt with a fallback on Directory Services
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#include <pwd.h>
#include <stddef.h>		// for offsetof()
#include <stdlib.h> // for malloc()
#include <string.h> // for strcmp()
#include <time.h>
#include <unistd.h> // for crypt()
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/message.h>
#include <servers/bootstrap.h>
#include "checkpw.h"

// begin copied from SharedConsts.h
typedef struct {
	unsigned int	msgt_name : 8,
					msgt_size : 8,
					msgt_number : 12,
					msgt_inline : 1,
					msgt_longform : 1,
					msgt_deallocate : 1,
					msgt_unused : 1;
} mach_msg_type_t;

typedef struct sObject
{
	unsigned long	type;
	unsigned long	count;
	unsigned long	offset;
	unsigned long	used;
	unsigned long	length;
} sObject;

typedef struct sComData
{
	mach_msg_header_t	head;
	mach_msg_type_t		type;
	unsigned long		fDataSize;
	unsigned long		fDataLength;
	unsigned long		fMsgID;
	unsigned long		fPID;
	unsigned long		fPort;
	unsigned long           fIPAddress;
	sObject				obj[ 10 ];
	char				data[ 1 ];
} sComData;

#define kMsgBlockSize	(1024 * 4)					// Set to average of 4k
#define kObjSize		(sizeof( sObject ) * 10)	// size of object struct
#define kIPCMsgLen		kMsgBlockSize				// IPC message block size

#define kIPCMsgSize	sizeof( sIPCMsg )

typedef struct sIPCMsg
{
	mach_msg_header_t	fHeader;
	unsigned long		fMsgType;
	unsigned long		fCount;
	unsigned long		fOf;
	unsigned long		fMsgID;
	unsigned long		fPID;
	unsigned long		fPort;
	sObject				obj[ 10 ];
	char				fData[ kIPCMsgLen ];
	mach_msg_security_trailer_t	fTail;
} sIPCMsg;

typedef enum {
	kResult					= 4460,
	ktDataBuff				= 4466,
} eValueType;

enum eDSServerCalls {
	/*  9 */ kCheckUserNameAndPassword = 9
};
// end copied from SharedConsts.h

int checkpw( const char* userName, const char* password )
{
	struct passwd* pw = NULL;
    int status;

	// Check username, NULL can crash in getpwnam
	if (!userName)
		return CHECKPW_UNKNOWNUSER;
    
    pw = getpwnam( userName );
	if (pw == NULL)
		return CHECKPW_UNKNOWNUSER;

    status = checkpw_internal(userName, password, pw);
    endpwent();
    return status;
}

int checkpw_internal( const char* userName, const char* password, const struct passwd* pw )
{
	int siResult = CHECKPW_FAILURE;
	kern_return_t	result = err_none;
	mach_port_t		bsPort = 0;
	mach_port_t		serverPort = 0;
	mach_port_t		replyPort = 0;
	const char *const srvrName = "DirectoryService";
	sIPCMsg*		msg = NULL;
	unsigned long	len = 0;
	long			curr = 0;
	unsigned long	i = 0;

	
	do {
		// Special case for empty password (this explicitly denies UNIX-like behavior)
		if (pw->pw_passwd == NULL || pw->pw_passwd[0] == '\0') {
			if (password == NULL || password[0] == '\0')
				siResult = CHECKPW_SUCCESS;
			else
				siResult = CHECKPW_BADPASSWORD;
				
			break;
		}

		// check password, NULL crashes crypt()
		if (!password)
		{
			siResult = CHECKPW_BADPASSWORD;
			break;
		}
		// Correct password hash
		if (strcmp(crypt(password, pw->pw_passwd), pw->pw_passwd) == 0) {
			siResult = CHECKPW_SUCCESS;
			break;
		}

		// Try Directory Services directly

		result = mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &replyPort );
		if ( result != err_none ) {
			siResult = CHECKPW_FAILURE;
			break;
		}

		result = task_get_bootstrap_port( mach_task_self(), &bsPort );
		if ( result != err_none ) {
			siResult = CHECKPW_FAILURE;
			break;
		}

		// check if DirectoryService is alive
		result = bootstrap_look_up( bsPort, (char *)srvrName, &serverPort );
		if ( result != err_none ) {
			siResult = CHECKPW_FAILURE;
			break;
		}

		// ask directory services to do auth
		msg = calloc( sizeof( sIPCMsg ), 1 );
		if ( msg == NULL ) {
			siResult = CHECKPW_FAILURE; // memory error
			break;
		}

		// put username and password into message
		msg->obj[0].type = ktDataBuff;
		msg->obj[0].count = 1;
		msg->obj[0].offset = offsetof(struct sComData, data);

		// User Name
		len = strlen( userName );
		if (curr + len + sizeof(unsigned long) > kIPCMsgLen)
		{
			siResult = CHECKPW_FAILURE;
			break;
		}
		memcpy( &(msg->fData[ curr ]), &len, sizeof( unsigned long ) );
		curr += sizeof( unsigned long );
		memcpy( &(msg->fData[ curr ]), userName, len );
		curr += len;

		// Password
		len = strlen( password );
		if (curr + len + sizeof(unsigned long) > kIPCMsgLen)
		{
			siResult = CHECKPW_FAILURE;
			break;
		}
		memcpy( &(msg->fData[ curr ]), &len, sizeof( unsigned long ) );
		curr += sizeof ( unsigned long );
		memcpy( &(msg->fData[ curr ]), password, len );
		curr += len;
		msg->obj[0].used = curr;
		msg->obj[0].length = curr;
		
		msg->fHeader.msgh_bits			= MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND );
		msg->fHeader.msgh_size			= sizeof( sIPCMsg ) - sizeof( mach_msg_security_trailer_t );
		msg->fHeader.msgh_id			= kCheckUserNameAndPassword;
		msg->fHeader.msgh_remote_port	= serverPort;
		msg->fHeader.msgh_local_port	= replyPort;
	
		msg->fMsgType	= kCheckUserNameAndPassword;
		msg->fCount		= 1;
		msg->fOf		= 1;
		msg->fPort		= replyPort;
		msg->fPID		= getpid();
		msg->fMsgID		= time( NULL ) + kCheckUserNameAndPassword;
			
		// send the message
		result = mach_msg_send( (mach_msg_header_t*)msg );
		if ( result != MACH_MSG_SUCCESS ) {
			siResult = CHECKPW_FAILURE;
			break;
		}
			
		// get reply
		memset( msg, 0, kIPCMsgLen );
	
		result = mach_msg( (mach_msg_header_t *)msg, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
							0, kIPCMsgSize, replyPort, 300 * 1000, MACH_PORT_NULL );

		if ( result != MACH_MSG_SUCCESS ) {
			siResult = CHECKPW_FAILURE;
			break;
		}

		if ( msg->fCount != 1 ) {
			// couldn't get reply
			siResult = CHECKPW_FAILURE;
			break;
		}

		for (i = 0; i < 10; i++ )
		{
			if ( msg->obj[ i ].type == (unsigned long)kResult )
			{
				siResult = msg->obj[ i ].count;
				break;
			}
		}
		
	} while (0);
	
	if (msg != NULL) {
		free(msg);
		msg = NULL;
	}
	
	if ( replyPort != 0 )
		mach_port_deallocate( mach_task_self(), replyPort );
	

	return siResult;
}
