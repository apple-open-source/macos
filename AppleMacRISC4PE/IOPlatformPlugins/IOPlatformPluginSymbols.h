/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformPluginSymbols.h,v $
//		Revision 1.6  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.5  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.5  2003/06/04 00:00:51  eem
//		More PID stuff, working towards support for forced meta states.
//		
//		Revision 1.4.2.4  2003/06/01 14:52:51  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.4.2.3  2003/05/31 08:11:34  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.4.2.2  2003/05/26 10:07:14  eem
//		Fixed most of the bugs after the last cleanup/reorg.
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
//		Revision 1.1.2.1  2003/05/10 06:32:34  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		
//		

#ifndef _IOPLATFORMPLUGINSYMBOLS_H
#define _IOPLATFORMPLUGINSYMBOLS_H

/*
 * the platform plugin's symbol extern const'd for usage by helper classes
 */

class OSSymbol;
class OSNumber;
class IOPlatformPlugin;

extern const OSSymbol * gIOPPluginForceUpdateKey;
extern const OSSymbol * gIOPPluginForceUpdateAllKey;
extern const OSSymbol * gIOPPluginForceSensorCurValKey;
extern const OSSymbol * gIOPPluginReleaseForcedSensorKey;
extern const OSSymbol * gIOPPluginForceControlTargetValKey;
extern const OSSymbol * gIOPPluginReleaseForcedControlKey;
extern const OSSymbol * gIOPPluginForceCtrlLoopMetaStateKey;
extern const OSSymbol * gIOPPluginReleaseForcedCtrlLoopKey;
extern const OSSymbol * gIOPPluginVersionKey;
extern const OSSymbol * gIOPPluginTypeKey;
extern const OSSymbol * gIOPPluginLocationKey;
extern const OSSymbol * gIOPPluginZoneKey;
extern const OSSymbol * gIOPPluginCurrentValueKey;
extern const OSSymbol * gIOPPluginPollingPeriodKey;
extern const OSSymbol * gIOPPluginRegisteredKey;
extern const OSSymbol * gIOPPluginSensorDataKey;
extern const OSSymbol * gIOPPluginControlDataKey;
extern const OSSymbol * gIOPPluginCtrlLoopDataKey;
extern const OSSymbol * gIOPPluginSensorIDKey;
extern const OSSymbol * gIOPPluginSensorFlagsKey;
extern const OSSymbol * gIOPPluginCurrentStateKey;
extern const OSSymbol * gIOPPluginLowThresholdKey;
extern const OSSymbol * gIOPPluginHighThresholdKey;
extern const OSSymbol * gIOPPluginTypeTempSensor;
extern const OSSymbol * gIOPPluginTypePowerSensor;
extern const OSSymbol * gIOPPluginTypeVoltageSensor;
extern const OSSymbol * gIOPPluginTypeCurrentSensor;
extern const OSSymbol * gIOPPluginTypeADCSensor;
extern const OSSymbol * gIOPPluginControlIDKey;
extern const OSSymbol * gIOPPluginControlFlagsKey;
extern const OSSymbol * gIOPPluginTargetValueKey;
extern const OSSymbol * gIOPPluginControlMinValueKey;
extern const OSSymbol * gIOPPluginControlMaxValueKey;
extern const OSSymbol * gIOPPluginTypeSlewControl;
extern const OSSymbol * gIOPPluginTypeFanRPMControl;
extern const OSSymbol * gIOPPluginTypeFanPWMControl;
extern const OSSymbol * gIOPPluginControlClass;
extern const OSSymbol * gIOPPluginSensorClass;
extern const OSSymbol * gIOPPluginEnvInternalOvertemp;
extern const OSSymbol * gIOPPluginEnvExternalOvertemp;
extern const OSSymbol * gIOPPluginEnvDynamicPowerStep;
extern const OSSymbol * gIOPPluginEnvControlFailed;
extern const OSSymbol * gIOPPluginCtrlLoopIDKey;
extern const OSSymbol * gIOPPluginCtrlLoopMetaState;
extern const OSSymbol * gIOPPluginThermalLocalizedDescKey;
extern const OSSymbol * gIOPPluginThermalValidConfigsKey;
extern const OSSymbol * gIOPPluginThermalMetaStatesKey;
extern const OSSymbol * gIOPPluginPlatformID;

extern const OSNumber * gIOPPluginZero;
extern const OSNumber * gIOPPluginOne;

extern IOPlatformPlugin * platformPlugin;

#endif	// _IOPLATFORMPLUGINSYMBOLS_H
