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
	 * Copyright © 1997-2000 Apple Computer Inc. All Rights Reserved.
	 * @author   Mike Johnson
	 *
	 * Set tabs every 4 characters.
	 *
	 * Edit History
	 * 25feb99   mlj      Initial conversion from banana.
	 */



	/*****	For fans of kprintf, IOLog and debugging infrastructure of the	*****/
	/*****	string ilk, please modify the ELG and PAUSE macros or their		*****/
	/*****	associated EvLog and Pause functions to suit your taste. These	*****/
	/*****	macros currently are set up to log events to a wraparound		*****/
	/*****	buffer with minimal performance impact. They take 2 UInt32		*****/
	/*****	parameters so that when the buffer is dumped 16 bytes per line,	*****/
	/*****	time stamps (~1 microsecond) run down the left side while		*****/
	/*****	unique 4-byte ASCII codes can be read down the right side.		*****/
	/*****	Preserving this convention facilitates different maintainers	*****/
	/*****	using different debugging styles with minimal code clutter.		*****/

#define USE_ELG   0				    // for debugging
#define kEvLogSize  (4096*16)		// 16 pages = 64K = 4096 events

#if USE_ELG /* (( */
#define ELG(A,B,ASCI,STRING)    EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#define PAUSE(A,B,ASCI,STRING)  Pause( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#else /* ) not USE_ELG: (   */
#define ELG(A,B,ASCI,S)
#define PAUSE(A,B,ASCI,STRING)	IOLog( "MESH: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) )
#endif /* USE_ELG )) */


#ifndef SynchronizeIO
#define SynchronizeIO()     eieio()     /* TEMP */
#endif /* SynchronizeIO */

	enum { kMaxAutosenseByteCount = 255 };

	enum
	{
	    kMESHRegisterBase	= 0,
	    kDBDMARegisterBase	= 1,
	    kNumberRegisters	= 2
	};

        /* Operation flags and options: */

    typedef enum BusPhase   /* These are the real SCSI bus phases (from busStatus0):   */
    {
        kBusPhaseDATO   = 0,
        kBusPhaseDATI,
        kBusPhaseCMD,
        kBusPhaseSTS,
        kBusPhaseReserved1,
        kBusPhaseReserved2,
        kBusPhaseMSGO,
        kBusPhaseMSGI
    } BusPhase;

        /* Command to be executed by IO thread.                     */
        /* These are ultimately derived from ioctl control values.  */

    typedef enum
    {   kCommandExecute,            /* Execute IOSCSIRequest    */
        kCommandResetBus,           /* Reset bus                */
        kCommandAbortRequest        /* Abort IO thread          */
    } CommandOperation;

        /* We read target messages using a simple state machine.    */
        /* On entrance to MSGI phase, fMsgInState = kMsgInInit.     */
        /* Continue reading messages until either                   */
        /* fMsgInState == kMsgInReady or the target changes phase   */
        /* (which is an error).                                     */
    typedef enum MsgInState
    {
        kMsgInInit = 0,     /*  0 Not reading a message (must be zero)      */
        kMsgInReading,      /*  1 MSG input state: reading counted data     */
        kMsgInCounting,     /*  2 MSG input state: reading count byte       */
        kMsgInReady         /*  3 MSG input state: a msg is now available   */
    } MsgInState;

		/* These values represent no active request:	*/
    enum
    {   kInvalidTarget	= 0xFF,
        kInvalidLUN		= 0xFF,
		kInvalidTag		= 0xFFFFFFFF
    };

        /* The default initiator bus ID (needs to be fetched from NVRAM).    */
    enum
	{
		kInitiatorIDDefault = 7,
		kMaxTargetID		= 8,
		kMaxDMATransfer		= 0xF000ul,
		kMaxMemCursorSegs	= 15
	};
#define APPLE_SCSI_RESET_DELAY  250 /* Msec */




    typedef struct MeshRegister     /* Mesh registers:  */
    {
        volatile UInt8      transferCount0;     UInt8   pad00[ 0x0F ];
        volatile UInt8      transferCount1;     UInt8   pad01[ 0x0F ];
        volatile UInt8      xFIFO;              UInt8   pad02[ 0x0F ];
        volatile UInt8      sequence;           UInt8   pad03[ 0x0F ];
        volatile UInt8      busStatus0;         UInt8   pad04[ 0x0F ];
        volatile UInt8      busStatus1;         UInt8   pad05[ 0x0F ];
        volatile UInt8      FIFOCount;          UInt8   pad06[ 0x0F ];
        volatile UInt8      exception;          UInt8   pad07[ 0x0F ];
        volatile UInt8      error;              UInt8   pad08[ 0x0F ];
        volatile UInt8      interruptMask;      UInt8   pad09[ 0x0F ];
        volatile UInt8      interrupt;          UInt8   pad10[ 0x0F ];
        volatile UInt8      sourceID;           UInt8   pad11[ 0x0F ];
        volatile UInt8      destinationID;      UInt8   pad12[ 0x0F ];
        volatile UInt8      syncParms;          UInt8   pad13[ 0x0F ];
        volatile UInt8      MESHID;             UInt8   pad14[ 0x0F ];
        volatile UInt8      selectionTimeOut;
    } MeshRegister;

        /* The following structure shadows the MESH chip registers: */

    typedef union MESHShadow
    {   UInt32      longWord[ 3 ];      /* for debugging ease.                  */
        struct
        {   UInt8   interrupt;          /* Interrupt                            */
            UInt8   error;              /* Error register                       */
            UInt8   exception;          /* Exception register                   */
            UInt8   FIFOCount;          /* FIFO count                           */

            UInt8   busStatus0;         /* Bus phase + REQ, ACK, & ATN signals  */
            UInt8   busStatus1;         /* RST, BSY, SEL                        */
            UInt8   interruptMask;      /* Interrupt mask for debugging         */
            UInt8   transferCount0;     /* low  order byte of transfer count    */

            UInt8   transferCount1;     /* high order byte of transfer count    */
            UInt8   sequence;           /* Sequence register                    */
            UInt8   syncParms;          /* syncParms for debugging              */
            UInt8   destinationID;      /* Target ID                            */
        } mesh;
    } MESHShadow;

        /* MESH Register set offsets    */

    enum
    {
        kMeshTransferCount0 =   0x00,
        kMeshTransferCount1 =   0x10,
        kMeshFIFO           =   0x20,
        kMeshSequence       =   0x30,
        kMeshBusStatus0     =   0x40,
        kMeshBusStatus1     =   0x50,
        kMeshFIFOCount      =   0x60,
        kMeshException      =   0x70,
        kMeshError          =   0x80,
        kMeshInterruptMask  =   0x90,
        kMeshInterrupt      =   0xA0,
        kMeshSourceID       =   0xB0,
        kMeshDestinationID  =   0xC0,
        kMeshSyncParms      =   0xD0,
        kMeshMESHID         =   0xE0,
        kMeshSelTimeOut     =   0xF0
    };

	enum { kMeshMESHID_Value = 0x02 };     /* Read value of kMESHID lo 5 bits only */


        /* MESH commands & modifiers for Sequence register: */

    typedef enum
    {
        kMeshNoOpCmd            =   0x00,
        kMeshArbitrateCmd       =   0x01,
        kMeshSelectCmd          =   0x02,
        kMeshCommandCmd         =   0x03,
        kMeshStatusCmd          =   0x04,
        kMeshDataOutCmd         =   0x05,
        kMeshDataInCmd          =   0x06,
        kMeshMessageOutCmd      =   0x07,
        kMeshMessageInCmd       =   0x08,
        kMeshBusFreeCmd         =   0x09,
            								/* non interrupting:    */
        kMeshEnableParity       =   0x0A,
        kMeshDisableParity      =   0x0B,
        kMeshEnableReselect     =   0x0C,
        kMeshDisableReselect    =   0x0D,
        kMeshResetMESH          =   0x0E,
        kMeshFlushFIFO          =   0x0F,
            /* Sequence command modifier bits:  */
        kMeshSeqDMA     =   0x80,   /* Data Xfer for this command will use DMA  */
        kMeshSeqTMode   =   0x40,   /* Target mode - unused                     */
        kMeshSeqAtn     =   0x20    /* ATN is to be asserted after command      */
    } MeshCommand;

		/* The bus Status Registers 0 & 1 have the actual	*/
		/* bus signals AT TIME OF READ.						*/

    enum                        /* bus Status Register 0 bits:  */
    {
        kMeshIO     =   0x01,   /* phase bit    */
        kMeshCD     =   0x02,   /* phase bit    */
        kMeshMsg    =   0x04,   /* phase bit    */
        kMeshAtn    =   0x08,   /* Attention signal */
        kMeshAck    =   0x10,   /* Ack signal       */
        kMeshReq    =   0x20,   /* Request signal   */
        kMeshAck32  =   0x40,   /* unused - 32 bit bus  */
        kMeshReq32  =   0x80    /* unused - 32 bit bus  */
    };

    enum { kMeshPhaseMask = (kMeshMsg + kMeshCD + kMeshIO)   };

    enum                     /* bus Status Register 1 bits:  */
    {
        kMeshSel =   0x20,   /* Select signal    */
        kMeshBsy =   0x40,   /* Busy signal      */
        kMeshRst =   0x80    /* Reset signal     */
    };

    enum                                 /* Exception Register bits:   */
    {
        kMeshExcSelTO           =   0x01,   /* Selection timeout    */
        kMeshExcPhaseMM         =   0x02,   /* Phase mismatch       */
        kMeshExcArbLost         =   0x04,   /* lost arbitration     */
        kMeshExcResel           =   0x08,   /* reselection occurred */
        kMeshExcSelected        =   0x10,
        kMeshExcSelectedWAtn    =   0x20
    };

    enum                                    /* Error Register bits:     */
    {
        kMeshErrParity0         =   0x01,   /* parity error             */
        kMeshErrParity1         =   0x02,   /* unused - 32 bit bus      */
        kMeshErrParity2         =   0x04,   /* unused - 32 bit bus      */
        kMeshErrParity3         =   0x08,   /* unused - 32 bit bus      */
        kMeshErrSequence        =   0x10,   /* Sequence error           */
        kMeshErrSCSIRst         =   0x20,   /* Reset signal asserted    */
        kMeshErrDisconnected    =   0x40    /* unexpected disconnect    */
    };

    enum                                    /* Interrupt Register bits: */
    {
        kMeshIntrCmdDone    =   0x01,       /* command done             */
        kMeshIntrException  =   0x02,       /* exception occurred       */
        kMeshIntrError      =   0x04,       /* error     occurred       */
        kMeshIntrMask       =   (kMeshIntrCmdDone | kMeshIntrException | kMeshIntrError)
    };


    enum        /* Values for SyncParms MESH register:      */
    {           /* 1st nibble is offset, 2nd is period.     */
                /* Zero offset means async.                 */
        kSyncParmsAsync = 0x02, /* Async with min period = 2            */
        kSyncParmsFast  = 0xF0  /* offset = 15, period = Fast (10 MB/s) */
    };

        /* The following are specific to the MESH CCL               */
        /* Stage Names. (These were originally 'xxxx' identifiers,  */
        /* which is convenient for debugging, but results in many   */
        /* warning messages from the NeXT compiler.                 */
    enum
    {								/* kcclStageLabel:	*/
        kcclStageIdle   = 0,        /*  0 - 'Idle'		*/
        kcclStageInit,              /*  1 - 'Init'		*/
        kcclStageCCLx,              /*  2 - 'CCL~'		*/
        kcclStageArb,               /*  3 - ' Arb'		*/
        kcclStageSelA,              /*  4 - 'SelA'		*/
        kcclStageMsgO,              /*  5 - 'MsgO'		*/
        kcclStageCmdO,              /*  6 - 'CmdO'		*/
        kcclStageXfer,              /*  7 - 'Xfer'		*/
        kcclStageBucket,            /*  8 - 'Buck'		*/
        kcclStageSyncHack,          /*  9 - 'Hack'		*/
        kcclStageStat,              /*  A - ' Sta'		*/
        kcclStageMsgI,              /*  B - 'MsgI'		*/
        kcclStageFree,              /*  C - 'Free'		*/
        kcclStageGood,              /*  D - 'Good' 		*/
        kcclStageStop,              /*  E - '++++'		*/
        kcclTerminatorWithoutComma
    };

    /* offsets into the Channel Command List page:  */

#define kcclProblem     0x00    // Interrupt & Stop channel commands for anomalies
#define kcclCMDOdata    0x20    // reserve for 6, 10, 12 byte commands
#define kcclMSGOdata    0x30    // reserve for Identify, Tag stuff
#define kcclMSGOLast    0x3F    // reserve for last or only msg0ut byte
#define kcclMSGIdata    0x40    // reserve for Message In data
#define kcclBucket      0x48    // Bit Bucket
#define kcclStatusData  0x4F    // reserve for Status byte
//#define kcclSenseCDB    0x50    // CDB for (auto) Sense
#define kcclBatchSize   0x60    // Current MESH batch size
#define kcclStageLabel  0x6C    // storage for label of last stage entered.
#define kcclSense       0x70    // Channel Commands for (Auto)Sense
#define kcclPrototype   0xC0    // Prototype MESH 4-command Transfer sequence
#define kcclReadBuf8    0x100   // Buffer for non-8-byte-aligned reads
#define kcclStart       0x120   // Channel Program starts here with Arbitrate
#define kcclBrProblem   0x140   // channel command to wait for cmdDone & Br if problem
#define kcclMsgoStage   0x190   // Branch to single byte Message-Out
#define kcclMsgoBranch  0x1B0   // Branch to single byte Message-Out
#define kcclMsgoMTC     0x1D8   // MESH Transfer Count for MSGO (low order only)
#define kcclMsgoDTC     0x1F0   // DMA  Transfer Count for MSGO (low order only)
#define kcclLastMsgo    0x210   // Channel commands to put last/only byte of Message-Out
#define kcclCmdoStage   0x290   // Start of Command phase
#define kcclCmdoMTC     0x2C8   // MESH Transfer Count for CMDO (low order only)
#define kcclCmdoDTC     0x2E0   // DMA  Transfer Count for CMDO (low order only)
#define kcclReselect    0x2F0   // Reselect enters CCL here - Branch to xfer data
#define kcclOverrun     0x320   // data overrun - dump the excess in the bit bucket
#define kcclOverrunMESH 0x370   // data overrun - patch the MESH Seq Reg I/O
#define kcclOverrunDBDMA 0x380  // data overrun - patch the DBDMA I/O
#define kcclSyncCleanUp 0x3B0   // clean up at end of Sync xfer
#define kcclGetStatus   0x3D0   // Finish up with Status, Message In, and Bus Free
#define kcclMsgiStage   0x420   // Get Message-In hopefully Command Complete
#define kcclBusFreeStage 0x480  // enable Reselect and go Bus Free
#define kcclMESHintr    0x4D0   // transaction done or going well
#define kcclDataXfer    0x500   // INPUT or OUTPUT channel commands for data


    /* generic relocation types:    */

#define kRelNone    0x00    /* default - no relocation                      */
#define kRelMESH    0x01    /* Relocate to MESH register area               */
#define kRelCP      0x02    /* Relocate to Channel Program area             */
#define kRelCPdata  0x03    /* Relocate to Channel Program data structure   */
#define kRelPhys    0x04    /* Relocate to user Physical address space      */
#define kRelNoSwap  0x05    /* don't relocate or swap (Label)               */

    /* Relocatable ADDRESS types:   */

#define kRelAddress         0xFF        <<8 /* relocatable address mask         */
#define kRelAddressMESH     kRelMESH    <<8 /* MESH physical address            */
#define kRelAddressCP       kRelCP      <<8 /* Channel Program Physical address */
#define kRelAddressPhys     kRelPhys    <<8 /* User data Physical address       */

    /* Relocatable COMMAND-DEPENDENT types: */

#define kRelCmdDep      0xFF        /* relocatable command-dependent mask           */
#define kRelCmdDepCP    kRelCP      /* Channel Program command-dependent (branch)   */
#define kRelCmdDepLabel kRelNoSwap  /* Channel Program label - don't swap           */


    /* Channel Program macros:  */

#define STAGE(v)        STORE_QUAD  | KEY_SYSTEM    | 4, kcclStageLabel, v, kRelAddressCP | kRelCmdDepLabel
#define CLEAR_CMD_DONE  STORE_QUAD  | KEY_SYSTEM    | 1, kMeshInterrupt, kMeshIntrCmdDone, kRelAddressMESH
#define CLEAR_INT_REG   STORE_QUAD  | KEY_SYSTEM    | 1, kMeshInterrupt, kMeshIntrMask, kRelAddressMESH
#define CLR_PHASEMM     STORE_QUAD  | KEY_SYSTEM    | 1, kMeshInterrupt, kMeshIntrCmdDone | kMeshIntrException, kRelAddressMESH
#define MOVE_1(a,v,r)   STORE_QUAD  | KEY_SYSTEM    | 1, a, v, r
#define MOVE_4(a,v,r)   STORE_QUAD  | KEY_SYSTEM    | 4, a, v, r
#define MESH_REG(a,v)   STORE_QUAD  | KEY_SYSTEM    | 1, a, v, kRelAddressMESH
#define MESH_REG_WAIT(a,v)  STORE_QUAD  | KEY_SYSTEM | kWaitIfTrue | 1, a, v, kRelAddressMESH

#define MSGO(a,c)       OUTPUT_LAST | kBranchIfFalse | kWaitIfTrue | c, a,                kcclProblem, kRelAddressCP   | kRelCmdDepCP
#define CMDO(c)         OUTPUT_LAST | kBranchIfFalse | kWaitIfTrue | c, kcclCMDOdata,     kcclProblem, kRelAddressCP   | kRelCmdDepCP
#define MSGI(c)         INPUT_LAST  | kBranchIfFalse | kWaitIfTrue | c, kcclMSGIdata,     kcclProblem, kRelAddressCP   | kRelCmdDepCP
#define STATUS_IN       INPUT_LAST  | kBranchIfFalse | kWaitIfTrue | 1, kcclStatusData,   kcclProblem, kRelAddressCP   | kRelCmdDepCP
#define BUCKET          INPUT_LAST  | kBranchIfFalse               | 8, kcclBucket,       kcclProblem, kRelAddressCP   | kRelCmdDepCP

#define BRANCH(a)        NOP_CMD | kBranchAlways,                0, a, kRelCmdDepCP
#define BR_IF_PROBLEM    NOP_CMD | kBranchIfFalse | kWaitIfTrue, 0, kcclProblem, kRelCmdDepCP
#define BR_NO_PROBLEM(a) NOP_CMD | kBranchIfTrue               , 0, a, kRelCmdDepCP
#define STOP(L)          STOP_CMD,                               0, L, kRelCmdDepLabel
#define INTERRUPT(a)     NOP_CMD | kIntAlways, 0, a, 0
#define RESERVE          0xCEFECEFE, 0xCEFECEFE, 0xCEFECEFE, 0xCEFECEFE
#define WAIT_4_CMDDONE   NOP_CMD | kWaitIfTrue, 0, 0, 0

#define SWAP(x) (UInt32)OSSwapInt32( (UInt32)(x) )


        /* Return values from startCommand:    */

	enum
    {   kHardwareStartOK,       /* command started successfully */
		kHardwareStartBusy
    };



		/**** THESE NEED TO BE VOLATILE	*****/
	struct DBDMAChannelRegisters		/*  DBDMA channel registers:	*/
	{
		volatile UInt32		channelControl;
		volatile UInt32 	channelStatus;
		volatile UInt32 	commandPtrHi;		/* implementation optional		*/
		volatile UInt32 	commandPtrLo;
		volatile UInt32 	interruptSelect;
		volatile UInt32 	branchSelect;
		volatile UInt32 	waitSelect;
		volatile UInt32 	transferModes;
		volatile UInt32 	data2PtrHi;			/* implementation optional		*/
		volatile UInt32 	data2PtrLo;			/* implementation optional		*/

		volatile UInt32 	reserved1;
		volatile UInt32 	addressHi;			/* implementation optional		*/
		volatile UInt32 	reserved2[4];
	};
    typedef struct DBDMAChannelRegisters	DBDMAChannelRegisters;


    struct DBDMADescriptor	/* Define the DBDMA Channel Command descriptor.	*/
    {
        UInt32  operation;       /* cmd | key | i | b | w | reqCount   */
        UInt32  address;
        UInt32  cmdDep;
        UInt32  result;         /* xferStatus | resCount   */
    };
    typedef struct DBDMADescriptor		DBDMADescriptor;

        /* Define the DBDMA channel command operations and modifiers:	*/

    enum        /* Command.cmd operations   */
    {
        OUTPUT_MORE     = 0x00000000,
        OUTPUT_LAST     = 0x10000000,
        INPUT_MORE      = 0x20000000,
        INPUT_LAST      = 0x30000000,
        STORE_QUAD      = 0x40000000,
        LOAD_QUAD       = 0x50000000,
        NOP_CMD         = 0x60000000,
        STOP_CMD        = 0x70000000,
        kdbdmaCmdMask   = 0xF0000000
    };


    enum
    {       /* Command.key modifiers                            */
            /* (choose one for INPUT, OUTPUT, LOAD, and STORE)  */
        KEY_STREAM0         = 0x00000000,   /* default modifier*/
        KEY_STREAM1         = 0x01000000,
        KEY_STREAM2         = 0x02000000,
        KEY_STREAM3         = 0x03000000,
        KEY_REGS            = 0x05000000,
        KEY_SYSTEM          = 0x06000000,
        KEY_DEVICE          = 0x07000000,
        kdbdmaKeyMask       = 0x07000000,   /* Command.i modifiers (choose one for INPUT, OUTPUT, LOAD, STORE, and NOP)*/
        kIntNever           = 0x00000000,   /* default modifier     */
        kIntIfTrue          = 0x00100000,
        kIntIfFalse         = 0x00200000,
        kIntAlways          = 0x00300000,
        kdbdmaIMask         = 0x00300000,   /* Command.b modifiers (choose one for INPUT, OUTPUT, and NOP)*/
        kBranchNever        = 0x00000000,   /* default modifier     */
        kBranchIfTrue       = 0x00040000,
        kBranchIfFalse      = 0x00080000,
        kBranchAlways       = 0x000C0000,
        kdbdmaBMask         = 0x000C0000,   /* Command.w modifiers (choose one for INPUT, OUTPUT, LOAD, STORE, and NOP)*/
        kWaitNever          = 0x00000000,   /* default modifier     */
        kWaitIfTrue         = 0x00010000,
        kWaitIfFalse        = 0x00020000,
        kWaitAlways         = 0x00030000,
        kdbdmaWMask         = 0x00030000,    /* operation masks     */
    };


	class meshSCSIController;

    typedef struct globals      /* Globals for this module (not per instance)   */
    {
        UInt32       evLogFlag; // debugging only
        UInt8       *evLogBuf;
        UInt8       *evLogBufe;
        UInt8       *evLogBufp;
        UInt8       intLevel;

        MESHShadow  shadow; // move to per instance??? /* Last MESH register state      */

        UInt32      cclLogAddr,     cclPhysAddr;    // for debugging/miniMon ease
        UInt32      meshAddr;                       // for debugging/miniMon ease
		class meshSCSIController	*meshInstance;
    } globals;


	typedef struct PrivCmdData
	{
		IOMemoryDescriptor	*mdp;				/* Memory Descriptor Pointer	*/
		UInt32				xferCount;
		UInt32				savedDataPosition;	/* getPosition at disconnect	*/
		SCSIResults			results;
		bool				isWrite;
		bool                negotiatingSDTR;
		bool                negotiatingSDTRComplete;
	} PrivCmdData;


class meshSCSIController : public IOSCSIParallelController
{
    OSDeclareDefaultStructors( meshSCSIController )	/* Constructor & Destructor stuff	*/

protected:

	bool	configure( IOService *provider, SCSIControllerInfo *controllerDataSize );
	void	executeCommand(	IOSCSIParallelCommand* );
	void	cancelCommand(	IOSCSIParallelCommand* );
	void	resetCommand(	IOSCSIParallelCommand* );
	void	free();

private:    

	IOReturn	initializeHardware();
	IOReturn	getHardwareMemoryMaps();
	IOReturn	allocHdwAndChanMem();
	IOReturn	doHBASelfTest();
	IOReturn 	setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
	
	void   		setSCSIActiveTermState(bool enableTermPower);
	
	void		initCP();

	void 		interruptOccurred( IOInterruptEventSource *ies, int intCount );
	void		doHardwareInterrupt();
	void		processInterrupt();

	UInt8		startCommand();
	void		setupMsgO();
	void		clearCPResults();
	void		updateCP( bool reselecting );
	void		runDBDMA ( UInt32 offset, UInt32 stageLabel );
	void 		completeCommand();

	void		disconnect();
	void		updateCurrentIndex();
	void	    handleReselectionInterrupt(); /* Process a reselection interrupt. */
	bool		getReselectionTargetID();
	IOReturn	reselectNexus();

	void		startBucket();

	void		setSeqReg( MeshCommand meshCommand );
	void		getHBARegsAndClear( bool clearInts );
	void		setIntMask( UInt8 interruptMask );
	IOReturn	resetBus();
	IOReturn	waitForMesh( bool clearInterrupts );

	void		abortActiveCommand();
	void		abortDisconnectedCommand();

	void		doInterruptStageArb();
	void		doInterruptStageSelA();
	void		doInterruptStageMsgO();
	void		doInterruptStageCmdO();
	void		doInterruptStageXfer();
	void		doInterruptStageGood();

	IOReturn	DoMessageInPhase();           /* Handle MSGI phase.   */
	void	    ProcessMSGI();
	void	    issueReject();
	
private:

    IOService				*fProvider;
    IOSCSIParallelCommand		*fCmd;
    PrivCmdData				*fCmdData;
    IOMemoryMap				*fIOMap;
    IOInterruptEventSource	*fInterruptEvent;

    IOMemoryMap		*fSCSIMemoryMap;
	MeshRegister	*fMESHAddr;				/* MESH registers (logical)			*/
	UInt32			fMESHPhysAddr;			/* MESH registers (physical)		*/

    IOMemoryMap			*fDBDMAMemoryMap;
	UInt8				*dbdmaAddr;			/* DBDMA registers (logical)		*/
	IOPhysicalAddress	dbdmaAddrPhys;		/* DBDMA registers (physical)		*/

	IOPhysicalAddress fCCLPhysAddr;			/* Channel Command List (physical)	*/
	UInt8			*fCCL;					/* Channel Command List (logical)	*/
	UInt32			fCCLSize;				/* Channel Command List size		*/
	UInt32			fDBDMADescriptorMax;    /* max # Channel Commands			*/

    IOBigMemoryCursor	*fMemoryCursor;		// pointer to Big-endian memory Cursor

	UInt32			fReadAlignmentCount;	// hack for DBDMA bug at start of
	UInt32			fReadAlignmentIndex;	// Read when buffer is misaligned

	SCSITargetLun	fCurrentTargetLun;
	UInt32       	fTag;					/* Last tag value					*/
	UInt8			fTagType;				/* Last tag type - simple queue...	*/
	UInt8			fSyncParms[ 8 ];

    UInt8       	*fMsgOutPtr;			/* ptr to message-out data 			*/

		/* These variables manage Message-In bus phase. Because the */
		/* Message-In handler uses programmed IO, fMsgInCount and   */
		/* fMsgInState are actually local variables to the message  */
		/* reader, and are here for debugging convenience.          */

    UInt8       fMsgInBuffer[ 16 ];
    SInt8       fMsgInCount;            /* Message bytes still to read  */
    MsgInState  fMsgInState;            /* How are we handling messages */

#define kFlagMsgIn_Reject       0x01
#define kFlagMsgIn_Disconnect   0x02
    UInt8       fMsgInFlag;

#define kFlagMsgOut_SDTR        0x01
#define kFlagMsgOut_Queuing     0x02
    UInt8       fMsgOutFlag;

    UInt8       fInitiatorID;           /* Our SCSI ID					*/
    UInt8       fInitiatorIDMask;       /* BusID bitmask for selection	*/
    UInt8       fSelectionTimeout;      /* In MESH 10 msec units		*/

    UInt8		fFlagIncompleteDBDMA;	/* Need more DMA				*/
    UInt8		fFlagReselecting;		/* Reselection in progress		*/

};/* end class MESH */
