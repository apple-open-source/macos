/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * Copyright 1996 1995 by Apple Computer, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/*
 * MKLINUX-1.0DR2
 */

/* 1 April 1997 Simon Douglas:
 *	Stolen wholesale from MkLinux.
 *	Added nonblocking adb poll from interrupt level for the debugger.
 *	Acknowledge before response so polled mode can work from inside the adb handler.
 *
 * 18 June 1998 sdouglas
 *	Start IOKit version. Fix errors from kCudaSRQAssertMask. Use ool cmd & reply buffers,
 *	not fixed len in packet. Does queueing here.
 *
 * 20 Nov 1998 suurballe
 *	Port to C++
 *
 * July 9 2001	adam.w
 *	Remove from kernel and make into KEXT
 *
 * 21 Nov 2001 zhangrob
 *	Fix Radar bug #2807111 - Not able to wake up Yosemite from doze using front panel switch
 *
 * 7 Dec 2001 galcher
 *	Fix Radar bug #2819221 - AppleOnboardDisplay not loading in Jaguar.  When this KEXT was
 *							 moved out of xnu, two symbols that AppleOnboardDisplay relied
 *							 on being able to call became no longer available to it.  Fixed
 *							 by adding two callPlatformFunction() calls that provide access
 *							 to the functions located at the (now missing) symbols.
 *							 Also - generally cleaned up the file and made it more readable.
 * 7 Mar 2002 galcher
 *	Fix Radar bug #2647253 - Cuda: Failure to Restart Automatically After Power Failure.
 *							 Power Manager is now providing a new API for telling anyone who
 *							 cares whether or not a given machine has ability to auto-restart
 *							 when power is removed from the system.  If that property is not
 *							 there, then the Energy Saver preference panel does not show us
 *							 the selection that allows us to choose the option to allow or
 *							 disallow (or see the Options tab for that matter).  Added the
 *							 appropriate setting of the property, as well as added the new
 *							 call to the PowerMgr API to publish the fact that we support
 *							 auto-restart on power loss.  That way, whenever the Energy Saver
 *							 PrefsPanel gets around to implementing that, it will continue
 *							 to work.
 *  Fix Radar bug #2302480 - Cuda: No way to disable auto restart after power outage.
 *							 Easy enough fix - turns out that ::start was calling a routine
 *							 that initializes the state of whether we should auto-restart or
 *							 not to "Yes".  This fix also required changing the way the
 *							 routine called in ::start handled things to incorporate setting
 *							 the "FileServer" property to make sure the older-style Energy
 *							 Save PrefsPanel would know that we have that option available.
 *							 ALSO - changed AppleCuda's ::newUserClient to allow the passing
 *							 in of the kApplePMUUserClientMagicCookie as a viable cookie.
 *							 Otherwise the current Energy Saver PrefsPanel passes the wrong
 *							 magic cookie and the user is unable to actually CHANGE the
 *							 "Auto-Restart" setting, even tho they can click the check box
 *							 in the PrefsPanel all they want.
 */


#include "AppleCuda.h"
#include "AppleCudaUserClient.h"
#include "IOCudaADBController.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/IOPM.h> 

#include <IOKit/assert.h>

#define super IOService

OSDefineMetaClassAndStructors(AppleCuda,IOService)

static  void 	cuda_interrupt ( AppleCuda * self );

static  void    cuda_process_response(AppleCuda * self);
static  void    cuda_transmit_data(AppleCuda * self);
static  void    cuda_expected_attention(AppleCuda * self);
static  void    cuda_unexpected_attention(AppleCuda * self);
static  void    cuda_receive_data(AppleCuda * self);
static  void    cuda_receive_last_byte(AppleCuda * self);
static  void    cuda_collision(AppleCuda * self);
static  void    cuda_idle(AppleCuda * self);

static  void    cuda_poll(AppleCuda * self);
static  void    cuda_error(AppleCuda * self);
static  void    cuda_send_request(AppleCuda * self);
static IOReturn cuda_do_sync_request( AppleCuda * self,
						cuda_request_t * request, bool polled);
static	void	cuda_do_state_transition_delay(AppleCuda * self);

static int Cuda_PE_poll_input(unsigned int options, char * c);
static int Cuda_PE_read_write_time_of_day(unsigned int options, long * secs);
static int Cuda_PE_halt_restart(unsigned int type);
static int Cuda_PE_write_IIC(unsigned char addr, unsigned char reg,
				unsigned char data);

static void
autopollArrived ( OSObject *inCuda, IOInterruptEventSource *, int );

static int set_cuda_power_message ( int command );
static int set_cuda_file_server_mode ( int command );
static int set_cuda_poweruptime(long secs);
static void cuda_async_set_power_message_enable( thread_call_param_t param, thread_call_param_t );
static void cuda_async_set_file_server_mode( thread_call_param_t param, thread_call_param_t ) ;

bool CudahasRoot( OSObject * us, void *, IOService * yourDevice );

static IOReturn AppleCudaReadIIC( UInt8 address, UInt8 * buffer, IOByteCount * count );
static IOReturn AppleCudaWriteIIC( UInt8 address, const UInt8 * buffer, IOByteCount * count );


//
// inline functions
//

static __inline__ unsigned char cuda_read_data(AppleCuda * self)
{
volatile unsigned char val;

    val = *self->cuda_via_regs.shift;
	eieio();
    return( val );
}

static __inline__ int cuda_get_result(cuda_request_t *request)
{
int status = ADB_RET_OK;
int theStatus = request->a_reply.a_header[1];
    
    if ( theStatus & kCudaTimeOutMask )
	{
        status = ADB_RET_TIMEOUT;
    }
// #if 0
//    // these are expected before autopoll mask is set
//	else if ( theStatus & kCudaSRQAssertMask )
//	{
//        status = ADB_RET_UNEXPECTED_RESULT;
//    }
// #endif
	else if ( theStatus & kCudaSRQErrorMask )
	{
        status = ADB_RET_REQUEST_ERROR;
    }
	else if ( theStatus & kCudaBusErrorMask )
	{
        status = ADB_RET_BUS_ERROR;
    }

    return status;
}

static __inline__ void cuda_lock( AppleCuda * self )
{
    if( !self->cuda_polled_mode )
        IOSimpleLockLock( self->cuda_request_lock );
}

static __inline__ void cuda_unlock( AppleCuda * self )
{
    if( !self->cuda_polled_mode )
        IOSimpleLockUnlock( self->cuda_request_lock );
}

//
// 		(Static) Globals
//

static AppleCuda * gCuda;


// **********************************************************************************
// init
//
// **********************************************************************************
bool AppleCuda::init ( OSDictionary * properties = 0 )
{
    return super::init(properties);
}




// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleCuda::start ( IOService * nub )
{
int				i;
IOMemoryMap		* viaMap;
unsigned char	* cuda_base;

    kprintf( "%s::start being called", getName() );
    if( !super::start(nub))
		return false;
    
    gCuda = this;
    // initialize callPlatformFunction symbols
    cuda_check_any_interrupt = OSSymbol::withCString("cuda_check_any_interrupt");
	cuda_read_i2c            = OSSymbol::withCString( "read_iic" );
	cuda_write_i2c           = OSSymbol::withCString( "write_iic" );
    
    workLoop				 = NULL;
    eventSrc				 = NULL;
    ourADBinterface			 = NULL;
    _rootDomain				 = 0; 
    _wakeup_from_sleep		 = false;

    workLoop				 = IOWorkLoop::workLoop();
    if ( !workLoop )
	{
	    kprintf( "%s::start is bailing (workloop creation failed)\n", getName() );
	    return false;
    }
    
    eventSrc = IOInterruptEventSource::interruptEventSource( this, autopollArrived );
    if (!eventSrc || ( kIOReturnSuccess != workLoop->addEventSource( eventSrc ) ) )
	{
	    kprintf( "%s::start is bailing (workLoop->addEvent failed)\n", getName() );
	    return false;
    }

    if( 0 == (viaMap = nub->mapDeviceMemoryWithIndex( 0 )) )
	{
	    IOLog( "%s: no via memory\n", getName() );
	    kprintf( "%s::start is bailing (memory mapping failed)\n", getName() );
	    return( false );
    }

    cuda_base = (unsigned char *)viaMap->getVirtualAddress();
    kprintf( "%s:  VIA base = %08x\n", getName(), (UInt32)cuda_base );

    ourADBinterface = new IOCudaADBController;
    if ( !ourADBinterface )
	{
	    kprintf( "%s::start is bailing (no ADB interface available)\n", getName() );
	    return( false );
    }
    if ( !ourADBinterface->init(0,this) )
	{
	    kprintf( "%s::start is bailing (ADB init failed)\n", getName() );
	    return( false );
    }
    
    if ( !ourADBinterface->attach( this) )
	{
	    kprintf( "%s::start is bailing (ADB attach failed)\n", getName() );
	    return( false );
    }
    
    cuda_request_lock = IOSimpleLockAlloc();
    IOSimpleLockInit( cuda_request_lock );

    cuda_via_regs.dataB         	    = cuda_base;
    cuda_via_regs.handshakeDataA        = cuda_base+0x0200;
    cuda_via_regs.dataDirectionB        = cuda_base+0x0400;
    cuda_via_regs.dataDirectionA        = cuda_base+0x0600;
    cuda_via_regs.timer1CounterLow      = cuda_base+0x0800;
    cuda_via_regs.timer1CounterHigh     = cuda_base+0x0A00;
    cuda_via_regs.timer1LatchLow        = cuda_base+0x0C00;
    cuda_via_regs.timer1LatchHigh       = cuda_base+0x0E00;
    cuda_via_regs.timer2CounterLow      = cuda_base+0x1000;
    cuda_via_regs.timer2CounterHigh     = cuda_base+0x1200;
    cuda_via_regs.shift         	    = cuda_base+0x1400;
    cuda_via_regs.auxillaryControl      = cuda_base+0x1600;
    cuda_via_regs.peripheralControl     = cuda_base+0x1800;
    cuda_via_regs.interruptFlag    	    = cuda_base+0x1A00;
    cuda_via_regs.interruptEnable       = cuda_base+0x1C00;
    cuda_via_regs.dataA         	    = cuda_base+0x1E00;

    // we require delays of this duration between certain state transitions
    clock_interval_to_absolutetime_interval(200, 1, &cuda_state_transition_delay);

    // Set the direction of the cuda signals.  ByteACk and TIP are output and
    // TREQ is an input

    *cuda_via_regs.dataDirectionB |= (kCudaByteAcknowledgeMask | kCudaTransferInProgressMask);
    *cuda_via_regs.dataDirectionB &= ~kCudaTransferRequestMask;

    // Set the clock control.  Set to shift data in by external clock CB1.

    *cuda_via_regs.auxillaryControl = (*cuda_via_regs.auxillaryControl | kCudaTransferMode) &
								    kCudaSystemRecieve;

    // Clear any posible cuda interrupt.

    if ( *cuda_via_regs.shift )
		;

    // Initialize the internal data.
    
    cuda_interrupt_state    = CUDA_STATE_IDLE;
    cuda_transaction_state  = CUDA_TS_NO_REQUEST;
    cuda_is_header_transfer = false;
    cuda_is_packet_type 	= false;
    cuda_transfer_count 	= 0;
    cuda_current_response   = NULL;
    for( i = 0; i < NUM_AP_BUFFERS; i++ )
	{
	    cuda_unsolicited[ i ].a_buffer = cuda_autopoll_buffers[ i ];
    }

    // Terminate transaction and set idle state

    cuda_neg_tip_and_byteack( this );

    // we want to delay 4 mS for ADB reset to complete

    IOSleep( 4 );

    // Clear pending interrupt if any...

    (void)cuda_read_data( this );

    // Issue a Sync Transaction, ByteAck asserted while TIP is negated.

    cuda_assert_byte_ack( this );

    // Wait for the Sync acknowledgement, cuda to assert TREQ

    cuda_wait_for_transfer_request_assert( this );

    // Wait for the Sync acknowledgement interrupt.

    cuda_wait_for_interrupt( this );

    // Clear pending interrupt

    (void)cuda_read_data( this );

    // Terminate the sync cycle by Negating ByteAck

    cuda_neg_byte_ack( this );

    // Wait for the Sync termination acknowledgement, cuda negates TREQ.

    cuda_wait_for_transfer_request_neg( this );

    // Wait for the Sync termination acknowledgement interrupt.

    cuda_wait_for_interrupt( this );

    // Terminate transaction and set idle state, TIP negate and ByteAck negate.
    cuda_neg_transfer_in_progress( this );

    // Clear pending interrupt, if there is one...
    (void)cuda_read_data( this );

// #if 0
//	    cuda_polled_mode = true;
// #else
#define	VIA_DEV_CUDA		2
    nub->registerInterrupt(VIA_DEV_CUDA,
			this, (IOInterruptAction) cuda_interrupt);
    nub->enableInterrupt(VIA_DEV_CUDA);
// #endif

    PE_poll_input             = Cuda_PE_poll_input;
    PE_read_write_time_of_day = Cuda_PE_read_write_time_of_day;
    PE_halt_restart           = Cuda_PE_halt_restart;
    PE_write_IIC              = Cuda_PE_write_IIC;

    publishResource( "IOiic0", this );
    publishResource( "IORTC",  this );

    _power_message_thread = thread_call_allocate ((thread_call_func_t) 	cuda_async_set_power_message_enable, (thread_call_param_t)this);

    // asynchronously set the file server mode initially to OFF
    thread_call_enter (_power_message_thread);
    _fileserver_thread    = thread_call_allocate((thread_call_func_t)
                                                cuda_async_set_file_server_mode,
                                                (thread_call_param_t)this);
    thread_call_enter( _fileserver_thread );

    registerService();	//Gossamer needs to find this driver for waking up G3

    // report to PM / Energy Saver / anyone else that cares,
    // that we support auto-restart on power loss (new for Jaguar)
	getPMRootDomain()->publishFeature( "FileServer" );

    _cuda_power_state = 1;  //default is wake state
    //We want to know when sleep is about to occur
    addNotification( gIOPublishNotification,serviceMatching("IOPMrootDomain"),
                 (IOServiceNotificationHandler)CudahasRoot, this, 0 );

    ourADBinterface->start( this );

    kprintf( "%s::start exiting normally\n", getName() );

    return true;
}




/* Here are some power management functions so we can tell when system is going to sleep. */

bool CudahasRoot( OSObject * us, void *, IOService * yourDevice )
{
    if (( yourDevice != NULL ) && ( ((AppleCuda *)us)->_rootDomain == 0) )
    {
        ((AppleCuda *)us)->_rootDomain = (IOPMrootDomain *) yourDevice;
        ((IOPMrootDomain *)yourDevice)->registerInterestedDriver((IOService *) us);
    }
    return true;
}   
 



IOReturn AppleCuda::powerStateWillChangeTo ( IOPMPowerFlags theFlags, unsigned long unused1,
    IOService* unused2)
{
    if ( ! (theFlags & IOPMPowerOn) )
    {
        _cuda_power_state = 0;  //0 means sleeping
    }
    return IOPMAckImplied;
}




IOReturn AppleCuda::powerStateDidChangeTo ( IOPMPowerFlags theFlags, unsigned long unused1,
    IOService* unused2)
{
    if (theFlags & IOPMPowerOn)
    {
        _cuda_power_state = 1;  //1 means awake
		_wakeup_from_sleep = false; //normally it is false
    }
    return IOPMAckImplied;
}



// *****************************************************************************
// getWorkLoop
//
// Return the cuda's workloop.
//
// *****************************************************************************
IOWorkLoop *AppleCuda::getWorkLoop() const
{
    return( workLoop );
}




// *****************************************************************************
// free
//
// Release everything we may have allocated.
//
// *****************************************************************************
void AppleCuda::free ( void )
{
    if ( workLoop )
	{
	    workLoop->release();
    }
    if ( eventSrc )
	{
	    eventSrc->release();
    }
    if ( ourADBinterface )
	{
	    ourADBinterface->release();
    }
    if (_rootDomain) 
    {
        _rootDomain->deRegisterInterestedDriver((IOService *) this); 
        _rootDomain = 0;
    }
    super::free();
}




// **********************************************************************************
// registerForADBInterrupts
//
// Some driver is calling to say it is prepared to receive "unsolicited" adb
// interrupts (e.g. autopoll keyboard and trackpad data).  The parameters identify
// who to call when we get one.
// **********************************************************************************
void AppleCuda::registerForADBInterrupts ( ADB_callback_func handler, IOService * caller )
{
    autopoll_handler = handler;
    ADBid            = caller;
}




// **********************************************************************************
// autopollArrived
//
// **********************************************************************************
static void autopollArrived ( OSObject * CudaDriver, IOInterruptEventSource *, int )
{
    ((AppleCuda *)CudaDriver)->serviceAutopolls();
}




#define RB_BOOT		1	/* Causes reboot, not halt.  Is in xnu/bsd/sys/reboot.h */
extern "C" {
	void boot(int paniced, int howto, char * command);
}





static void cuda_async_set_power_message_enable( thread_call_param_t param, thread_call_param_t )
{
    set_cuda_power_message( kADB_powermsg_enable );
}




static void cuda_async_set_file_server_mode( thread_call_param_t param, thread_call_param_t )
{
AppleCuda * me = (AppleCuda *) param;

    // initialize our auto-restart state at KEXT load-time to be OFF
    me->setFileServerMode( false );
}   




// **********************************************************************************
// serviceAutopolls
//      We get here just before calling autopollHandler() in IOADBController.cpp
// **********************************************************************************
void AppleCuda::serviceAutopolls ( void )
{
cuda_packet_t *	response;

	while( inIndex != outIndex )
	{
        response = &cuda_unsolicited[ outIndex ];

		//Check for power messages, which are handled differently from regular
		//  autopoll data coming from mouse or keyboard.
		if (response->a_header[0] == ADB_PACKET_POWER)
		{
		unsigned char flag, cmd;

			flag = response->a_header[1];
			cmd  = response->a_header[2];

			if ( (flag == kADB_powermsg_flag_chassis) &&
				 (cmd == kADB_powermsg_cmd_chassis_off) )
			{
				//thread_call_func(cuda_async_set_power_message_enable,
				//(thread_call_param_t)this, true);
				//thread_call_enter (_power_message_thread);
			AbsoluteTime deadline;

				if (_rootDomain)
				{
					if (_cuda_power_state)
					{
						//Put system to sleep now
						_rootDomain->receivePowerNotification (kIOPMSleepNow);
					}
					else //If asleep, wake up the system
					{
						//Tickle activity timer in root domain.  This will not
						// wake up machine that is in demand-sleep, but it will
						// wake up an inactive system that dozed
						_rootDomain->receivePowerNotification(kIOPMPowerButton); 
					}
				}
				clock_interval_to_deadline(1, kSecondScale, &deadline);
				thread_call_enter_delayed(_power_message_thread, deadline);
			}
			else if ((flag == kADB_powermsg_flag_keyboardpwr)
				&&  (cmd == kADB_powermsg_cmd_keyboardoff))
			{
				//set_cuda_power_message(kADB_powermsg_continue);
				//This needs to by async so Beige G3 ADB won't lock up
				//thread_call_func(cuda_async_set_power_message_enable, 
				//(thread_call_param_t)this, true);
				thread_call_enter (_power_message_thread);

			}
        }
        if ( ADBid != NULL )
		{
           (*autopoll_handler)(ADBid,response->a_header[2],response->a_bcount,response->a_buffer);
        }

        outIndex = (outIndex + 1) & (NUM_AP_BUFFERS - 1);

	} //end of while loop

}




// **********************************************************************************
// doSyncRequest
//
// **********************************************************************************
IOReturn AppleCuda::doSyncRequest ( cuda_request_t * request )
{
	return( cuda_do_sync_request(this, request, false) );
}





// **********************************************************************************
// AppleCudaWriteIIC
//
// **********************************************************************************
static IOReturn
AppleCudaWriteIIC( UInt8 address, const UInt8 * buffer, IOByteCount * count )
{
IOReturn	   ret;
cuda_request_t cmd;
    
    if( !gCuda )
        return( kIOReturnUnsupported );

    adb_init_request( &cmd );

    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_GET_SET_IIC;
    cmd.a_cmd.a_header[2] = address;
    cmd.a_cmd.a_hcount    = 3;
    cmd.a_cmd.a_buffer    = (UInt8 *) buffer;
    cmd.a_cmd.a_bcount    = *count;

    ret = cuda_do_sync_request( gCuda, &cmd, true );

    *count = cmd.a_cmd.a_bcount;

    return( ret );
}




// **********************************************************************************
// AppleCudaReadIIC
//
// **********************************************************************************
static IOReturn
AppleCudaReadIIC( UInt8 address, UInt8 * buffer, IOByteCount * count )
{
IOReturn	   ret;
cuda_request_t cmd;
    
    if( !gCuda )
        return( kIOReturnUnsupported );

    adb_init_request( &cmd );

    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_GET_SET_IIC;
    cmd.a_cmd.a_header[2] = address;
    cmd.a_cmd.a_hcount    = 3;
    cmd.a_reply.a_buffer  = buffer;
    cmd.a_reply.a_bcount  = *count;

    ret    = cuda_do_sync_request( gCuda, &cmd, true );
    *count = cmd.a_reply.a_bcount;

    return( ret );
}




// **********************************************************************************
// callPlatformFunction
//
// **********************************************************************************
IOReturn AppleCuda::callPlatformFunction(const OSSymbol *functionName,
										bool waitForFunction,
										void *param1, void *param2,
										void *param3, void *param4)
{  
IOReturn ioReturn = kIOReturnBadArgument;

    if (functionName == cuda_check_any_interrupt)
    {
	bool	*hasint;

		hasint  = (bool *)param1;
		*hasint = false;

		if ( inIndex != outIndex )
		{
			*hasint = true;
		}

		if ( _wakeup_from_sleep )
		{
			*hasint = true;
		}
		ioReturn = kIOReturnSuccess;
    }
	// These restore access to two routines // that were previously in
	// the kernel and directly callable.  on-board video drivers need
	// access to these routines (AppleCudaReadIIC(), // AppleCudaWriteIIC())
	// for backlight and geometry adjustments.
	else if ( functionName == cuda_read_i2c )
	{
        // make sure address argument is not bigger than UInt8
        if ( ( ((UInt32) param1) & ~0xFF ) == 0 )
            ioReturn = AppleCudaReadIIC( /* address */ (UInt8)			param1,
                                        /* buffer  */  (UInt8 *)		param2,
                                        /* count   */  (IOByteCount *)	param3 );
	}
	else if ( functionName == cuda_write_i2c )
	{
        // make sure address argument is not bigger than UInt8
        if ( ( ((UInt32) param1) & ~0xFF ) == 0 )
            ioReturn = AppleCudaWriteIIC(/* address */ (UInt8)			param1,
                                        /* buffer  */  (UInt8 *)		param2,
                                        /* count   */  (IOByteCount *)	param3 );
        // else - return what is in ioReturn by default (error)
	}
    
	// the call might not be ours to catch - call our superClass just in case
	else
		return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);

    return( ioReturn );
}




void
AppleCuda::setWakeTime( UInt32 waketime )
{
    //Call this function with waketime=0 in order to allow normal sleep again
    _wakeup_from_sleep = false;
    if (waketime != 0)
	{
        timerSrc = IOTimerEventSource::timerEventSource( (OSObject*)this, WakeupTimeoutHandler );

		if ( !timerSrc || (workLoop->addEventSource(timerSrc) != kIOReturnSuccess) )
		{
			IOLog( "Cuda can not register timeout event\n" );
			return;
		}

		timerSrc->setTimeoutMS( waketime );
    }
}




void
AppleCuda::WakeupTimeoutHandler( OSObject *object, IOTimerEventSource *timer )
{
    gCuda->_wakeup_from_sleep = true;
    if ( gCuda->_rootDomain )
    {
		gCuda->_rootDomain->activityTickle( 0,0 );
    }
}




void
AppleCuda::setPowerOnTime( UInt32 newTime )
{
long long_secs;

    if ( newTime != 0 )
	{
		Cuda_PE_read_write_time_of_day( kPEReadTOD, &long_secs );
		set_cuda_poweruptime( (long)newTime + long_secs );
    }
}





// --------------------------------------------------------------------------
//
// Method: setFileServerMode
//
// Purpose:
//		setFileServerMode allows the user to control whether or not Cuda
//		will, upon having power unexpectedly taken away, will cause a
//		machine to automatically restart when power is re-applied.
//		At power-up the "file server" flag is cleared, according to the
//		ERS.
//
//	Input(s):
//		fileServerMode		bool	1	(true) tell Cuda to enable auto-
//										restart.
//									0	(false) tells Cuda NOT to cause
//										auto-restarts.
// --------------------------------------------------------------------------
void
AppleCuda::setFileServerMode( bool fileServerMode )
{
    set_cuda_file_server_mode( (int) fileServerMode );

    // IOLog( "%s: setting 'FileServer' property to %d\n", getName(), fileServerMode );
    setProperty( "FileServer", (bool) fileServerMode );
}





#if 0
// --------------------------------------------------------------------------
//
// Method: setWakeOnRing
//
// Purpose:
//		setWakeOnRing allows the user to control whether or not Cuda
//		will, wake up if the modem detects an incoming call.
//
//		NOTE - this is NOT supported because the Cuda firmware does
//				not support it.
//
//	Input(s):
//		setForWake			bool	1	(true) tell Cuda to enable wake-on-ring.
//									0	(false) tells Cuda NOT to wake-on-ring.
//
// --------------------------------------------------------------------------
void
AppleCuda::setWakeOnRing( bool setForWake )
{
}
#endif



void AppleCuda::demandSleepNow(void)
{
    if (_rootDomain)
    {
		_rootDomain->receivePowerNotification( kIOPMSleepNow );
    }
}




// --------------------------------------------------------------------------
//
// Method: newUserClient
//
// Purpose:
//        newUserClient is called by the IOKit manager to create the
//        kernel receiver of a user request. The "type" is a qualifier
//        shared between the kernel and user client class instances..
// --------------------------------------------------------------------------

#define kAppleCudaUserClientMagicCookie 0x0C00DA
#define kApplePMUUserClientMagicCookie	0x0101BEEF	// soon to be "generic" PM user client magic cookie

IOReturn
AppleCuda::newUserClient(task_t			owningTask,
                        void			* securityToken,
                        UInt32      	magicCookie,
                        IOUserClient	** handler)
{
IOReturn			ioReturn = kIOReturnSuccess;
AppleCudaUserClient * client = NULL;


    if ( IOUserClient::clientHasPrivilege(securityToken, "root") != kIOReturnSuccess )
	{
        IOLog( "%s::newUserClient: Can't create user client, not privileged\n", getName() );
        return( kIOReturnNotPrivileged );
    }

    // Check that this is a user client type that we support.
    // type is known only to this driver's user and kernel
    // classes. It could be used, for example, to define
    // read or write privileges. In this case, we look for
    // a private value.
    switch( magicCookie )
    {
    case kAppleCudaUserClientMagicCookie:
    case kApplePMUUserClientMagicCookie:
        // Construct a new client instance for the requesting task.
        // This is, essentially  client = new AppleCudaUserClient;
        //				... create metaclasses ...
        //				client->setTask(owningTask)
        client = AppleCudaUserClient::withTask( owningTask );
        if ( client == NULL )
		{
            ioReturn = kIOReturnNoResources;
            IOLog( "%s::newUserClient: Unable to create user client\n", getName() );
        }
        break;

    default:
        ioReturn = kIOReturnInvalid;
        IOLog( "%s::newUserClient: bad magic cookie\n", getName() );
    }

    if ( ioReturn == kIOReturnSuccess )
	{
        // Attach ourself to the client so that this client instance
        // can call us.
        if ( client->attach(this) == false )
		{
            ioReturn = kIOReturnError;
            IOLog( "%s::newUserClient: Unable to attach user client\n", getName() );
        }
    }

    if ( ioReturn == kIOReturnSuccess )
	{
        // Start the client so it can accept requests.
        if ( client->start(this) == false )
		{
            ioReturn = kIOReturnError;
            IOLog( "%s::newUserClientt: Unable to start user client\n", getName() );
        }
    }

    if ( (ioReturn != kIOReturnSuccess) && (client != NULL) )
	{
        client->detach(this);
        client->release();
    }

    *handler = client;
    return( ioReturn );
}




// **********************************************************************************
// cuda_do_sync_request
//
// **********************************************************************************
IOReturn cuda_do_sync_request ( AppleCuda * self, cuda_request_t * request, bool polled )
{
bool				wasPolled = false;
IOInterruptState	ints;

    if( !polled )
	{
        request->sync = IOSyncer::create();
		request->needWake = true;
    }

    ints = IOSimpleLockLockDisableInterrupt( self->cuda_request_lock );

    if ( polled )
	{
        wasPolled = self->cuda_polled_mode;
        self->cuda_polled_mode = polled;
    }

    if ( self->cuda_last_request )
		self->cuda_last_request->a_next = request;
    else
		self->cuda_request = request;

    self->cuda_last_request = request;

    if ( self->cuda_interrupt_state == CUDA_STATE_IDLE )
		cuda_send_request(self);

    if ( polled )
	{
        cuda_poll( self );
        self->cuda_polled_mode = wasPolled;
        assert( 0 == self->cuda_request );
        assert( 0 == self->cuda_last_request );
    }

    IOSimpleLockUnlockEnableInterrupt( self->cuda_request_lock, ints );

    if( !polled )
		request->sync->wait();

    return cuda_get_result(request);
}





// **********************************************************************************
// Cuda_PE_read_write_time_of_day
//
// **********************************************************************************
static int Cuda_PE_read_write_time_of_day ( unsigned int options, long * secs )
{
cuda_request_t cmd;
    
    adb_init_request( &cmd );
    
    cmd.a_cmd.a_hcount    = 2;
    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    
    switch( options )
	{
	case kPEReadTOD:
            cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_GET_REAL_TIME;
            cmd.a_reply.a_buffer  = (UInt8 *)secs;
            cmd.a_reply.a_bcount  = sizeof(*secs);
	    break;

	case kPEWriteTOD:
            cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_SET_REAL_TIME;
            cmd.a_cmd.a_buffer    = (UInt8 *)secs;
            cmd.a_cmd.a_bcount    = sizeof(*secs);
	    break;

	default:
	    return 1;
    }

    return( cuda_do_sync_request( gCuda, &cmd, true ) );
}





// **********************************************************************************
// Cuda_PE_halt_restart
//
// **********************************************************************************
static int Cuda_PE_halt_restart ( unsigned int type )
{
cuda_request_t cmd;
    
    adb_init_request( &cmd );
    
    cmd.a_cmd.a_hcount    = 2;
    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    
    switch( type )
	{
	case kPERestartCPU:
        cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_RESTART_SYSTEM;
	    break;

	case kPEHaltCPU:
        cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_POWER_DOWN;
	    break;

	default:
	    return( 1 );
    }

    return( cuda_do_sync_request( gCuda, &cmd, true ) );
}




// **********************************************************************************
// Set the power-on time.  2001
// **********************************************************************************
static int set_cuda_poweruptime( long secs )
{
cuda_request_t	cmd;
long			localsecs = secs;

    adb_init_request( &cmd );

    cmd.a_cmd.a_hcount    = 2;
    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;

    cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_SET_POWER_UPTIME;
    cmd.a_cmd.a_buffer    = (UInt8 *)&localsecs;
    cmd.a_cmd.a_bcount    = 4;

    return( cuda_do_sync_request( gCuda, &cmd, true ) );
}





// **********************************************************************************
//
//	Routine:	set_cuda_file_server_mode
//
// In case this machine loses power, it will automatically reboot when power is
//   restored.  Only desktop machines have Cuda, so this feature will not affect
//   PowerBooks.
//
//	Inputs:		command		int		(misleading) it isn't a COMMAND as such.  it is
//									a boolean, with 1 == turn_ON, 0 = turn_OFF
//
// **********************************************************************************
static int set_cuda_file_server_mode ( int command )
{ 
cuda_request_t cmd;

    adb_init_request( &cmd ); 
    
    cmd.a_cmd.a_hcount    = 3;
    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_FILE_SERVER_FLAG;
    cmd.a_cmd.a_header[2] = command;
	    

	return( cuda_do_sync_request( gCuda, &cmd, true ) );
}       





// **********************************************************************************
// Fix front panel power key (mostly on Yosemites) so that one press won't power
//   down the entire machine
//
// **********************************************************************************
static int set_cuda_power_message ( int command )
{
cuda_request_t cmd;
    
    if ( command >= kADB_powermsg_invalid )
	    return 0;  //invalid Cuda power request
    
    adb_init_request( &cmd );

    cmd.a_cmd.a_hcount    = 3;
    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_SET_POWER_MESSAGES;
    cmd.a_cmd.a_header[2] = command;

    return( cuda_do_sync_request( gCuda, &cmd, true ) );
}





// **********************************************************************************
// Cuda_PE_write_IIC
//
// **********************************************************************************
static int Cuda_PE_write_IIC ( unsigned char addr, unsigned char reg, unsigned char data )
{
cuda_request_t cmd;
    
    adb_init_request(&cmd);
    
    cmd.a_cmd.a_header[0] = ADB_PACKET_PSEUDO;
    cmd.a_cmd.a_header[1] = ADB_PSEUDOCMD_GET_SET_IIC;
    cmd.a_cmd.a_header[2] = addr;
    cmd.a_cmd.a_header[3] = reg;
    cmd.a_cmd.a_header[4] = data;
    cmd.a_cmd.a_hcount    = 5;
    
    return( cuda_do_sync_request( gCuda, &cmd, true ) );
}





// **********************************************************************************
// Cuda_PE_poll_input
//
// **********************************************************************************
static int Cuda_PE_poll_input ( unsigned int, char * c )
{
AppleCuda		* self = gCuda;
int 			interruptflag;
UInt8			code;
cuda_packet_t	* response;
static char		keycodes2ascii[] = //0123456789abcdef
									"asdfhgzxcv_bqwer"	//00
									"yt123465=97-80]o"	//10
									"u[ip\nlj'k;_,/nm."	//20
									"\t_";				//30
    
    *c = 0xff;
    
    if( !self )
	{
	    return 1;
    }
    
    self->cuda_polled_mode = true;

    interruptflag = *self->cuda_via_regs.interruptFlag & kCudaInterruptMask;
    eieio();

    if( interruptflag )
	{
	    cuda_interrupt( self );
    }
    
    if( self->inIndex != self->outIndex )
	{
		response = &self->cuda_unsolicited[ self->outIndex ];
		if( ((response->a_header[2] >> 4) == 2)
			&&  (response->a_bcount > 1) )
		{
			code = response->a_buffer[0];
			if( code < sizeof(keycodes2ascii) )
			{
				*c = keycodes2ascii[ code ];
			}
		}
        self->outIndex = self->inIndex;
    }

    self->cuda_polled_mode = false;
    return( 0 );
}




//
//	----- internal -----
//


// **********************************************************************************
// cuda_send_request
//
// **********************************************************************************
static void cuda_send_request ( AppleCuda * self )
{

    // The data register must written with the data byte 25uS
    // after examining TREQ or we run the risk of getting out of sync
    // with Cuda. So call with disabled interrupts and spinlock held.

    // Check if we can commence with the packet transmission.  First, check if
    // Cuda can service our request now.  Second, check if Cuda wants to send
    // a response packet now.

    if( !cuda_is_transfer_in_progress( self ) )
	{
		// Set the shift register direction to output to Cuda by setting
		// the direction bit.

        cuda_set_data_direction_to_output( self );

        // Write the first byte to the shift register
        cuda_write_data( self, self->cuda_request->a_cmd.a_header[0] );

        // Set up the transfer state info here.

        self->cuda_is_header_transfer = true;
        self->cuda_transfer_count     = 1;

        // Make sure we're in idle state before transaction, and then
        // assert TIP to tell Cuda we're starting command
        cuda_neg_byte_ack( self );
        cuda_assert_transfer_in_progress( self );

        // The next state is going to be a transmit state, if there is
        // no collision.  This is a requested response but call it sync.

        self->cuda_interrupt_state   = CUDA_STATE_TRANSMIT_EXPECTED;
        self->cuda_transaction_state = CUDA_TS_SYNC_RESPONSE;
    } 

#if 0
    else
	{
		IOLog("Req = %x, state = %x, TIP = %x\n", self->cuda_request,
			self->cuda_interrupt_state, cuda_is_transfer_in_progress(self));
    }
#endif
}




// **********************************************************************************
// cuda_poll
//
// **********************************************************************************
static void cuda_poll( AppleCuda * self )
{
    do
	{
        cuda_wait_for_interrupt( self );
		cuda_interrupt( self );
    } while( self->cuda_interrupt_state != CUDA_STATE_IDLE );
}





// **********************************************************************************
// cuda_process_response
//  Execute at secondary interrupt.
//
// **********************************************************************************
static void cuda_process_response ( AppleCuda * self )
{
volatile cuda_request_t *	request;
unsigned int				newIndex;

    // Almost ready for the next state, which should be a Idle state.
    // Just need to notifiy the client.

    if ( self->cuda_transaction_state == CUDA_TS_SYNC_RESPONSE )
	{
        // dequeue reqeuest
        cuda_lock( self );
        request = self->cuda_request;
        if( NULL == ( self->cuda_request = request->a_next ) )
		{
            self->cuda_last_request = NULL;
		}
        cuda_unlock( self );

        // wake the sync request thread
        if ( ((cuda_request_t *)request)->needWake )
		{
            ((cuda_request_t *)request)->sync->signal();
		}

    }
    else
	{
		if ( self->cuda_transaction_state == CUDA_TS_ASYNC_RESPONSE )
		{
        	newIndex = (self->inIndex + 1) & (NUM_AP_BUFFERS - 1);
        	if( newIndex != self->outIndex )
			{
       			self->inIndex = newIndex;
        	}
#if 0
			else
			{
            		// drop this packet, and reuse the buffer
			}
#endif

        	if ( !self->cuda_polled_mode )
			{
				// wake thread to service autopolls
				self->eventSrc->interruptOccurred(0, 0, 0);
			}
        }
    }
    return;
}





// **********************************************************************************
// cuda_interrupt
//
// **********************************************************************************
static void cuda_interrupt ( AppleCuda * self )
{
unsigned char interruptState;

    // Get the relevant signal in determining the cause of the interrupt:
    // the shift direction, the transfer request line and the transfer
    // request line.

    interruptState = cuda_get_interrupt_state( self );

//kprintf("%02x",interruptState);

    switch ( interruptState )
	{
	case kCudaReceiveByte:
	    cuda_receive_data( self );
	    break;
    
	case kCudaReceiveLastByte:
	    cuda_receive_last_byte( self );
	    break;
    
	case kCudaTransmitByte:
	    cuda_transmit_data( self );
	    break;
    
	case kCudaUnexpectedAttention:
	    cuda_unexpected_attention( self );
	    break;
    
	case kCudaExpectedAttention:
	    cuda_expected_attention( self );
	    break;
    
	case kCudaIdleState:
	    cuda_idle( self );
	    break;
    
	case kCudaCollision:
	    cuda_collision( self );
	    break;
    
	// Unknown interrupt, clear it and leave.
	default:
	    cuda_error( self );
	    break;
    }
}





// **********************************************************************************
// cuda_transmit_data
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_transmit_data ( AppleCuda * self )
{
    // Clear the pending interrupt by reading the shift register.

    if ( self->cuda_is_header_transfer )
	{
        // There are more header bytes, write one out.
        cuda_write_data( self, self->cuda_request->a_cmd.a_header[self->cuda_transfer_count++] );

        // Toggle the handshake line.
        if ( self->cuda_transfer_count >= self->cuda_request->a_cmd.a_hcount )
		{
            self->cuda_is_header_transfer = FALSE;
            self->cuda_transfer_count     = 0;
        }

        cuda_toggle_byte_ack( self );
    }
    else
	{
		if ( self->cuda_transfer_count < self->cuda_request->a_cmd.a_bcount )
		{
			// There are more command bytes, write one out and update the pointer
			cuda_write_data( self,
				*(self->cuda_request->a_cmd.a_buffer + self->cuda_transfer_count++) );
			// Toggle the handshake line.
			cuda_toggle_byte_ack( self );
    	}
		else
		{
			(void)cuda_read_data( self );
			// There is no more command bytes, terminate the send transaction.
			// Cuda should send a expected attention interrupt soon.

			cuda_neg_tip_and_byteack( self );

			// The next interrupt should be a expected attention interrupt.

			self->cuda_interrupt_state = CUDA_STATE_ATTN_EXPECTED;
		}
    }
}





// **********************************************************************************
// cuda_expected_attention
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_expected_attention ( AppleCuda * self )
{
    // Clear the pending interrupt by reading the shift register.

    (void)cuda_read_data( self  );

    // Allow the VIA to settle directions.. else the possibility of
    // data corruption.
    cuda_do_state_transition_delay( self );

    if ( self->cuda_transaction_state ==  CUDA_TS_SYNC_RESPONSE )
	{
        self->cuda_current_response           = (cuda_packet_t*)&self->cuda_request->a_reply;
    }
    else
	{
        self->cuda_current_response           = &self->cuda_unsolicited[ self->inIndex ];
        self->cuda_current_response->a_hcount = 0;
        self->cuda_current_response->a_bcount = MAX_AP_RESPONSE;
    }

    self->cuda_is_header_transfer = true;
    self->cuda_is_packet_type     = true;
    self->cuda_transfer_count     = 0;

    // Set the shift register direction to input.
    cuda_set_data_direction_to_input( self );

    // Start the response packet transaction.
    cuda_assert_transfer_in_progress( self );

    // The next interrupt should be a receive data interrupt.
    self->cuda_interrupt_state = CUDA_STATE_RECEIVE_EXPECTED;
}





// **********************************************************************************
// cuda_expected_attention
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_unexpected_attention ( AppleCuda * self )
{
    // Clear the pending interrupt by reading the shift register.
    (void)cuda_read_data( self );

    // Get ready for a unsolicited response.
    self->cuda_current_response           = &self->cuda_unsolicited[ self->inIndex ];
    self->cuda_current_response->a_hcount = 0;
    self->cuda_current_response->a_bcount = MAX_AP_RESPONSE;

    self->cuda_is_header_transfer         = TRUE;
    self->cuda_is_packet_type             = TRUE;
    self->cuda_transfer_count             = 0;

    // Start the response packet transaction, Transaction In Progress
    cuda_assert_transfer_in_progress( self );

    // The next interrupt should be a receive data interrupt and the next
    // response should be an async response.
    self->cuda_interrupt_state           = CUDA_STATE_RECEIVE_EXPECTED;
    self->cuda_transaction_state         = CUDA_TS_ASYNC_RESPONSE;
}





// **********************************************************************************
// cuda_receive_data
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_receive_data ( AppleCuda * self )
{
    if ( self->cuda_is_packet_type )
	{
	unsigned char packetType;

        packetType = cuda_read_data( self );
        self->cuda_current_response->a_header[self->cuda_transfer_count++] = packetType;

        if ( packetType == ADB_PACKET_ERROR )
		{
            self->cuda_current_response->a_hcount = 4;
        }
		else
		{
            self->cuda_current_response->a_hcount = 3;
        }
        self->cuda_is_packet_type = false;
        cuda_toggle_byte_ack( self );
    }
    else
	{
		if ( self->cuda_is_header_transfer )
		{
			self->cuda_current_response->a_header[self->cuda_transfer_count++] =
				cuda_read_data( self );

			if ( self->cuda_transfer_count >= self->cuda_current_response->a_hcount )
			{
				self->cuda_is_header_transfer = FALSE;
				self->cuda_transfer_count     = 0;
			}
			cuda_toggle_byte_ack( self );
		}
		else 
		{
			if ( self->cuda_transfer_count < self->cuda_current_response->a_bcount ) 
			{
				// Still room for more bytes. Get the byte and tell Cuda to continue.
				// Toggle the handshake line, ByteAck, to acknowledge receive.

				*(self->cuda_current_response->a_buffer + self->cuda_transfer_count++) =
					cuda_read_data( self );
				cuda_toggle_byte_ack( self );
			}
			else 
			{
				// Cuda is still sending data but the buffer is full.
				// Normally should not get here.  The only exceptions are open ended
				// request such as  PRAM read...  In any event time to exit.

				self->cuda_current_response->a_bcount = self->cuda_transfer_count;

				cuda_read_data( self );

				cuda_process_response( self );
				cuda_neg_tip_and_byteack( self );
			}
		}
    }
}




// **********************************************************************************
// cuda_receive_last_byte
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_receive_last_byte ( AppleCuda * self )
{

    if ( self->cuda_is_header_transfer )
	{
        self->cuda_current_response->a_header[self->cuda_transfer_count++]  =
            cuda_read_data( self  );

        self->cuda_transfer_count = 0;
    }
    else
	{
		if ( self->cuda_transfer_count < self->cuda_current_response->a_bcount )
		{
			*(self->cuda_current_response->a_buffer + self->cuda_transfer_count++) =
			cuda_read_data( self );
    	}
		else
		{
			/* Overrun -- ignore data */
			(void) cuda_read_data( self );
    	}
    }
    self->cuda_current_response->a_bcount = self->cuda_transfer_count;
    // acknowledge before response so polled mode can work
    //	from inside the handler
    cuda_neg_tip_and_byteack( self );
    cuda_process_response( self );
}





// **********************************************************************************
// cuda_collision
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_collision ( AppleCuda * self )
{
    // Clear the pending interrupt by reading the shift register.
    (void)cuda_read_data( self );
    
    // Negate TIP to abort the send.  Cuda should send a second attention
    // interrupt to acknowledge the abort cycle.
    cuda_neg_transfer_in_progress( self );
    
    // The next interrupt should be an expected attention and the next
    // response packet should be an async response.
    
    self->cuda_interrupt_state    = CUDA_STATE_ATTN_EXPECTED;
    self->cuda_transaction_state  = CUDA_TS_ASYNC_RESPONSE;
    
    /* queue the request */
    self->cuda_is_header_transfer = false;
    self->cuda_transfer_count     = 0;
}





// **********************************************************************************
// cuda_idle
//  Executes at hardware interrupt level.
//
// **********************************************************************************
static void cuda_idle ( AppleCuda * self )
{

    // Clear the pending interrupt by reading the shift register.
    (void)cuda_read_data( self );
    
    cuda_lock( self );
	// Set to the idle state.
    self->cuda_interrupt_state = CUDA_STATE_IDLE;
	// See if there are any pending requests.
    if( self->cuda_request )
	{
	    cuda_send_request( self );
    }
    cuda_unlock( self );
}





// **********************************************************************************
// cuda_error
//
// **********************************************************************************
static void cuda_error ( AppleCuda * self )
{
    //printf("{Error %d}", self->cuda_transaction_state);
    
    // Was looking at cuda_transaction_state - doesn't seem right
    
    switch ( self->cuda_interrupt_state )
	{
	case CUDA_STATE_IDLE:
	    cuda_neg_tip_and_byteack( self );
	    break;
    
	case CUDA_STATE_TRANSMIT_EXPECTED:
	    if ( self->cuda_is_header_transfer && self->cuda_transfer_count <= 1 )
		{
			cuda_do_state_transition_delay( self );
			cuda_neg_transfer_in_progress( self );
			cuda_set_data_direction_to_input( self );
			panic (" CUDA - TODO FORCE COMMAND BACK UP!\n" );
	    }
	    else
		{
			self->cuda_interrupt_state = CUDA_STATE_ATTN_EXPECTED;
			cuda_neg_tip_and_byteack( self );
	    }
	    break;
    
	case CUDA_STATE_ATTN_EXPECTED:
	    cuda_assert_transfer_in_progress( self );
    
	    cuda_do_state_transition_delay( self );
	    cuda_set_data_direction_to_input( self );
	    cuda_neg_transfer_in_progress( self );
	    panic("CUDA - TODO CHECK FOR TRANSACTION TYPE AND ERROR");
	    break;
    
	case CUDA_STATE_RECEIVE_EXPECTED:
	    cuda_neg_tip_and_byteack( self );
	    panic( "Cuda - TODO check for transaction type and error\n" );
	    break;
    
	default:
	    cuda_set_data_direction_to_input( self );
	    cuda_neg_tip_and_byteack( self );
	    break;
    }
}





static void cuda_do_state_transition_delay( AppleCuda * self )
{
AbsoluteTime	deadline;

    clock_absolutetime_interval_to_deadline( self->cuda_state_transition_delay,
											&deadline);
    clock_delay_until( deadline );
}
