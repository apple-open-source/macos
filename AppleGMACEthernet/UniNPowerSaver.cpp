
/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").	You may not use this file except in compliance with the
 * License.	 Please obtain a copy of the License at
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


#include <libkern/OSByteOrder.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "UniNEnet.h"
#include "UniNEnetMII.h"

#define super IOEthernetController



	/****** From iokit/IOKit/pwr_mgt/IOPMpowerState.h 
		struct IOPMPowerState
		{
		unsigned long	version;				// version number of this struct

		IOPMPowerFlags	capabilityFlags;		// bits that describe the capability 
		IOPMPowerFlags	outputPowerCharacter;	// description (to power domain children) 
		IOPMPowerFlags	inputPowerRequirement;	// description (to power domain parent)

		unsigned long	staticPower;			// average consumption in milliwatts
		unsigned long	unbudgetedPower;		// additional consumption from separate power supply (mw)
		unsigned long	powerToAttain;			// additional power to attain this state from next lower state (in mw)

		unsigned long	timeToAttain;			// (microseconds)
		unsigned long	settleUpTime;			// (microseconds)
		unsigned long	timeToLower;			// (microseconds)
		unsigned long	settleDownTime;			// (microseconds)

		unsigned long	powerDomainBudget;		// power in mw a domain in this state can deliver to its children
		};
	*******/

	static IOPMPowerState	ourPowerStates[ kNumOfPowerStates ] =
	{
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 1, IOPMDeviceUsable | IOPMMaxPerformance, IOPMPowerOn, IOPMPowerOn,
				50, 0, 0, kUniNsettle_time, kUniNsettle_time, kUniNsettle_time,
				kUniNsettle_time, 0 }
	};


	// Method: registerWithPolicyMaker - Called by superclass - not by
	//		Power Management
	//
	// Purpose:
	//	 Initialize the driver for power management and register
	//	 ourselves with policy-maker.

IOReturn UniNEnet::registerWithPolicyMaker( IOService *policyMaker )
{
	IOReturn	rc;


	if ( fBuiltin )
		 rc = policyMaker->registerPowerDriver( this, ourPowerStates, kNumOfPowerStates );
	else rc = super::registerWithPolicyMaker( policyMaker );	// return unsupported

	ELG( IOThreadSelf(), rc, 'RwPM', "registerWithPolicyMaker" );

	return rc;
}/* end registerWithPolicyMaker */


	// Method: maxCapabilityForDomainState
	//
	// Purpose:
	//		  returns the maximum state of card power, which would be
	//		  power on without any attempt to power manager.

unsigned long UniNEnet::maxCapabilityForDomainState( IOPMPowerFlags domainState )
{
	ELG( IOThreadSelf(), domainState, 'mx4d', "maxCapabilityForDomainState" );

	if ( domainState & IOPMPowerOn )
		return kNumOfPowerStates - 1;

	return 0;
}/* end maxCapabilityForDomainState */


	// Method: initialPowerStateForDomainState
	//
	// Purpose:
	// The power domain may be changing state.	If power is on in the new
	// state, that will not affect our state at all.  If domain power is off,
	// we can attain only our lowest state, which is off.

unsigned long UniNEnet::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
	ELG( IOThreadSelf(), domainState, 'ip4d', "initialPowerStateForDomainState" );

	if ( domainState & IOPMPowerOn )
	   return kNumOfPowerStates - 1;

	return 0;
}/* end initialPowerStateForDomainState */


	// Method: powerStateForDomainState
	//
	// Purpose:
	//		   The power domain may be changing state.	If power is on in the new
	// state, that will not affect our state at all.  If domain power is off,
	// we can attain only our lowest state, which is off.

unsigned long UniNEnet::powerStateForDomainState(IOPMPowerFlags domainState )
{
	ELG( IOThreadSelf(), domainState, 'ps4d', "UniNEnet::powerStateForDomainState" );

	if ( domainState & IOPMPowerOn )
		return 1;						// This should answer What If?

	return 0;
}/* end powerStateForDomainState */


	// This method sets up the PHY registers for low power.
	// Copied from stopEthernetController() in OS9.

void UniNEnet::stopPHY()
{
	UInt32	  val32;
	UInt16	  i, val16;

	ELG( fWOL, fPHYType, '-Phy', "UniNEnet::stopPHY" );

	if ( !fBuiltin || (fPHYType == 0) )
		return;

	if ( fWOL == false )
	{		// disabling MIF interrupts on the 5201 is explicit
		if ( fPHYType == 0x5201 )
			miiWriteWord( 0x0000, MII_BCM5201_INTERRUPT );
	}

		/* Turn off PHY status-change polling to prevent immediate wakeup:	*/
	val32 = READ_REGISTER( MIFConfiguration );
	val32 &= ~kMIFConfiguration_Poll_Enable;
	WRITE_REGISTER( MIFConfiguration, val32 );

	if ( fWOL )
	{
			// For multicast filtering these bits must be enabled
		WRITE_REGISTER( RxMACConfiguration,		kRxMACConfiguration_Hash_Filter_Enable
											  | kRxMACConfiguration_Strip_FCS
											  | kRxMACConfiguration_Rx_Mac_Enable );

		UInt16	*p16;
		p16 = (UInt16*)myAddress.bytes;

		WRITE_REGISTER( WOLMagicMatch[ 2 ], p16[ 0 ] );		// enet address
		WRITE_REGISTER( WOLMagicMatch[ 1 ], p16[ 1 ] );
		WRITE_REGISTER( WOLMagicMatch[ 0 ], p16[ 2 ] );

		WRITE_REGISTER( WOLPatternMatchCount, kWOLPatternMatchCount_M | kWOLPatternMatchCount_N );

		val32 = kWOLWakeupCSR_Magic_Wakeup_Enable;		// Assume GMII
		if ( !(fXIFConfiguration & kXIFConfiguration_GMIIMODE) )
			 val32 |= kWOLWakeupCSR_Mode_MII;			// NG - indicate non GMII
		WRITE_REGISTER( WOLWakeupCSR, val32 );
	}
	else
	{
		WRITE_REGISTER( RxMACConfiguration, 0 );
		IOSleep( 4 ); 		// it takes time for enable bit to clear
	}

	WRITE_REGISTER( TxMACConfiguration, 0 );
	WRITE_REGISTER( XIFConfiguration,	0 );

	fTxConfiguration &= ~kTxConfiguration_Tx_DMA_Enable;
	WRITE_REGISTER( TxConfiguration, fTxConfiguration );
	fRxConfiguration &= ~kRxConfiguration_Rx_DMA_Enable;
	WRITE_REGISTER( RxConfiguration, fRxConfiguration );

	if ( !fWOL )
	{
			// this doesn't power down stuff, but if we don't hit it then we can't
			// superisolate the transceiver
		WRITE_REGISTER( SoftwareReset, kSoftwareReset_TX | kSoftwareReset_RX );

		i = 0;
		do
		{
			IODelay( 10 );
			if ( i++ >= 100 )
			{
				ALRT( 0, val32, 'Sft-', "UniNEnet::stopPHY - timeout on SoftwareReset" );
				break;
			}
			val32 = READ_REGISTER( SoftwareReset );
		} while ( (val32 & (kSoftwareReset_TX | kSoftwareReset_RX)) != 0 );

		WRITE_REGISTER( TxMACSoftwareResetCommand, kTxMACSoftwareResetCommand_Reset );
		WRITE_REGISTER( RxMACSoftwareResetCommand, kRxMACSoftwareResetCommand_Reset );

			// This is what actually turns off the LINK LED

		switch ( fPHYType )
		{
		case 0x5400:
		case 0x5401:
#if 0
				// The 5400 has read/write privilege on this bit,
				// but 5201 is read-only.
			miiWriteWord( MII_CONTROL_POWERDOWN, MII_CONTROL );
#endif
			break;

		case 0x5221:
				// 1: enable shadow mode registers in 5221 (0x1A-0x1E)
			miiReadWord( &val16, MII_BCM5221_TestRegister );
			miiWriteWord( val16 | MII_BCM5221_ShadowRegEnableBit, MII_BCM5221_TestRegister );	

				// 2: Force IDDQ mode for max power savings
				// remember..after setting IDDQ mode we have to "hard" reset
				// the PHY in order to access it.
			miiReadWord( &val16, MII_BCM5221_AuxiliaryMode4 );
			miiWriteWord( val16 | MII_BCM5221_SetIDDQMode, MII_BCM5221_AuxiliaryMode4 );
			break;

		case 0x5241:
				// 1: enable shadow register mode
			miiReadWord( &val16, MII_BCM5221_TestRegister );
			miiWriteWord( val16 | MII_BCM5221_ShadowRegEnableBit, MII_BCM5221_TestRegister );	

				// 2: Set standby power bit
			miiReadWord( &val16, MII_BCM5221_AuxiliaryMode4 );
			miiWriteWord( val16 | MII_BCM5241_StandbyPowerMode, MII_BCM5221_AuxiliaryMode4 );
			break;

		case 0x5201:
#if 0
			miiReadWord( &val16, MII_BCM5201_AUXMODE2 );
			miiWriteWord( val16 & ~MII_BCM5201_AUXMODE2_LOWPOWER,  MII_BCM5201_AUXMODE2 );
#endif

			miiWriteWord( MII_BCM5201_MULTIPHY_SUPERISOLATE, MII_BCM5201_MULTIPHY );
			break;


		case 0x5411:
		case 0x5421:
		default:
			miiWriteWord( MII_CONTROL_POWERDOWN, MII_CONTROL );
			break;
		}/* end SWITCH on PHY type */

			/* Put the MDIO pins into a benign state.							*/
			/* Note that the management regs in the PHY will be inaccessible.	*/
			/* This is to guarantee max power savings on Powerbooks and			*/
			/* to eliminate damage to Broadcom PHYs.							*/
	
		WRITE_REGISTER( MIFConfiguration, kMIFConfiguration_BB_Mode );	// bit bang mode
	
		WRITE_REGISTER( MIFBitBangClock,		0x0000 );
		WRITE_REGISTER( MIFBitBangData,			0x0000 );
		WRITE_REGISTER( MIFBitBangOutputEnable, 0x0000 );
		WRITE_REGISTER( XIFConfiguration,		kXIFConfiguration_GMIIMODE
											 |  kXIFConfiguration_MII_Int_Loopback );
		val32 = READ_REGISTER( XIFConfiguration );	/// ??? make sure it takes.
	}// end of non-WOL case

	return;
}/* end stopPHY */


	// start the PHY
	//// This routine really doesn't do much with the PHY. startPHY is a misnomer.
	//// It is called only by wakeUp - move the code there.

void UniNEnet::startPHY()
{
	UInt16	  val16;


	ELG( this, fPHYType, 'Phy+', "startPHY" );

	fTxConfiguration |= kTxConfiguration_Tx_DMA_Enable;
	WRITE_REGISTER( TxConfiguration, fTxConfiguration );

	fRxConfiguration |= kRxConfiguration_Rx_DMA_Enable;
	WRITE_REGISTER( RxConfiguration, fRxConfiguration );

	fTxMACConfiguration |= kTxMACConfiguration_TxMac_Enable;
	WRITE_REGISTER( TxMACConfiguration, fTxMACConfiguration );

	fRxMACConfiguration |= kRxMACConfiguration_Rx_Mac_Enable | kRxMACConfiguration_Hash_Filter_Enable;
	if ( fIsPromiscuous )
		 fRxMACConfiguration &= ~kRxMACConfiguration_Strip_FCS;
	else fRxMACConfiguration |=  kRxMACConfiguration_Strip_FCS;

	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );

		/* These registers are only for the Broadcom 5201.
		   We write the auto low power mode bit here because if we do it earlier
		   and there is no link then the xcvr registers become unclocked and
		   unable to be written
		 */
	if ( fPHYType == 0x5201 )
	{
			// Ask Enrique why the following 2 lines are not necessary in OS 9.
			// These 2 lines should take the PHY out of superisolate mode.
		 	// All MII inputs are ignored until the PHY is out of isolate mode.

		miiReadWord( &val16, MII_BCM5201_MULTIPHY );
		miiWriteWord( val16 & ~MII_BCM5201_MULTIPHY_SUPERISOLATE, MII_BCM5201_MULTIPHY );

#if 0
			// Automatically go into low power mode if no link
		miiReadWord( &val16, MII_BCM5201_AUXMODE2 );
		miiWriteWord( val16 | MII_BCM5201_AUXMODE2_LOWPOWER, MII_BCM5201_AUXMODE2 );
#endif
	}

	WRITE_REGISTER( RxKick, fRxRingElements - 4 );	/// Why is this in PHY code?
	return;
}/* end startPHY */
