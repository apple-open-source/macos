/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header DirServiceMain
 * Resource constant definitions.
 */

#ifndef __DirServiceMain_h__
#define __DirServiceMain_h__ 1


// #defines that describe this build.
#define PRODUCT		"Directory Server "
#ifdef DEBUG
#define PRODUCTVER	"X (Debug)"
#else
#define PRODUCTVER	"X"
#endif

#define APPNAME		"DirectoryService"
#define APPFULLNAME	PRODUCT APPNAME

#define VER_MAJ		1
#define VER_MIN		0
#ifdef DEBUG
#define STAGE		developStage
#define VER_DEV		1
#define CAND_VER	1
#else	/* DEBUG */
#define STAGE		finalStage
#define VER_DEV		0
#endif	/* DEBUG */

#if rez
# ifdef DEBUG
#  define APPVERSION $$format("%d.%dd%dc%d (Debug)", VER_MAJ, VER_MIN, VER_DEV, CAND_VER)
# else
#  define APPVERSION $$format("%d.%d", VER_MAJ, VER_MIN)
#endif	/* DEBUG */
#endif	/* rez */


// Main App's error types
typedef enum {
		kAppNoErr					=    0,
		kAppInvalidOSTypeErr		= -666,
		kAppInvalidMachineTypeErr,
		kAppInvalidOSReleaseErr,
		kAppInvalidUserErr,
		kAppAlreadyRunningErr,
		kAppInvalidFSErr,
		kAppFSTooSmallErr,
		kAppRunDirErr,
		kAppMemoryErr,
		kAppUnknownErr				= 0xFF
} eAppError;

// Machine (hardware) types
typedef enum {
	kMachineUnknown = 0,
	kMachinePowerMac,
	kMachineIntel
} eMachineType;


// Operating System types
typedef enum {
	kOSUnknown = 0,
	kOSRhapsody,
	kOSMacOSX,
	kOSDarwin
} eOSType;


// OS Releases tyeps
typedef enum {
	kOSReleaseUnknown		= 0,
	kOSReleaseRhapDev,
	kOSReleaseMacOSXServer,
	kOSReleaseMacOSXDestop
} eOSRelease;


// Application specific constants.
enum eAppResourceSigs {
	kAppSignature = 'ISDs'		// Creator signature and resource type
};

// STR# offsets
enum eStdAlertStrOffsets {
	kStdAlertTitleStr = 1,
	kStdAlertTextStr = 2
};

// STR# ResID's for error conditions.
enum eStartupErr {
	kStartupOK = 0,
	kErrOSTooOld = 200,
	kErrNoThreads,
	kErrOTTooOld,
	kErrNoAppleTalk,
	kErrNoMachineName,
	kErrNoMemory,
	kErrNoRegistry,
	// Warnings
	kWarnOSTooOld = 400
};

#endif //  __DirServiceMain_h__

