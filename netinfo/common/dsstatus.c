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

#include <NetInfo/dsstatus.h>

char *dsstatus_message(dsstatus s)
{
	switch (s)
	{
		case DSStatusOK: return "Operation Succeeded";
		case DSStatusInvalidStore: return "Invalid Datastore";
		case DSStatusNoFile: return "No Such File";
		case DSStatusReadFailed: return "Read Failed";
		case DSStatusWriteFailed: return "Write Failed";
		case DSStatusInvalidUpdate: return "Invalid Update on Clone";
		case DSStatusDuplicateRecord: return "Duplicate Record in Datastore";
		case DSStatusNoRootRecord: return "No Root Record in Datastore";
		case DSStatusLocked: return "Datastore Locked";
		case DSStatusInvalidRecord: return "Invalid Record";
		case DSStatusNoData: return "No Data";
		case DSStatusInvalidRecordID: return "Invalid Record ID";
		case DSStatusInvalidPath: return "Invalid Path";
		case DSStatusInvalidKey: return "Invalid Key";
		case DSStatusStaleRecord: return "Stale Record Serial Number";
		case DSStatusPathNotLocal: return "Path Not Local to Datastore";
		case DSStatusConstraintViolation: return "Constraint Violation";
		case DSStatusNamingViolation: return "Naming Violation";
		case DSStatusObjectClassViolation: return "Object Class Violation";
		case DSStatusInvalidSessionMode: return "Invalid Session Mode";
		case DSStatusInvalidSession: return "Invalid Session";
		case DSStatusAccessRestricted: return "Access Restricted";
		case DSStatusReadRestricted: return "Read Access Restricted";
		case DSStatusWriteRestricted: return "Write Access Restricted";
		case DSStatusFailed: return "Operation Failed";
	}

	return "Unknown Status Code";
}
