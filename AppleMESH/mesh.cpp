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

    /**
     * Copyright 1997-2000 Apple Computer Inc. All Rights Reserved.
     *	author    Mike Johnson
     *
     * Set tabs every 4 characters.
     *
     * Edit History
     * 25feb99   mlj      Initial conversion from banana sources.
     */


#include <IOKit/scsi/IOSCSIParallelInterface.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <mach/clock_types.h>

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/ppc/IODBDMA.h>

#include "mesh.h"

 	extern void		IOGetTime( mach_timespec_t *clock_time );
	extern void		kprintf( const char *, ... );
    extern void     call_kdp();     // for debugging



#define super	IOSCSIParallelController

	OSDefineMetaClassAndStructors( meshSCSIController, IOSCSIParallelController )	;



	static globals		g;	/**** Instantiate the globals ****/


    /* Channel Program. Note that this script must match the offsets    */
    /* specified in mesh.h. This script is copied into the  			*/
    /* channel command area (with appropriate entries byte-swapped so 	 */
    /* it ends up with the correct endian-ness).                     	*/
    /* Lines beginning with "slash, star, star, slash" are modified     */
    /* by the driver before it starts the Channel Program.              */

static const DBDMADescriptor   gDescriptorList[] =
{
            /* 0x00 kcclProblem - Branch here for anomalies */

    {   MESH_REG( kMeshInterruptMask, kMeshIntrMask )   },  // Enable MESH interrupt
    {   STOP( kcclStageCCLx )                           },  // anomaly

            /* 0x20 through 0x60 - Data for information phases: */

    {   RESERVE                                         },  // kcclCMDOdata - CDB ( 6,10,12,16 bytes)
    {   RESERVE                                         },  // kcclMSGOdata - MSGO data (last byte @3F)
    {   RESERVE                                         },  // kcclMSGIdata - MSGI data & STATUS
    {   RESERVE   /**** NO LONGER USED ****/            },  // kcclSenseCDB - CDB for (auto) Sense
    {   RESERVE                                         },  // kcclBatchSize, kcclStageLabel

            /* 0x70 - kcclSense - AutoSense commands - no longer used:  */

    {   RESERVE                                         },  // Spare
    {   RESERVE                                         },  // Spare
    {   RESERVE                                         },  // Spare
    {   RESERVE                                         },  // Spare
    {   RESERVE                                         },  // Spare

            /* 0xC0 - kcclPrototype - Prototype MESH 4-command Transfer sequence:   */

    {   MOVE_4( kcclBatchSize, 0, kRelAddressCP )       },  // MESH batch size
    {   MESH_REG( kMeshTransferCount1, 0 )              },  // Set high order Transfer Count
    {   MESH_REG( kMeshTransferCount0, 0 )              },  // Set low order Transfer Count
    {   MESH_REG( kMeshSequence, kMeshDataInCmd | kMeshSeqDMA )},  // Assume Data-In

    {   RESERVE                                         },  // kcclReadBuf8
    {   RESERVE                                         },  // spare

            /* 0x120 kcclStart - Arbitrate (START CHANNEL PROGRAM HERE):    */
            /* 0x140 kcclBrProblem                                          */

    {   STAGE( kcclStageArb )                           },
    {   MESH_REG( kMeshSequence, kMeshArbitrateCmd )    },  // issue Arbitrate
    {   BR_IF_PROBLEM                                   },  // branch if exception or error

            /* 0x150 - Select with Attention:  */

    {   STAGE( kcclStageSelA )                          },
    {   CLEAR_CMD_DONE                                  },
    {   MESH_REG( kMeshSequence, kMeshSelectCmd | kMeshSeqAtn ) },  // select with attention
    {   BR_IF_PROBLEM                                   },          // branch if failed

            /* 0x190 kcclMsgoStage - Message-Out:    */

    {   STAGE( kcclStageMsgO )                          },
    {   CLEAR_CMD_DONE                                  },

            /* 0x1B0 kcclMsgoBranch - modify this BRANCH to fall through for multibyte messages:    */

/**/{   BRANCH( kcclLastMsgo )                          },  // kcclMsgoBranch - go do only byte of Msg

            /* 0x1C0 - do all but last byte of multibyte message:  */

    {   MESH_REG( kMeshTransferCount1,  0x00 )          },  // count does include last byte
/**/{   MESH_REG( kMeshTransferCount0,  0xFF )          },  // kcclMsgoMTC - modify MESH xfer count here
    {   MESH_REG( kMeshSequence, kMeshMessageOutCmd | kMeshSeqAtn | kMeshSeqDMA )   },  // DMA MsgO with ATN
/**/{   MSGO( kcclMSGOdata, 255 )                       },  // kcclMsgoDTC - output all but last byte
    {   CLEAR_CMD_DONE                                  },

            /* 0x210 kcclLastMsgo - wait for REQ signal before dropping ATN:    */

    {   MESH_REG( kMeshInterruptMask, 0 )               },  // inhibit MESH interrupt
    {   MESH_REG_WAIT( kMeshSequence, kMeshStatusCmd | kMeshSeqAtn ) }, // gen PhaseMM
    {   CLEAR_INT_REG                                   },  // clear PhaseMM & CmdDone
    {   MESH_REG( kMeshInterruptMask, kMeshIntrException | kMeshIntrError ) },  // re-enable ERR/EXC Ints

            /* 0x250 - put out the last or only byte of Message-Out phase:  */

    {   MESH_REG( kMeshTransferCount1,  0x00 )          },
    {   MESH_REG( kMeshTransferCount0,  0x01 )          },
    {   MESH_REG( kMeshSequence, kMeshMessageOutCmd | kMeshSeqDMA ) },// no more ATN
    {   MSGO( kcclMSGOLast, 1 )                         },

            /* 0x290 kcclCmdoStage - Command Out:  */

    {   STAGE( kcclStageCmdO )                          },
    {   CLEAR_CMD_DONE                                  },
    {   MESH_REG( kMeshTransferCount1,  0x00 )          },
/**/{   MESH_REG( kMeshTransferCount0,  0x06 )          },  // kcclCmdoMTC - Set MESH xfer count to 6
    {   MESH_REG( kMeshSequence, kMeshCommandCmd | kMeshSeqDMA )},  // Command phase with DMA on
/**/{   CMDO( 6 )                                       },  // kcclCmdoDTC - output the CDB

            /* 0x2F0 - DATA XFER - branch to the built CCL @ 0x05D0:    */
            /* also, kcclReselect - reselect code enters here:          */

    {   CLEAR_CMD_DONE                                  },
    {   STAGE( kcclStageXfer )                          },
    {   BRANCH( kcclDataXfer )                          },  // go do Xfer CCL

            /* 0x320 kcclOverrun - dump excess data in the bit bucket:  */
            /* Exc and Err are still disabled.                          */

    {   STAGE( kcclStageBucket )                        },
    {   MESH_REG( kMeshTransferCount1,  0x00 )          },  // set MESH Transfer Count to max
    {   MESH_REG( kMeshTransferCount0,  0x00 )          },
    {   CLR_PHASEMM                                     },
    {   MESH_REG( kMeshInterruptMask, kMeshIntrException | kMeshIntrError ) },  // re-enable ERR/EXC Ints
/**/{   MESH_REG( kMeshSequence, kMeshDataInCmd | kMeshSeqDMA ) }, // set Seq Reg
/**/{   BUCKET                                          },  // OUT/INPUT_LAST the bits
    {   BR_NO_PROBLEM( kcclOverrunDBDMA )               },  // loop til PhaseMismatch
    {   BR_IF_PROBLEM                                   },  // take the interrupt now

            /* 0x3B0 kcclSyncCleanUp - clean up after Sync xfer:  */
    {   CLEAR_INT_REG                                   },  // clear PhaseMM & CmdDone (& Err?)
    {   MESH_REG( kMeshInterruptMask, kMeshIntrException | kMeshIntrError ) },  // re-enable ERR/EXC Ints

            /* 0x3D0 kcclGetStatus - setup CCL for status, command complete and bus free:   */

    {   STAGE( kcclStageStat )                          },
    {   MESH_REG( kMeshTransferCount1,  0x00 )          },
    {   MESH_REG( kMeshTransferCount0,  0x01 )          },  // set MESH xfer count to 1
    {   MESH_REG( kMeshSequence, kMeshStatusCmd | kMeshSeqDMA )},// Status-in phase with DMA on
    {   STATUS_IN                                       },  // input the status byte

            /* 0x420 - Message In:  */

    {   STAGE( kcclStageMsgI )                          },
    {   CLEAR_CMD_DONE                                  },
    {   MESH_REG( kMeshTransferCount1, 0x00 )           },
    {   MESH_REG( kMeshTransferCount0, 0x01 )           },  // set MESH xfer count to 1
    {   MESH_REG( kMeshSequence, kMeshMessageInCmd | kMeshSeqDMA )},   // Status-in phase with DMA on
    {   MSGI( 1 )                                       },  // get the Message-In byte

            /* 0x480 - Bus Free:   */

    {   STAGE( kcclStageFree )                          },
    {   CLEAR_CMD_DONE                                  },
    {   MESH_REG( kMeshSequence, kMeshEnableReselect )  },  // Enable Reselect
    {   MESH_REG( kMeshSequence, kMeshBusFreeCmd )      },  // Bus Free phase
    {   BR_IF_PROBLEM                                   },  // branch if failed

            /* 0x4D0 kcclMESHintr - Good completion:    */

    {   STAGE( kcclStageGood )                          },
    {   MESH_REG( kMeshInterruptMask, kMeshIntrMask )   },  // latch MESH interrupt
    {   STOP( kcclStageStop )                           },  // Stop

        /* The rest of the Channel Program area is used for autosense   */
        /* and data transfer channel commands:                          */
        /*  kcclDataXfer    Start of data transfer channel commands     */

}; /* end gDescriptorList structure */

    const UInt32 gDescriptorListSize = sizeof( gDescriptorList );


        /* MAX_DMA_XFER is set so that we don't have to worry about the     */
        /* ambiguous "zero" value in the MESH and DBDMA transfer registers  */
        /* that can mean either 65536 bytes or zero bytes.                  */

#define MAX_DMA_XFER    0x0000F000	// round down to nearest page


    enum                       /*****	values for g.intLevel:		*****/
    {
        kLevelISR       = 0x80, /* In Interrupt Service Routine         */
        kLevelLocked    = 0x40, /* MESH interrupts locked out           */
        kLevelSIH       = 0x20, /* In Secondary Interrupt Handler       */
        kLevelLatched   = 0x10  /* Interrupt latched                    */
    };


#if USE_ELG
static void AllocateEventLog( UInt32 size )
{
    if ( g.evLogBuf )
		return;

	g.evLogFlag = 0;            /* assume insufficient memory   */
	g.evLogBuf = (UInt8*)IOMalloc( size );
	if ( !g.evLogBuf )
	{
		kprintf( "probe - MESH evLog allocation failed " );
		return;
	}

	bzero( g.evLogBuf, size );
	g.evLogBufp	= g.evLogBuf;
	g.evLogBufe	= g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
//  g.evLogFlag  = 0xFEEDBEEF;	// continuous wraparound
//	g.evLogFlag  = 'step';		// stop at each ELG
g.evLogFlag  = 0x0333;		// any nonzero - don't wrap - stop logging at buffer end

IOLog( "AllocateEventLog - &globals=%8x buffer=%8x\n",
			(UInt32)&g, (UInt32)g.evLogBuf );

    return;
}/* end AllocateEventLog */


static void EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	register UInt32		*lp;           /* Long pointer      */
	mach_timespec_t		time;

	if ( g.evLogFlag == 0 )
		return;

	IOGetTime( &time );

    lp = (UInt32*)g.evLogBufp;
    g.evLogBufp += 0x10;

    if ( g.evLogBufp >= g.evLogBufe )       /* handle buffer wrap around if any */
    {    g.evLogBufp  = g.evLogBuf;
        if ( g.evLogFlag != 0xFEEDBEEF )    // make 0xFEEDBEEF a symbolic ???
            g.evLogFlag = 0;                /* stop tracing if wrap undesired   */
    }

        /* compose interrupt level with 3 byte time stamp:  */

	*lp++ = (g.intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp   = ascii;

    if( g.evLogFlag == 'step' )
	{	static char	code[ 5 ] = {0,0,0,0,0};
		*(UInt32*)&code = ascii;
	//	kprintf( "%8x mesh: %8x %8x %s        %s\n", time.tv_nsec>>10, a, b, code, str );
		kprintf( "%8x mesh: %8x %8x %s\n", time.tv_nsec>>10, a, b, code );
        IOLog( "%8x MESH: %8x %8x %s\n",
                   time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
	}

    return;
}/* end EvLog */


static void Pause( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
    char        work [ 256 ];
    char        name[] = "meshSCSIController:";
    char        *bp = work;
    UInt8       x;
    int         i;


    EvLog( a, b, ascii, str );
    EvLog( '****', '** P', 'ause', "*** Pause" );

    bcopy( name, bp, sizeof( name ) );
    bp += sizeof( name ) - 1;

    *bp++ = '{';                               // prepend p1 in hex:
    for ( i = 7; i >= 0; --i )
    {
        x = a & 0x0F;
        if ( x < 10 )
             x += '0';
        else x += 'A' - 10;
        bp[ i ] = x;
        a >>= 4;
    }
    bp += 8;

    *bp++ = ' ';                               // prepend p2 in hex:

    for ( i = 7; i >= 0; --i )
    {
        x = b & 0x0F;
        if ( x < 10 )
             x += '0';
        else x += 'A' - 10;
        bp[ i ] = x;
        b >>= 4;
    }
    bp += 8;
    *bp++ = '}';

    *bp++ = ' ';

    for ( i = sizeof( work ) - (int)(bp - work); i && (*bp++ = *str++); --i )   ;

//	kprintf( work );
//	panic( work );
	Debugger( work );
//  call_kdp();         // ??? use kdp=3 in boot parameters
    return;
}/* end Pause */
#endif // USE_ELG



bool meshSCSIController::configure(	IOService			*provider,
									SCSIControllerInfo	*controllerInfo )
{
	IOReturn		ioReturn = kIOReturnInternalError;


	g.intLevel		= 0;
	g.meshInstance	= this;
#if USE_ELG
    AllocateEventLog( kEvLogSize );
    ELG( g.evLogBufp, &g.evLogFlag, 'MESH', "configure - event logging set up." );
#endif /* USE_ELG */

	ELG( this, provider, 'Cnfg', "configure" );

    fProvider = (IOPCIDevice*)provider;

	ioReturn = initializeHardware();
	if ( ioReturn != kIOReturnSuccess )
		return false;

		/* Register our interrupt handler routine:	*/

    fInterruptEvent = IOInterruptEventSource::interruptEventSource(
							(OSObject*)this,
							(IOInterruptEventAction)&meshSCSIController::interruptOccurred,
							provider,
							0 );

    if ( fInterruptEvent == NULL )
    {
		PAUSE( 0, 0, 'IES-', "registerMESHInterrupt - can't register interrupt action" );
        return false;
    }

    getWorkLoop()->addEventSource( fInterruptEvent );
    fInterruptEvent->enable();

		/* allocate a big-endian memory cursor:	*/

    fMemoryCursor = IOBigMemoryCursor::withSpecification( kMaxDMATransfer, kMaxDMATransfer );
    if ( fMemoryCursor == NULL )
    {
		PAUSE( 0, kMaxDMATransfer, 'Mem-', "mesh::start - IOBigMemoryCursor::withSpecification NG" );
        return false;
    }


		/* Fill in the  SCSIControllerInfo structure and return:	*/

	controllerInfo->initiatorId				= 7;

	controllerInfo->maxTargetsSupported		= 8;
	controllerInfo->maxLunsSupported		= 8;

	controllerInfo->minTransferPeriodpS		= 100000;	/* picoSecs for 10 MHz	*/
	controllerInfo->maxTransferOffset		= 15;
	controllerInfo->maxTransferWidth		= 1;

	controllerInfo->maxCommandsPerController= 0;
	controllerInfo->maxCommandsPerTarget	= 8;		// 0 is unlimited
	controllerInfo->maxCommandsPerLun		= 0;

	controllerInfo->tagAllocationMethod		= kTagAllocationPerLun;
	controllerInfo->maxTags					= 256;

	controllerInfo->commandPrivateDataSize	= sizeof( PrivCmdData );

	controllerInfo->disableCancelCommands	= false;

		/*power management code*/
		
	#define number_of_power_states 2

    static IOPMPowerState ourPowerStates[number_of_power_states] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
    };
    	
	PMinit();
	registerPowerDriver(this,ourPowerStates,number_of_power_states);
	getProvider()->joinPMtree(this);
	return true;
}/* end configure */


	// Method: setPowerState

IOReturn meshSCSIController::setPowerState(	unsigned long	powerStateOrdinal,
									IOService		*whatDevice )
{						 
		if (powerStateOrdinal == 1)     	        	
        	setSCSIActiveTermState(true);        	
        else
        	setSCSIActiveTermState(false);
        	 
        return IOPMAckImplied;
}/* end setPowerState */


void meshSCSIController:: setSCSIActiveTermState(bool enableTermPower)

{
    IOService *heathrow;
    heathrow = waitForService(serviceMatching("Heathrow"));
    if(heathrow) 
    {
	UInt32 heathrowFCROffset = 0x38;
	UInt32 scsiEnMask = 1<<10;
    	UInt32 heathrowIDOffset = 0x34;
    	UInt32 scsiTermPowerMask = 1<<26;
	       
       heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_safeWriteRegUInt32"),
              false, (void *)heathrowFCROffset, (void *)scsiEnMask, enableTermPower ? (void *)scsiEnMask:0, 0);           
       heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_safeWriteRegUInt32"),
              false, (void *)heathrowIDOffset, (void *)scsiTermPowerMask, enableTermPower ? 0 : (void *)scsiTermPowerMask, 0);           
     }
}

void meshSCSIController::executeCommand( IOSCSIParallelCommand *scsiCommand )
{
	SCSICDBInfo			scsiCDB;
	SCSITargetParms		targetParms;
	UInt8				msgByte;
	UInt8				rc;


	if ( fCmd || (g.intLevel & kLevelLatched) )
	{
		disableCommands();
		rescheduleCommand( scsiCommand );
		ELG( fCmd, scsiCommand, 'Busy', "executeCommand - bus busy so bounce this cmd" );
		return;
	}

    fCmd 		= scsiCommand;
    fCmdData	= (PrivCmdData*)scsiCommand->getCommandData();
	bzero( fCmdData, sizeof( *fCmdData ) );

    scsiCommand->getTargetLun( &fCurrentTargetLun );
    scsiCommand->getCDB( &scsiCDB );
    scsiCommand->getDevice( kIOSCSIParallelDevice )->getTargetParms( &targetParms );

	ELG( scsiCommand, *(UInt16*)&fCurrentTargetLun<<16 | (scsiCDB.cdbTag & 0xFFFF), 'Exec', "meshSCSIController::executeCommand" );
	ELG( *(UInt32*)&scsiCDB.cdb[0], *(UInt32*)&scsiCDB.cdb[4] , '=CDB', "executeCommand - CDB" );

	fMsgOutFlag     = 0;
	fMsgOutPtr		= &fCCL[ kcclMSGOdata ];

		/* Identify byte:	*/

    msgByte = kSCSIMsgIdentify | kSCSIMsgEnableDisconnectMask | fCurrentTargetLun.lun;
    if ( scsiCDB.cdbFlags & kCDBFlagsNoDisconnect )
         msgByte &= ~kSCSIMsgEnableDisconnectMask;
    *fMsgOutPtr++ = msgByte;

		/* Tag msg:	*/

    if ( scsiCDB.cdbTagMsg )
    {
        *fMsgOutPtr++ = scsiCDB.cdbTagMsg;
        *fMsgOutPtr++ = scsiCDB.cdbTag;
		ELG( 0, scsiCDB.cdbTagMsg<<16 | scsiCDB.cdbTag, ' tag', "meshSCSIController::executeCommand - tag" );
    }

		/* Abort msg:	*/

    if ( scsiCDB.cdbAbortMsg )
    {
		ELG( scsiCommand->getOriginalCmd(), scsiCDB.cdbAbortMsg, 'Abor', "meshSCSIController::executeCommand - abort msg." );
        *fMsgOutPtr++ = scsiCDB.cdbAbortMsg;
    }

		/* Sync negotiation msg:	*/

	fCmdData->negotiatingSDTR = fCmdData->negotiatingSDTRComplete = false;
	if ( scsiCDB.cdbFlags & kCDBFlagsNegotiateSDTR )
    {	
        fCmdData->negotiatingSDTR = true;
        *fMsgOutPtr++ = kSCSIMsgExtended;
        *fMsgOutPtr++ = 3;
        *fMsgOutPtr++ = kSCSIMsgSyncXferReq;
		
        if( targetParms.transferPeriodpS < 100000 )
        	*fMsgOutPtr++ = 100000 / 4000;
        else
        	*fMsgOutPtr++ = targetParms.transferPeriodpS / 4000;

        *fMsgOutPtr++ = targetParms.transferOffset;
		
    }


		/***** Try to start the command on the hardware:	*****/

	rc = startCommand();			/* Call the hardware layer.	*/

	if ( rc != kHardwareStartOK )
	{								/* Hardware can't start now	*/
		ELG( fCmd, 0, 'Exe-', "meshSCSIController::executeCommand - command bounced back" );
		rescheduleCommand( fCmd );
		fCmd = NULL;
	}

    return;
}/* end executeCommand */


void meshSCSIController::cancelCommand( IOSCSIParallelCommand *scsiCommand )
{
    IOSCSIParallelCommand	*origCmd;
 	PrivCmdData		*origCmdData;
	SCSIResults		results;

    origCmd = scsiCommand->getOriginalCmd();

	ELG( scsiCommand, origCmd, 'Can-', "meshSCSIController::cancelCommand" );

	if ( origCmd )	/* if original command still around, complete it:	*/
	{
		origCmd->getResults( &results );
		origCmdData					= (PrivCmdData*)origCmd->getCommandData();
		results.bytesTransferred	= origCmdData->results.bytesTransferred;
		origCmd->setResults( &results );
		origCmd->complete();
	}

    scsiCommand->complete();
	return;
}/* end cancelCommand */


void meshSCSIController::resetCommand( IOSCSIParallelCommand *scsiCommand )
{
	ELG( scsiCommand, 0, 'Rst-', "meshSCSIController::resetCommand" );
	resetBus();
	fCmdData = (PrivCmdData*)scsiCommand->getCommandData();
	bzero( &fCmdData->results, sizeof( fCmdData->results ) );
	scsiCommand->setResults( &fCmdData->results );
    scsiCommand->complete();
	return;
}/* end resetCommand */


    /* Fetch the device's bus address and interrupt port number.  */
    /* Also, allocate one page of memory for the channel program. */

IOReturn meshSCSIController::initializeHardware()
{
    IOReturn        ioReturn;
	int				i;

	ELG( 0, 0, 'IniH', "initializeHardware" );


	fInitiatorID		= kInitiatorIDDefault;
	fInitiatorIDMask	= 1 << kInitiatorIDDefault;	/* BusID bitmask for reselection. */

	for ( i = 0; i < 8; ++i)
		fSyncParms[ i ] = kSyncParmsAsync;

	ioReturn = getHardwareMemoryMaps();

    if ( ioReturn == kIOReturnSuccess )
		 ioReturn = allocHdwAndChanMem();

    if ( ioReturn == kIOReturnSuccess )
		 ioReturn = doHBASelfTest();

    if ( ioReturn == kIOReturnSuccess )
    {
		ioReturn = resetBus();
        fMESHAddr->sourceID = fInitiatorID;
    }

    return ioReturn;
}/* end initializeHardware */


IOReturn meshSCSIController::getHardwareMemoryMaps()
{

	if ( !fSCSIMemoryMap )
	{
	    fSCSIMemoryMap = fProvider->mapDeviceMemoryWithIndex( kMESHRegisterBase );
	    if ( !fSCSIMemoryMap )
		{
			ELG( 0, 0, 'Map-', "getHardwareMemoryMaps - can't map MESH." );
			return kIOReturnInternalError;
	    }

		fMESHPhysAddr	= fSCSIMemoryMap->getPhysicalAddress();
		fMESHAddr		= (MeshRegister*)fSCSIMemoryMap->getVirtualAddress();
		ELG( fMESHPhysAddr, fMESHAddr, '=MSH', "getHardwareMemoryMaps - MESH regs" );
        g.meshAddr = (UInt32)fMESHAddr;      // for debugging, miniMon ...
	}

	if ( !fDBDMAMemoryMap )
	{
	    fDBDMAMemoryMap	= fProvider->mapDeviceMemoryWithIndex( kDBDMARegisterBase );
	    if ( !fDBDMAMemoryMap )
		{
			ELG( 0, 0, 'map-', "getHardwareMemoryMaps - can't map DBDMA." );
			return kIOReturnInternalError;
	    }
		dbdmaAddrPhys	= fDBDMAMemoryMap->getPhysicalAddress();
		dbdmaAddr		= (UInt8*)fDBDMAMemoryMap->getVirtualAddress();
		ELG( dbdmaAddrPhys, dbdmaAddr, '=DMA', "getHardwareMemoryMaps - DBDMA regs" );
#if CustomMiniMon
		gMESH_DBDMA      = (UInt32)dbdmaAddr;
		gMESH_DBDMA_Phys = (UInt32)dbdmaAddrPhys;
#endif /* CustomMiniMon */
	}

	return kIOReturnSuccess;
}/* end getHardwareMemoryMaps */



    /* Fetch the device's bus address and allocate one page of memory   */
    /* for the channel command. (Strictly speaking, we don't need an    */
    /* entire page, but we can use the rest of the page for a permanent */
    /* status log).                                                     */
    /* @param   deviceDescription   Specify the device to initialize.   */
    /* @return  kIOReturnSuccess if successful, else an error status.       */

IOReturn meshSCSIController::allocHdwAndChanMem()
{
        /* Set the default selection timeout to the MESH value (10 msec units). */

    fSelectionTimeout = 250 / 10;   // ??? symbolic

		/* Allocate a page of wired-down memory in the kernel:	*/

    fCCLSize  = page_size;
    fCCL      = (UInt8*)IOMallocContiguous( fCCLSize, page_size, &fCCLPhysAddr );
    if ( !fCCL )
    {   PAUSE( 0, fCCLSize, 'CCA-', "allocHdwAndChanMem - can't allocate channel command area.\n" );
        return kIOReturnNoMemory;
    }

		/* Remember the number of DBDMA descriptors that    */
		/* can be used for data transfer channel commands.  */

	fDBDMADescriptorMax = (fCCLSize - kcclDataXfer) / sizeof( DBDMADescriptor );

	g.cclPhysAddr   = (UInt32)fCCLPhysAddr;  // for debugging ease
	g.cclLogAddr    = (UInt32)fCCL;

	ELG( fCCLPhysAddr, fCCL, '=CCL', "allocHdwAndChanMem - CCL phys/logical addresses." );
	initCP();

        /* What do we do on failure? Should we try to deallocate    */
        /* the stuff we created, or will the system do this for us? */

    return kIOReturnSuccess;
}/* end allocHdwAndChanMem */


    /* Perform one-time-only channel command program initialization.    */

void meshSCSIController::initCP()
{
    register DBDMADescriptor        *dst = (DBDMADescriptor*)fCCL;
    register const DBDMADescriptor  *src = gDescriptorList;
    UInt32                          i;
	DBDMAChannelRegisters			*DBDMARegs = (DBDMAChannelRegisters*)dbdmaAddr;


	ELG( src, dst, 'I CP', "initCP - Init the Channel Program" );

        /* Set the interrupt, branch, and wait DBDMA registers.         */
        /* Caution: the following MESH interrupt register bits are      */
        /* EndianSwapped, reverse polarity and in a different position. */
        /* The pattern is: 0xvv00mm00, where mm is a mask byte          */
        /* and vv is a value byte to match. (After EndianSwapping).     */
        /*  0x80    means NO errors         (kMeshIntrError)            */
        /*  0x40    means NO exceptions     (kMeshIntrException)        */
        /*  0x20    means NO command done   (kMeshIntrCmdDone)          */
        /*  Branch Select is used with BRANCH_FALSE                     */

//  DBDMASetInterruptSelect( 0x00000000 );  /* Never let DBDMA interrupt    */
//  DBDMASetWaitSelect( 0x00200020 );       /* Wait until command done      */
//  DBDMASetBranchSelect( 0x00C000C0 );     /* Branch if exception or error */

	DBDMARegs->interruptSelect  = 0x00000000;   /* Never let DBDMA interrupt    */
	DBDMARegs->waitSelect       = 0x20002000;   /* Wait until command done      */
	DBDMARegs->branchSelect     = 0xC000C000;   /* Br if Exc or Err             */
    SynchronizeIO();

        /* Relocate and EndianSwap the global channel command list   */
        /* into the page that is shared with the DBDMA device.       */

    for ( i = 0; i < gDescriptorListSize; i += sizeof( DBDMADescriptor )  )
    {
        dst->operation  = SWAP( src->operation );    /* copy command with count  */

        switch ( src->result & kRelAddress )
        {
        case kRelAddressMESH:
            dst->address = SWAP( src->address + fMESHPhysAddr );
            break;
        case kRelAddressCP:
            dst->address = SWAP( src->address + (UInt32)fCCLPhysAddr );
            break;
        case kRelAddressPhys:
            dst->address = SWAP( src->address );
            break;
        default:
            dst->address = SWAP( src->address );
            break;
        }

        switch ( src->result & kRelCmdDep )
        {
        case kRelCmdDepCP:
            dst->cmdDep = SWAP( src->cmdDep + (UInt32)fCCLPhysAddr );
            break;
        case kRelCmdDepLabel:
            dst->cmdDep = src->cmdDep;
            break;
        default:
            dst->cmdDep = SWAP( src->cmdDep );
            break;
        }

        dst->result = 0;
        src++;
        dst++;
    } /* FOR all elements in the descriptor list */
    return;
}/* end initCP */


    /* doHBASelfTest - MESH chip self-test. (Minimal: it could be extended.)	*/

IOReturn meshSCSIController::doHBASelfTest()
{
    IOReturn    ioReturn = kIOReturnSuccess;
    UInt8       tempByte;


    ELG( fMESHPhysAddr, fMESHAddr, 'Test', "doHBASelfTest" );

    if ( ioReturn == kIOReturnSuccess )
    {
        tempByte = fMESHAddr->MESHID & 0x1F;
        if ( tempByte < kMeshMESHID_Value )
        {
            PAUSE( 0, tempByte, 'hba-', "doHBASelfTest - Invalid MESH chip ID .\n" );
            ioReturn = kIOReturnNoDevice;
        }
    }
    return  ioReturn;
}/* end doHBASelfTest */


void meshSCSIController::interruptOccurred( IOInterruptEventSource *ies, int intCount )
{
//	DBDMAChannelRegisters	*DBDMARegs = (DBDMAChannelRegisters*)dbdmaAddr;


//	ELG( ies, intCount, 'Int+', "interruptOccurred" );
	g.intLevel |=  kLevelISR;                               /* set ISR flag     */
	g.intLevel &= ~kLevelLatched;                           /* clear latched    */
//	ELG( DBDMARegs->channelStatus, DBDMARegs->commandPtrLo, 'Int+', "interruptOccurred." );
	ELG( fCmd, *(UInt32*)&fCCL[ kcclStageLabel ], 'Int+', "interruptOccurred." );
//	ELG( *(UInt32*)0xF3000024, *(UInt32*)0xF300002C, 'Int ', "interruptOccurred." );

	doHardwareInterrupt();						/**** HANDLE THE INTERRUPT  ****/

//	ELG( fCmd, *(UInt32*)0xF300002C, 'Intx', "interruptOccurred." );

    g.intLevel &= ~kLevelISR;                  /* clear ISR flag    */
    return;
}/* end interruptOccurred */


	/* doHardwareInterrupt - called from Workloop in superclass	*/

void meshSCSIController::doHardwareInterrupt()
{
	DBDMAChannelRegisters	*DBDMARegs = (DBDMAChannelRegisters*)dbdmaAddr;


	getHBARegsAndClear( true );    			/* get the MESH registers   */
	setIntMask( 0 );						/* Disable MESH interrupts  */

    fFlagReselecting = false;

    if ( g.shadow.mesh.interrupt == 0 )
    {       /* Interrupts can occur with no bits set in the         */
            /* interrupt register one way:                          */
            /*  -   Eating interrupts in the driver (the ASIC       */
            /*          latches the interrupt even though the       */
            /*          driver or Channel Program clears the MESH   */
            /*          interrupt register).                        */
		PAUSE(  DBDMARegs->commandPtrLo,
				(g.shadow.mesh.busStatus0 << 8) | g.shadow.mesh.busStatus1,
				'ISR?',
				"doHardwareInterrupt - spurious interrupt" );

        if ( !fCmd )
        {		/* if no  request:	*/
			setIntMask( kMeshIntrMask );	/* Enable interrupts				    */
			enableCommands();				/* let superclass issue another command	*/
        }
        return;
    }/* end IF no bit set in interrupt register */

///	dbdma_flush( DBDMA_MESH_SCSI );         /* DBDMA may be hung in */
///	dbdma_stop(  DBDMA_MESH_SCSI );         /* middle of transfer.  */
	DBDMARegs->channelControl = SWAP( 0x20002000 );		// set FLUSH bit
	SynchronizeIO();
	DBDMARegs->channelControl = SWAP( 0x80000000 );		// clr RUN   bit
	SynchronizeIO();

//  invalidate_cache_v( (vm_offset_t)fCCL, fCCLSize );

        /* If the DBDMA was running a channel command, handle this  */
        /* (this could be done at a lower priority level).          */

    if ( *(UInt32*)&fCCL[ kcclStageLabel ] )
    {
		processInterrupt();
        return;
    }

        /* This was not a DBDMA completion.         */
        /* See if the last MESH operation completed */
        /* without errors or exceptions.            */

    if ( g.shadow.mesh.interrupt == kMeshIntrCmdDone )
    {
            /* This was presumably a Programmed IO completion.  */

        if ( fCmd )
        {       /* The command has not completed yet.                   */
                /* We need to wait for a phase stabilizing interrupt.   */

            PAUSE( 0, 0, 'dhi-', "doHardwareInterrupt - MESH interrupt problem: need phase stabilizing wait.\n" );
            return;
        }
        else
        {       /* There is no active command.                  */
                /* This is presumably a bus-free completion.    */
			setIntMask( kMeshIntrMask );	/* Re-enable interrupts					*/
			enableCommands();				/* let superclass issue another command	*/
            return;
        }
    }/* end IF CmdDone without Err or Exc */

        /* None of the above "completion" states occurred.      */
        /* Either a command completed unsuccessfully, or we     */
        /* were reselected. First, check for phase mismatch.    */
    if ( g.shadow.mesh.interrupt == (kMeshIntrCmdDone | kMeshIntrException)
     &&  g.shadow.mesh.exception == kMeshExcPhaseMM )
    {
            PAUSE( 0, 0, 'DHI-', "doHardwareInterrupt - MESH interrupt problem: phase mismatch interrupt.\n" );
    }
    else
    {       /* Handle reselection and all other problems separately.    */
        processInterrupt();
    }
    return;
}/* end doHardwareInterrupt */


    /* Respond to a DBDMA channel command completion interrupt  */
    /* or some error or exception condition.                    */

void meshSCSIController::processInterrupt()
{
	UInt32			stage;          /* Stage in the Channel Program */
	UInt32			cclIndex;       /* Index of CCL descriptor      */
	UInt32			count;          /* transfer count               */
	UInt8			phase;          /* Current bus phase            */
	IOReturn		rc;


		/* Get the state of the DBDMA:	*/

    stage       = *(UInt32*)&fCCL[ kcclStageLabel ];
	cclIndex    = SWAP( ((DBDMAChannelRegisters*)dbdmaAddr)->commandPtrLo ) - (UInt32)fCCLPhysAddr;
    *(UInt32*)&fCCL[ kcclStageLabel ] = 0;

		/* Check for SCSI Bus Reset Detected:	*/

	if ( g.shadow.mesh.error & kMeshErrSCSIRst )
	{
		ELG( fCmd, stage, 'BRst', "Process interrupt with no active request\n" );
		fCmd = NULL;
		super::resetOccurred();
		setIntMask( kMeshIntrMask );
		enableCommands();				/* let superclass issue another command	*/
    	return;
	}

    if ( fCmd == NULL )
    {
        if ( g.shadow.mesh.exception & kMeshExcResel )
        {
			handleReselectionInterrupt();
    	    return;
        }
			/* There is no active request and we are not reselecting.   */
			/* Can get here if Reject/Abort occurs or after a BusFree   */
			/* command is put in the Sequence register and we exit the  */
			/* interrupt.                                               */
		ELG( 0, 0, 'Int0', "Process interrupt with no active request\n" );
		setIntMask( kMeshIntrException | kMeshIntrError );

		enableCommands();			/* let superclass issue another command	*/
    	return;
    }/* end IF had no active command */

        /* There is an active request - switch on stage of the DBDMA:		*/

    switch ( stage )
    {
    case kcclStageGood:                         /* Normal completion        */
        doInterruptStageGood();
        break;

    case kcclStageInit:                         /* Value before DBDMA runs  */
    case kcclStageArb:                          /* Arbitration anomaly      */
        doInterruptStageArb();
        break;

    case kcclStageSelA:                         /* Selection anomaly       */
        doInterruptStageSelA();
        break;

    case kcclStageMsgO:                         /* Message Out              */
        doInterruptStageMsgO();
        break;

    case kcclStageCmdO:                         /* Command stage anomaly   */
        doInterruptStageCmdO();
        break;

    case kcclStageXfer:
		doInterruptStageXfer();					/* DMA transfer complete    */
        break;

    case kcclStageStat:             /* Synchronous, odd transfer, data-out  */
                                    /* OR no data, disconnect               */

            /* Don't use updateCurrentIndex here because */
            /* kcclStageStat destroys TC with a 1.       */
        count  = *(UInt32*)&fCCL[ kcclBatchSize ];		/* Our transfer count   */
		fCmdData->results.bytesTransferred += count;	/* Increment data index */

		if ( fReadAlignmentCount )	// Hack for Radar 1670626
		{
			fCmdData->mdp->writeBytes( fReadAlignmentIndex, &fCCL[ kcclReadBuf8 ], fReadAlignmentCount );
			fReadAlignmentCount = 0;
		}

        ELG( count, fCmdData->results.bytesTransferred, 'Uidx', "processInterrupt" );
        *(UInt32*)&fCCL[ kcclBatchSize ] = 0;              /* Clear our count       */

            /* Analyze the current bus signals: */

        if ( !(g.shadow.mesh.busStatus0 & kMeshReq) )
        {       /* Get here if Sync Read or Write is too short as   */
                /* in reading 512 bytes from a 2K block of CD-ROM.  */
			startBucket();
            return;
        }/* end IF no REQ signal */

        phase = g.shadow.mesh.busStatus0 & kMeshPhaseMask;
        switch ( phase )
        {
        case kBusPhaseMSGI:
			rc = DoMessageInPhase();
			break;

		case kBusPhaseDATO:
		case kBusPhaseDATI:
                /* Get here if Async Read or Write is too short as  */
                /* in reading 512 bytes from a 2K block of CD-ROM   */
            startBucket();
            break;

        default:
			PAUSE( 0, phase, 'pmm-', "processInterrupt - expected Status phase.\n" );
            break;
        }/* end SWITCH on phase */
        break;

    case kcclStageBucket:
        count  = *(UInt32*)&fCCL[ kcclBatchSize ];			/* Our transfer count   */
		fCmdData->results.bytesTransferred += count;		/* Increment data index */
        *(UInt32*)&fCCL[ kcclBatchSize ] = 0;				/* Clear our count       */

        ELG( count, fCmdData->results.bytesTransferred, 'Buck',
									"processInterrupt - bit bucket done.\n" );

			/* MESH doesn't always have a phase mismatch when completing a		*/
			/* synchronous data write. It may be that the drive is going to		*/
			/* MsgIn with SDP and/or disconnect instead of Status and the		*/
			/* timing is different. If the current phase is not a data phase,	*/
			/* AND the TC and FIFO count indicate no data overflowed AND		*/
			/* and all the data transferred, don't set an error condition.		*/

        phase = g.shadow.mesh.busStatus0 & kMeshPhaseMask;

		count  = g.shadow.mesh.transferCount1 << 8 | g.shadow.mesh.transferCount0;
		count += g.shadow.mesh.FIFOCount;
		count &= 0xFFFF;

		if ( (count != 0)
		  || (fCmdData->xferCount != fCmdData->results.bytesTransferred)
		  || !(phase > kBusPhaseDATI) )
		{
			fCmdData->results.adapterStatus = kSCSIAdapterStatusOverrun;
		}
		setSeqReg( kMeshFlushFIFO );					/* flush the FIFO        */
		runDBDMA(  kcclGetStatus, kcclStageStat );
        break;

	case kcclStageMsgI:	/* DBDMA stopped in MsgIn stage after Status stage:		*/
			/* Radar 2253653 - A SCSI scanner is taking a long time to drop Req	*/
			/* after MESH asserts Ack to the Status byte. Because the Status	*/
			/* command got cmdDone and MESH doesn't wait for the next phase,	*/
			/* the channel program proceeds to the next phase and issues a		*/
			/* DMA,MsgIn to the Sequence register which drops Ack while Req		*/
			/* is still asserted. MESH shouldn't do that. MESH then sees a		*/
			/* Phase Mismatch on Status phase while expecting MsgIn phase thus	*/
			/* causing an Exception interrupt.									*/
		ELG( 0, fMESHAddr->busStatus0 << 8 | fMESHAddr->busStatus1, '?mi-',
                "processInterrupt - Phase Mismatch on MsgIn stage.\n" );
		fFlagIncompleteDBDMA = false;				/* indicate no-more-data	*/
		if ( !(fMESHAddr->busStatus0 & kMeshReq) )
			IOSleep( 1 );
		getHBARegsAndClear( true );			/* get and log the MESH registers	*/
		phase = g.shadow.mesh.busStatus0 & (kMeshPhaseMask | kMeshReq);
		if ( phase == (kMeshReq | kBusPhaseMSGI) )
		{
			DoMessageInPhase();		/* hopefully get Command Complete message	*/
		}
        break;

    case kcclStageFree:			/* DBDMA stopped in BusFree stage:	*/
    default:                    /* Can't happen?   */
        PAUSE( cclIndex, stage, 'P i-', "processInterrupt - strange or unknown interrupt for device.\n" );
        break;
    }/* end SWITCH on Channel Program stage */

    return;
}/* end processInterrupt */


	/* Start a SCSI transaction for the specified command.		*/

UInt8 meshSCSIController::startCommand()
{
    fCmd->getPointers( &fCmdData->mdp, &fCmdData->xferCount, &fCmdData->isWrite );

	clearCPResults();		/* clear the result field in all the Channel Commands	*/

	{
		fCmdData->results.bytesTransferred	= 0;
		fCmdData->savedDataPosition			= 0;
		updateCP( false );   /* Update the DBDMA Channel Program    */
	}

		/***** Can a caller override the default timeout?   *****/

	fMESHAddr->selectionTimeOut	= fSelectionTimeout;
	fMESHAddr->destinationID	= fCurrentTargetLun.target;
//	fMESHAddr->syncParms		= kSyncParmsAsync;
	fMESHAddr->syncParms		= fSyncParms[ fCurrentTargetLun.target ];
	SynchronizeIO();
	runDBDMA( kcclStart, kcclStageInit );
	if ( g.intLevel & kLevelLatched )		/* return Busy if reselecting	*/
		return kHardwareStartBusy;			/* or some other issue.			*/
	return kHardwareStartOK;
}/* end startCommand */


    /* Initialize the data transfer channel command list for a normal SCSI  */
    /* command. The channel command list has a complex structure of         */
    /* transfer groups and items, where:                                    */
    /*  transfer group      The number of bytes transferred by a single     */
    /*                      MESH operation. This will be from 1 to          */
    /*                      kMaxDMATransfer (65536 - 4096).					*/
    /*  transfer item       The number of bytes transferred by a single     */
    /*                      DBDMA operation. These bytes are guaranteed     */
    /*                      to be physically-contiguous.                    */
    /* Thus, the data transfer CCL looks like the following:                */
    /*      Prolog 1:       Load MESH with the first group count.           */
    /*      Item 1.1:       Load DBDMA with the first physical address and  */
    /*                      item count.                                     */
    /*      Item 1.2 etc:   Load DBDMA with the next physical address and   */
    /*                      item count.                                     */
    /*      Prolog 2, etc.  Load MESH with the next group count.            */
    /*      Item 2.1, etc.  Load DBDMA with the next group of physical      */
    /*                      addresses.                                      */
    /*      Stop/Branch     If all of the data transfer commands fit in the */
    /*                      channel command list, branch to the Status phase*/
    /*                      channel command. Otherwise, stop transfer       */
    /*                      (which stops in Data phase) and rebuild the     */
    /*                      command list for the next set of data.          */
    /* Note that the last DBDMA command must be INPUT_LAST or OUTPUT_LAST   */
    /* to handle synchronous transfer odd-byte disconnect.                  */

void meshSCSIController::updateCP( bool reselecting )
{
	DBDMADescriptor     *descProto = (DBDMADescriptor*)&fCCL[ kcclPrototype ];
	DBDMADescriptor     *descriptorPtr;     /* current data descriptor          */
	DBDMADescriptor     *descriptorMax;     /* beyond the last data descriptor  */
	DBDMADescriptor     *preamblePtr;       /* current prolog descriptor        */
	SCSICDBInfo			scsiCDB;
	UInt32              dbdmaOpProto;       /* prototype Opcode for DBDMA       */
	UInt32              dbdmaOp;            /* Opcode for DBDMA                 */
	UInt32              meshSeq;            /* Opcode for MESH request          */
	SInt32              transferLength;     /* Number of bytes left to transfer */
	UInt32              totalXferLen   = 0; /* Total length of this transfer    */
	UInt32              groupLength;        /* Number of bytes in this group    */
	UInt8               syncParms;          /* Fast synchronous param value     */
	UInt32				actualRanges;
	IOPhysicalSegment	range[ kMaxMemCursorSegs ];
	DBDMADescriptor     *dp;
	UInt32				rangeLength, rangeLocation;
	UInt32				i;
	IOReturn            ioReturn   = kIOReturnSuccess;


	fReadAlignmentCount = 0;	/* Clear Read misalignment condition	*/

        /* How many descriptors can we store (need some slop for the    */
        /* terminator commands). Get a pointer to the first free        */
        /* descriptor and the total number of bytes left to transfer in */
        /* this IO request.                                             */

    descriptorPtr   = (DBDMADescriptor*)&fCCL[ kcclDataXfer ];
    descriptorMax   = &descriptorPtr[ fDBDMADescriptorMax - 16 ];
	transferLength  = fCmdData->xferCount - fCmdData->results.bytesTransferred;
    ELG( descriptorPtr, transferLength, 'UpCP', "updateCP" );

    if ( !reselecting )
    {
        setupMsgO();			/* Setup for Message Out phase. */
    	fCmd->getCDB( &scsiCDB );

                                /* Setup for Command phase:     */
		fCCL[ kcclCmdoMTC ]  = scsiCDB.cdbLength;    /* MESH transfer count  */
        fCCL[ kcclCmdoDTC ]  = scsiCDB.cdbLength;    /* DBDMA count          */
        bcopy( &scsiCDB.cdb[0], &fCCL[ kcclCMDOdata ], scsiCDB.cdbLength );
    }/* end IF not reselecting */

        /* Generate MESH "sequence" & DBDMA "operation" for Input or Output:    */

	if ( fCmdData->isWrite )
    {   dbdmaOpProto    = OUTPUT_MORE | kBranchIfFalse;
        meshSeq         = kMeshDataOutCmd | kMeshSeqDMA;
    }
 	else
	{   dbdmaOpProto    = INPUT_MORE | kBranchIfFalse;
		meshSeq         = kMeshDataInCmd | kMeshSeqDMA;
    }

    *(UInt32*)&fCCL[ kcclBatchSize ] = 0;

    while ( ioReturn == kIOReturnSuccess
            && transferLength > 0
            && descriptorPtr < descriptorMax )
    {
            /* Do one group, ie, enough CCs to fill a MESH transfer count.  */
            /* There are more data to be transferred, and CCL space to store*/
            /* another group of data. First, leave space for the preamble.  */

        preamblePtr      = descriptorPtr;
        groupLength      = 0;
        descriptorPtr   += 4;               /* Preamble takes 4 descriptors */

			/* Do one group of pages:   */

		actualRanges = fMemoryCursor->getPhysicalSegments(
										fCmdData->mdp,
										fCmdData->results.bytesTransferred + totalXferLen,
										range,
										kMaxMemCursorSegs );
		if ( actualRanges == 0 )
			break;

		ELG( range[0].length, range[0].location, 'Rng1', "updateCP - 1st range" );

		for ( i = 0; i < actualRanges; i++ )
		{	rangeLocation	= range[i].location;
			rangeLength		= range[i].length;
			groupLength	   += rangeLength;

				// 29apr99 mlj Radar 1670626 - the DBDMAs in Grand Central (and
				// either Heathrow or O'Hare) have a bug. On the initial
				// data xfer of a Read, if the buffer is not aligned on an
				// 8-byte boundary, and the transfer ends before the boundary is
				// reached, then the memory in front of the buffer is trashed.
				// If these conditions apply, we read the misaligned bytes
				// into the CCL at kcclReadBuf8 now and copy them to the
				// user buffer later when we get an interrupt. The 1st range
				// is split and 2 DBDMA Channel Commands are generated.

			if ( (i == 0)								// 1st range of group
			  && (totalXferLen == 0)					// 1st group of batch
			  && (!fCmdData->isWrite)					// READing
			  && (rangeLocation & 0x07) )				// not 8-byte aligned
			{
				ELG( rangeLength, rangeLocation, 'Rd-8', "updateCP - non-8-byte-aligned read" );
				fReadAlignmentIndex	= fCmdData->results.bytesTransferred;
				fReadAlignmentCount	= 8 - (rangeLocation & 0x07);
				dbdmaOp       		= dbdmaOpProto | fReadAlignmentCount;
				rangeLength		   -= fReadAlignmentCount;
				transferLength	   -= fReadAlignmentCount;
				if ( transferLength <= 0 )
					 dbdmaOp |= (INPUT_MORE ^ INPUT_LAST);    /* add LAST to cmd  */

				descriptorPtr->operation    = SWAP( dbdmaOp );
				descriptorPtr->address		= SWAP( (UInt32)fCCLPhysAddr + kcclReadBuf8 );
				descriptorPtr->cmdDep       = SWAP( (UInt32)fCCLPhysAddr + kcclProblem );
				descriptorPtr->result       = 0;    // for debugging

				descriptorPtr++;

				if ( rangeLength == 0 )
					continue;
				rangeLocation += fReadAlignmentCount;
			}/* end IF READing and buffer is misaligned at batch start */

			dbdmaOp          = dbdmaOpProto | rangeLength;
			transferLength  -= rangeLength;
			if ( transferLength <= 0 )
				 dbdmaOp |= (INPUT_MORE ^ INPUT_LAST);    /* add LAST to cmd  */

			descriptorPtr->operation    = SWAP( dbdmaOp );
			descriptorPtr->address      = SWAP( rangeLocation );
			descriptorPtr->cmdDep       = SWAP( (UInt32)fCCLPhysAddr + kcclProblem );
			descriptorPtr->result       = 0;    // for debugging

			descriptorPtr++;
		}/* end FOR each range in this group */

        if ( groupLength == 0 )
        {
                /* Nothing was built - we apparently failed to get              */
                /* a physical address. Note: there is a potential problem with  */
                /* the following sequence as the *previous* DBDMA command, if   */
                /* any, should be changed to set xxPUT_LAST.                    */

            ELG( 0, 0, 'Grp-', "updateCP - groupLength is 0" );
            preamblePtr->operation  = SWAP( NOP_CMD | kBranchIfFalse | kWaitIfTrue );
            preamblePtr->address    = 0;
            preamblePtr->cmdDep     = SWAP( (UInt32)fCCLPhysAddr + kcclProblem );
            preamblePtr->result     = 0;
            descriptorPtr           = preamblePtr + 1;
            ioReturn = kIOReturnInvalid;    /* Exit the outer loop      */
        }
        else
        {
            totalXferLen += groupLength;

                /* This group is complete. Fill in the preamble.        */
                /* The preamble consists of the following commands:     */
                /*  [0] Move <totalXferLen> to kcclBatchSize            */
                /*  [1] Store group length high-byte in MESH            */
                /*      transfer count 1 register                       */
                /*  [2] Store group length low-byte in MESH             */
                /*      transfer count 1 register                       */
                /*  [3] Store the input/output command in the MESH      */
                /*      sequence register.                              */
                /* If the command finishes prematurely (perhaps the     */
                /* device wants to disconnect), the interrupt service   */
                /* routine will use totalXferLen - the residual byte    */
                /* count to determine the number of bytes xferred.      */

            descProto[0].cmdDep = totalXferLen; // update batch size
            descProto[1].cmdDep = SWAP( groupLength >> 8 );
            descProto[2].cmdDep = SWAP( groupLength & 0xFF );
            descProto[3].cmdDep = SWAP( meshSeq );
            bcopy( descProto, preamblePtr, sizeof( DBDMADescriptor ) * 4 );
            ELG( preamblePtr, totalXferLen, '=Tot', "updateCP - set preamble" );

                /* If there is another group, wait for */
                /* cmdDone and clear it:               */
            if ( transferLength > 0 )
            {       /* Wait for CmdDone: */
                bcopy( &fCCL[ kcclBrProblem ], descriptorPtr, sizeof( DBDMADescriptor ) );
                ++descriptorPtr;
                    /* Clear CmdDone:    */
                    /* HACK - if we reached the end of the CCL page,       */
                    /* we don't want to clear cmdDone because we will lose */
                    /* an interrupt. So, this instruction may be deleted   */
                    /* down below. (Radar 2298440)                         */
                descriptorPtr->operation = SWAP( STORE_QUAD | KEY_SYSTEM | 1 );
                descriptorPtr->address   = SWAP( fMESHPhysAddr + kMeshInterrupt );
                descriptorPtr->cmdDep    = SWAP( kMeshIntrCmdDone );
                descriptorPtr->result    = 0;
                ++descriptorPtr;
            }/* end IF not last group */
        }/* end if/ELSE a group was built */
    }/* end outer WHILE */

        /* All of the data have been transferred (or we ran off the end	*/
        /* of the CCL). Update the transfer start index to reflect on	*/
        /* what we attempt to transfer in this DATA operation. If		*/
        /* we completed DATA phase, branch to the Status Phase CCL;     */
        /* if not, stop the channel command so we can reload the CCL    */
        /* with the next big chunk.                                     */
        /* When the transfer completes, the last prolog will have stored*/
        /* the total number of bytes transferred in a known location in */
        /* the CCL area.                                                */
        /* Now, append the data transfer postamble to handle            */
        /* synchronous odd-byte disconnect and jump to status phase     */
        /* (or just stop if there's more DMA)                           */


         /* Do some synchronous data transfer cleanup: */

	syncParms = fSyncParms[ fCurrentTargetLun.target ];
    fMESHAddr->syncParms = syncParms;
    SynchronizeIO();
    ELG( fMsgOutFlag, syncParms, 'SynP', "updateCP - sync parms" );

    if ( ((syncParms & 0xF0) || (fMsgOutFlag & kFlagMsgOut_SDTR))  // Sync?
     && (totalXferLen > 0)         // any data moving?
     && (transferLength == 0) )    // end of xfer?
    {
        fFlagIncompleteDBDMA = false;               /* indicate complete xfer  */

            /* MESH has a problem at the end of Synchronous transfers.          */
            /* If the target is fast enough, it can move from data phase to     */
            /* Status phase while MESH still has ACKed bytes in its FIFO and    */
            /* the DBDMA is still running. MESH raises PhaseMismatch Exception  */
            /* causing an interrupt in which we must empty the FIFO and move    */
            /* the bytes to the user's buffer by programmed IO.                 */
            /* If the target is not fast enough, we can save the interrupt and  */
            /* bypass the mess.                                                 */
            /* So, we do the following:                                         */
            /* 1)  Enable only MESH Err interrupts; disable Exc and CmdDone.    */
            /* 2)  Don't Wait; Branch if an interrupt may have already occurred.*/
            /* 3)  Wait for cmdDone at least for TC = FIFO count = 0 and        */
            /*     maybe including PhaseMismatch. Branch to SyncCleanup if PMM. */
            /* 4)  Assume an interphase condition as opposed to an              */
            /*     overrun condition and Branch Always to get Status.           */

            /* If the Channel Program gets this far, the OUTPUT_LAST        */
            /* has finished writing its data to the FIFO and MESH may still */
            /* be putting bytes on the bus OR the INPUT_LAST has read all   */
            /* its data from the FIFO and MESH has already ACKed them.      */
            /* There may be or not some time before REQ appears again,      */
            /* either for data overrun or the next phase.                   */

            /* Disable Exc and CmdDone (leave Err enabled): */

        descriptorPtr->operation    = SWAP( STORE_QUAD | KEY_SYSTEM | 1 );
        descriptorPtr->address      = SWAP( fMESHPhysAddr + kMeshInterruptMask );
        descriptorPtr->cmdDep       = SWAP( kMeshIntrError );
        descriptorPtr->result       = 0;
        ++descriptorPtr;

            /* Take the interrupt if PhaseMismatch not definitely caught.        */
            /* Branch (don't wait for cmdDone) if Exc may have already occurred: */

        descriptorPtr->operation    = SWAP( NOP_CMD | kBranchIfFalse );
        descriptorPtr->address      = 0;
        descriptorPtr->cmdDep       = SWAP( (UInt32)fCCLPhysAddr + kcclProblem );
        descriptorPtr->result       = 0;
            /* Radar 2281306 ( and 2272931 ):                                 */
            /* Output may completely fit in the FIFO and not make it out      */
            /* to the SCSI bus if the target disconnects after the command.   */
            /* If that's possible, wait here for cmdDone and                  */
            /* take the PhaseMismatch interrupt. This situation occurred on a */
            /* Mode Select with an output of 12 bytes. Do this to prevent     */
            /* the Stage from advancing from kcclStageXfer so that proper     */
            /* cleanup can take place.                                        */
		if ( (totalXferLen <= 16) && fCmdData->isWrite )
		{
			descriptorPtr->operation = SWAP( NOP_CMD | kWaitIfTrue | kBranchIfFalse );
		}
        ++descriptorPtr;

            /* Possible PhaseMisMatch caught after FIFO emptied. */
            /* Wait for cmdDone. If Exc, branch to SyncCleanUp:  */

        descriptorPtr->operation    = SWAP( NOP_CMD | kWaitIfTrue | kBranchIfFalse );
        descriptorPtr->address      = 0;
        descriptorPtr->cmdDep       = SWAP( (UInt32)fCCLPhysAddr + kcclSyncCleanUp );
        descriptorPtr->result       = 0;
        descriptorPtr++;

            /* Interphase condition or possible overrun.  */
            /* 29sep98 PhaseMismatch occurred even after  */
            /* CmdDone was set.                           */


            /* Branch Always to assume we will bit bucket some data: */

        descriptorPtr->operation    = SWAP( NOP_CMD | kBranchAlways );
        descriptorPtr->address      = 0;
        descriptorPtr->cmdDep       = SWAP( (UInt32)fCCLPhysAddr + kcclOverrun );
        descriptorPtr->result       = 0;
        descriptorPtr++;

            /* Fix up the DataOverrun code just in case: */

        dp = (DBDMADescriptor*)&fCCL[ kcclOverrunMESH ];
		if ( fCmdData->isWrite )
        {   dp->cmdDep = SWAP( kMeshDataOutCmd | kMeshSeqDMA );
            dp = (DBDMADescriptor*)&fCCL[ kcclOverrunDBDMA ];
            dp->operation = SWAP( OUTPUT_LAST | kBranchIfFalse | 8 );
        }
		else
		{   dp->cmdDep = SWAP( kMeshDataInCmd | kMeshSeqDMA );
            dp = (DBDMADescriptor*)&fCCL[ kcclOverrunDBDMA ];
            dp->operation = SWAP( INPUT_LAST | kBranchIfFalse | 8 );
        }
    }/* end IF last of Synchronous transfer */
    else
    {
            /* Async or incomplete Sync. Append Branches to finish this process: */

            /* If this is a partial transfer, set 'incomplete' flag.  */

        if ( transferLength > 0 )
             fFlagIncompleteDBDMA = true;    /* set incomplete        */
        else fFlagIncompleteDBDMA = false;   /* assume complete xfer  */


        if ( fFlagIncompleteDBDMA )
        {                        /* Delete the ccl to clear cmdDone:  */
             --descriptorPtr;    /* see HACK note above.              */
        }
        else if ( totalXferLen > 0 )
        {       /* If something moved AND (Radar 2298440) xfer completed,  */
                /*  Wait & Branch if problem:                              */
                /* Radar 2272931 - If entire output fits in FIFO, then     */
                /* the OUTPUT_LAST completes OK without a PhaseMismatch if */
                /* the target disconnects right after the command phase.   */
            bcopy( &fCCL[ kcclBrProblem ], descriptorPtr, sizeof( DBDMADescriptor ) );
            descriptorPtr++;
        }
            /* Assume all's well - Branch to get status: */
        descriptorPtr->operation    = SWAP( NOP_CMD | kBranchAlways );
        descriptorPtr->address      = 0;
        descriptorPtr->cmdDep       = SWAP( (UInt32)fCCLPhysAddr + kcclGetStatus );
        descriptorPtr->result       = 0;

            /* If this is a partial transfer, set 'incomplete' flag and */
            /* change the Branch from GetStatus to Good:                */

        if ( fFlagIncompleteDBDMA )
        {       /* change last Branch from Status to Good: */
            descriptorPtr->cmdDep = SWAP( (UInt32)fCCLPhysAddr + kcclMESHintr );
            ELG( descriptorPtr, transferLength, 'Part', "updateCP - built partial CCL." );
        }
        descriptorPtr++;
    }/* end if/ELSE Async or partial xfer */
    return;
}/* end updateCP */


void meshSCSIController::clearCPResults()
{
    register DBDMADescriptor    *dp = (DBDMADescriptor*)&fCCL[ kcclStart ];
    register int                i;


        /*  Don't clear the reserved areas or prototypes    */

    for ( i = (gDescriptorListSize - kcclStart) / sizeof ( DBDMADescriptor ); i; --i )
    {
        dp->result = 0;
        dp++;
    }

    return;
}/* end clearCPResults */


    /* Set up the channel commands for MsgO phase.  */

void meshSCSIController::setupMsgO()
{
    UInt8       msgoSize;


    fMsgOutPtr--;       /* treat the last or only byte special (drop ATN)   */
    msgoSize = fMsgOutPtr - &fCCL[ kcclMSGOdata ];
    if( msgoSize == 0 )
    {       /* Identify byte only:  */
        *(UInt32*)&fCCL[ kcclMsgoBranch ] = SWAP( NOP_CMD | kBranchAlways );
    }
    else    /* multibyte message - set counts for all but last byte:    */
    {   fCCL[ kcclMsgoMTC ]  = msgoSize;
        fCCL[ kcclMsgoDTC ]  = msgoSize;
            /* NOP the BRANCH:  */
        *(UInt32*)&fCCL[ kcclMsgoBranch ] = SWAP( NOP_CMD );
    }
    fCCL[ kcclMSGOLast ] = *fMsgOutPtr;           /* position last byte   */
    return;
}/* end setupMsgO */


    /* Start a Channel Program at the given offset  */
    /* with the specified stage label.              */

void meshSCSIController::runDBDMA( UInt32 offset, UInt32 stageLabel )
{
    register UInt8			intReg;
	mach_timespec_t			arbEndTime, curTime;
	DBDMAChannelRegisters	*DBDMARegs = (DBDMAChannelRegisters*)dbdmaAddr;


    fMsgInFlag = 0;                                 /* clear message-in flags.  */

    *(UInt32*)&fCCL[ kcclStageLabel ] = stageLabel;	/* set the stage            */

        /* Let MESH interrupt only for errors or exceptions, but not cmdDone    */
    setIntMask( kMeshIntrException | kMeshIntrError );

    intReg = fMESHAddr->interrupt;
    switch ( intReg )
    {
    case kMeshIntrCmdDone:
        if ( !fFlagReselecting )    // ??? Don't drop ACK fm MSG-IN or Sync data flows
                /* clear any pending command interrupts (but not reselect et al)    */
            fMESHAddr->interrupt = kMeshIntrCmdDone;     SynchronizeIO();
        /***** fall through *****/
    case 0:
            /* This is a Go:                                            */
            /* Flush any CCL and related data to the CCL physical page  */
            /* that may still be sitting in cache:                      */
	flush_dcache( (vm_offset_t)fCCL, fCCLSize, false );

//	ELG( *(UInt32*)0xF3000020, *(UInt32*)0xF300002C, 'G C+', "runDBDMA." );

        if ( offset == kcclStart )
        {
            fFlagReselecting = false;
         	setSeqReg( kMeshArbitrateCmd );     /* ARBITRATE                */

                /* wait 50 mikes or cmdDone, whichever comes first: */

			IOGetTime( &arbEndTime );
			arbEndTime.tv_nsec += 50000;
			if ( arbEndTime.tv_nsec >= NSEC_PER_SEC )
			{	 arbEndTime.tv_nsec -= NSEC_PER_SEC;
				 arbEndTime.tv_sec  += 1;
			}

            while ( true )
            {
				getHBARegsAndClear( false );						/* get regs without hosing  */
				IOGetTime( &curTime );
				if ( g.shadow.mesh.interrupt & kMeshIntrCmdDone )
					break;
				if ( curTime.tv_sec  < arbEndTime.tv_sec   )	continue;
				if ( curTime.tv_nsec >= arbEndTime.tv_nsec )	break;
			}/* end wait cmdDone or 50 mikes */

            if ( g.shadow.mesh.interrupt == kMeshIntrCmdDone )
            {       /* No err, no exc: Arbitration won: */
                fMESHAddr->interrupt = kMeshIntrCmdDone;
                SynchronizeIO();
                setSeqReg( kMeshDisableReselect );						/* disable reselect		*/
                offset = 0x150;						// ??? fix this. Point to Select/Atn
                *(UInt32*)&fCCL[ kcclStageLabel ] = kcclStageSelA;		/* set stage to Select	*/
            }/* end IF won Arbitration */
            else    /* Arbitration not won - CAUTION - HACK AHEAD.              */
            {       /* Sometimes, MESH does not return ArbLost as it says in    */
                    /* the documentation. Instead, it waits for the winner to   */
                    /* get off the bus (usually after the 250 ms timeout) and   */
                    /* then MESH continues its arbitration. This wastes 250 ms  */
                    /* of valuable bus time. Further, IOmega's Zip drive has a  */
                    /* nasty bug whereby if its reselection is snubbed and it   */
                    /* times out, it leaves the I/O signal asserted on the bus  */
                    /* even as other activity on the bus unrelated to the Zip   */
                    /* is ongoing.                                              */
                    /* We don't need to hack if ArbLost is indicated correctly  */
                    /* or Reselect is indicated. If either is true, don't bother*/
                    /* starting the DBDMA; rather, let the interrupt already    */
                    /* latched handle the situation.                            */

                if ( !(g.shadow.mesh.exception & (kMeshExcArbLost | kMeshExcResel)) )
                {
                    ELG( '****', '****', 'HACK', "runDBDMA - Arbitrate HACK." );
						// ??? check FIFO count for presence of Target ID ???
                    setSeqReg( kMeshResetMESH );        /* hack it: whack it	*/
					getHBARegsAndClear( true );			/* get regs/clear		*/
                    setSeqReg( kMeshEnableReselect );   /* Let reselect again	*/
					getHBARegsAndClear( false );		/* get regs/preserve	*/
                    if ( g.shadow.mesh.interrupt == 0 )
                        PAUSE( 0, 0, 'Arb*', "runDBDMA - Arbitrate/Reselect problem." );
                }
                if ( g.shadow.mesh.interrupt )      /* If Err or Exc set,           			*/
					break;							/* break SWITCH - Don't start the DBDMA.	*/
            }/* end ELSE lost Arbitration */
        }/* end IF DBDMA to start at Arbitrate */

        getHBARegsAndClear( false );				// ??? debug: see if ACK still set
        ELG( 0, offset<<16 | stageLabel, 'DMA+', "runDBDMA" );
	///	dbdma_start( DBDMA_MESH_SCSI, (dbdma_command_t*)((UInt32)fCCLPhysAddr + offset) );
		DBDMARegs->commandPtrLo = SWAP( (UInt32)fCCLPhysAddr + offset );
		SynchronizeIO();
		DBDMARegs->channelControl = SWAP( 0x80008000 );	// set RUN bit
		SynchronizeIO();
		return;
    }/* end SWITCH on interrupt register */


	ELG( 0, intReg, 'Pnd-', "runDBDMA - interrupt probably pending (reselect?)." );
	getHBARegsAndClear( false );						/* display regs without clearing		*/
	*(UInt32*)&fCCL[ kcclStageLabel ] = kcclStageIdle;	/* set stage to none					*/
	g.intLevel |= kLevelLatched;    					/* set latched-interrupt flag.  		*/
	setIntMask( kMeshIntrMask );						/* make sure MESH interrupts enabled	*/
    return;
}/* end runDBDMA */


void meshSCSIController::completeCommand()
{
    SCSINegotiationResults      negotiationResult, *negResult;
    UInt32                      transferPeriod;
    
	ELG( fCmdData->results.bytesTransferred, fCmdData->results.scsiStatus, ' IOC', "meshSCSIController::completeCommand" );

	switch ( fCmdData->results.scsiStatus )
	{
	case kSCSIStatusGood:
		break;

	case kSCSIStatusCheckCondition:

		ELG( 0, 0, 'Chek', "meshSCSIController::completeCommand - Check Condition" );
		break;

	case kSCSIStatusQueueFull:
	default:
		ELG( fCmd, fCmdData->results.scsiStatus, 'Sta?', "meshSCSIController::completeCommand - bad status" );
		break;
	}/* end SWITCH on SCSI status */

    negResult = 0;
    if ( fCmdData->negotiatingSDTR )
    {
        bzero( &negotiationResult, sizeof(SCSINegotiationResults) );
        
        negotiationResult.returnCode = kIOReturnIOError;
        if ( fCmdData->negotiatingSDTRComplete == true )
        {
            negotiationResult.returnCode = kIOReturnSuccess;
        
            negotiationResult.transferOffset = fSyncParms[ fCurrentTargetLun.target ] >> 4;
            if ( negotiationResult.transferOffset != 0 )
            {
                transferPeriod = fSyncParms[ fCurrentTargetLun.target ] & 0x0f;
                if ( transferPeriod == 0 )
                {
                    negotiationResult.transferPeriodpS = 100;
                }
                else
                {
                    negotiationResult.transferPeriodpS = 80 + 40 * transferPeriod;
                }
            }    
            negotiationResult.transferWidth = 1;
        }
        negResult = &negotiationResult;
    }  
      
    fCmd->setResults( &fCmdData->results, negResult );
    fCmd->complete();

	fCmd						= NULL;
	fCmdData					= NULL;
	fCurrentTargetLun.target	= kInvalidTarget;
	fCurrentTargetLun.lun		= kInvalidLUN;
	return;
}/* end completeCommand */


void meshSCSIController::free()
{
	DBDMAChannelRegisters	*DBDMARegs = (DBDMAChannelRegisters*)dbdmaAddr;


	if ( fMESHAddr )
	{
		setSeqReg( kMeshResetMESH );
	}

	if ( DBDMARegs )
	{
	//	dbdma_stop( DBDMA_MESH_SCSI );
		DBDMARegs->channelControl = SWAP( 0x20002000 );		// set FLUSH bit
		SynchronizeIO();
		DBDMARegs->channelControl = SWAP( 0x80000000 );		// clr RUN   bit
		SynchronizeIO();
	}

	if ( fInterruptEvent )
	{
		fInterruptEvent->disable();
		fInterruptEvent->release();
		fInterruptEvent = 0;
	}

    if ( fCCL )
    {
        IOFreeContiguous( (void*)fCCL, fCCLSize );
        fCCL = NULL;
    }

	if ( fMemoryCursor )
	{
		 fMemoryCursor->release();
		 fMemoryCursor = 0;
	}

	if ( g.evLogBuf )
	{
		IOFree( (void*)g.evLogBuf, kEvLogSize );
		g.evLogBuf = 0;
	}

	super::free();
	return;
}/* end free */


    /* startBucket -  Start the channel commands to run the bit bucket. */

void meshSCSIController::startBucket()
{
    DBDMADescriptor     *dp;      /* current data descriptor          */
    UInt32              dbdmaOp;  /* Opcode for DBDMA                 */
    UInt32              meshSeq;  /* Opcode for MESH request          */


    ELG( fCmd, fCmdData, 'Bkt-', "startBucket" );

        /* Generate MESH "sequence" & DBDMA "operation" for Input or Output:   */

	if ( fCmdData->isWrite )
    {   dbdmaOp = OUTPUT_MORE | kBranchIfFalse | 8;
        meshSeq = kMeshDataOutCmd | kMeshSeqDMA;
    }
	else
    {   dbdmaOp = INPUT_MORE | kBranchIfFalse | 8;
        meshSeq = kMeshDataInCmd | kMeshSeqDMA;
    }

    dp = (DBDMADescriptor*)&fCCL[ kcclOverrunMESH  ];	dp->cmdDep    = meshSeq;
    dp = (DBDMADescriptor*)&fCCL[ kcclOverrunDBDMA ];	dp->operation = dbdmaOp;

    runDBDMA( kcclDataXfer, kcclStageBucket );
    return;
}/* end startBucket */


	/* The Channel Program ran to completion without problems.	*/

void meshSCSIController::doInterruptStageGood()
{
	UInt32			totalXferLen;


		/* Retrieve the total number of bytes transferred   */
		/* in the last data phase and reset it:				*/
    totalXferLen = *(UInt32*)&fCCL[ kcclBatchSize ];
    *(UInt32*)&fCCL[ kcclBatchSize ] = 0;

    ELG( fCmd, totalXferLen, 'Good', "doInterruptStageGood" );

		/* We are completing a normal command.                  */
		/* Update the transfer count and current data pointer.  */

	fCmdData->results.bytesTransferred += totalXferLen;

	if ( fReadAlignmentCount )	// Hack for Radar 1670626
	{
		fCmdData->mdp->writeBytes( fReadAlignmentIndex, &fCCL[ kcclReadBuf8 ], fReadAlignmentCount );
		fReadAlignmentCount = 0;
	}

	if ( fFlagIncompleteDBDMA == false )
	{
			/* Yes, the IO is really complete:  */

		fCmdData->results.returnCode = kIOReturnSuccess;
		fCmdData->results.scsiStatus = fCCL[ kcclStatusData ];
		fCmd->setResults( &fCmdData->results );

		completeCommand();
	}
	else
	{       /* The CCL ended, but the caller expected more data.    */
			/* Restart the CCL.                                     */
			/* Don't regenerate arbitration or command stuff.       */

		updateCP( true );
		runDBDMA( kcclDataXfer, kcclStageXfer );
		return;
	}/* end ELSE need to continue Channel Program */

        /* Since IO completed (otherwise, we would have exited in the   */
        /* "return" above), check whether a reselection attempt         */
        /* is piggy-backed on top of the good DBDMA completion.         */

    if ( g.shadow.mesh.exception & kMeshExcResel )
        handleReselectionInterrupt();
    else           /* Nothing happening. Try to start another request.  */
    {
		setIntMask( kMeshIntrException | kMeshIntrError );	/* Re-enable ints for reselect  */
		enableCommands();									/* let superclass issue another cmd	*/
    }

    return;
}/* end doInterruptStageGood */


    /* Process a normal data phase interrupt. IO is not complete.   */
    /* There are several reasons why we might get here:             */
    /*  -- autosense completion (which could be a separate stage)   */
    /*  -- DMA completion with more DMA to do                       */
    /*  -- Bus phase mismatch (short transfer or disconnect, MsgIn) */
    /* Note that we know that we are not in autosense.              */

void meshSCSIController::doInterruptStageXfer()
{
    UInt32                  count;          /* DMA transfer count   */
    UInt8                   phase;          /* Current bus phase    */
    IOReturn                rc;
    int                     goAround;


    count = fCmdData->results.bytesTransferred;

	updateCurrentIndex();

    do
    {   goAround = false;       /* assume loop not repeated */
		setSeqReg( kMeshFlushFIFO );

            /* We've cleaned up the mess from the previous data transfer.   */
            /* Look at the current bus phase. The channel command waited    */
            /* for REQ to be set before interrupting the processor.         */

        phase = g.shadow.mesh.busStatus0 & kMeshPhaseMask;
        	/* REQ is set, right?   */

        switch ( phase )
        {
        case kBusPhaseSTS:
            fFlagIncompleteDBDMA = false;       /* indicate no-more-data    */
            runDBDMA(  kcclGetStatus, kcclStageStat );
            break;

        case kBusPhaseMSGI:
			rc = DoMessageInPhase();
			if ( rc == kIOReturnSuccess && fCmd )
				goAround = true;                /* msg ok & not disconnect  */
			break;

        case kBusPhaseDATO:
        case kBusPhaseDATI:
            if ( count != fCmdData->results.bytesTransferred )
            {       /* Data phase had already started:  */
                PAUSE( 0, phase, 'dat-', "doInterruptStageXfer - unexpected Data phase.\n" );
            }
            else
            {       /* try starting data phase again    */
                runDBDMA( kcclDataXfer, kcclStageXfer );
            }
            break;

        default:
			PAUSE( 0, phase, 'Phs-', "doInterruptStageXfer - bogus phase.\n" );
            break;
        }/* end SWITCH on phase */
    } while ( goAround );
    return;
}/* end doInterruptStageXfer */


    /* doInterruptStageArb - Process an anomaly during arbitration.                 */

void meshSCSIController::doInterruptStageArb()
{
    PAUSE( 0, 0, 'Arb-', "doInterruptStageArb - Lost arbitration.\n" );

	disableCommands();
	rescheduleCommand( fCmd );
	fCmd = 0;

    if ( g.shadow.mesh.exception & kMeshExcResel )
    {   if ( g.shadow.mesh.error & kMeshErrDisconnected )
        {
                /* 18sep98 - Sometimes MESH gets real confused when its        */
                /* arbitration loses to a target's reselect arbitration.       */
                /* The registers show Exc:ArbLost, Resel and Err:UnExpDisc.    */
                /* The FIFO count is 1 (should be SCSI ID bits) while the      */
                /* BusStatus0,1 registers show IO and Sel both of which are    */
                /* set by the reselecting Target.                              */
                /* The SCSI bus analyzer shows the following events occcurring */
                /* within a few microseconds of BSY being set by the target:   */
                /*      bus free for at least hundreds of microseconds         */
                /*      Target raises BSY along with its ID bit                */
                /*      Target raises SEL                                      */
                /*      Target raises IO to indicate reselection               */
                /*      Target adds MESH's ID bit                              */
                /*      Target drops BSY                                       */
                /*      MESH raises BSY to accept reselection                  */
                /* **** MESH drops BSY **** here is where MESH is confused     */
                /*      Target stays on bus for 250 milliseconds.              */
                /* To solve this problem, whack MESH with a RstMESH.           */

            ELG( ' Rst', 'MESH', 'UEP-', "doInterruptStageArb - Resel/Unexpected Disconnect.\n" );
            setSeqReg( kMeshResetMESH );				/* completes quickly */
			getHBARegsAndClear( true );					/* clear cmdDone     */
            setSeqReg( kMeshEnableReselect );
            setIntMask( kMeshIntrMask );				/* Enable Interrupts */
            return;                      /* now wait for another reselect interrupt */
        }
        handleReselectionInterrupt();
    }
    else
    {       /* 22sep97 - lost arbitration without reselection.      */
            /* Probably lost the reselect condition processing an   */
            /* error or something.                                  */
        ELG( 0, 0, 'ARB-', "doInterruptStageArb - Lost arbitration without reselect.\n" );
    }
    return;
}/* end doInterruptStageArb */


    /* Process an anomaly during target selection.  */

void meshSCSIController::doInterruptStageSelA()
{
    ELG( fCmd, 0, 'Sel-', "doInterruptStageSelA - Selection stage.\n" );
    if ( fCmd )
    {	fCmdData->results.adapterStatus = kSCSIAdapterStatusSelectionTimeout;
        completeCommand();
    }
	setSeqReg( kMeshEnableReselect );
	setSeqReg( kMeshBusFreeCmd );			/* clear ATN signal MESH left on	*/
	getHBARegsAndClear( false );			/* check MESH registers				*/

	if ( g.shadow.mesh.exception & kMeshExcResel )
	{
		handleReselectionInterrupt();
	}
	else
	{	setIntMask( kMeshIntrMask );			/* Enable Interrupts    */
			/* enableCommands on the Bus-Free interrupt.				*/
			/* If we do it here, we'll get a command, start the DBDMA,	*/
			/* and get the Bus-Free interrupt seeming to be spurious.	*/
	}
    return;
}/* end doInterruptStageSelA */


    /* Process an anomaly during Message-Out phase. */
    /* Target probably doing Message Reject (0x07). */

void meshSCSIController::doInterruptStageMsgO()
{
    UInt8       phase;
    IOReturn    rc;


    phase = g.shadow.mesh.busStatus0 & kMeshPhaseMask;      /* phase me */
    ELG( fCmd, phase, 'Mgo-', "doInterruptStageMsgO - error during msg-out phase.\n" );

    switch ( phase )					/* Probably negotiating Sync	*/
    {
	case kBusPhaseMSGI:
		rc = DoMessageInPhase();
		if ( rc != kIOReturnSuccess )
		{
			PAUSE( 0, rc, ' MI-',
						"doInterruptStageMsgO - MsgIn during MsgOut phase.\n" );
		//  ??? need to get to bus-free from here
		//  ??? need to blow off the IO
		}
		else
		{   ELG( 0, fMsgInFlag, 'rej?', "doInterruptStageMsgO - got MsgIn.\n" );
			if ( fMsgInFlag & kFlagMsgIn_Reject )
				abortActiveCommand();
		}
		break;

	default:
		PAUSE( fMsgInFlag, phase, 'mgo-',
					"doInterruptStageMsgO - unknown phase during MsgOut phase.\n" );
		break;
    }
    return;
}/* end doInterruptStageMsgO */


    /* doInterruptStageCmdO - Process an anomaly during command stage.  */

void meshSCSIController::doInterruptStageCmdO()
{
	SCSICDBInfo		scsiCDB;
    UInt8			phase;
    IOReturn		rc;


		/* See if this is part of the normal AbortTag/BusDeviceReset process:	*/

	fCmd->getCDB( &scsiCDB );
	if ( scsiCDB.cdbAbortMsg )
    {
		setSeqReg( kMeshFlushFIFO );						/* flush the FIFO	*/
		getHBARegsAndClear( true );							/* clear cmdDone	*/

        ELG( fCmd, fCmd->getOriginalCmd(), 'Abo-', "doInterruptStageCmdO - Aborting." );

		completeCommand();				/* complete the cmd with Abort msg		*/
		setIntMask( kMeshIntrMask );	/* Re-enable interrupts					*/
		enableCommands();				/* let superclass issue another command	*/
        return;
    }

        /* Not aborting - something bad happened: */

    phase = g.shadow.mesh.busStatus0 & kMeshPhaseMask;      /* phase me */
    ELG( fCmd, phase, 'CMD?', "doInterruptStageCmdO - anomaly during Cmd phase.\n" );

    if ( phase == kBusPhaseMSGI )
    {       /* We are probably negotiating SDTR or          */
            /* getting rejected on a nonzero LUN.           */
		rc = DoMessageInPhase();
        if ( rc != kIOReturnSuccess )
        {
            PAUSE( 0, rc, ' mi-',
                        "doInterruptStageCmdO - MsgIn during Cmd phase.\n" );
        }
        else
        {       /* Message processed - where do we go from here?    */

			if ( !fCmd )							/* if Rejected,	*/
				return;								/* return		*/

            phase = g.shadow.mesh.busStatus0 & kMeshPhaseMask;
            switch ( phase )
            {
            case kBusPhaseSTS:
				runDBDMA( kcclCmdoStage, kcclStageInit );
                break;

            case kBusPhaseMSGO:
                fMsgOutPtr = &fCCL[ kcclMSGOdata ];
                setupMsgO();
                runDBDMA( kcclMsgoStage, kcclStageInit );
                break;

            case kBusPhaseCMD:
                runDBDMA( kcclCmdoStage, kcclStageInit );
                break;
            }
        }
    }
    else if ( phase == kBusPhaseSTS )           /* Probably Check Condition    */
    {                                           /* Perhaps block # invalid     */
                                                /* or target dislikes Cmd after*/
        setSeqReg( kMeshFlushFIFO );            /* 6th byte of 10-byte Cmd.    */
        fFlagIncompleteDBDMA = false;           /* indicate no-more-data       */
		runDBDMA(  kcclGetStatus, kcclStageStat );
    }
    else
    {
        PAUSE( 0, phase, 'Phs?', "doInterruptStageCmdO - error during Command phase.\n" );
    }
    return;
}/* end doInterruptStageCmdO */


    /* We are in MSGI phase. Read the bytes. Return true if an entire       */
    /* message was read (we may still be in MSGI phase). Note that this     */
    /* is done by programmed IO, which will fail (logging the error) if     */
    /* the target sets MSGI but does not send us a message quickly enough.  */
    /* This method is called from the normal data transfer interrupt when   */
    /* the target enters message in phase, and from the reselection         */
    /* interrupt handler when we read a valid reselection target ID.        */
    /* Note that MESH interrupts are disabled on exit.                      */
enum	/// ??? header file
{
	kSCSIMsgOneByteMax	= 0x1Fu,
	kSCSIMsgTwoByteMin	= 0x20u,
	kSCSIMsgTwoByteMax	= 0x2Fu
};

IOReturn meshSCSIController::DoMessageInPhase()
{
    register UInt8      messageByte;
    UInt32              index = 0;
    IOReturn            ioReturn = kIOReturnSuccess;


        /* We do not necessarily have a valid command in this method.   */
        /* While we're processing Message-In bytes, we don't want any   */
        /* MESH hardware interrupts.                                    */

	setIntMask( 0 );							/* no MESH interrupt latching   */
	setSeqReg( kMeshFlushFIFO );				/* Flush the FIFO   */

    fMsgInCount = 0;
    fMsgInState = kMsgInInit;

    while ( fMsgInState != kMsgInReady			/* Disconnect makes fCmd	*/
         && ioReturn == kIOReturnSuccess )		/* go away					*/
    {
        fMESHAddr->transferCount1	= 0;
        fMESHAddr->transferCount0	= 1;                /* get single byte  */
		setSeqReg( kMeshMessageInCmd );					/* issue MsgIn      */

        ioReturn = waitForMesh( true );					/* wait for cmdDone */
        if ( ioReturn != kIOReturnSuccess )
        {
            PAUSE( *(UInt32*)&fCurrentTargetLun, ioReturn, 'Mgi-', "DoMessageInPhase - Target hung: message in timeout.\n" );
            break;           /* Bus reset here? */
        }

        if ( (g.shadow.mesh.exception  & kMeshExcPhaseMM)
         ||  (g.shadow.mesh.busStatus0 & kMeshPhaseMask) != kBusPhaseMSGI )
        {
            break;                  /* exit loop if no longer in Msg-In phase   */
        }

        if ( g.shadow.mesh.FIFOCount == 0 )
        {
            PAUSE( *(UInt16*)&fCurrentTargetLun, 0, 'mgi-', "DoMessageInPhase - no message byte.\n" );
            break;
        }

        fMsgInBuffer[ index++ ] = messageByte = fMESHAddr->xFIFO;	/***** get msg byte	*****/

        switch ( fMsgInState )
        {
        case kMsgInInit:				/* This is the first message byte.		*/
			if ( (messageByte == kSCSIMsgCmdComplete)
			  || (messageByte >= (UInt8)kSCSIMsgIdentify) )
			{		 /* This is 1-byte cmdComplete or Identify message.				*/ 
				fMsgInState = kMsgInReady;
			}
			else if ( messageByte == kSCSIMsgExtended )
            {       /* This is an extended message. The next byte has the count.	*/
                fMsgInState = kMsgInCounting;
            }
			else if ( messageByte <= kSCSIMsgOneByteMax )
			{		/* These are other 1-byte messages.	*/
				fMsgInState = kMsgInReady;
			}
            else if ( messageByte >= kSCSIMsgTwoByteMin
                  &&  messageByte <= kSCSIMsgTwoByteMax )
            {		/* This is a two-byte message.					*/
                    /* Set the count and read the next byte.		*/
                fMsgInState = kMsgInReading;	/* Need one more	*/
                fMsgInCount = 1;
            }
            else
            {       /* This is an unknown message. */
                fMsgInState = kMsgInReady;
            }
            break;

        case kMsgInCounting:        /* Count byte of multi-byte message:   */
            fMsgInCount = messageByte;
            fMsgInState = kMsgInReading;
            break;

        case kMsgInReading:                 /* Body of multi-byte message:  */
            if ( --fMsgInCount <= 0 )
                fMsgInState = kMsgInReady;
            break;

        default:
            PAUSE( 0, 0, 'Msg-', "DoMessageInPhase  - Bogus MSGI state!\n" );
            fMsgInState = kMsgInReady;
            break;
        }/* end SWITCH on MSGI state */

        if ( fMsgInState == kMsgInReady )
        {
			ProcessMSGI();
            fMsgInState = kMsgInInit;
            index = 0;
			
            if ( fMsgInBuffer[0] == kSCSIMsgDisconnect
			 ||  fMsgInBuffer[0] == kSCSIMsgCmdComplete )	/* Radar 2253653	*/
                ioReturn = kIOReturnIOError;     /* break out of WHILE loop  */

            if ( fMsgInFlag & kFlagMsgIn_Reject )
            {
                abortActiveCommand();
                break;
            }

            if ( fFlagReselecting )
                break;  /* Take Identify only - leave +ACK  */
        }/* end IF have a complete message-in to process */
    }/* end WHILE there are more message bytes */

        /***** If the target switches out of MSGI phase without *****/
        /***** sending a complete message, we should do some    *****/
        /***** sort of error recovery.                          *****/

    if ( fMsgInState != kMsgInInit )
    {
        PAUSE( *(UInt32*)&fCurrentTargetLun, fMsgInState, 'MGI-', "DoMessageInPhase - incomplete message.\n" );
        if ( ioReturn == kIOReturnSuccess )
             ioReturn  = kIOReturnIOError;       /* General IO error     */
    }

    return  ioReturn;
}/* end DoMessageInPhase */


    /* ProcessMSGI - DoMessageInPhase has read a complete message.  */
    /* Process it (this will probably change our internal state).   */

void meshSCSIController::ProcessMSGI()
{
        /* Note that, during reselection, we may not have           */
        /* a current target or LUN, nor possibly a valid command    */


	UInt8			sdtr;
	UInt8			period;


    ELG( fCmd, *(UInt32*)fMsgInBuffer, '<Msg', "ProcessMSGI" );

    switch ( fMsgInBuffer[0] )
    {
    case kSCSIMsgCmdComplete:
        if ( fCmd )
        {
                /* This command is complete. Clear interrupts and   */
                /* allow subsequent MESH interrupts. Then tell the  */
                /* MESH to wait for the target to release the bus.  */

			setSeqReg( kMeshEnableReselect );
			setIntMask( kMeshIntrMask );		/* Enable Ints		*/
			setSeqReg( kMeshBusFreeCmd );		/* cause Int		*/
			completeCommand();
			waitForMesh( false );			/* don't clear possible reselect	*/
			g.intLevel |= kLevelLatched;	/* set latched-interrupt flag		*/
			setIntMask( 0 );				/* prevent multiple MESH ints		*/
        }
        goto exit; /* Don't exit through the SWITCH end */

    case kSCSIMsgLinkedCmdComplete:
    case kSCSIMsgLinkedCmdCompleteFlag:
        PAUSE( *(UInt16*)&fCurrentTargetLun, 0, 'pmi-', "ProcessMSGI - linked command complete not supported.\n" );
		abortActiveCommand();
        break;

    case kSCSIMsgNop:
        break;

    case kSCSIMsgRestorePointers:
        if ( fCmd )
			fCmdData->results.bytesTransferred = fCmdData->savedDataPosition;
        break;

    case kSCSIMsgSaveDataPointers:
        if ( fCmd )
			fCmdData->savedDataPosition = fCmdData->results.bytesTransferred;
        break;

    case kSCSIMsgDisconnect:
            /* Move this request to the disconnect queue, enable reselection,		*/
            /* re-enable MESH interrupts, and wait (here) for bus free, but			*/
            /* don't eat the interrupt.												*/

        fMsgInFlag |= kFlagMsgIn_Disconnect;
		disconnect();								/* requeue active   */
		setSeqReg( kMeshEnableReselect );			/* enable reselect  */
		setIntMask( kMeshIntrMask );				/* Enable Ints      */
        setSeqReg( kMeshBusFreeCmd );				/* issue BusFree    */

            /* wait for Bus Free command to complete:    */

		waitForMesh( false );				/* don't clear possible reselect    */

            /* Interrupt for bus-free now latched. Prevent a double interrupt,  */
            /* 1 from bus-free + 1 from reselect from occurring.                */
            /* This fixes the following BADNESS:                                */
            /*      Issue bus-free for disconnect.                              */
            /*      Interrupt occurs in microseconds - even before exiting      */
            /*      "interruptOccurred" routine.                                */
            /*      Mach queues message to driverKit.                           */
            /*      Exit "interruptOccurred" routine.                           */
            /*      DriverKit dequeues and starts handling 1st Mach message.    */
            /*      Interrupt occurs for reselect while driverKit running.      */
            /*      Mach queues 2nd message to driverKit.                       */
            /*      DriverKit invokes MESH driver for 1st msg.                  */
            /*      MESH driver sees cmdDone fm bus-free AND reselect exception.*/
            /*      MESH driver handles reselect by setting up and running      */
            /*      DBDMA. MESH driver exits.                                   */
            /*      DriverKit invokes MESH driver with 2nd Mach message.        */
            /*      MESH driver handles this as a DBDMA completion and royally  */
            /*      messes up.                                                  */

        g.intLevel |= kLevelLatched;            /* set latched-interrupt flag   */
        setIntMask( 0 );						/* prevent multiple MESH ints   */
        break;

    case kSCSIMsgRejectMsg:
        ELG( *(UInt16*)&fCurrentTargetLun, fMsgOutFlag, 'Rej-', "ProcessMSGI - Reject." );
        fMsgInFlag |= kFlagMsgIn_Reject;
        break;

    case kSCSIMsgSimpleQueueTag:
		fTagType	= fMsgInBuffer[0];
		fTag		= fMsgInBuffer[1];
		ELG( 0, fTag, '=Tag', "Simple Queue Tag" );
		break;

    case kSCSIMsgExtended:

			/* Have multi-byte message, presumably Synchronous Negotiation.		*/

			/* Radar 2425000 - Some drives will do Target Initiated Negotiation	*/
			/* if not disabled by jumper. The negotiation can occur before		*/
			/* the Command phase or after. Both Wide and Sync can occur in the 	*/
			/* same transaction. I tried to be nice and respond with a 			*/
			/* corresponding negotiation but ended up issuing a Message-Reject.	*/
			/* The biggest problem occurs if the drive negotiates Synchronous	*/
			/* just before Data phase. As soon as the target sees the Ack from	*/
			/* the last byte of the message, it starts spewing Reqs for data	*/
			/* for which we are not yet ready.									*/

        switch ( fMsgInBuffer[ 2 ] )			/* switch on the msg code byte  */
        {

		case kSCSIMsgWideDataXferReq:			/* handle Wide negotiation:		*/
			ELG( *(UInt32*)&fMsgInBuffer[0], 0, 'TINW', "ProcessMSGI - TIN WDTR" );

			issueReject();
			break;

        case kSCSIMsgSyncXferReq:				/* handle sync negotiation:     */
												/* Get period in  nanoseconds	*/
			period = fMsgInBuffer[ 3 ] * 4;		/* SCSI uses 4ns granularity	*/

				/* determine target responding or initiating?	*/
			if ( !fCmdData->negotiatingSDTR )
			{	/* Target Initiating Negotiation - reject it:	*/
				ELG( *(UInt32*)&fMsgInBuffer[0], *(UInt32*)&fMsgInBuffer[4], 'TINS', "ProcessMSGI - TIN SDTR" );
				issueReject();
			}/* end IF target is initiating negotiation */
			else
			{
				if ( fMsgInBuffer[ 4 ] == 0 )   /* check offset                 */
				{
					sdtr = kSyncParmsAsync;     /* Offset == 0 implies async    */
				}
				else                            /* synchronous:                 */
				{
					if ( period == 100 )        /* special-case 100=FAST    */
					{
						sdtr = kSyncParmsFast & 0x0F;
					}
					else    /* Older CD-ROMs get here.                      */
					{       /* The MESH manual says:                        */
							/* period = 4 * clk + 2 * clk * P               */
							/* where:                                       */
							/*      period is the target nanoseconds        */
							/*      clk is the MESH clock rate which is     */
							/*          20 nanoseconds for a 50 MHz clock   */
							/*      P is the 1-nibble period code we stuff  */
							/*          in the syncParms register           */
							/* So:                                          */
							/*      period = 4 * 20 + 2 * 20 * P            */
							/*      period = 80 + 40 * P                    */
							/*      P = (period - 80) / 40                  */
							/* Since P must round up for safety:            */
							/*      P = ((period - 80) + 39) / 40           */
							/*      P = (period - 41) / 40                  */
							/* A value of P == 3 results in 5 MB/s          */
						sdtr = (UInt8)((period - 41) / 40);
						ELG( *(UInt32*)&fMsgInBuffer[0], fMsgInBuffer[4]<<24 | sdtr, 'SDTR', "ProcessMSGI - SDTR" );
					}
				}/* end ELSE have offset ergo Synchronous */

					/*  OR in the offset.   */
				sdtr |= (fMsgInBuffer[ 4 ] << 4);

				fMESHAddr->syncParms = sdtr;
				SynchronizeIO();
				fSyncParms[ fCurrentTargetLun.target ] = sdtr;				
				fCmdData->negotiatingSDTRComplete = true;
			}/* end ELSE Target is responding to negotiation */
            break;

        default:
            PAUSE( *(UInt16*)&fCurrentTargetLun, fMsgInBuffer[0], 'PMi-', "ProcessMSGI - unsupported extended message.\n" );
			abortActiveCommand();
            break;
        }/* end SWITCH on extended message code */
        break;

    default:	/* Better be Identify with LUN:	*/
        if ( fMsgInBuffer[0] >= kSCSIMsgIdentify )
        {
			fCurrentTargetLun.lun = fMsgInBuffer[0] & 0x07;
        }
        else
        {
            PAUSE( *(UInt16*)&fCurrentTargetLun, fMsgInBuffer[0], 'mi -', "ProcessMSGI - unsupported message: rejected.\n" );
			abortActiveCommand();
        }
    }/* end SWITCH on message selection */

exit:
    return;
}/* end ProcessMSGI */


void meshSCSIController::issueReject()
{
	ELG( 0, 0, 'RjM-', "issueReject" );
	fMESHAddr->busStatus0 = kMeshAtn;	/***** Raise Atn			*****/
	SynchronizeIO();
	IODelay( 30 );

	setSeqReg( kMeshBusFreeCmd );		/* clr ACK & provoke PMM		*/
	waitForMesh( true );				/* wait til phase change & PMM	*/

	fMESHAddr->busStatus0 = 0;			/***** Clear Atn			*****/
	SynchronizeIO();
	IOSleep( 1 );						/* yield time to other processes*/

	fMESHAddr->transferCount1	= 0;	/* TC hi						*/
	fMESHAddr->transferCount0	= 1;	/* TC lo						*/
	SynchronizeIO();
	setSeqReg( kMeshMessageOutCmd );

	fMESHAddr->xFIFO = kSCSIMsgRejectMsg;
	SynchronizeIO();

	waitForMesh( true );		/* wait for TC = FIFOcnt = 0 (cmdDone)	*/

	setSeqReg( kMeshBusFreeCmd );		/* clr ACK & provoke PMM		*/
	waitForMesh( true );				/* wait for phase change		*/
	return;
}/* end issueReject */


    /* Process a reselection interrupt. */

void meshSCSIController::handleReselectionInterrupt()
{
    IOReturn    ioReturn;


    fFlagReselecting = true;
	disableCommands();						/* tell superclass we're busy	*/

	fMESHAddr->interrupt = kMeshIntrMask;	/* clr Exc, Err regs to prevent SeqErr*/

        /* Sometimes MESH gives a bogus Disconnected error during Reselection.  */
        /* 31mar98 - Issuing an Abort message, causes "unexpected disconnect".  */
        /* When Err:UnexpDisc and Exc:Resel are simultaneously set, the         */
        /* busStatus0,1 registers may not be current.                           */
    if ( g.shadow.mesh.error & kMeshErrDisconnected )
    {
        setSeqReg( kMeshBusFreeCmd );
		waitForMesh( true );			// now maybe busStatus0,1 are live
        PAUSE( 0, 0, 'Dsc-',
                "handleReselectionInterrupt: Caught disconnected glitch\n" );
    }/* End IF bus disconnect error */

        /* Read the target ID (which should be our initiator ID OR'd with the       */
        /* Target and the Identify byte with the reselecting LUN. Store this        */
        /* in fCurrentTargetLun. Note that, during reselection, we will				*/
        /* have a NULL gCurrentCommand and a valid fCurrentTargetLun. 				*/
        /* If we get a valid reselection target, call the message in phase			*/
        /* directly to read the LUN byte.                                           */
        /* @return true if successful.                                              */

    if ( fMESHAddr->FIFOCount == 0 )
    {
        PAUSE( 0, 0, 'HRI-', "handleReselectionInterrupt - Empty FIFO in reselection.\n" );
        return;
    }
    else    /* get the Target ID bit from the bus out of the FIFO   */
    {       /* then, get the msg-in Identify byte for the LUN.      */
        if ( getReselectionTargetID() )
        {
            if ( DoMessageInPhase() != kIOReturnSuccess )   /* get Identify  */
            {
                PAUSE( 0, 0, 'Id -', "handleReselectionInterrupt - Expected Identify byte after reselection.\n" );
            }
        }
        else return;
    }

        /* Try to find an untagged command for this Target/LUN: */

	fTag = kInvalidTag;
    ioReturn = reselectNexus();

    if ( ioReturn != kIOReturnSuccess )
    {		/* No untagged command, try to get a Tag. Hope that	*/
			/* you're still in Message-In phase at this point.	*/
        if ( DoMessageInPhase() != kIOReturnSuccess )	/* get Tag msg  */
        {
            PAUSE( 0, 0, 'tag-', "handleReselectionInterrupt - Expected tag message.\n" );
			/// need to bus device reset since it seems confused.
        }
        ioReturn = reselectNexus();
    }

    if ( ioReturn == kIOReturnSuccess )
    {
		ELG(	fCmd,
				fTag<<16 | fCurrentTargetLun.lun<<8 | fCurrentTargetLun.target,
                'Resl',     "handleReselectionInterrupt" );
			/* If reselectNexus succeeded, fCmd is set to the command.			*/
			/* Clear out the channel command Results and build the channel		*/
            /* command to continue operation.									*/

        clearCPResults();
        updateCP( true );			/* Bypass arbitrate/select/command sequence	*/
        runDBDMA( kcclReselect, kcclStageInit );
    }
    else
    {       /* There is no associated command.              */
            /* Reject the reselection attempt.              */
        PAUSE( *(UInt16*)&fCurrentTargetLun, fTag, 'Rsl-',
            "handleReselectionInterrupt - No command for reselection attempt.\n" );
        abortActiveCommand();
    }
    return;
}/* end handleReselectionInterrupt */


    /* Validate the target's reselection byte (put on the bus before	*/
    /* reselecting us). Erase the initiator ID and convert the other	*/
    /* bit into an index. The algorithm should be faster than a			*/
    /* sequential search, but it probably doesn't matter much.			*/
    /* Return true if successful (fCurrentTarget is now valid).			*/

bool meshSCSIController::getReselectionTargetID()
{
    register UInt8      targetID    = 0;
    register UInt8      bitValue    = 0;        /* Suppress warning         */
    register UInt8      targetBits;
    bool				success     = false;


    targetBits  = fMESHAddr->xFIFO;				/***** Read the FIFO    *****/
    targetBits &= ~fInitiatorIDMask;            /* Remove our bit           */
    if ( targetBits )
    {                       /* Is there another bit?    */
        bitValue        = targetBits;
        if ( bitValue > 0x0F )
        {
            targetID    += 4;
            bitValue    >>= 4;
        }
        if ( bitValue > 0x03 )
        {
            targetID    += 2;
            bitValue    >>= 2;
        }
        if ( bitValue > 0x01 )
        {
            targetID    += 1;
        }
        targetBits      &= ~(1 << targetID);    	/* Remove the target mask   */
        if ( targetBits == 0 )
        {                                       	/* Was exactly one set?     */
            success = true;                     	/* Yes: success!            */
			fCurrentTargetLun.target = targetID;	/* Save the current target  */
        }
    }

    if ( !success )
        PAUSE( targetID, targetBits, 'rsl-', "getReselectionTargetID - Expected Identify byte after reselection.\n" );

    return  success;
}/* end getReselectionTargetID */



IOReturn meshSCSIController::resetBus()
{
    UInt8       defaultSelectionTimeout = 25;   // mlj ??? fix this value
	DBDMAChannelRegisters	*DBDMARegs = (DBDMAChannelRegisters*)dbdmaAddr;


	ELG( 0, 0, 'RstB', "resetBus" );

		/* Stop the DBDMA:	*/

	DBDMARegs->channelControl = SWAP( 0x20002000 );		// set FLUSH bit
	SynchronizeIO();
	DBDMARegs->channelControl = SWAP( 0x80000000 );		// clr RUN   bit
	SynchronizeIO();

        /* Reset interrupts, the MESH Hardware Bus Adapter, and the DMA engine. */

	setIntMask( 0 );

    setSeqReg( kMeshResetMESH );			/* completes quickly			*/
	getHBARegsAndClear( true );				/* clear cmdDone                */

///	dbdma_reset( DBDMA_MESH_SCSI );

        /* Init state variables:    */

	fFlagIncompleteDBDMA = false;

        /* Smash all active command state (just in case):   */

	fCmd						= NULL;
	fCmdData					= NULL;
    fCurrentTargetLun.target	= kInvalidTarget;
	fCurrentTargetLun.lun		= kInvalidLUN;
    fMsgInState					= kMsgInInit;
    fMsgOutPtr					= &fCCL[ kcclMSGOdata ];

	fMESHAddr->busStatus1 = kMeshRst;	/***** ASSERT RESET SIGNAL *****/
	SynchronizeIO();
	IODelay( 25 );						/* leave asserted for 25 mikes */
	fMESHAddr->busStatus1 = 0;			/***** CLEAR  RESET SIGNAL *****/
	SynchronizeIO();

		/* Delay for 250 msec after resetting the bus.          */
		/* This serves two purposes: it gives the MESH time to  */
		/* stabilize (about 10 msec is sufficient) and gives    */
		/* some devices time to re-initialize themselves.       */

	IOSleep( APPLE_SCSI_RESET_DELAY );      /* Give Targets time to clean up */
	setSeqReg( kMeshResetMESH );			/* clear Err condition           */
	getHBARegsAndClear( true );				/* check regs                    */

    fMESHAddr->selectionTimeOut = defaultSelectionTimeout;
    SynchronizeIO();

    enableCommands();				/* let superclass issue another command	*/

    return kIOReturnSuccess;
}/* end resetBus */


    /* Wait for an immediate (non-interrupting) command to complete.    */
    /* Note that it spins while waiting. It is timed to prevent a buggy */
    /* chip or target from hanging the system.                          */

IOReturn meshSCSIController::waitForMesh( bool clearInterrupts )
{
	mach_timespec_t		time, startTime, endTime, waitTime;
    IOReturn    		ioReturn = kIOReturnSuccess;


	IOGetTime( &time );
	startTime = endTime = time;
	waitTime.tv_sec		= 0;
	waitTime.tv_nsec	= 250000000;       // mlj - make it 250 milliseconds for SONY CD-ROM;
	ADD_MACH_TIMESPEC( &endTime, &waitTime );

    for ( g.shadow.mesh.interrupt = 0; g.shadow.mesh.interrupt == 0; )
    {
		getHBARegsAndClear( clearInterrupts );

		IOGetTime( &time );
        if ( time.tv_sec < endTime.tv_sec )
			continue;
		if ( time.tv_nsec > endTime.tv_nsec )
        {       /* It took too long! We're dead.    */
            PAUSE( 0, 0, 'WFM-', "waitForMesh - MESH chip does not respond to command.\n" );
            ioReturn = kIOReturnInternalError;
            break;
        }
    }/* end FOR */
	ELG( time.tv_sec - startTime.tv_sec, time.tv_nsec - startTime.tv_nsec, ' WFM', "waitForMesh" );
    return  ioReturn;
}/* end waitForMesh */


    /* Send a command to the MESH chip. This may cause an interrupt.    */

void meshSCSIController::setSeqReg( MeshCommand meshCommand )
{
    ELG( (fMESHAddr->interruptMask<<16) | fMESHAddr->interrupt, meshCommand, '=Seq', "setSeqReg" );

    if ( fMESHAddr->interruptMask & kMeshIntrCmdDone
      && meshCommand <= kMeshBusFreeCmd )
        ELG( fMESHAddr->interrupt, fMESHAddr->interruptMask, 'Trig',
                    "setSeqReg - may trigger interrupt." );

    fMESHAddr->sequence = (UInt8)meshCommand;	/***** DO IT    *****/
    SynchronizeIO();
    IODelay( 1 );                               /* G3 is too fast   */

    return;
}/* end setSeqReg */


    /* Retrieve the MESH volatile register contents,        */
    /* storing them in the global register shadow.          */
    /* @param   clearInts   YES to clear MESH interrupts.   */

void meshSCSIController::getHBARegsAndClear( bool clearInts )
{
    register MeshRegister   *mesh = fMESHAddr;


    g.shadow.mesh.interrupt         = mesh->interrupt;
    g.shadow.mesh.error             = mesh->error;
    g.shadow.mesh.exception         = mesh->exception;
    g.shadow.mesh.FIFOCount         = mesh->FIFOCount;

    g.shadow.mesh.busStatus0        = mesh->busStatus0;
    g.shadow.mesh.busStatus1        = mesh->busStatus1;
    g.shadow.mesh.transferCount1    = mesh->transferCount1;
    g.shadow.mesh.transferCount0    = mesh->transferCount0;

    g.shadow.mesh.sequence          = mesh->sequence;           // debugging
    g.shadow.mesh.interruptMask     = mesh->interruptMask;      // debugging
    g.shadow.mesh.syncParms         = mesh->syncParms;          // debugging
    g.shadow.mesh.destinationID     = mesh->destinationID;      // debugging

    ELG( g.shadow.longWord[ 0 ], g.shadow.longWord[ 1 ], clearInts ? 'Regs' : 'regs', "getHBARegsAndClear." );

    if ( g.shadow.mesh.error )	// this occurs when DBDMA -> Seq while reselect
	{							// OR Exc:reselect occurs just before busFree->Seq reg.
		ELG( g.shadow.mesh.interruptMask, g.shadow.mesh.sequence, 'Err-',
									"getHBARegsAndClear - MESH error detected" );
		mesh->interrupt = kMeshIntrError;
		SynchronizeIO();
	}

        /* It is possible to have the Reselected bit set in the Exception   */
        /* register without an Exception bit in the interrupt register.     */
        /* This may be caused by timing window where we clear the interrupt */
        /* register with the interrupt register instead of 0x07.            */
        /* Handle this by faking an exception.                              */
        /* 04may98 - it is also possible to have PhaseMisMatch set in the   */
        /* Exception register without Exception indicated in the Interrupt  */
        /* register. This happened when a Synchronous output finished and   */
        /* the target went to Message-In phase with Save-Data-Pointer.      */


    if ( g.shadow.mesh.exception )
         g.shadow.mesh.interrupt |= kMeshIntrException;

    if ( clearInts && g.shadow.mesh.interrupt )
    {
        mesh->interrupt = g.shadow.mesh.interrupt;
        SynchronizeIO();
    }
    return;
}/* end getHBARegsAndClear */


void meshSCSIController::setIntMask( UInt8 mask )
{
    ELG( (fMESHAddr->interrupt<<16) | fMESHAddr->interruptMask, mask, 'Mask', "setIntMask" );
    fMESHAddr->interruptMask = mask;         /* enable whatever  */
    SynchronizeIO();
    return;
}/* end setIntMask */


void meshSCSIController::abortActiveCommand()
{
    IOReturn        ioReturn;


    ELG( fCmd, 0, '-AB*', "abortActiveCommand" );
	if ( fCmd )
	{
		fCmdData->results.adapterStatus = kSCSIAdapterStatusProtocolError;	// ??? use kIOAborted?
		completeCommand();
	}

    getHBARegsAndClear( true );				/* clear possible cmdDone et al  */
    setIntMask( 0 );						/* Disable MESH interrupts       */

	fMsgInFlag = 0;							/* clear kFlagMsgIn_Reject et al */

    fMESHAddr->busStatus0 = kMeshAtn;		/***** Raise ATN signal      *****/
    SynchronizeIO();

    setSeqReg( kMeshBusFreeCmd );			/* clear ACK                     */
	waitForMesh( true );					/* wait for PhaseMM              */

    if ( (g.shadow.mesh.busStatus0 & (kMeshPhaseMask | kMeshReq))
                                   == (kBusPhaseMSGO | kMeshReq) )
    {           /* this is what we want:    */
            setSeqReg( kMeshFlushFIFO );			/* Flush the FIFO           */
            fMESHAddr->transferCount0	= 1;        /* set TC low = 1           */
            fMESHAddr->transferCount1	= 0;
            fMESHAddr->busStatus0		= 0;        /***** clear ATN signal *****/
            SynchronizeIO();

                /* Issue the Message Out sending the Abort on its way.  */
                /* Note that this will cause an Unexpected-Disconnect.  */
            setSeqReg(  kMeshMessageOutCmd );		/* drop ATN signal         */
            fMESHAddr->xFIFO = kSCSIMsgAbort;		/* put out the Abort byte  */
            ioReturn = waitForMesh( true );			/* wait for cmdDone        */
            if ( ioReturn == kIOReturnSuccess )
            {
				setSeqReg( kMeshEnableReselect );		/* bus about to go free    */
				setIntMask( kMeshIntrMask );			/* Enable interrupts       */
             	setSeqReg( kMeshBusFreeCmd );			/* Clr ACK & go Bus-Free   */
                g.intLevel |= kLevelLatched;			/* set latched-int flag    */
                return;
            }
    }/* end IF MSGO phase and REQ is set */

        /***** USE THE HAMMER - NUKE THE BUS: *****/

    ELG( 0, 0, '-AB-', "abortActiveCommand - target refused to enter MSGO phase" );
    resetBus();
	super::resetOccurred();
    return;
}/* end abortActiveCommand */


    /* IO associated with fCmd has disconnected.  */
    /* Place it on the disconnected command queue and       */
    /* enable another transaction.                          */

void meshSCSIController::disconnect()
{
	fCmd						= NULL;
	fCmdData					= NULL;
	fCurrentTargetLun.target	= kInvalidTarget;
	fCurrentTargetLun.lun		= kInvalidLUN;

        /* Since there is no active command, the caller */
        /* must configure the bus interface to wait for */
        /* bus free, then allow reselection.            */

    return;
}/* end disconnect */


    /* The specified target, LUN, and queueTag is trying to reselect.   */
    /* If we have a xxxCommandBuffer for this TLQ nexus on disconnectQ,    */
    /* remove it, make it the current fCmd, and return YES.   */
    /* Else return NO. A value of zero for queueTag indicates a         */
    /* nontagged command (zero is never used as the queue tag value for */
    /* a tagged command).                                               */

IOReturn meshSCSIController::reselectNexus()
{
	fCmd = findCommandWithNexus( fCurrentTargetLun, fTag );
	if ( fCmd )
	{	fCmdData = (PrivCmdData*)fCmd->getCommandData();
		ELG( fCmd, *(UInt16*)&fCurrentTargetLun<<16 | (UInt16)fTag, '=Nex', "reselectNexus" );
		return kIOReturnSuccess;
	}
	else
	{	if ( fTag != kInvalidTag )
			PAUSE( 0, *(UInt16*)&fCurrentTargetLun<<16 | (UInt16)fTag, 'Nex-', "reselectNexus" );
	}

    return  kIOReturnInternalError;
}/* end reselectNexus */


void meshSCSIController::updateCurrentIndex()
{
    UInt32          count;                           /* DMA transfer count */
    UInt32          length = g.shadow.mesh.FIFOCount;
    UInt8           buffer[ 16 ];
    UInt32          i;


        /* Calculate the number of bytes xferred by this channel command.   */
        /* We don't trust the DBDMA residual count.                         */

    count  = *(UInt32*)&fCCL[ kcclBatchSize ];		/* Our transfer count   */
    if ( count == 0 )                               /* If batch is empty,   */
        return;                                     /* look at nothing else.*/
    count -= g.shadow.mesh.transferCount1 << 8;     /* MESH residual high   */
    count -= g.shadow.mesh.transferCount0;          /* MESH residual low    */
	fCmdData->results.bytesTransferred += count;	/* Increment data index */
    *(UInt32*)&fCCL[ kcclBatchSize ] = 0;			/* Clear DBDMA count	*/

	if ( fReadAlignmentCount )						// Hack for Radar 1670626
	{
		fCmdData->mdp->writeBytes( fReadAlignmentIndex, &fCCL[ kcclReadBuf8 ], fReadAlignmentCount );
		fReadAlignmentCount = 0;
	}

        /* Check the FIFO, if empty, increment the current data pointer.    */
        /* If there is stuff in it, we have more work to do.                */

    if ( g.shadow.mesh.FIFOCount )                /* If data in FIFO:       */
    {
			/* We didn't process these bytes in the FIFO - adjust index		*/
		fCmdData->results.bytesTransferred -= g.shadow.mesh.FIFOCount;

		if ( fCmdData->isWrite )						/* If Writing:	*/
        {
			setSeqReg( kMeshFlushFIFO );
        }
		else    /* Must be Reading:                                     */
        {       /* On a Read with data left in the FIFO, we must copy   */
                /* the FIFO directly into the user's data buffer:       */

            ELG( fCmdData->results.bytesTransferred, g.shadow.mesh.FIFOCount, 'FIFO',
                                "updateCurrentIndex - copy FIFO to user buffer." );
			count = fCmdData->xferCount - fCmdData->results.bytesTransferred;
            if ( count > length )
                 count = length;

                /* FYI - emptying the FIFO causes cmdDone to get set.   */

            for ( i = 0; i < count; i++ )
                buffer[ i ] = fMESHAddr->xFIFO;

			fCmdData->mdp->writeBytes( fCmdData->results.bytesTransferred, buffer, count );

            fCmdData->results.bytesTransferred += count;
        }/* end if/ELSE must be Reading */
    }/* end IF FIFO was not empty */

    ELG( 0, fCmdData->results.bytesTransferred, 'UpIx', "updateCurrentIndex" );
    return;
}/* end updateCurrentIndex */
