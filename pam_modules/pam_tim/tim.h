/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/*
 * Authenticate a user using TIM only.
 */
int timAuthenticate2WayRandom(const char *user, const char *passwd);

/*
 * Set user's password to pw. Agent is the current login user,
 * should be obtained from PAM_RUSER environment variable or
 * getlogin().
 */
int timSetPasswordForUser(const char *agent, const char *apw, const char *target, const char *tpw);

/*
 * Status string.
 */
const char *timStatusString(int status);

/*
 * TIM status codes.
 */
#define TimStatusOK				0
#define TimStatusContinue			1
#define TimStatusServerTimeout			2
#define TimStatusInvalidHandle			3
#define TimStatusSendFailed			4
#define TimStatusReceiveFailed			5
#define TimStatusBadPacket			6
#define TimStatusInvalidTag			7
#define TimStatusInvalidSession			8
#define TimStatusInvalidName			9
#define TimStatusUserUnknown			10
#define TimStatusUnrecoverablePassword 		11
#define TimStatusAuthenticationFailed		12
#define TimStatusBogusServer			13
#define TimStatusOperationFailed		14
#define TimStatusNotAuthorized			15
#define TimStatusNetInfoError			16
#define TimStatusContactMaster			17
#define TimStatusServiceUnavailable		18
#define TimStatusInvalidPassword		19
