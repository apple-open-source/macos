/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 2001 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stddef.h>		// for offsetof()
#include <stdlib.h>		// for malloc()
#include <string.h>		// for strcmp()
#include <time.h>
#include <unistd.h>		// for crypt()
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/message.h>
#include <servers/bootstrap.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
/* #define PAM_SM_PASSWORD */

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_mod_misc.h>

#define PASSWORD_PROMPT	"Password:"
#define OLD_PASSWORD_PROMPT "Enter login(DirectoryService) password:"
#define NEW_PASSWORD_PROMPT "New password:"
#define AGAIN_PASSWORD_PROMPT "Retype new password:"

static int authenticateUsingDirectoryServices(const char *userName,
					      const char *password);

// begin copied from SharedConsts.h
typedef struct {
    unsigned int msgt_name:8,
	msgt_size:8,
	msgt_number:12,
	msgt_inline:1, msgt_longform:1, msgt_deallocate:1, msgt_unused:1;
} mach_msg_type_t;

typedef struct sObject {
    unsigned long type;
    unsigned long count;
    unsigned long offset;
    unsigned long used;
    unsigned long length;
} sObject;

typedef struct sComData {
    mach_msg_header_t head;
    mach_msg_type_t type;
    unsigned long fDataSize;
    unsigned long fMsgID;
    unsigned long fPID;
    unsigned long fPort;
    sObject obj[10];
    char data[1];
    mach_msg_trailer_t tail;
} sComData;

#define kMsgBlockSize   (1024 * 4)	// Set to average of 4k
#define kObjSize                (sizeof( sObject ) * 10)	// size of object struct
#define kIPCMsgLen              kMsgBlockSize + kObjSize	// IPC message block size

#define kIPCMsgSize     sizeof( sIPCMsg )

typedef struct sIPCMsg {
    mach_msg_header_t fHeader;
    unsigned long fMsgType;
    unsigned long fCount;
    unsigned long fOf;
    unsigned long fMsgID;
    unsigned long fPID;
    unsigned long fPort;
    sObject obj[10];
    char fData[kIPCMsgLen];
    mach_msg_trailer_t fTail;
} sIPCMsg;

typedef enum {
    kResult = 4460,
    ktDataBuff = 4466,
} eValueType;

enum eDSServerCalls {
    /*  8 */ kCheckUserNameAndPassword = 8
};

// end copied from SharedConsts.h

static int
authenticateUsingDirectoryServices(const char *userName,
				   const char *password)
{
    int siResult = PAM_SERVICE_ERR;
    kern_return_t result = err_none;
    mach_port_t bsPort = 0;
    mach_port_t serverPort = 0;
    mach_port_t replyPort = 0;
    const char *const srvrName = "DirectoryService";
    sIPCMsg *msg = NULL;
    unsigned long len = 0;
    long curr = 0;
    unsigned long i = 0;


    do {
	result =
	    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
			       &replyPort);
	if (result != err_none) {
	    siResult = PAM_SERVICE_ERR;
	    break;
	}

	result = task_get_bootstrap_port(mach_task_self(), &bsPort);
	if (result != err_none) {
	    siResult = PAM_SERVICE_ERR;
	    break;
	}
	// check if DirectoryService is alive
	result = bootstrap_look_up(bsPort, (char *) srvrName, &serverPort);
	if (result != err_none) {
	    siResult = PAM_BUF_ERR;
	    break;
	}
	// ask directory services to do auth
	msg = calloc(sizeof(sIPCMsg), 1);
	if (msg == NULL) {
	    siResult = PAM_BUF_ERR;	// memory error
	    break;
	}
	// put username and password into message
	msg->obj[0].type = ktDataBuff;
	msg->obj[0].count = 1;
	msg->obj[0].offset = offsetof(struct sComData, data);

	// User Name
	len = strlen(userName);
	memcpy(&(msg->fData[curr]), &len, sizeof(unsigned long));
	curr += sizeof(unsigned long);
	memcpy(&(msg->fData[curr]), userName, len);
	curr += len;

	// Password
	len = strlen(password);
	memcpy(&(msg->fData[curr]), &len, sizeof(unsigned long));
	curr += sizeof(unsigned long);
	memcpy(&(msg->fData[curr]), password, len);
	curr += len;
	msg->obj[0].used = curr;
	msg->obj[0].length = curr;

	msg->fHeader.msgh_bits =
	    MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
			   MACH_MSG_TYPE_MAKE_SEND);
	msg->fHeader.msgh_size =
	    sizeof(sIPCMsg) - sizeof(mach_msg_header_t);
	msg->fHeader.msgh_id = kCheckUserNameAndPassword;
	msg->fHeader.msgh_remote_port = serverPort;
	msg->fHeader.msgh_local_port = replyPort;

	msg->fMsgType = kCheckUserNameAndPassword;
	msg->fCount = 1;
	msg->fOf = 1;
	msg->fPort = replyPort;
	msg->fPID = getpid();
	msg->fMsgID = time(NULL) + kCheckUserNameAndPassword;

	// send the message
	result = mach_msg_send((mach_msg_header_t *) msg);
	if (result != MACH_MSG_SUCCESS) {
	    siResult = PAM_SERVICE_ERR;
	    break;
	}
	// get reply
	memset(msg, 0, kIPCMsgLen);

	msg->fHeader.msgh_bits =
	    MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, MACH_RCV_TIMEOUT);
	msg->fHeader.msgh_size = kIPCMsgSize;
	msg->fHeader.msgh_remote_port = serverPort;
	msg->fHeader.msgh_local_port = replyPort;

	result =
	    mach_msg((mach_msg_header_t *) msg,
		     MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
		     msg->fHeader.msgh_size,
		     msg->fHeader.msgh_local_port, 300 * 1000,
		     MACH_PORT_NULL);

	if (result == MACH_MSG_SUCCESS) {
	    siResult = PAM_SERVICE_ERR;
	    break;
	}

	if (msg->fCount != 1) {
	    /* couldn't get reply */
	    siResult = PAM_AUTHINFO_UNAVAIL;
	    break;
	}

	for (i = 0; i < 10; i++) {
	    if (msg->obj[i].type == (unsigned long) kResult) {
		siResult = msg->obj[i].count;
		break;
	    }
	}

    }
    while (0);

    if (msg != NULL) {
	free(msg);
	msg = NULL;
    }

    if (replyPort != 0)
	mach_port_deallocate(mach_task_self(), replyPort);

    return siResult;
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc,
		    const char **argv)
{
    int status;
    const char *user;
    const char *password;
    struct options options;

    pam_std_option(&options, NULL, argc, argv);

    status = pam_get_user(pamh, &user, NULL);
    if (status != PAM_SUCCESS) {
	return status;
    }

    status = pam_get_pass(pamh, &password, PASSWORD_PROMPT, &options);
    if (status != PAM_SUCCESS) {
	return status;
    }

    return authenticateUsingDirectoryServices(user, password);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags, int argc,
		 const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t * pamh, int flags,
		    int argc, const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t * pamh, int flags,
		     int argc, const char **argv)
{
    return PAM_SUCCESS;
}

PAM_MODULE_ENTRY("pam_directoryservice");
