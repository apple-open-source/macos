/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __DSSTATUS_H__
#define __DSSTATUS_H__
#include <sys/types.h>

typedef enum
{
	DSStatusOK = 0,
	DSStatusInvalidStore = 1001,
	DSStatusNoFile = 1002,
	DSStatusReadFailed = 1003,
	DSStatusWriteFailed = 1004,
	DSStatusInvalidUpdate = 1005,
	DSStatusDuplicateRecord = 1006,
	DSStatusNoRootRecord = 1007,
	DSStatusLocked = 1008,
	DSStatusInvalidRecord = 2001,
	DSStatusNoData = 2002,
	DSStatusInvalidRecordID = 2003,
	DSStatusInvalidPath = 2004,
	DSStatusInvalidKey = 2005,
	DSStatusStaleRecord = 2006,
	DSStatusPathNotLocal = 2007,
	DSStatusConstraintViolation = 2008,
	DSStatusNamingViolation = 2009,
	DSStatusObjectClassViolation = 2010,
	DSStatusInvalidSessionMode = 3001,
	DSStatusInvalidSession = 3002,
	DSStatusAccessRestricted = 4001,
	DSStatusReadRestricted = 4002,
	DSStatusWriteRestricted = 4003,
	DSStatusFailed = 9999
} dsstatus;

char *dsstatus_message(dsstatus);

#endif __DSSTATUS_H__
