/*
 * Copyright (c) 1998-2020 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "RootDomainUserClient.h"
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <sys/proc.h>

#define super IOUserClient2022

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(RootDomainUserClient, IOUserClient2022)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
RootDomainUserClient::initWithTask(task_t owningTask, void *security_id,
    UInt32 type, OSDictionary * properties)
{
	if (properties) {
		properties->setObject(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}

	if (!super::initWithTask(owningTask, security_id, type, properties)) {
		return false;
	}

	fOwningTask = owningTask;
	task_reference(fOwningTask);
	return true;
}


bool
RootDomainUserClient::start( IOService * provider )
{
	assert(OSDynamicCast(IOPMrootDomain, provider));
	if (!super::start(provider)) {
		return false;
	}
	fOwner = (IOPMrootDomain *)provider;

	setProperty(kIOUserClientDefaultLockingKey, kOSBooleanTrue);
	setProperty(kIOUserClientDefaultLockingSetPropertiesKey, kOSBooleanTrue);
	setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanFalse);

	setProperty(kIOUserClientEntitlementsKey, kOSBooleanFalse);

	return true;
}

IOReturn
RootDomainUserClient::secureSleepSystem( uint32_t *return_code )
{
	return secureSleepSystemOptions(NULL, 0, return_code);
}

IOReturn
RootDomainUserClient::secureSleepSystemOptions(
	const void      *inOptions,
	IOByteCount     inOptionsSize,
	uint32_t        *returnCode)
{
	int             local_priv = 0;
	int             admin_priv = 0;
	IOReturn        ret = kIOReturnNotPrivileged;

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeLocalUser);
	local_priv = (kIOReturnSuccess == ret);

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
	admin_priv = (kIOReturnSuccess == ret);

	if ((local_priv || admin_priv) && fOwner) {
		OSString        *unserializeErrorString = NULL;
		OSObject        *unserializedObject = NULL;
		OSDictionary    *sleepOptionsDict = NULL; // do not release

		proc_t p;
		p = (proc_t)get_bsdtask_info(fOwningTask);
		if (p) {
			fOwner->setProperty("SleepRequestedByPID", proc_pid(p), 32);
		}

		if (inOptions) {
			unserializedObject = OSUnserializeXML((const char *)inOptions, inOptionsSize, &unserializeErrorString);
			sleepOptionsDict = OSDynamicCast( OSDictionary, unserializedObject);
			if (!sleepOptionsDict) {
				IOLog("IOPMRootDomain SleepSystem unserialization failure: %s\n",
				    unserializeErrorString ? unserializeErrorString->getCStringNoCopy() : "Unknown");
			}
		}

		if (sleepOptionsDict) {
			// Publish Sleep Options in registry under root_domain
			fOwner->setProperty( kRootDomainSleepOptionsKey, sleepOptionsDict);
		} else {
			// No options
			// Clear any pre-existing options
			fOwner->removeProperty( kRootDomainSleepOptionsKey );
		}

		*returnCode = fOwner->sleepSystemOptions( sleepOptionsDict );
		OSSafeReleaseNULL(unserializedObject);
		OSSafeReleaseNULL(unserializeErrorString);
	} else {
		*returnCode = kIOReturnNotPrivileged;
	}

	return kIOReturnSuccess;
}

IOReturn
RootDomainUserClient::secureSetAggressiveness(
	unsigned long   type,
	unsigned long   newLevel,
	int             *return_code )
{
	int             local_priv = 0;
	int             admin_priv = 0;
	IOReturn        ret = kIOReturnNotPrivileged;

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeLocalUser);
	local_priv = (kIOReturnSuccess == ret);

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
	admin_priv = (kIOReturnSuccess == ret);

	if ((local_priv || admin_priv) && fOwner) {
		*return_code = fOwner->setAggressiveness(type, newLevel);
	} else {
		*return_code = kIOReturnNotPrivileged;
	}
	return kIOReturnSuccess;
}

IOReturn
RootDomainUserClient::secureSetMaintenanceWakeCalendar(
	IOPMCalendarStruct      *inCalendar,
	uint32_t                *returnCode)
{
	int                     admin_priv = 0;
	IOReturn                ret = kIOReturnNotPrivileged;

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
	admin_priv = (kIOReturnSuccess == ret);

	if (admin_priv && fOwner) {
		*returnCode = fOwner->setMaintenanceWakeCalendar(inCalendar);
	} else {
		*returnCode = kIOReturnNotPrivileged;
	}
	return kIOReturnSuccess;
}

IOReturn
RootDomainUserClient::secureSetUserAssertionLevels(
	uint32_t    assertionBitfield)
{
	int                     admin_priv = 0;
	IOReturn                ret = kIOReturnNotPrivileged;

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
	admin_priv = (kIOReturnSuccess == ret);

	if (admin_priv && fOwner) {
		ret = fOwner->setPMAssertionUserLevels(assertionBitfield);
	} else {
		ret = kIOReturnNotPrivileged;
	}
	return kIOReturnSuccess;
}

IOReturn
RootDomainUserClient::secureGetSystemSleepType(
	uint32_t    *outSleepType, uint32_t *sleepTimer)
{
	int                     admin_priv = 0;
	IOReturn                ret;

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
	admin_priv = (kIOReturnSuccess == ret);

	if (admin_priv && fOwner) {
		ret = fOwner->getSystemSleepType(outSleepType, sleepTimer);
	} else {
		ret = kIOReturnNotPrivileged;
	}
	return ret;
}

IOReturn
RootDomainUserClient::secureAttemptIdleSleepAbort(
	uint32_t    *outReverted)
{
	int                     admin_priv = 0;
	IOReturn                ret;

	ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
	admin_priv = (kIOReturnSuccess == ret);

	if (admin_priv && fOwner) {
		*outReverted = (uint32_t) fOwner->attemptIdleSleepAbort();
	} else {
		ret = kIOReturnNotPrivileged;
	}
	return ret;
}

IOReturn
RootDomainUserClient::clientClose( void )
{
	terminate();

	return kIOReturnSuccess;
}

void
RootDomainUserClient::stop( IOService *provider)
{
	if (fOwningTask) {
		task_deallocate(fOwningTask);
		fOwningTask = NULL;
	}

	super::stop(provider);
}

IOReturn
RootDomainUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args )
{
	static const IOExternalMethodDispatch2022 dispatchArray[] = {
		[kPMSetAggressiveness] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 2,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 1,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMGetAggressiveness] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 1,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 1,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSleepSystem] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 1,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMAllowPowerChange] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 1,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMCancelPowerChange] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 1,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMShutdownSystem] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMRestartSystem] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSleepSystemOptions] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = sizeof(uint32_t),
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSetMaintenanceWakeCalendar] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = sizeof(IOPMCalendarStruct),
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = sizeof(uint32_t),
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSetUserAssertionLevels] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 1,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMActivityTickle] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSetClamshellSleepState] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 1,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMGetSystemSleepType] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 2,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSleepWakeWatchdogEnable] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSleepWakeDebugTrig] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMSetDisplayPowerOn] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 1,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kPMRequestIdleSleepRevert] = {
			.function                 = &RootDomainUserClient::externalMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = 0,
			.checkScalarOutputCount   = 1,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
	};

	return dispatchExternalMethod(selector, args, dispatchArray, sizeof(dispatchArray) / sizeof(dispatchArray[0]), this, NULL);
}
IOReturn
RootDomainUserClient::externalMethodDispatched(OSObject * target, void * reference, IOExternalMethodArguments * arguments)
{
	IOReturn    ret = kIOReturnBadArgument;
	RootDomainUserClient * me = (typeof(me))target;
	switch (arguments->selector) {
	case kPMSetAggressiveness:
		ret = me->secureSetAggressiveness(
			(unsigned long)arguments->scalarInput[0],
			(unsigned long)arguments->scalarInput[1],
			(int *)&arguments->scalarOutput[0]);
		break;

	case kPMGetAggressiveness:
		ret = me->fOwner->getAggressiveness(
			(unsigned long)arguments->scalarInput[0],
			(unsigned long *)&arguments->scalarOutput[0]);
		break;

	case kPMSleepSystem:
		ret = me->secureSleepSystem(
			(uint32_t *)&arguments->scalarOutput[0]);
		break;

	case kPMAllowPowerChange:
		ret = me->fOwner->allowPowerChange(
			arguments->scalarInput[0]);
		break;

	case kPMCancelPowerChange:
		ret = me->fOwner->cancelPowerChange(
			arguments->scalarInput[0]);
		break;

	case kPMShutdownSystem:
		// deprecated interface
		ret = kIOReturnUnsupported;
		break;

	case kPMRestartSystem:
		// deprecated interface
		ret = kIOReturnUnsupported;
		break;

	case kPMSleepSystemOptions:
		ret = me->secureSleepSystemOptions(
			arguments->structureInput,
			arguments->structureInputSize,
			(uint32_t *)&arguments->structureOutput);
		break;
	case kPMSetMaintenanceWakeCalendar:
		ret = me->secureSetMaintenanceWakeCalendar(
			(IOPMCalendarStruct *)arguments->structureInput,
			(uint32_t *)&arguments->structureOutput);
		arguments->structureOutputSize = sizeof(uint32_t);
		break;

	case kPMSetUserAssertionLevels:
		ret = me->secureSetUserAssertionLevels(
			(uint32_t)arguments->scalarInput[0]);
		break;

	case kPMActivityTickle:
		if (me->fOwner->checkSystemCanSustainFullWake()) {
			me->fOwner->reportUserInput();
			me->fOwner->setProperty(kIOPMRootDomainWakeTypeKey, "UserActivity Assertion");
		}
		ret = kIOReturnSuccess;
		break;

	case kPMSetClamshellSleepState:
		me->fOwner->setClamShellSleepDisable(arguments->scalarInput[0] ? true : false,
		    IOPMrootDomain::kClamshellSleepDisablePowerd);
		ret = kIOReturnSuccess;
		break;

	case kPMGetSystemSleepType:
		ret = me->secureGetSystemSleepType(
			(uint32_t *) &arguments->scalarOutput[0],
			(uint32_t *) &arguments->scalarOutput[1]);
		break;

#if defined(__i386__) || defined(__x86_64__)
	case kPMSleepWakeWatchdogEnable:
		ret = clientHasPrivilege(me->fOwningTask, kIOClientPrivilegeAdministrator);
		if (ret == kIOReturnSuccess) {
			me->fOwner->sleepWakeDebugEnableWdog();
		}
		break;

	case kPMSleepWakeDebugTrig:
		ret = clientHasPrivilege(me->fOwningTask, kIOClientPrivilegeAdministrator);
		if (ret == kIOReturnSuccess) {
			me->fOwner->sleepWakeDebugTrig(false);
		}
		break;
#endif

	case kPMSetDisplayPowerOn:
		ret = clientHasPrivilege(me->fOwningTask, kIOClientPrivilegeAdministrator);
		if (ret == kIOReturnSuccess) {
			me->fOwner->setDisplayPowerOn((uint32_t)arguments->scalarInput[0]);
		}
		break;

	case kPMRequestIdleSleepRevert:
		ret = me->secureAttemptIdleSleepAbort(
			(uint32_t *) &arguments->scalarOutput[0]);
		break;


	default:
		// bad selector
		return kIOReturnBadArgument;
	}

	return ret;
}

/* getTargetAndMethodForIndex
 * Not used. We prefer to use externalMethod() for user client invocations.
 * We maintain getTargetAndExternalMethod since it's an exported symbol,
 * and only for that reason.
 */
IOExternalMethod *
RootDomainUserClient::getTargetAndMethodForIndex(
	IOService ** targetP, UInt32 index )
{
	// DO NOT EDIT
	return super::getTargetAndMethodForIndex(targetP, index);
}

/* setPreventative
 * Does nothing. Exists only for exported symbol compatibility.
 */
void
RootDomainUserClient::setPreventative(UInt32 on_off, UInt32 types_of_sleep)
{
	return;
}           // DO NOT EDIT
