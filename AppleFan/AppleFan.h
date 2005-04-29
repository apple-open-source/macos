/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _APPLEFAN_H
#define _APPLEFAN_H

#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/i2c/PPCI2CInterface.h>

__BEGIN_DECLS
#include <kern/thread_call.h>
__END_DECLS

// Uncomment to enable debug output and fan monitor app support
// #define APPLEFAN_DEBUG 1

#ifdef DLOG
#undef DLOG
#endif

#ifdef APPLEFAN_DEBUG
#define DLOG(fmt, args...) kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Bit definitions for the ADM1030 chip.  See the datasheet for explanations.

// Register addresses to use as I2C subaddress
#define kConfigReg1		0x00
#define kConfigReg2		0x01
#define kExtTempReg		0x06
#define	kLocalTempReg	0x0A
#define kRemoteTempReg	0x0B
#define kFanCharReg		0x20
#define kSpeedCfgReg	0x22
#define kFanFilterReg	0x23
#define kLocTminTrange	0x24
#define kRmtTminTrange	0x25

// Config Register 1 0x00
#define kMonitorEnable		0x01
#define kINTEnable			0x02
#define kTACHModeSelect		0x04
#define kPWMInvertEnable	0x08
#define kFanFaultEnable		0x10

// bits 5 and 6 determine which thermo drives auto fan speed control
#define kPWMModeSelectMask	0x60	// mask for this field
#define kPWMModeRemote		0x00	// remote temp drives fan speed
#define kPWMModeFastest		0x60	// faster of local,remote drives fan speed

#define kAutoEnable			0x80

// Config Register 2 0x01
#define kPWMOutputEnable	0x01
// unused					0x02
#define kTACHInputEnable	0x04
// unused					0x08
#define kLocTempINTEnable	0x10
#define kRmtTempINTEnable	0x20
// unused					0x40
#define kSWReset			0x80

// Extended Temperature Resolution Register 0x06
#define kRemoteExtMask	0x07
#define kRemoteExtShift	5

#define kLocalExtMask	0xC0

// Fan Characteristics Register 0x20
#define kSpinUpTimeMask	0x07
#define kSpinUp200MS	0x00
#define kSpinUp400MS	0x01
#define kSpinUp600MS	0x02
#define kSpinUp800MS	0x03
#define kSpinUp1000MS	0x04
#define kSpinUp2000MS	0x05	// power-on default
#define kSpinUp4000MS	0x06
#define kSpinUp8000MS	0x07

#define kPWMFreqMask	0x38
#define kPWMFreq11Hz	0x00
#define kPWMFreq15Hz	0x08
#define kPWMFreq23Hz	0x10
#define kPWMFreq31Hz	0x18	// power-on default
#define kPWMFreq37Hz	0x20
#define kPWMFreq46Hz	0x28
#define kPWMFreq62Hz	0x30
#define kPWMFreq93Hz	0x38

#define kSpeedRangeMask	0xC0
#define kSpeedRange2647	0x00
#define kSpeedRange1324	0x40	// power-on default
#define	kSpeedRange662	0x80
#define	kSpeedRange331	0xC0

// Fan Speed Config Register 0x22
#define kDutyCycleOff	0x00
#define kDutyCycle07	0x01
#define kDutyCycle14	0x02
#define kDutyCycle20	0x03
#define kDutyCycle27	0x04
#define kDutyCycle33	0x05
#define kDutyCycle40	0x06
#define kDutyCycle47	0x07
#define kDutyCycle53	0x08
#define kDutyCycle60	0x09
#define kDutyCycle67	0x0A
#define kDutyCycle73	0x0B
#define kDutyCycle80	0x0C
#define kDutyCycle87	0x0D
#define kDutyCycle93	0x0E
#define kDutyCycleFull	0x0F

// Fan Filter Register 0x23
#define kFilterEnable	0x01
// Unused				0x02
// ADC Sample Rate (bits 4:2)
#define kSampleRateMask	0x1C
#define kSampleRate87_5Hz	0x00
#define kSampleRate175Hz	0x04
#define kSampleRate350Hz	0x08
#define kSampleRate700Hz	0x0C
#define kSampleRate1_4KHz	0x10	// power-on default
#define kSampleRate2_8KHz	0x14
#define kSampleRate5_6KHz	0x18
#define kSampleRate11_2KHz	0x1C
// Ramp rate (bits 6:5)
#define	kRampRateMask	0x60
#define kRampRate1			0x00
#define kRampRate2			0x20
#define kRampRate4			0x40	// power-on default
#define kRampRate8			0x60
#define kSpinUpDisable	0x80

// Local/Remote Temp T_min/T_range - 0x24,0x25
#define kTminMask		0xF8
#define kTrangeMask		0x07

// Operating parameters are stored in device tree properties
#define kDefaultParamsKey		"default-params"
#define kFanPollingPeriodKey	"fan-polling-period"
#define kFanSpeedTableKey		"fan-speed-table"
#define kSpeedupDelayKey		"fan-speedup-delay"
#define kSlowdownDelayKey		"fan-slowdown-delay"
#define kHysteresisTempKey		"fan-hysteresis-temp"
#define kFanCurrentSpeedKey		"fan-current-speed"
#define kCPUCurrentTempKey		"cpu-current-temp"

// Property key for platform function.  The value is the phandle of the
// ds1775 thermistor's device tree node.
#define kGetTempSymbol	"platform-getTemp"

// If we get a setProperties with this key, it means there is a request
// to sync cpu temp and current fan speed to reflect current real settings
#define kForceUpdateSymbol	"force-update"

// Convert seconds to milliseconds
#define sec2ms(nSeconds) (1000*nSeconds)

// Convert SInt16 temperature representation to a readable string
#define temp2str(myTemp, myString) \
	sprintf(myString, "%d", myTemp >> 8);

// Number of times to retry failed I2C requests
#define kNumRetries	10

// 20 deg C margin for error between MLB and CPU temp
#define kTempMaxDelta 20

typedef struct {
	UInt8 config1;
	UInt8 config2;
	UInt8 fan_char;
	UInt8 speed_cfg;
	UInt8 fan_filter;
	UInt8 loc_tmin_trange;
	UInt8 rmt_tmin_trange;
} adm1030_regs_t;

// Power management - we need to be notified when the system wakes so we
// can compensate for the temperature discontinuity introduced across sleep
// (system cools down when sleeping...).  We define 2 power states, and when
// the system comes out of sleep we set Tmin appropriately so the fan doesn't
// spin up like mad.
enum {
	kPowerOff,
	kPowerOn,
	kNumPowerStates
};

// The fan speed is driven by a table lookup.  The table is simply an array
// of SInt16 temperature values, where each element corresponds to the speed
// at which the fan will move to the next higher speed.  For example, if
// speed_table[0] contains 52 degrees, the fan will run at speed zero unless
// the current temp is above 52 degrees.  If the current temp is above 52
// degrees, the fan will check the next table entry, and iterate until it finds
// the appropriate value.  Obviously, if the current temp is greater than
// speed_table[kNumFanSpeeds], the fan will run at maximum speed.

#define kNumFanSpeeds	16
typedef SInt16 fan_speed_table_t[kNumFanSpeeds];

// Compatible string for ADM1030
#define kADM1030Compatible	"adm1030"

class AppleFan : public IOService
{
	OSDeclareDefaultStructors(AppleFan)

	private:
		PPCI2CInterface	*I2C_iface;	// I2C driver for my smbus
		IOService *cpu_thermo;	// AppleCPUThermo reads ds1775 for me
		UInt32 fThermoPHandle;	// phandle of temp-monitor node

		UInt8 fI2CBus;		// bus identifier
		UInt8 fI2CAddr;		// 7-bit i2c address

		adm1030_regs_t		fSavedRegs;

		fan_speed_table_t	fSpeedTable;

		UInt64				fSpeedupDelay;	// Must wait this many nanoseconds between fan speedups
		UInt64				fSlowdownDelay;	// Must wait this many nanoseconds between fan slowdowns

		SInt16				fHysteresisTemp;

		AbsoluteTime		fLastTransition;	// Timestamp of last speed change

		UInt64				fPollingPeriod;	// fan polling period in nanoseconds

		UInt8				fLastFanSpeed;
		SInt16				fLastRmtTemp;
		AbsoluteTime		fWakeTime;

		unsigned long fCurrentPowerState;

		thread_call_t timerCallout;

		const OSSymbol *pollingPeriodKey;	
		const OSSymbol *speedTableKey;
		const OSSymbol *speedupDelayKey;
		const OSSymbol *slowdownDelayKey;
		const OSSymbol *hysteresisTempKey;
		const OSSymbol *getTempSymbol;

#ifdef APPLEFAN_DEBUG
		const OSSymbol *currentSpeedKey;
		const OSSymbol *currentCPUTempKey;
		const OSSymbol *forceUpdateKey;
#endif

		bool initParms(IOService *provider);
		bool initHW(IOService *provider);
		//bool getLocalAndRemoteTemps(SInt16 *loc_temp, SInt16 *rmt_temp);
		bool getRemoteTemp(SInt16 *rmt_temp);
		bool getCPUTemp(SInt16 *cpu_temp);
		void doUpdate(bool first);

		void setFanSpeed(UInt8 speed, SInt16 cpu_temp, bool first);
		void setADM1030SpeedMagically(UInt8 desiredSpeed, SInt16 rmt_temp);

		void doSleep(void);
		void doWake(void);
		void doRestart(void);
		void setRestartMode(void);

		bool saveADM1030State(adm1030_regs_t *);
		void restoreADM1030State(adm1030_regs_t *);

		bool doI2COpen(void);
		void doI2CClose(void);
		bool doI2CRead(UInt8 sub, UInt8 *bytes, UInt16 len);
		bool doI2CWrite(UInt8 sub, UInt8 *bytes, UInt16 len);

		// Publish various state to the I/O registry
#ifdef APPLEFAN_DEBUG
		void publishSpeedTable(void);
		void publishPollingPeriod(void);
		void publishDelays(void);
		void publishHysteresisTemp(void);
		void publishCurrentSpeed(void);
		void publishCurrentCPUTemp(void);
#endif

	public:

#ifdef APPLEFAN_DEBUG
		virtual IOReturn setProperties(OSObject *properties);
#endif
		void parseDict(OSDictionary *props);
		virtual IOReturn powerStateWillChangeTo(IOPMPowerFlags flags,
				unsigned long stateNumber, IOService *whatDevice);
		virtual IOReturn setPowerState(unsigned long powerStateOrdinal,
				IOService *whatDevice);
		virtual IOReturn powerStateDidChangeTo(IOPMPowerFlags flags,
				unsigned long stateNumber, IOService *whatDevice);
		virtual IOService *probe(IOService *provider, SInt32 *score);
		virtual bool init(OSDictionary *dict);
		virtual void free(void);
		virtual bool start(IOService *provider);
		virtual void stop(IOService *provider);
		static void	timerEventOccured( void * self );
		static IOReturn sPMNotify(void *target, void *refCon,
				long unsigned int messageType, IOService *provider,
				void *messageArg, vm_size_t argSize);
};

#endif