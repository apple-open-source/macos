/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Hardware independent (relatively) code for the Burgundy Controller
 *
 * HISTORY
 *
 *
 */

#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareDetect.h"
#include "AudioHardwareOutput.h"
#include "AudioHardwareInput.h"
#include "AudioHardwareMux.h"
#include "AudioHardwarePower.h"

#include "AppleBurgundyAudio.h"
#include "burgundy_hw.h"

#include "Apple02DBDMAAudioDMAEngine.h"

/*
 * Prototyes for the "very private methods" at the end of this
 * file. They provide access to the burgundy registers:
 */
static void Burgundy_writeSoundControlReg( volatile UInt8*, int);
static int  Burgundy_readCodecSenseLines( volatile UInt8*);
static int  Burgundy_readCodecReg( volatile UInt8*, int);
static void Burgundy_writeCodecReg( volatile UInt8*, int, int);

/* =============================================================
 * VERY Private Methods used to access to the burgundy registers
 * ============================================================= */

static void
Burgundy_writeCodecReg( volatile UInt8 *ioBaseBurgundy, int regInfo, int value )
{
  u_int32_t		regBits;
  u_int32_t		regValue;
  u_int32_t		i;
  volatile u_int32_t	CodecControlReg;

  struct _reg
  {
      UInt8	unused0;
      UInt8  size;
      UInt8  addr;
      UInt8  offset;
  } *reg = (struct _reg *)&regInfo;


  regBits =   (kCodecCtlReg_Write | kCodecCtlReg_Reset)
            | ((u_int32_t) reg->addr                    * kCodecCtlReg_Addr_Bit)
            | ((u_int32_t)(reg->size + reg->offset - 1) * kCodecCtlReg_LastByte_Bit);

  for ( i=0; i < reg->size; i++ )
  {
      regValue = regBits | ((u_int32_t)(reg->offset + i) * kCodecCtlReg_CurrentByte_Bit) | (value & 0xFF);
      OSWriteLittleInt32(ioBaseBurgundy, kCodecCtlReg, regValue );
      eieio();


      debugIOLog (3,  "PPCSound(burgundy): CodecWrite = %08x\n\r", regValue );

      value >>= 8;

      regBits &= ~kCodecCtlReg_Reset;

      do
      {
          CodecControlReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecCtlReg  );
          eieio();
      }
      while ( CodecControlReg & kCodecCtlReg_Busy );
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  This function is used to read any Burgundy CODEC register.  The routine also
//  updates member variables to indicate the current state of the configuration
//  pins.  To update the configuration pin status, issue this function with any 
//  target register and then access the configuration pin status.  Burgundy pipelining 
//  is NOT functional so it is necessary to operate two parallel state machines 
//  to handle the codec control and status transactions when reading a Burgundy 
//  codec register.  NOTE:  The transaction level mechanism used under MacOS 9
//  to retry the register access if the current register access is preempted by
//  another register access from interrupt level has not been ported to this
//  member function.  Any register access that is preempted by another register
//  access will result in erroneous data.  A blocking mechanism should be implemented
//  here to prevent this situation.  Then this note should be removed...


static int Burgundy_readCodecReg( volatile UInt8 *ioBaseBurgundy, int regInfo )
{
   u_int32_t		regBits;
  u_int32_t		regValue = 0;
  u_int32_t		i,j;
  int			value;
  u_int32_t		byteCounter;
  u_int32_t		currentCounter;
  volatile u_int32_t	CodecControlReg;
  volatile u_int32_t	CodecStatusReg;


  struct _reg
  {
      UInt8	unused0;
      UInt8  size;
      UInt8  addr;
      UInt8  offset;
  } *reg = (struct _reg *)&regInfo;

  value   = 0;

  regBits =   (kCodecCtlReg_Reset)
            | ((u_int32_t) reg->addr                    * kCodecCtlReg_Addr_Bit)
            | ((u_int32_t)(reg->size + reg->offset - 1) * kCodecCtlReg_LastByte_Bit);

  CodecStatusReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  );
  eieio();
  byteCounter = ((CodecStatusReg & kCodecStatusReg_ByteCounter_Mask) / kCodecStatusReg_ByteCounter_Bit + 1) & 0x03;

  for ( i=0; i < reg->size; i++ )
  {
      regValue = regBits | ((u_int32_t)(reg->offset + i) * kCodecCtlReg_CurrentByte_Bit);
      OSWriteLittleInt32(ioBaseBurgundy, kCodecCtlReg, regValue );
      eieio();
      regBits &= ~kCodecCtlReg_Reset;

      do
      {
          CodecControlReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecCtlReg  );
          eieio();
      }
      while ( CodecControlReg & kCodecCtlReg_Busy );

      j=0;
      do
      {
          CodecStatusReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  );
          eieio();
          currentCounter = ((CodecStatusReg & kCodecStatusReg_ByteCounter_Mask) / kCodecStatusReg_ByteCounter_Bit) & 0x03;
      }
      while ( (byteCounter != currentCounter) && (j++ < 1000));

      byteCounter++;

      IODelay(10);
      CodecStatusReg =  OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  );

      value |= ((CodecStatusReg & kCodecStatusReg_Data_Mask) / kCodecStatusReg_Data_Bit) << 8*i;
  }


    // debugIOLog (3,  "PPCSound(burgundy): CodecRead = %08x %08x\n\r", regValue, value );

  return value;
}

static int Burgundy_readCodecSenseLines( volatile UInt8 *ioBaseBurgundy )
{
    return ((OSReadLittleInt32( ioBaseBurgundy, kCodecStatusReg  ) / kCodecStatusReg_Sense_Bit) & kCodecStatusReg_Sense_Mask);
}


static void Burgundy_writeSoundControlReg( volatile UInt8 *ioBaseBurgundy, int value )
{

  debugIOLog (3,  "PPCSound(burgundy): SoundControlReg = %08x", value);
  OSWriteLittleInt32( ioBaseBurgundy, kSoundCtlReg, value );
  eieio();
}



#define super Apple02Audio

OSDefineMetaClassAndStructors( AppleBurgundyAudio, Apple02Audio )


/* ==============
 * Public Methods
 * ============== */
bool AppleBurgundyAudio::init(OSDictionary * properties)
{
    debugIOLog (3, "+ AppleBurgundyAudio::init");
    if (!super::init(properties)) 
        return false;

    mVolLeft = 0;
    mVolRight = 0;
    mIsMute = false;
    mLogicalInput = 0;
    mVolumeMuteIsActive = false;
    mMuxMix = 0;
    
    debugIOLog (3, "- AppleBurgundyAudio::init");
    return true;    
}

void AppleBurgundyAudio::free() {
    debugIOLog (3, "+ AppleBurgundyAudio::free");
    super::free();
    debugIOLog (3, "- AppleBurgundyAudio::free");
}

IOService* AppleBurgundyAudio::probe(IOService* provider, SInt32* score) {

    IORegistryEntry *sound = 0;
    IORegistryEntry *perch = 0;
    IOService *result = 0;
    

    // We CAN fail the type check:
    super::probe(provider, score);
    *score = kIODefaultProbeScore;
	// we may be on a Beige G3
    perch = IORegistryEntry::fromPath("/perch", gIODTPlane);
    
    if(perch) {
        // there is a perch we are on a beige G3
        OSData *s = NULL;

        s = OSDynamicCast(OSData, perch->getProperty("compatible"));

        if (s) {
            if(s->isEqualTo("burgundy", sizeof("burgundy")-1) || 
                (s->isEqualTo("DVD-Video and Audio/Video", sizeof("DVD-Video and Audio/Video")-1)) ||
                (s->isEqualTo("bordeaux", sizeof("bordeaux")-1))) {
             
                    *score = *score+2;
                    result = this;
                    goto BAIL;
            }    
                
        }
    }
    
	// we are on a new world : the registry is assumed to be fixed
    sound = provider->childFromPath("sound", gIODTPlane);
         
    if(sound) {
        OSData *tmpData;
        
        tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
        if(tmpData) {
            if(tmpData->isEqualTo(kBurgundyModelName, sizeof(kBurgundyModelName) -1) ) {
                *score = *score+1;
                result = this;
                goto BAIL;
            } 
        }  
    } 

BAIL:
    debugIOLog (3, "- AppleBurgundyAudio::probe");
    return (result);
}

bool AppleBurgundyAudio::initHardware(IOService* provider)
{
    bool myreturn = true;

    debugIOLog (3, "+ AppleBurgundyAudio::initHardware");
    
    super::initHardware(provider);

// this code moved to sndHWPostThreadedInit () for [3284411]
/*
    gCanPollSatus = true;
    checkStatus(true);    
//    flushAudioControls();
        
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &AppleBurgundyAudio::timerCallback, timerInterval);

	// aml 6.17.02, burgundy internal mic is mono
	mInternalMicDualMonoMode = e_Mode_CopyRightToLeft;		
    
    duringInitialization = false;
 */
    
    debugIOLog (3, "- AppleBurgundyAudio::initHardware");
    return myreturn;
}

void AppleBurgundyAudio::setDeviceDetectionActive(){
    gCanPollSatus = true;
}
    
void AppleBurgundyAudio::setDeviceDetectionInActive(){
    gCanPollSatus = false;
}


void AppleBurgundyAudio::timerCallback(OSObject *target, IOAudioDevice *device)
{
    AppleBurgundyAudio *burgundy;

    burgundy = OSDynamicCast(AppleBurgundyAudio, target);
    if (burgundy) 
        burgundy->checkStatus(false);
}




// --------------------------------------------------------------------------
// Method: checkStatusRegister
//
// Purpose:
//        if the argument is true mutes the internal speaker, otherwise
//        it "unmutes" it.
void AppleBurgundyAudio::checkStatus(bool force)
{
    UInt32 tempInSense, i;
    AudioHardwareDetect *theDetect;
    UInt32 mextdev;
    
    if(false == gCanPollSatus)
        return;
    mextdev = 0;
    
    tempInSense = sndHWGetInSenseBits();
    
    if( (curInsense != tempInSense) || force) {
        curInsense = tempInSense;
        AudioDetects = super::getDetectArray();
        if(AudioDetects) {
            for(i = 0; i < AudioDetects->getCount(); i++) {
                theDetect = OSDynamicCast(AudioHardwareDetect, AudioDetects->getObject(i));
                if ( theDetect ) {
					mextdev |= theDetect->refreshDevices(curInsense);
				}
            }
            super::setCurrentDevices(mextdev);

			//	Opportunity to omit Balance controls if on the internal mono speaker...	[3042660]	begin {
			useMasterVolumeControl = FALSE;
			if (NULL != driverDMAEngine) {
				if ( layoutYosemite == mLayoutID  ) {
					//	The master control should be used when headphones are NOT present.
					if ( kSndHWCPUHeadphone != ( mextdev & kSndHWCPUHeadphone ) ) {
						useMasterVolumeControl = TRUE;
					}
				}
			}
			debugIOLog (3,  "... useMasterVolumeControl %d", useMasterVolumeControl );
			AdjustControls ();					//	rbm	30 Sept 2002					[3042660]	} end
        }
    }
}


#pragma mark -- sndHW METHODS --

// sndHWXXXXX functions
void AppleBurgundyAudio::sndHWInitialize(IOService *provider){
    IOMemoryMap *map;
    UInt32 idx, tmpReg;
        
    debugIOLog (3, "+ AppleBurgundyAudio::sndHWInitialize");

	ourProvider = provider;
    map = provider->mapDeviceMemoryWithIndex(Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex);
    ioBaseBurgundy = (UInt8 *)map->getVirtualAddress();
    
    if(!ioBaseBurgundy) debugIOLog (3, "We have no mermory map !!!");
    curInsense = 0xFFFF;
        
	mLayoutID = GetDeviceID ();
	
	minVolume = kBURGUNDY_MINIMUM_HW_VOLUME;
	maxVolume = kBURGUNDY_MAXIMUM_HW_VOLUME;

	// update local variables for the settling time 
    for( idx = kBurgundyPhysOutputPort13-kBurgundyPhysOutputPort13; idx <= kBurgundyPhysOutputPort17-kBurgundyPhysOutputPort13; idx++ ){
		localSettlingTime[idx] = 0;  //this is an array of 5 stuff
    }	
		
	// set the IO part sub frame 0
    soundControlRegister = ( kSoundCtlReg_InSubFrame0      | \
                             kSoundCtlReg_OutSubFrame0     | \
                             kSoundCtlReg_Rate_44100        );
    Burgundy_writeSoundControlReg(ioBaseBurgundy, soundControlRegister);

    	
	//Verify we are on a Burgundy hardware by getting the type
	// Enable the programmable output
    // tmpReg = kOutputCtl0Reg_OutCtl0_High | kOutputCtl0Reg_OutCtl1_High;
    tmpReg = kOutputCtl0Reg_OutCtl1_High;
    Burgundy_writeCodecReg( ioBaseBurgundy, kOutputCtl0Reg,tmpReg);
	
	// tmpReg = kOutputCtl2Reg_OutCtl2_High | kOutputCtl2Reg_OutCtl3_High| kOutputCtl2Reg_OutCtl4_High;
	// Burgundy_writeCodecReg( ioBaseBurgundy, kOutputCtl2Reg,tmpReg);
	
	// Muxes configuration : Mux 0 has port 1, Mux 1 has port 3, Mux 2 has port 5
    tmpReg =kBurgundyAMux_0SelPort_1;
    Burgundy_writeCodecReg( ioBaseBurgundy, kMux01Reg , tmpReg);
            
	// select input 6 for test on Yosemite
    tmpReg  = kBurgundyAMux_2SelPort_5;
    Burgundy_writeCodecReg( ioBaseBurgundy, kMux2Reg , kMux2Reg_Mux2L_SelectPort6L|kMux2Reg_Mux2R_SelectPort6R);
	
	// configure the mixers : mixer0 has nothing, 
	// mixer 1 has the input builtin signal, 
	// mixer2 connect digital stream A        
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX0Reg, 0);
    mMuxMix = kBurgundyW_2_n;
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg,kBurgundyW_2_n); 	// this for input selection
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX2Reg, kBurgundyW_A_n);
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX3Reg, 0);
 
	// apply the default gain 0xDF to mixer normalization
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ0LReg, kMXEQ_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ0RReg, kMXEQ_Default_Gain );    
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ1LReg, kMXEQ_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ1RReg, kMXEQ_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ2LReg, kMXEQ_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ2RReg, kMXEQ_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ3LReg, kMXEQ_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kMXEQ3RReg, kMXEQ_Default_Gain );

	// configure the ouput stream selection 
	// OS_O listens to mixer 2, OS_1 listens to Mixer 2 ==> this goes to output
	// OS_E listen to mixer 1 this is the input
    Burgundy_writeCodecReg( ioBaseBurgundy, kOSReg, kBurgundyOS_0_MXO_2| kBurgundyOS_1_MXO_2 | kBurgundyOS_E_MXO_1);
        
	// set the amplification OS/AP
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP0LReg , 0xFF);  
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP0RReg , 0xFF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP1RReg , 0xFF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP1LReg , 0xFF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP2LReg , 0xFF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP2RReg , 0xFF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP3LReg , 0xFF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAP3RReg , 0xFF);

	// Mute everything
    Burgundy_writeCodecReg(ioBaseBurgundy, kOutputMuteReg, 0x00);
    
	// prepare the output amplification
    Burgundy_writeCodecReg( ioBaseBurgundy, kOutputLvlPort14Reg, 0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kOutputLvlPort15Reg, 0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kOutputLvlPort16Reg, 0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kOutputLvlPort17Reg, 0);
    
	// set up the gains :
	// Unity to gain and Pain 0 
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS0LReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS0LReg,0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS0RReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS0RReg,0);
    
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS1LReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS1LReg,0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS1RReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS1RReg,0);
    
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS2LReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS2LReg,0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS2RReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS2RReg,0);
    
	// Set up modem to unity
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS3LReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS3RReg,0xDF);

	// Set up PCMCIA to unity
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS4LReg,0xDF);
    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS4RReg,0xDF);

	// Default gain for all digital input
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASALReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASARReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASBLReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASBRReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASCLReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASCRReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASDLReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASDRReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASELReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASERReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASFLReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASFRReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASGLReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASGRReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASHLReg, kGAS_Default_Gain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kGASHRReg, kGAS_Default_Gain );
    
	// set analog gain on input to 0
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA0Reg, 0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA1Reg, 0); 
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA2Reg, 0);
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA3Reg, 0); 

	// enable digital output driver OS_E on subframe 0 
    Burgundy_writeCodecReg( ioBaseBurgundy, kSDInReg, 0 );             
    Burgundy_writeCodecReg( ioBaseBurgundy, kSDInReg2, kBurgundyOS_EOutputEnable );
	
    debugIOLog (3, "- AppleBurgundyAudio::sndHWInitialize");
}

void AppleBurgundyAudio::sndHWPostDMAEngineInit (IOService *provider) {
	if (NULL != driverDMAEngine)
		driverDMAEngine->setSampleLatencies (kBurgundySampleLatency, kBurgundySampleLatency);
}

// This method handles everything after the initHW thread is done, keeping the 
// same sequence of code as before the thread was introduced. [3284411]
void AppleBurgundyAudio::sndHWPostThreadedInit (IOService *provider) 
{
    AbsoluteTime		timerInterval;
	
	gCanPollSatus = true;
    checkStatus(true);    
        
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &AppleBurgundyAudio::timerCallback, timerInterval);

	mInternalMicDualMonoMode = e_Mode_CopyRightToLeft;		
    
    duringInitialization = false;
}

UInt32 AppleBurgundyAudio::sndHWGetCurrentSampleFrame (void) {
	return OSReadLittleInt32 (ioBaseBurgundy, 0x50);
}

void AppleBurgundyAudio::sndHWSetCurrentSampleFrame (UInt32 value) {
	OSWriteLittleInt32 (ioBaseBurgundy, 0x50, value);
}

UInt32 	AppleBurgundyAudio::sndHWGetInSenseBits(){
    UInt32 status, inSense;
    
    inSense = 0;
	// we need to add the parrallel input part
    status = Burgundy_readCodecSenseLines(ioBaseBurgundy) & kBurgundyInSenseMask;

    if(status & kBurgundyInSense0) 
        inSense |= kBurgundyInSense3;
    if(status & kBurgundyInSense1)
        inSense |= kBurgundyInSense2;
    if(status & kBurgundyInSense2)
        inSense |= kBurgundyInSense1;
    if(status & kBurgundyInSense3) 
        inSense |= kBurgundyInSense0;

    return(inSense);
}


UInt32 	AppleBurgundyAudio::sndHWGetRegister(UInt32 regNum){
    return(CodecControlRegister[regNum]);
}

IOReturn  AppleBurgundyAudio::sndHWSetRegister(UInt32 regNum, UInt32 value){
    UInt32 result = 0;
    return(result);

}

UInt32	AppleBurgundyAudio::sndHWGetConnectedDevices(void){
    return(super::getCurrentDevices());
}

/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/


UInt32	AppleBurgundyAudio::sndHWGetActiveOutputExclusive(void){
    UInt32 result = 0;
    return(result);
}

IOReturn   AppleBurgundyAudio::sndHWSetActiveOutputExclusive(UInt32 outputPort ){
    IOReturn result= kIOReturnSuccess;
    UInt32	physicalPort, tempOutputReg;
    UInt32 	logicalOutputPort;
    
    logicalOutputPort = outputPort;
    
    if( logicalOutputPort > kSndHWOutput5)
        return(kIOReturnError);
	
    physicalPort = GetPhysicalOutputPort(logicalOutputPort);	

    tempOutputReg = kBurgundyMuteAll;
    
	mModemActive = FALSE;
	mInternalSpeakerActive = FALSE;
	mExternalSpeakerActive = FALSE;
	mHeadphonesActive = FALSE;
	mMonoSpeakerActive = FALSE;

    switch(physicalPort) {
        case kBurgundyPhysOutputPortNone:
            debugIOLog (3, " ++ Writing Port None");
            break;
        case kBurgundyPhysOutputPort13:
            debugIOLog (3, " ++ Writing Port 13");
			mModemActive = TRUE;
            tempOutputReg |= ( kBurgundyMuteOffState << kBurgundyPort13MonoMute);
            break;
        case kBurgundyPhysOutputPort14:
            debugIOLog (3, " ++ Writing Port 14");
			mInternalSpeakerActive = TRUE;
            tempOutputReg |= (( kBurgundyMuteOffState << kBurgundyPort14LMute) | ( kBurgundyMuteOffState << kBurgundyPort14RMute));
            break;
        case kBurgundyPhysOutputPort15:
            debugIOLog (3, " ++ Writing Port 15");
			mExternalSpeakerActive = TRUE;
            tempOutputReg |= (( kBurgundyMuteOffState << kBurgundyPort15LMute) | ( kBurgundyMuteOffState << kBurgundyPort15RMute));
            break;
        case kBurgundyPhysOutputPort16:
            debugIOLog (3, " ++ Writing Port 16");
			mHeadphonesActive = TRUE;
            tempOutputReg |= (( kBurgundyMuteOffState << kBurgundyPort16LMute) | ( kBurgundyMuteOffState << kBurgundyPort16RMute));
            break;
        case kBurgundyPhysOutputPort17:
            debugIOLog (3, " ++ Writing Port 17");
			mMonoSpeakerActive = TRUE;
            tempOutputReg |= ( kBurgundyMuteOffState << kBurgundyPort17MonoMute);
            break;        
    } 
    
    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputMuteReg, tempOutputReg);
    return(result);
}

UInt32 	AppleBurgundyAudio::sndHWGetActiveInputExclusive(void){
    UInt32 result = mLogicalInput;
    return(result);
}

IOReturn   AppleBurgundyAudio::sndHWSetActiveInputExclusive(UInt32 input ){
    UInt32 physicalInput , curPhysicalInput, tmpReg;
    UInt8  muxToBe, curMux;
    
    debugIOLog (3, " + AppleBurgundyAudio::sndHWSetActiveInputExclusive : %lu", input);
    IOReturn result= kIOReturnSuccess;
    
	tmpReg = 0;
    if(mLogicalInput != input) {
        physicalInput = GetPhysicalInputPort(input );
        curPhysicalInput = GetPhysicalInputPort( mLogicalInput );
        curMux = GetInputMux(curPhysicalInput);
        muxToBe = GetInputMux(physicalInput);
          
		// first we disconnect the first input from the mixer 1;
		// - get the mux of the input
		// - putting the input analog gain to 0
		// - putting the digital gain to 0dB
		// - disconnecting the input from the mixer one
        if( 0!= curPhysicalInput) {
            switch( curMux ) {
                case kBurgundyMux0:		
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS0LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS0LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS0RReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS0RReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA0Reg, 0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg, 0);
                    break;
                case kBurgundyMux1:
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS1LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS1LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS1RReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS1RReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA1Reg, 0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg,0);
                    break;						//	fall through
                case kBurgundyMux2:
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS2LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS2LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS2RReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS2RReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA2Reg, 0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg, 0);
                    break;                                            //	fall through
                default:
                    break;
            }
        }
    
		// select the mux 
		// reactivate the digital gain : 
		// put the input to mixer 1
        if( physicalInput != kBurgundyPhysInputPortNone ) {
            switch( muxToBe ) {
                case kBurgundyMux0:		
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS0LReg,0xDF);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS0LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS0RReg,0xDF);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS0RReg,0);
                    // select the mux
                    switch(physicalInput) {
                        case kBurgundyPhysInputPort1:
                            tmpReg =kBurgundyAMux_0SelPort_1;
                            break;
                        case kBurgundyPhysInputPort2:
                            tmpReg =kBurgundyAMux_0SelPort_2;
                            break;
                        case kBurgundyPhysInputPort3:
                            tmpReg =kBurgundyAMux_0SelPort_3;
                            break;
                    }
                    
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMux01Reg , tmpReg);
					// connect the mux tp the mixer
                    mMuxMix = kBurgundyW_0_n;
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg,mMuxMix);
                    break;
                case kBurgundyMux1:
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS1LReg,0xDF);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS1LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS1RReg,0xDF);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS1RReg,0);
                    switch(physicalInput) {
                        case kBurgundyPhysInputPort4:
                            tmpReg = kBurgundyAMux_1SelPort_4;
                            break;
                        case kBurgundyPhysInputPort5:
                            tmpReg = kBurgundyAMux_1SelPort_5;
                            break;
                    }
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMux01Reg , tmpReg);
                    mMuxMix = kBurgundyW_1_n;
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg,mMuxMix);
                    break;						
                case kBurgundyMux2:
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS2LReg,0xDF);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS2LReg,0);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kGAS2RReg,0xDF);
                    Burgundy_writeCodecReg( ioBaseBurgundy, kPAS2RReg,0);
                    switch(physicalInput) {
                        case kBurgundyPhysInputPort6:
                            tmpReg = kBurgundyAMux_2SelPort_6;
                            break;
                        case kBurgundyPhysInputPort7:
                            tmpReg = kBurgundyAMux_2SelPort_7;
                            break;
                    }
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMux2Reg , tmpReg);
                    mMuxMix = kBurgundyW_2_n;
                    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg,mMuxMix);
                    break;                                            
                default:
                    break;
            }
        }                                						
        mLogicalInput = input;  // keep track of our new current active input for this system 
    }
    debugIOLog (3, " - AppleBurgundyAudio::sndHWSetActiveInputExclusive");

    return(result);
}

// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleBurgundyAudio::AdjustControls (void) {
	IOFixed							mindBVol;
	IOFixed							maxdBVol;
	Boolean							mustUpdate;

	debugIOLog (3, "+ AdjustControls()");
	FailIf (NULL == driverDMAEngine, Exit);
	mustUpdate = FALSE;

	mindBVol = kBURGUNDY_MIN_VOLUME;
	maxdBVol = kBURGUNDY_MAX_VOLUME;

	//	Must update if any of the following conditions exist:
	//	1.	No master volume control exists AND the master volume is the target
	//	2.	The master volume control exists AND the master volume is not the target
	//	3.	the minimum or maximum dB volume setting for the left volume control changes
	//	4.	the minimum or maximum dB volume setting for the right volume control changes
	
	if ((NULL == outVolMaster && TRUE == useMasterVolumeControl) ||
		(NULL != outVolMaster && FALSE == useMasterVolumeControl) ||
		(NULL != outVolLeft && outVolLeft->getMinValue () != minVolume) ||
		(NULL != outVolLeft && outVolLeft->getMaxValue () != maxVolume) ||
		(NULL != outVolRight && outVolRight->getMinValue () != minVolume) ||
		(NULL != outVolRight && outVolRight->getMaxValue () != maxVolume)) {
		mustUpdate = TRUE;
	}
	if (TRUE == mustUpdate) {
		debugIOLog (3, "AdjustControls: mindBVol = %d.0x%x, maxdBVol = %d.0x%x", 
			(unsigned int)( 0 != mindBVol & 0x80000000 ? ( mindBVol >> 16 ) | 0xFFFF0000 : mindBVol >> 16 ), 
			(unsigned int)( mindBVol << 16 ), 
			(unsigned int)( 0 != maxdBVol & 0x80000000 ? ( maxdBVol >> 16 ) | 0xFFFF0000 : maxdBVol >> 16 ), 
			(unsigned int)( maxdBVol << 16 ) );
	
		driverDMAEngine->pauseAudioEngine ();
		driverDMAEngine->beginConfigurationChange ();
	
		if (TRUE == useMasterVolumeControl) {
			// We have only the master volume control (possibly not created yet) and have to remove the other volume controls (possibly don't exist)
			if (NULL == outVolMaster) {
				debugIOLog (3, "AdjustControls: deleteing descrete channel controls and creating master control");
				// remove the existing left and right volume controls
				if (NULL != outVolLeft) {
					lastLeftVol = outVolLeft->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolLeft);
					outVolLeft = NULL;
				} 
		
				if (NULL != outVolRight) {
					lastRightVol = outVolRight->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolRight);
					outVolRight = NULL;
				}
	
				// Create the master control
				outVolMaster = IOAudioLevelControl::createVolumeControl((lastLeftVol + lastRightVol) / 2, minVolume, maxVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDAll,
													kIOAudioControlChannelNameAll,
													kOutVolMaster, 
													kIOAudioControlUsageOutput);
	
				if (NULL != outVolMaster) {
					driverDMAEngine->addDefaultAudioControl(outVolMaster);
					outVolMaster->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
					outVolMaster->flushValue ();
				}
			}
		} else {
			// or we have both controls (possibly not created yet) and we have to remove the master volume control (possibly doesn't exist)
			if (NULL == outVolLeft) {
				debugIOLog (3, "AdjustControls: deleteing master control and creating descrete channel controls");
				// Have to create the control again...
				if (lastLeftVol > kBURGUNDY_MAXIMUM_HW_VOLUME && NULL != outVolMaster) {
					lastLeftVol = outVolMaster->getIntValue ();
				}
				outVolLeft = IOAudioLevelControl::createVolumeControl (lastLeftVol, kBURGUNDY_MINIMUM_HW_VOLUME, kBURGUNDY_MAXIMUM_HW_VOLUME, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultLeft,
													kIOAudioControlChannelNameLeft,
													kOutVolLeft,
													kIOAudioControlUsageOutput);
				if (NULL != outVolLeft) {
					driverDMAEngine->addDefaultAudioControl (outVolLeft);
					outVolLeft->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
			
			if (NULL == outVolRight) {
				// Have to create the control again...
				if (lastRightVol > kBURGUNDY_MAXIMUM_HW_VOLUME && NULL != outVolMaster) {
					lastRightVol = outVolMaster->getIntValue ();
				}
				outVolRight = IOAudioLevelControl::createVolumeControl (lastRightVol, kBURGUNDY_MINIMUM_HW_VOLUME, kBURGUNDY_MAXIMUM_HW_VOLUME, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultRight,
													kIOAudioControlChannelNameRight,
													kOutVolRight,
													kIOAudioControlUsageOutput);
				if (NULL != outVolRight) {
					driverDMAEngine->addDefaultAudioControl (outVolRight);
					outVolRight->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
	
			if (NULL != outVolMaster) {
				driverDMAEngine->removeDefaultAudioControl (outVolMaster);
				outVolMaster = NULL;
			}
		}
	
		if (NULL != outVolMaster) {
			outVolMaster->setMinValue (minVolume);
			outVolMaster->setMinDB (mindBVol);
			outVolMaster->setMaxValue (maxVolume);
			outVolMaster->setMaxDB (maxdBVol);
			if (outVolMaster->getIntValue () > maxVolume) {
				outVolMaster->setValue (maxVolume);
			}
			outVolMaster->flushValue ();
		}
	
		if (NULL != outVolLeft) {
			outVolLeft->setMinValue (minVolume);
			outVolLeft->setMinDB (mindBVol);
			outVolLeft->setMaxValue (maxVolume);
			outVolLeft->setMaxDB (maxdBVol);
			if (outVolLeft->getIntValue () > maxVolume) {
				outVolLeft->setValue (maxVolume);
			}
			outVolLeft->flushValue ();
		}
	
		if (NULL != outVolRight) {
			outVolRight->setMinValue (minVolume);
			outVolRight->setMinDB (mindBVol);
			outVolRight->setMaxValue (maxVolume);
			outVolRight->setMaxDB (maxdBVol);
			if (outVolRight->getIntValue () > maxVolume) {
				outVolRight->setValue (maxVolume);
			}
			outVolRight->flushValue ();
		}
	
		driverDMAEngine->completeConfigurationChange ();
		driverDMAEngine->resumeAudioEngine ();
	}

Exit:
	debugIOLog (3, "- AdjustControls()");
	return kIOReturnSuccess;
}

UInt32 	AppleBurgundyAudio::sndHWGetProgOutput(){
    UInt32 result = 0, outputConfigPins;

	outputConfigPins = 0;		// just to quiet the warning in the compiler
	// outputConfigPins = Burgundy_readCodecReg( ioBaseBurgundy, kOutputCtl0Reg);
    result |= ( outputConfigPins & kBurgundyOut_Ctrl_0State ) ? kSndHWProgOutput0 : 0;
    result |= ( outputConfigPins & kBurgundyOut_Ctrl_1State ) ? kSndHWProgOutput1 : 0;
	
	// outputConfigPins = Burgundy_readCodecReg( ioBaseBurgundy, kOutputCtl2Reg);
    result |= ( outputConfigPins & kBurgundyOut_Ctrl_2State ) ? kSndHWProgOutput2 : 0;
    result |= ( outputConfigPins & kBurgundyOut_Ctrl_3State ) ? kSndHWProgOutput3 : 0;
    result |= ( outputConfigPins & kBurgundyOut_Ctrl_4State ) ? kSndHWProgOutput4 : 0;	
    
    return(result);
}

IOReturn   AppleBurgundyAudio::sndHWSetProgOutput(UInt32 outputBits){
    IOReturn result= kIOReturnSuccess;
    UInt32 outputConfigPins;
    
    outputConfigPins = 0;
    if( outputBits & ( kSndHWProgOutput0 ) )
            outputConfigPins |= kBurgundyOut_Ctrl_0State;
    if( outputBits & ( kSndHWProgOutput1 ) )
            outputConfigPins |= kBurgundyOut_Ctrl_1State;
//	Burgundy_writeCodecReg( ioBaseBurgundy, kOutputCtl0Reg, outputConfigPins);
	
    outputConfigPins = 0;
    if( outputBits & ( kSndHWProgOutput2 ) )
        outputConfigPins |= kBurgundyOut_Ctrl_2State;
    if( outputBits & ( kSndHWProgOutput3 ) )
        outputConfigPins |= kBurgundyOut_Ctrl_3State;
    if( outputBits & ( kSndHWProgOutput4 ) )
        outputConfigPins |= kBurgundyOut_Ctrl_4State;
	// Burgundy_writeCodecReg( ioBaseBurgundy, kOutputCtl2Reg,outputConfigPins);
    
    return(result);
}
    
// control function
bool AppleBurgundyAudio::sndHWGetSystemMute(void){
     return (mIsMute);
}

IOReturn  AppleBurgundyAudio::sndHWSetSystemMute (bool mutestate){
	IOReturn			result = kIOReturnSuccess;
	UInt32				mutes;

	mutes = 0;		// mute everything and keep the compiler happy
	if (mutestate != mIsMute) {
		mIsMute = mutestate;
		if (mutestate) {
			// we are muting. We do it by disconnecting the output from the mixer
			// we let only the OS_E stream for the sound input. Ther may be other
			// possibilities
			// Burgundy_writeCodecReg( ioBaseBurgundy, kOSReg, kBurgundyOS_E_MXO_1);

			// Mute all the amps
			mutestate = kBurgundyMuteAll;
        } else {
			// we reconnect everything
			// Burgundy_writeCodecReg( ioBaseBurgundy, kOSReg, kBurgundyOS_0_MXO_2| kBurgundyOS_1_MXO_2 | kBurgundyOS_E_MXO_1);

			// Set the volume to the correct volume as it might have been changed while we were muted
			sndHWSetSystemVolume (gVolLeft, gVolRight);
			// Unmute the amp currently in use
			if (TRUE == mModemActive) {
				mutes =  kBurgundyMuteOffState << kBurgundyPort13MonoMute;
			} else if (TRUE == mInternalSpeakerActive) {
				mutes =  kBurgundyMuteOffState << kBurgundyPort14LMute | kBurgundyMuteOffState << kBurgundyPort14RMute;
			} else if (TRUE == mExternalSpeakerActive) {
				mutes =  kBurgundyMuteOffState << kBurgundyPort15LMute | kBurgundyMuteOffState << kBurgundyPort15RMute;
			} else if (TRUE == mHeadphonesActive) {
				mutes =  kBurgundyMuteOffState << kBurgundyPort16LMute | kBurgundyMuteOffState << kBurgundyPort16RMute;
			} else if (TRUE == mMonoSpeakerActive) {
				mutes =  kBurgundyMuteOffState << kBurgundyPort17MonoMute;
			}
        } 
		Burgundy_writeCodecReg (ioBaseBurgundy, kOutputMuteReg, mutes);
    }

    return (result);
}

bool AppleBurgundyAudio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume){
    UInt8 leftAttn, rightAttn;
    UInt8 comAttn;
    UInt32 tLeftVolume, tRightVolume;

	// mVolLeft and leftVolume between 0 and 16
    if (leftVolume != mVolLeft) 
        mVolLeft = leftVolume; 
    tLeftVolume = mVolLeft;
    if (0 == tLeftVolume) tLeftVolume = 1; // leftVolume between 1 and 16
    tLeftVolume -= 1; 		    // leftVolume between 0 and 15	
    leftAttn = 15 - (UInt8) tLeftVolume; // leftAttn between 15 and 0	

    if (rightVolume != mVolRight) 
        mVolRight = rightVolume;
    tRightVolume = mVolRight;
    if (0 == tRightVolume) tRightVolume = 1;
    tRightVolume -= 1; 
    rightAttn = 15 - (UInt8) tRightVolume; // rightAttn between 15 and 0	    

	UInt32		muteStates;
	muteStates = Burgundy_readCodecReg (ioBaseBurgundy, kOutputMuteReg);

	if (mVolLeft == 0) {
		// Mute headphone and speaker because volume is 0
		muteStates &= ~(kOutputMuteReg_Port14L | kOutputMuteReg_Port16L);
	} else {
		// Mute either the headphone or the speaker, depending on which we're not using
		if (TRUE == mInternalSpeakerActive) {
			muteStates |=  kBurgundyMuteOffState << kBurgundyPort14LMute;
		} else if (TRUE == mHeadphonesActive) {
			muteStates |=  kBurgundyMuteOffState << kBurgundyPort16LMute;
		}
	}

	if (mVolRight == 0) {
		// Mute headphone and speaker because volume is 0
		muteStates &= ~(kOutputMuteReg_Port14R | kOutputMuteReg_Port16R);
	} else {
		// Mute either the headphone or the speaker, depending on which we're not using
		if (TRUE == mInternalSpeakerActive) {
			muteStates |=  kBurgundyMuteOffState << kBurgundyPort14RMute;
		} else if (TRUE == mHeadphonesActive) {
			muteStates |=  kBurgundyMuteOffState << kBurgundyPort16RMute;
		}
	}

    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputMuteReg, muteStates);

    if ((mVolLeft == 0) && (mVolRight == 0)) {
        mVolumeMuteIsActive = TRUE;
	} else {
		mVolumeMuteIsActive = FALSE;
	}

#if 0
    if ((mVolLeft == 0) && (mVolRight == 0)) {
        mVolumeMuteIsActive = true; 
        Burgundy_writeCodecReg (ioBaseBurgundy, kGAP0LReg , 0x00);
        Burgundy_writeCodecReg (ioBaseBurgundy, kGAP0RReg , 0x00);
        Burgundy_writeCodecReg (ioBaseBurgundy, kGAP1RReg , 0x00);
        Burgundy_writeCodecReg (ioBaseBurgundy, kGAP1LReg , 0x00);
    } else {
        // we should do it better to mute one channel only to have a good balance
        if (mVolumeMuteIsActive) {  // we do it only if we come back from noting
            mVolumeMuteIsActive = false;
            Burgundy_writeCodecReg (ioBaseBurgundy, kGAP0LReg , 0xFF);
            Burgundy_writeCodecReg (ioBaseBurgundy, kGAP0RReg , 0xFF);
            Burgundy_writeCodecReg (ioBaseBurgundy, kGAP1RReg , 0xFF);
            Burgundy_writeCodecReg (ioBaseBurgundy, kGAP1LReg , 0xFF);
        }
    }
#endif

    comAttn = rightAttn << 4 | leftAttn;
    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputLvlPort13Reg, comAttn & 0x0F);
    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputLvlPort14Reg, comAttn);
    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputLvlPort15Reg, comAttn);
    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputLvlPort16Reg, comAttn);
    Burgundy_writeCodecReg (ioBaseBurgundy, kOutputLvlPort17Reg, comAttn & 0x0F);

    bool result = true;
    return(result);
}

IOReturn AppleBurgundyAudio::sndHWSetSystemVolume(UInt32 value){
    IOReturn result= kIOReturnSuccess;
    sndHWSetSystemVolume(value, value);
    return(result);
}

IOReturn AppleBurgundyAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain){
    IOReturn myReturn = kIOReturnSuccess; 
    UInt32 totGain;
    UInt8 galeft, garight;
    
    debugIOLog (3, "+ AppleBurgundyAudio::sndHWSetSystemInputGain (%ld, %ld)", leftGain, rightGain);
    
    totGain = 0;
    galeft = (UInt8) leftGain;
    garight = (UInt8) rightGain;
    
    totGain |= galeft;
    totGain |=  (garight <<4);
        
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA0Reg,   totGain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA1Reg,   totGain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA2Reg,   totGain );
    Burgundy_writeCodecReg( ioBaseBurgundy, kVGA3Reg,   leftGain );
    debugIOLog (3, "- AppleBurgundyAudio::sndHWSetSystemInputGain");
    return(myReturn);
}


IOReturn AppleBurgundyAudio::sndHWSetPlayThrough(bool playthroughstate){
	IOReturn result= kIOReturnSuccess;
    debugIOLog (3, "+ AppleBurgundyAudio::sndHWSetPlayThrough");
    
    if(playthroughstate) 
        Burgundy_writeCodecReg( ioBaseBurgundy, kMX2Reg, mMuxMix | kBurgundyW_A_n);
    else 
        Burgundy_writeCodecReg( ioBaseBurgundy, kMX2Reg, kBurgundyW_A_n);
         
    debugIOLog (3, "- AppleBurgundyAudio::sndHWSetPlayThrough");
    return(result);
}
    
// Power Management
IOReturn AppleBurgundyAudio::sndHWSetPowerState ( IOAudioDevicePowerState theState ) {
    IOReturn		myReturn;
    
    debugIOLog (3, "+ AppleBurgundyAudio::sndHWSetPowerState");
    myReturn = kIOReturnSuccess;
    switch ( theState )  {
        case kIOAudioDeviceSleep:	myReturn = performDeviceIdleSleep();	break;		//	When sleeping
        case kIOAudioDeviceIdle:	myReturn = performDeviceSleep();		break;		//	When no audio engines running
        case kIOAudioDeviceActive:	myReturn = performDeviceWake();			break;		//	audio engines running
        default:
            debugIOLog (3, "AppleDACAAudio::sndHWSetPowerState unknown power state");
            myReturn = kIOReturnBadArgument ;
            break ;
    }
    debugIOLog (3, "- AppleBurgundyAudio::sndHWSetPowerState");
    return ( myReturn );
}

// --------------------------------------------------------------------------
IOReturn	AppleBurgundyAudio::performDeviceWake () {
    debugIOLog (3, "+ AppleBurgundyAudio::performDeviceWake");
    debugIOLog (3, "- AppleBurgundyAudio::performDeviceWake");
    return ( kIOReturnSuccess );
}

// --------------------------------------------------------------------------
IOReturn	AppleBurgundyAudio::performDeviceSleep () {
    debugIOLog (3, "+ AppleBurgundyAudio::performDeviceSleep");
    debugIOLog (3, "- AppleBurgundyAudio::performDeviceSleep");
    return ( kIOReturnSuccess );
}

// --------------------------------------------------------------------------
IOReturn	AppleBurgundyAudio::performDeviceIdleSleep () {
    debugIOLog (3, "+ AppleBurgundyAudio::performDeviceIdleSleep");
    debugIOLog (3, "- AppleBurgundyAudio::performDeviceIdleSleep");
    return ( kIOReturnSuccess );
}

// Identification
UInt32 	AppleBurgundyAudio::sndHWGetType( void ){
    UInt32 result;
    debugIOLog (3, "+ AppleBurgundyAudio::sndHWGetType");
    result = 0;
    debugIOLog (3, "- AppleBurgundyAudio::sndHWGetType");
    return(result);
}

UInt32	AppleBurgundyAudio::sndHWGetManufacturer( void ){
    UInt32 result, info;
    debugIOLog (3, "+ AppleBurgundyAudio::sndHWGetManufacturer");

    info = sndHWGetRegister(kBurgundyIDReg);
	
    switch (info & kBurgundySiliconVendor){
        case kBurgundyManfCrystal:
            result = kSndHWManfCrystal;
            break;
        case kBurgundyManfTI:
            result = kSndHWManfTI;
            break;
        default:
            result = kSndHWManfUnknown;
            break;
    }
    debugIOLog (3, "- AppleBurgundyAudio::sndHWGetManufacturer");
    return(result);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleBurgundyAudio::GetDeviceID (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*deviceID;
	UInt32					theDeviceID;

	debugIOLog (3,  "+ AppleBurgundyAudio::GetDeviceID" );
	theDeviceID = 0;

	FailIf ( NULL == ourProvider, Exit );
	sound = ourProvider->childFromPath (kSoundEntry, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kDeviceID));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32*)tmpData->getBytesNoCopy ();
	if (NULL != deviceID) {
		theDeviceID = *deviceID;
	}

Exit:
	debugIOLog (3,  "- AppleBurgundyAudio::GetDeviceID returns %d", (unsigned int)theDeviceID );
	return theDeviceID;
}

#pragma mark -- BURGUNDY SPECIFIC --
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will disconnect an input sample stream (i.e. MUX) from
//	ALL mixers.  Argument "mixer" is a bit mapped enable of the mixer
//	sources to be disconnected from an input stream as follows:
//
//		kBurgundyW_0_n		mux_0	ports 1, 2 & 3		built-in 
//		kBurgundyW_1_n		mux_1	ports 4 & 5			built-in 
//		kBurgundyW_2_n		mux_2	ports 6 & 7			built-in 
//		kBurgundyW_3_n		mux_3	port 8				telephony modem record
//		kBurgundyW_4_n		mux_4	port 9				pcmcia
//		kBurgundyW_A_n				subframe 0			built-in digital play
//		kBurgundyW_B_n				subframe 1			telephony digital play
void AppleBurgundyAudio::DisconnectMixer(UInt32 mixer) {
    UInt32	temp;

    temp = Burgundy_readCodecReg( ioBaseBurgundy, kMX0Reg);
    temp &= ~mixer;
    temp |= kBurgundyW_A_n;	
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX0Reg, temp); // playthrough path always gets digital stream
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX2Reg, temp); // ext. speaker/line out is a mirror image of play path
			
    temp = Burgundy_readCodecReg( ioBaseBurgundy, kMX1Reg); // disconnect input from recording input stream
    temp &= ~mixer;
    Burgundy_writeCodecReg( ioBaseBurgundy, kMX1Reg, temp);
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleBurgundyAudio::GetPhysicalInputPort( UInt32 logicalPort )
 {
	UInt32			physicalPort;
	
	physicalPort = kBurgundyPhysInputPortIllegal;

        switch( logicalPort ){
            case kSndHWInputNone:	physicalPort = kBurgundyPhysInputPortNone;	break;
            case kSndHWInput1:		physicalPort = kBurgundyPhysInputPort1;		break;
            case kSndHWInput2:		physicalPort = kBurgundyPhysInputPort2;		break;
            case kSndHWInput3:		physicalPort = kBurgundyPhysInputPort3;		break;
            case kSndHWInput4:		physicalPort = kBurgundyPhysInputPort4;		break;
            case kSndHWInput5:		physicalPort = kBurgundyPhysInputPort5;		break;
            case kSndHWInput6:		physicalPort = kBurgundyPhysInputPort6;		break;
            case kSndHWInput7:		physicalPort = kBurgundyPhysInputPort7;		break;
            case kSndHWInput8:		physicalPort = kBurgundyPhysInputPort8;		break;
            case kSndHWInput9:		physicalPort = kBurgundyPhysInputPort9;		break;
        }
	
	return physicalPort;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleBurgundyAudio::GetPhysicalOutputPort(UInt32 logicalPort )
{
	UInt32			physicalPort;
	
	physicalPort = kBurgundyPhysOutputPortIllegal;
        
        switch( logicalPort ) {
            case kSndHWOutputNone:	physicalPort = kBurgundyPhysOutputPortNone;	break;
            case kSndHWOutput1:		physicalPort = kBurgundyPhysOutputPort14;	break;
            case kSndHWOutput2:		physicalPort = kBurgundyPhysOutputPort15;	break;
            case kSndHWOutput3:		physicalPort = kBurgundyPhysOutputPort16;	break;
            case kSndHWOutput4:		physicalPort = kBurgundyPhysOutputPort17;	break;
            case kSndHWOutput5:		physicalPort = kBurgundyPhysOutputPort13;	break;
        }
	
	return physicalPort;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Returns the port configuration so that maximum gain can be determined.
//	
//	DIGITAL ONLY:		                        9
//	ANALOG & DIGITAL:			1 2 3 4       8
//	PREAMP, ANALOG & DIGITAL:	        5 6 7
UInt32 	AppleBurgundyAudio::GetInputPortType(UInt32 inputPhysicalPort){

    UInt32 result;
	
    switch( inputPhysicalPort ) {
        case kBurgundyPhysInputPort1:		//	fallthrough
        case kBurgundyPhysInputPort2:		//	fallthrough
        case kBurgundyPhysInputPort3:		//	fallthrough
        case kBurgundyPhysInputPort4:		//	fallthrough
        case kBurgundyPhysInputPort8:
            result = kBurgundyPortType_AD;
            break;
        case kBurgundyPhysInputPort5:		//	fallthrough
        case kBurgundyPhysInputPort6:		//	fallthrough
        case kBurgundyPhysInputPort7:
            result = kBurgundyPortType_AAD;
            break;
        case kBurgundyPhysInputPort9:
            result = kBurgundyPortType_D;
            break;
        case kBurgundyPhysInputPortNone:	//	fallthrough
        default:
            result = kBurgundyPortType_NONE;
            break;
    }
	
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Several of the analog inputs are available as in input selection to
//	more than one mux.  This routine determines which mux is the primary
//	destination for a specific input source.  For inputs that have more
//	than one possible destination, the primary source is the lower orderd
//	destination multiplexer.
UInt8	AppleBurgundyAudio::GetInputMux(UInt32 physicalInput){
    UInt8 theMux;
	
    if( physicalInput > kSndHWInput9) {
        theMux = kBurgundyMuxNone;
        goto Exit;
    }
	
    switch( physicalInput ){
        case kBurgundyPhysInputPort1:	//	fall through (port 1, 2 & 3 share the same mux)
        case kBurgundyPhysInputPort2:	//	fall through (port 1, 2 & 3 share the same mux)
        case kBurgundyPhysInputPort3:
            theMux = kBurgundyMux0;		
            break;
        case kBurgundyPhysInputPort4:	//	fall through (port 4 & 5 share the same mux)
        case kBurgundyPhysInputPort5:
            theMux = kBurgundyMux1;	
            break;
        case kBurgundyPhysInputPort6:	//	fall through (port 6 & 7 share the same mux)
        case kBurgundyPhysInputPort7:
            theMux = kBurgundyMux2;	
            break;
        case kBurgundyPhysInputPort8:
            theMux = kBurgundyMux3;	
            break;
        case kBurgundyPhysInputPort9:
            theMux = kBurgundyMux4;	
            break;
        case kBurgundyPhysInputPortNone:
            theMux = kBurgundyMuxNone;	
            break;
    }
Exit:
    return theMux;
}

void	AppleBurgundyAudio::ReleaseMux(UInt8 mux){
	// __SndHWSetPan( system, kMinSoftwareGain, kMinSoftwareGain );
    UInt32 temp;

	temp = 0;
    switch( mux ){
        case kBurgundyMux0:
            temp = kBurgundyW_0_n;	
            break;			//	prepare to disconnect the mux 0 from mixer 'n' connection
        case kBurgundyMux1:
            temp = kBurgundyW_1_n;	
            break;			//	prepare to disconnect the mux 1 from mixer 'n' connection
        case kBurgundyMux2:
            temp = kBurgundyW_2_n;	
            break;			//	prepare to disconnect the mux 2 from mixer 'n' connection
        case kBurgundyMux4:
            temp = kBurgundyW_4_n;	
            break;			//	prepare to disconnect the mux 4 from mixer 'n' connection
        }
        
	// disconnect the mux to mixer connection
    DisconnectMixer( temp );										//
	// gGlobals->muxReservation[mux] = kSndHWInputNone;

}

void	AppleBurgundyAudio::ReserveMux(UInt8 mux, UInt32 physicalInput){

}

IOReturn AppleBurgundyAudio::setModemSound(bool state) {
    IOReturn myReturn = kIOReturnSuccess;

	debugIOLog (3, "+ AppleBurgundyAudio::setModemSound(%d)", state);
	// make sure that the imic is not enabled when playthrough is turned on -- there will be no FEEDBACK!!!

	if(TRUE == state) {
		gIsModemSoundActive = true;
		Burgundy_writeCodecReg( ioBaseBurgundy, kMX2Reg, kBurgundyW_3_n | kBurgundyW_A_n );
	} else {
		Burgundy_writeCodecReg( ioBaseBurgundy, kMX2Reg, kBurgundyW_A_n);
		super::setModemSound(state);
	}

	debugIOLog (3, "- AppleBurgundyAudio::setModemSound(%d)", state);
    return(myReturn);
}
