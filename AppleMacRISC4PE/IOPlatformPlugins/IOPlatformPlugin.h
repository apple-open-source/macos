/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: IOPlatformPlugin.h,v $
//		Revision 1.9  2003/07/17 06:57:36  eem
//		3329222 and other sleep stability issues fixed.
//		
//		Revision 1.8  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.7  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.6  2003/06/25 02:16:24  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.5.4.3  2003/06/20 09:07:33  eem
//		Added rising/falling slew limiters, integral clipping, etc.
//		
//		Revision 1.5.4.2  2003/06/20 01:39:58  eem
//		Although commented out in this submision, there is support here to nap
//		the processors if the fans are at min, with the intent of keeping the
//		heat sinks up to temperature.
//		
//		Revision 1.5.4.1  2003/06/19 10:24:16  eem
//		Pulled common PID code into IOPlatformPIDCtrlLoop and subclassed it with
//		PowerMac7_2_CPUFanCtrlLoop and PowerMac7_2_PIDCtrlLoop.  Added history
//		length to meta-state.  No longer adjust T_err when the setpoint changes.
//		Don't crank the CPU fans for overtemp, just slew slow.
//		
//		Revision 1.5  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.8  2003/06/06 12:16:49  eem
//		Updated strings, turned off debugging.
//		
//		Revision 1.4.2.7  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.4.2.6  2003/06/02 18:23:16  eem
//		Fixed compilation errors.
//		
//		Revision 1.4.2.5  2003/06/01 14:52:51  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.4.2.4  2003/05/31 08:11:34  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.4.2.3  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.4.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.4.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.4  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.3.2.1  2003/05/14 22:07:49  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.3  2003/05/13 02:13:51  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.2.2.1  2003/05/12 11:21:10  eem
//		Support for slewing.
//		
//		Revision 1.2  2003/05/10 06:50:33  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.1.1.2.4  2003/05/10 06:32:34  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.1.1.1.2.3  2003/05/05 21:29:37  eem
//		Checkin 1.1.0d11 for PD distro and submission.  Debugging turned off.
//		
//		Revision 1.1.1.1.2.2  2003/05/03 01:11:38  eem
//		*** empty log message ***
//		
//		Revision 1.1.1.1.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#ifndef _IOPLATFORMPLUGIN_H
#define _IOPLATFORMPLUGIN_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
//#include <IOKit/IOWorkLoop.h>
//#include <IOKit/IOCommandGate.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IODeviceTreeSupport.h>

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
	IOPPluginEventTimer				= 6		// no params used
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

	//IOWorkLoop *		workLoop;
	//IOCommandGate *		commandGate;

	IORecursiveLock *	gate;
	thread_call_t		timerCallout;
	IOPMrootDomain *	pmRootDomain;

	virtual void 		initSymbols( void );

    //static IOReturn		commandGateCaller(OSObject *object, void *arg0, void *arg1, void *arg2, void *arg3);

	virtual IOReturn	dispatchEvent(IOPPluginEventData *event);
	virtual IOReturn	handleEvent(IOPPluginEventData *event);

	OSArray *			sensors;
	OSArray *			sensorInfoDicts;

	OSArray *			controls;
	OSArray *			controlInfoDicts;

	OSArray *			ctrlLoops;
	OSArray *			ctrlLoopInfoDicts;

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
	IOReturn sleepHandler(void);

/*!
	@function wakeHandler
	@abstract serialized wake notification handler */
	IOReturn wakeHandler(void);

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
