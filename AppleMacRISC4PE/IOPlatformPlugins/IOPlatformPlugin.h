/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


#ifndef _IOPLATFORMPLUGIN_H
#define _IOPLATFORMPLUGIN_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
//#include <IOKit/IOWorkLoop.h>
//#include <IOKit/IOCommandGate.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMessage.h>

__BEGIN_DECLS
#include <kern/thread_call.h>
__END_DECLS

#include "IOPlatformPluginDefs.h"
#include "IOPlatformStateSensor.h"
#include "IOPlatformControl.h"
#include "IOPlatformCtrlLoop.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define PLUGIN_DEBUG 1

#ifdef PLUGIN_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// compile in the PlatformConsole accessor in setProperties()
#define IMPLEMENT_SETPROPERTIES 1

/*!
    @class IOPlatformPlugin
    @abstract A class for monitor system functions such as power and thermal */
class IOPlatformPlugin : public IOService
{
    OSDeclareDefaultStructors(IOPlatformPlugin)	

private:

protected:

/* Messages from all sources are put into an IOPPluginEventData structure and
	processed under the protection of a command gate.  The parameter contents
	are keyed off the event type. */
typedef struct IOPPluginEventData
{
	UInt32 eventType;
	void * param1;
	void * param2;
	void * param3;
	void * param4;
};

/* Event types */
enum {
	IOPPluginEventSetAggressiveness	= 0,	// param1 = selector, param2 = newLevel
	IOPPluginEventSystemWillSleep	= 1,	// params all NULL
	IOPPluginEventSystemDidWake		= 2,	// params all NULL
	IOPPluginEventMessage			= 3,	// param1 = type, param2 = IOService * sender, param3 = dict
	IOPPluginEventPlatformFunction	= 4,	// params match callPlatformFunction() params
	IOPPluginEventSetProperties		= 5,	// param1 = OSObject * properties
	IOPPluginEventTimer				= 6,	// no params used
	IOPPluginEventSystemRestarting	= 7		// params all NULL
};

/* message types */
enum {
	kIOPPluginMessageRegister			= 1,
	kIOPPluginMessageUnregister			= 2,
	kIOPPluginMessageLowThresholdHit	= 3,
	kIOPPluginMessageHighThresholdHit	= 4,
	kIOPPluginMessageCurrentValue		= 5,
	kIOPPluginMessageStateChanged		= 6,
	kIOPPluginMessagePowerMonitor		= 7,
	kIOPPluginMessageError				= 8,
	kIOPPluginMessageGetPlatformID		= 9
};

/* internal power state */
enum {
	kIOPPluginSleeping,
	kIOPPluginRunning
};

	/* This lock protects _ALL_ of the internal state and instance data within the plugin and provides serialization for all entry points */
	IORecursiveLock *	gate;

	/* the deadline timer thread callout */
	thread_call_t		timerCallout;

	/* a pointer to the root power domain, initialized in ::start() */
	IOPMrootDomain *	pmRootDomain;

	/* initializes all IOPlatformPluginFamily symbols */
	virtual void 		initSymbols( void );

	/* unsynchronized event dispatcher -- all unsynchronized entry points call through dispatchEvent(), which in turn aquires the gate lock and passes control to the synchronized event handler, handleEvent(). */
	virtual IOReturn	dispatchEvent(IOPPluginEventData *event);  /* BLOCKING */

	/* synchronized event handler -- processes events under protection of the gate lock */
	virtual IOReturn	handleEvent(IOPPluginEventData *event);

	/* the array of IOPlatformSensor objects */
	OSArray *			sensors;

	/* an array of pointers to the IOPlatformSensor::infoDict dictionaries of all registered sensors.  This array is published in the I/O registry */
	OSArray *			sensorInfoDicts;

	/* the array of IOPlatformControl objects */
	OSArray *			controls;

	/* an array of pointers to the IOPlatformControl::infoDict dictionaries of all registered controls.  This array is published in the I/O registry */
	OSArray *			controlInfoDicts;

	/* the array of IOPlatformCtrlLoop objects */
	OSArray *			ctrlLoops;

	/* an array of pointers to the IOPlatformCtrlLoop::infoDict dictionaries of all control loops.  This array is published in the I/O registry */
	OSArray *			ctrlLoopInfoDicts;

	/* a flag to record whether or not the environment dictionary has changed during the current pass through handleEvent().  This flag is cleared after every invocation of handleEvent().  If it is set during a pass, then all the control loops are notified of the environment change and allowed a chance to react immediately */
	bool				envChanged;

	int					pluginPowerState;

/*!	@var envInfo The platform plugin maintains environmental information about the condition of the system, including clamshell state, user preferences and failure state.  This information is accessed by IOPlatformCtrlLoops  so that they can correctly choose their metastate. */
	OSDictionary *envInfo;

/*!
	@function probeConfig
	@abstract The platform plugin calls this method at system startup time to determine the machine configuration, i.e. uniprocessor, dual processor, etc.  The returned value is an index into the ConfigArray array in the machine's thermal profile.  The default implementation returns 0.  Subclasses should override this method with code capable of determining the machine's config.  */
	virtual UInt8 probeConfig( void );

/*!	@var machineConfig
	@abstract The platform plugin caches the results of a getConfig() call in this variable.  getConfig() should only be called from IOPlatformPlugin::start a single time, all subsequent need of the machine configuration index should use the contents of this variable. */
	UInt8 machineConfig;

/*!
	@function initThermalProfile
	@abstract Parse thermal profile data and set up all internal data structures.  This prepares the platform plugin to accept registration and power management messages, and starts a control registration timer that will trigger a failure mechanism if a control doesn't register. */
	virtual bool initThermalProfile(IOService *nub);

/*!
	@function setTimeout
	@abstract Check for outstanding deadlines with all the ctrl loops, and set a timer callback if necessary. */
	void setTimeout( const AbsoluteTime now );

/*!
	@function timerHandler
	@abstract serialized timer callback handler */
	void timerHandler( const AbsoluteTime now );

/*!
	@function sleepHandler
	@abstract serialized sleep notification handler */
	virtual IOReturn sleepHandler(void);

/*!
	@function wakeHandler
	@abstract serialized wake notification handler */
	virtual IOReturn wakeHandler(void);

/*!
	@function restartHandler
	@abstract serialized restart notification handler */
	virtual IOReturn restartHandler(void);

/*!
	@function messageHandler
	@abstract serialized message() notification handler */
	IOReturn messageHandler(UInt32 type, IOService *sender, OSDictionary *dict);

/*!
	@function setAggressivenessHandler
	@abstract serialized setAggressiveness() handler */
	IOReturn setAggressivenessHandler(unsigned long selector, unsigned long newLevel);

/*!
	@function registerHandler
	@abstract handles registration messages */
	IOReturn registrationHandler( IOService *sender, OSDictionary *dict );

/*!
	@function setPropertiesHandler
	@abstract handles setProperties calls */
	IOReturn setPropertiesHandler( OSObject * properties );

/*!
	@function environmentChanged
	@abstract Notify the ctrl loops that the environment dict has changed */
	virtual void		environmentChanged( void );

	virtual bool initControls( const OSArray * controlDicts );
	virtual bool initSensors( const OSArray * sensorDicts );
	virtual bool initCtrlLoops( const OSArray * ctrlLoopDicts );
	virtual bool validOnConfig( const OSArray * validConfigs );

public:

	virtual UInt8 getConfig( void );

/*!
	@function lookupSensorByID
	@abstract Search the list of known sensors and return the first one with the given ID
	@param sensorID The Sensor ID of the desired sensor */
	virtual IOPlatformSensor * lookupSensorByID( const OSNumber * sensorID ) const;

/*!
	@function lookupControlByID
	@abstract Search the list of known controls and return the first one with the given ID
	@param ctrlID The Control ID of the desired controller */
	virtual IOPlatformControl * lookupControlByID( const OSNumber * controlID ) const;

/*!
	@function lookupCtrlLoopByID
	@abstract Search the list of control loops and return the first one with the given ID
	@param ctrlLoopID The Control Loop ID of the desired Control Loop */
	virtual IOPlatformCtrlLoop * lookupCtrlLoopByID( const OSNumber * ctrlLoopID ) const;

/*!
	@function setEnv
	@abstract environmental dictionary accessors with OSDictionary semantics */
	virtual bool		setEnv(const OSString *aKey, const OSMetaClassBase *anObject);
	virtual bool		setEnv(const char *aKey, const OSMetaClassBase *anObject);
	virtual bool		setEnv(const OSSymbol *aKey, const OSMetaClassBase *anObject);

	virtual bool 		setEnvArray( const OSSymbol * aKey, const OSObject * setter, bool setting );
	virtual bool		envArrayCondIsTrue( const OSSymbol *cond ) const;
	virtual bool		envArrayCondIsTrueForObject( const OSObject * obj, const OSSymbol *cond ) const;

/*!
	@function getEnv
	@abstract environmental dictionary getter with OSDictionary::getObject semanitics */
	virtual OSObject	*getEnv(const char *aKey) const;
	virtual OSObject	*getEnv(const OSString *aKey) const;
	virtual OSObject	*getEnv(const OSSymbol *aKey) const;

	virtual bool 		start(IOService *nub);
	virtual void		stop(IOService *nub);
	virtual IOService *	probe(IOService *nub, SInt32 *score);
	virtual bool		init(OSDictionary *dict);
	virtual void		free(void);

	virtual IOReturn	powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
	virtual IOReturn	powerStateDidChangeTo( IOPMPowerFlags theFlags, unsigned long, IOService*);
	static	IOReturn	sysPowerDownHandler(void *, void *, UInt32, IOService *, void *, vm_size_t);
    virtual IOReturn	message( UInt32 type, IOService * provider, void * argument = 0 );
    virtual IOReturn	setAggressiveness(unsigned long selector, unsigned long newLevel);
	virtual IOReturn	setProperties( OSObject * properties );

/*!
	@function timerEventOccured
	@abstract Timer callback routine, conforms to IOTimerEventSource::Action prototype */
    static void			timerEventOccured( void * self );

	virtual void		sleepSystem( void );

};

#endif // _IOPLATFORMPLUGIN_H
