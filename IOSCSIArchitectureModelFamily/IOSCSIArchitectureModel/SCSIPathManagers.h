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



#ifndef __IOKIT_SCSI_PATH_MANAGERS_H__
#define __IOKIT_SCSI_PATH_MANAGERS_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern includes
#include <libkern/c++/OSArray.h>

// IOKit includes
#include <IOKit/IOLocks.h>

// SCSI Parallel Interface Family includes
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>

// SCSI Architecture Model Family includes
#include "SCSITargetDevicePathManager.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIPathSet : public OSArray
{
	
	OSDeclareDefaultStructors ( SCSIPathSet )
	
public:
	
	static SCSIPathSet *	withCapacity ( unsigned int capacity );
	bool 					setObject ( const SCSITargetDevicePath * path );
	bool 					member ( const SCSITargetDevicePath * path ) const;
	bool					member ( const IOSCSIProtocolServices * interface ) const;
	void					removeObject ( const IOSCSIProtocolServices * interface );
	SCSITargetDevicePath *	getAnyObject ( void ) const;
	SCSITargetDevicePath *	getObject ( unsigned int index ) const;
	SCSITargetDevicePath *	getObjectWithInterface ( const IOSCSIProtocolServices * interface ) const;
	
};

class SCSIFailoverPathManager : public SCSITargetDevicePathManager
{
	
	OSDeclareDefaultStructors ( SCSIFailoverPathManager )
	
private:
	
	SCSIPathSet *	fPathSet;
	SCSIPathSet *	fInactivePathSet;
	IOLock *		fLock;
	
protected:
	
	bool InitializePathManagerForTarget (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	void free ( void );
	
public:
	
	static SCSIFailoverPathManager * Create (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	void							ExecuteCommand ( SCSITaskIdentifier request );
	virtual SCSIServiceResponse		AbortTask ( SCSILogicalUnitNumber theLogicalUnit, SCSITaggedTaskIdentifier theTag );
	virtual SCSIServiceResponse		AbortTaskSet ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		ClearACA ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		ClearTaskSet ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		LogicalUnitReset ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		TargetReset ( void );
	
	bool		AddPath ( IOSCSIProtocolServices * path );
	void		ActivatePath ( IOSCSIProtocolServices * path );
	void		InactivatePath ( IOSCSIProtocolServices * path );
	void		RemovePath ( IOSCSIProtocolServices * path );
	void		PathStatusChanged ( IOSCSIProtocolServices * path, SPIPortStatus newStatus );
	
};

class SCSIRoundRobinPathManager : public SCSITargetDevicePathManager
{
	
	OSDeclareDefaultStructors ( SCSIRoundRobinPathManager )
	
private:
	
	SCSIPathSet *	fPathSet;
	SCSIPathSet *	fInactivePathSet;
	IOLock *		fLock;
	UInt32			fPathNumber;
	
protected:
	
	bool InitializePathManagerForTarget (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	void free ( void );
	
public:
	
	static SCSIRoundRobinPathManager * Create (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	void							ExecuteCommand ( SCSITaskIdentifier request );
	virtual SCSIServiceResponse		AbortTask ( SCSILogicalUnitNumber theLogicalUnit, SCSITaggedTaskIdentifier theTag );
	virtual SCSIServiceResponse		AbortTaskSet ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		ClearACA ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		ClearTaskSet ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		LogicalUnitReset ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		TargetReset ( void );
	
	bool		AddPath ( IOSCSIProtocolServices * path );
	void		ActivatePath ( IOSCSIProtocolServices * path );
	void		InactivatePath ( IOSCSIProtocolServices * path );
	void		RemovePath ( IOSCSIProtocolServices * path );
	void		PathStatusChanged ( IOSCSIProtocolServices * path, SPIPortStatus newStatus );
	
};


#endif	/* __IOKIT_SCSI_PATH_MANAGERS_H__ */