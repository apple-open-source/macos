/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1996 NeXT Software, Inc.  All rights reserved. 
 *
 * i82557Private.cpp
 *
 */

#include "i82557.h"

//---------------------------------------------------------------------------
// Function: _intrACK
//
// Purpose:
//   Acknowledge all of the pending interrupt sources.
//
// Returns:
//   Return the interrupt status.
//
// CSR usage:
//   Read/Write: SCB status

static inline scb_status_t _intrACK(CSR_t * CSR_p)
{
    UInt32 cmdstatus = ReadLE32(&CSR_p->status);
    UInt16 status;

    if (cmdstatus == C_CSR_BAD) cmdstatus = 0;

    status = cmdstatus & SCB_STATUS_INT_MASK;
    if (status)
        WriteLE16(&CSR_p->status, status);   // ack pending interrupts.

    return status;
}

//---------------------------------------------------------------------------
// Function: _waitSCBCommandClear
//
// Purpose:
//   Wait for the SCB Command field to clear. This ensures that we don't
//   overrun the NIC's command unit.
//
// Returns:
//   true  if the SCB command field was cleared.
//   false if the SCB command field was not cleared.
//
// CSR usage:
//   Read: SCB command

static inline bool
_waitSCBCommandClear(CSR_t * CSR_p)
{
    for (int i = 0; i < SPIN_TIMEOUT; i++) {
        if (ReadLE32(&CSR_p->status) == C_CSR_BAD) break;
		if (!ReadLE8(&CSR_p->command)) return true;
		IODelay(SPIN_COUNT);
    }
    return false;	// hardware is not responding.
}

//---------------------------------------------------------------------------
// Function: _waitCUNonActive
//
// Purpose:
//   Waits for the Command Unit to become inactive.
//
// Returns:
//   true  if the CU has become inactive.
//   false if the CU remains active.
//
// CSR usage:
//   Read: SCB status

static inline bool
_waitCUNonActive(CSR_t * CSR_p)
{
    UInt32 cmdstatus;

    for (int i = 0; i < SPIN_TIMEOUT; i++) {
        cmdstatus = ReadLE32(&CSR_p->status);
        if (cmdstatus == C_CSR_BAD) break;
		if (CSR_VALUE(SCB_STATUS_CUS, cmdstatus) != SCB_CUS_ACTIVE)
			return true;
		IODelay(SPIN_COUNT);
	}
	return false;
}

//---------------------------------------------------------------------------
// Function: isCSRValid

bool Intel82557::isCSRValid()
{
    // Returns false to indicate that the CSR is no longer valid by
    // checking for all 1's in the CSR status and command word.
    // Certain bits are defined to never return zero. There shouldn't
    // be any side effects from reading this register.

    return ( ReadLE32(&CSR_p->status) != C_CSR_BAD );
}

//---------------------------------------------------------------------------
// Function: _polledCommand:WithAddress
//
// Purpose:
//   Issue a polled command to the NIC.

bool Intel82557::_polledCommand(cbHeader_t * hdr_p, IOPhysicalAddress paddr)
{
    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: _polledCommand:(%s): _waitSCBCommandClear failed\n",
               CUCommandString(CSR_VALUE(CB_CMD, ReadLE16(&hdr_p->command))),
			   getName());
		return false;
    }

    if (!_waitCUNonActive(CSR_p)) {
		VPRINT("%s: _polledCommand:(%s): _waitCUNonActive failed\n",
               CUCommandString(CSR_VALUE(CB_CMD, ReadLE16(&hdr_p->command))),
			   getName());
		return false;
    }

	// Set the physical address of the command block, and issue a
	// command unit start.

	WriteLE32(&CSR_p->pointer, paddr);
	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_CUC, SCB_CUC_START));
	ReadLE16(&CSR_p->status);
	prevCUCommand = SCB_CUC_START;

    for (int i = 0; (i < SPIN_TIMEOUT) && isCSRValid(); i++) {
		if (ReadLE16(&hdr_p->status) & CB_STATUS_C) {
            
            // If EL bit is set, then assume the CU engine is now in idle
            // state.

            if ( ReadLE16(&hdr_p->command) & CB_EL )
                cuIsIdle = true;
            
            return true;
        }
		IODelay(SPIN_COUNT);
    }
    return false;   // timeout occurred
}

//---------------------------------------------------------------------------
// Function: _abortReceive
//
// Purpose:
//   Abort the receive unit.

bool Intel82557::_abortReceive()
{
    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: _abortReceive: _waitSCBCommandClear failed\n", getName());
		return false;
    }

	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_RUC, SCB_RUC_ABORT));
	ReadLE16(&CSR_p->status);

    for (int i = 0; (i < SPIN_TIMEOUT) && isCSRValid(); i++) {
		if (CSR_VALUE(SCB_STATUS_RUS, ReadLE16(&CSR_p->status)) == SCB_RUS_IDLE)
			return true;
		IODelay(SPIN_COUNT);
    }

	VPRINT("%s: _abortReceive: timeout\n", getName());	
    return false;
}

//---------------------------------------------------------------------------
// Function: _startReceive
//
// Purpose:
//   Start the receive unit

bool Intel82557::_startReceive()
{
    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: _startReceive: _waitSCBCommandClear failed\n", getName());
		return false;
    }

	// Make sure the initial RFD has a link to its RBD
	WriteLE32(&headRfd->rbdAddr, headRfd->_rbd._paddr);

	WriteLE32(&CSR_p->pointer, headRfd->_paddr);
	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_RUC, SCB_RUC_START));
	ReadLE16(&CSR_p->status);

    for (int i = 0; (i < SPIN_TIMEOUT) && isCSRValid(); i++) {
		if (CSR_VALUE(SCB_STATUS_RUS, ReadLE16(&CSR_p->status)) == 
			SCB_RUS_READY) return true;
		IODelay(SPIN_COUNT);
    }

	VPRINT("%s: _startReceive: timeout\n", getName());		
    return false;
}

//---------------------------------------------------------------------------
// Function: _resetChip
//
// Purpose:
//   Issue a selective reset then a full reset.
//   This is done to avoid a PCI bus hang if the 82557 is in the midst of
//   a PCI bus cycle. The selective reset pauses the transmit and receive
//   engines.
//
void Intel82557::_resetChip()
{
    int i = 0;

	sendPortCommand(portSelectiveReset_e, 0);
    do {
		IOSleep(1);
    } while (isCSRValid() && ReadLE32(&CSR_p->port) && (++i < 100));

	sendPortCommand(portReset_e, 0);
    IOSleep(1);  // 10us min
    return;
}

//---------------------------------------------------------------------------
// Function: updateRFDFromMbuf
//
// Purpose:
//   Updated a RFD/RBD in order to attach it to a cluster mbuf.
//   XXX - assume cluster will never cross page boundary.

bool Intel82557::updateRFDFromMbuf(rfd_t * rfd_p, mbuf_t m)
{
	struct IOPhysicalSegment vector;
	UInt count;

	count = rxMbufCursor->getPhysicalSegments(m, &vector, 1);
	if (!count)
		return false;
	
	// Start modifying RFD
	//
	rfd_p->_rbd.buffer = vector.location;	// cursor is little-endian
//	WriteLE32(&rfd_p->_rbd.size, CSR_FIELD(RBD_SIZE, vector.length));

	rfd_p->_rbd._mbuf = m;
	
    return true;
}

//---------------------------------------------------------------------------
// Function: _initTcbQ
//
// Purpose:
//   Initialize the transmit control block queue.  Create a circularly
//   linked list of tcbs.

bool Intel82557::_initTcbQ( bool enable )
{
    int i;

    tcbQ.numFree = tcbQ.numTcbs = NUM_TRANSMIT_FRAMES;
    tcbQ.activeHead_p = tcbQ.activeTail_p = tcbQ.freeHead_p = tcbList_p;

    for (i = 0; i < tcbQ.numTcbs; i++) { /* free up buffers */
		if (tcbList_p[i]._mbuf) {
			freePacket(tcbList_p[i]._mbuf);
			tcbList_p[i]._mbuf = 0;
		}
    }
    bzero(tcbList_p, sizeof(tcb_t) * tcbQ.numTcbs);

	if (!enable)
		return true;

    for (i = 0; i < tcbQ.numTcbs; i++) {
		IOPhysicalAddress paddr;

        if (!getPhysAddress(&txRing, &tcbList_p[i], &tcbList_p[i]._paddr))
        {
            IOLog("i82557(tcbQ): Invalid TCB address\n");
            return false;
        }

        if (!getPhysAddress(&txRing, &tcbList_p[i]._tbds, &paddr))
        {
            IOLog("i82557(tcbQ): Invalid TBD address\n");
            return false;
        }
        WriteLE32(&tcbList_p[i].tbdAddr, paddr);
    
        if (i == (tcbQ.numTcbs - 1))
            tcbList_p[i]._next = &tcbList_p[0];
        else
            tcbList_p[i]._next = &tcbList_p[i + 1];
    }
    for (i = 0; i < tcbQ.numTcbs; i++) /* make physical links */
		WriteLE32(&tcbList_p[i].link, tcbList_p[i]._next->_paddr);
	
	return true;
}

//---------------------------------------------------------------------------
// Function: _setupRfd

static void _setupRfd(rfd_t * rfdList_p)
{
    for (int i = 0; i < NUM_RECEIVE_FRAMES; i++) {
		if (i == (NUM_RECEIVE_FRAMES - 1)) {
			/* mark tails and link the lists circularly */
			SetLE16(&rfdList_p[i].command, RFD_COMMAND_EL);
			rfdList_p[i]._next = &rfdList_p[0];
			SetLE32(&rfdList_p[i]._rbd.size, RBD_SIZE_EL);
			rfdList_p[i]._rbd._next = &rfdList_p[0]._rbd;
		}
		else {
			rfdList_p[i]._next = &rfdList_p[i + 1];
			rfdList_p[i]._rbd._next = &rfdList_p[i + 1]._rbd;
		}

		WriteLE32(&rfdList_p[i].link, rfdList_p[i]._next->_paddr);
		WriteLE32(&rfdList_p[i].rbdAddr,
                  (i == 0) ? rfdList_p[0]._rbd._paddr : C_NULL);
		
		WriteLE32(&rfdList_p[i]._rbd.link, rfdList_p[i]._rbd._next->_paddr);
		SetLE32(&rfdList_p[i]._rbd.size, CSR_FIELD(RBD_SIZE, MAX_BUF_SIZE));
	}
}

//---------------------------------------------------------------------------
// Function: _initRfdList
//
// Purpose:
//   Create a circularly linked list of receive frame descriptors, and 
//   populate them with receive buffers allocated from our special pool.

bool Intel82557::_initRfdList( bool enable )
{
    int  i;

    /* free allocated packet buffers */
	for (i = 0; i < NUM_RECEIVE_FRAMES; i++) {
		if (rfdList_p[i]._rbd._mbuf) {
			freePacket(rfdList_p[i]._rbd._mbuf);
		}
	}

    /* zero out the entire structure, and re-create it */
    bzero(rfdList_p, sizeof(rfd_t) * NUM_RECEIVE_FRAMES);

	if (!enable)
		return true;

	for (i = 0; i < NUM_RECEIVE_FRAMES; i++) {
        SetLE16(&rfdList_p[i].command, RFD_COMMAND_SF);
        
        if (!getPhysAddress(&rxRing, &rfdList_p[i], &rfdList_p[i]._paddr))
        {
            IOLog("%s: Invalid RFD address\n", getName());
            return false;
        }
        if (!getPhysAddress(&rxRing, &rfdList_p[i]._rbd,
                            &rfdList_p[i]._rbd._paddr))
        {
            IOLog("%s: Invalid RBD address\n", getName());
            return false;
        }
    }

	_setupRfd(rfdList_p);

    for (i = 0; i < NUM_RECEIVE_FRAMES; i++) {
		// Pre-load the receive ring with max size mbuf packets.
		//		
		mbuf_t m = allocatePacket(MAX_BUF_SIZE);
		if (!m)
			return false;

		if (updateRFDFromMbuf(&rfdList_p[i], m) == false) {
			IOLog("%s: updateRFDFromMbuf() error\n", getName());
			freePacket(m);
			return false;
		}
    }

    headRfd = rfdList_p;
    tailRfd = rfdList_p + NUM_RECEIVE_FRAMES - 1;

    return true;
}

//---------------------------------------------------------------------------
// Function: _resetRfdList
//
// Purpose:
//   Reset the RFD list before the receiver engine is restarted after
//   a resource shortage.

bool Intel82557::_resetRfdList()
{
    int                i;

	struct _cache {
		IOPhysicalAddress rbd_buffer;
		mbuf_t            rbd_mbuf;
		IOPhysicalAddress rfd_paddr;
		IOPhysicalAddress rbd_paddr;
	} * cache_p = (struct _cache *) KDB_buf_p;

	if ((sizeof(struct _cache) * NUM_RECEIVE_FRAMES) > kIOEthernetMaxPacketSize) {
		IOLog("%s: no space for cache data\n", getName());
		return false;
	}

    /* cache allocated packet buffers */
	for (i = 0; i < NUM_RECEIVE_FRAMES; i++) {
		cache_p[i].rbd_mbuf   = rfdList_p[i]._rbd._mbuf;
		cache_p[i].rbd_buffer = rfdList_p[i]._rbd.buffer;
		cache_p[i].rfd_paddr  = rfdList_p[i]._paddr;
		cache_p[i].rbd_paddr  = rfdList_p[i]._rbd._paddr;
	}

    /* zero out the entire structure, and re-create it */
    bzero(rfdList_p, sizeof(rfd_t) * NUM_RECEIVE_FRAMES);

	for (i = 0; i < NUM_RECEIVE_FRAMES; i++) {
		SetLE16(&rfdList_p[i].command, RFD_COMMAND_SF);
		rfdList_p[i]._paddr = cache_p[i].rfd_paddr;
		rfdList_p[i]._rbd._paddr = cache_p[i].rbd_paddr;
    }

	_setupRfd(rfdList_p);

    for (i = 0; i < NUM_RECEIVE_FRAMES; i++) {
		rfdList_p[i]._rbd.buffer = cache_p[i].rbd_buffer;
		rfdList_p[i]._rbd._mbuf  = cache_p[i].rbd_mbuf;
	}

    headRfd = rfdList_p;
    tailRfd = rfdList_p + NUM_RECEIVE_FRAMES - 1;

    return true;
}

//---------------------------------------------------------------------------
// Function: _mdiReadPHY:Register:Data
//
// Purpose:
//   Read the specified MDI register and return the results.

bool
Intel82557::_mdiReadPHY(UInt8 phyAddress, UInt8 regAddress, UInt16 * data_p)
{
    mdi_control_t mdi;

	mdi = CSR_FIELD(MDI_CONTROL_PHYADDR, phyAddress) |
	      CSR_FIELD(MDI_CONTROL_REGADDR, regAddress) |
		  CSR_FIELD(MDI_CONTROL_OPCODE,  MDI_CONTROL_OP_READ);
	
	WriteLE32(&CSR_p->mdiControl, mdi);
	IODelay(1);

	bool ready = false;
	for (int i = 0; (i < SPIN_TIMEOUT) && isCSRValid(); i++) {
		if (ReadLE32(&CSR_p->mdiControl) & MDI_CONTROL_READY) {
			ready = true;
            break;
		}
	    IODelay(1);
	}
	if (ready == false) {
	    VPRINT("%s: _mdiReadPHYRegisterSuccess timeout\n", getName());
	    return false;
	}

	*data_p = CSR_VALUE(MDI_CONTROL_DATA, ReadLE32(&CSR_p->mdiControl));
    return true;
}

//---------------------------------------------------------------------------
// Function: _mdiWritePHY:Register:Data
//
// Purpose:
//   Write the specified MDI register with the given data.

bool Intel82557::_mdiWritePHY(UInt8 phyAddress, UInt8 regAddress, UInt16 data)
{
    mdi_control_t mdi;

	mdi = CSR_FIELD(MDI_CONTROL_PHYADDR, phyAddress) |
	      CSR_FIELD(MDI_CONTROL_REGADDR, regAddress) |
		  CSR_FIELD(MDI_CONTROL_OPCODE,  MDI_CONTROL_OP_WRITE) |
		  CSR_FIELD(MDI_CONTROL_DATA,    data);

    WriteLE32(&CSR_p->mdiControl, mdi);
    IODelay(1);

	bool ready = false;
	for (int i = 0; (i < SPIN_TIMEOUT) && isCSRValid(); i++) {
		if (ReadLE32(&CSR_p->mdiControl) & MDI_CONTROL_READY) {
			ready = true;
			break;
		}
	    IODelay(1);
	}
	if (ready == false) {
	    VPRINT("%s: _mdiWritePHYRegisterData timeout\n", getName());
	    return false;
	}
    return true;
}

//---------------------------------------------------------------------------
// Function: nop
//
// Purpose:
//   Issue a polled NOP command to the NIC.

bool Intel82557::nop()
{
	cbHeader_t * nop_p = &overlay_p->nop;

    bzero(nop_p, sizeof(*nop_p));
	WriteLE16(&nop_p->command, CSR_FIELD(CB_CMD, CB_CMD_NOP) | CB_EL);
	WriteLE32(&nop_p->link, C_NULL);

    return _polledCommand(nop_p, overlay_paddr);
}

//---------------------------------------------------------------------------
// Function: config
//
// Purpose:
//   Issue a polled CONFIGURE command to the NIC.

bool Intel82557::config()
{
    UInt8 *	cb_p;    
	cb_configure_t * cfg_p = &overlay_p->configure;
	
    /*
     * Fill the configure command block
     */
    bzero(cfg_p, sizeof(*cfg_p));
	
	WriteLE16(&cfg_p->header.command, CSR_FIELD(CB_CMD, CB_CMD_CONFIGURE) |
              CB_EL);
	WriteLE32(&cfg_p->header.link, C_NULL);
	
	cb_p = cfg_p->byte;
	cb_p[0] = CSR_FIELD(CB_CB0_BYTE_COUNT, CB_CONFIG_BYTE_COUNT);
	
	cb_p[1] = CSR_FIELD(CB_CB1_TX_FIFO_LIMIT, CB_CB1_TX_FIFO_0) |
              CSR_FIELD(CB_CB1_RX_FIFO_LIMIT, CB_CB1_RX_FIFO_64);

	cb_p[3] = CB_CB3_MWI_ENABLE;	// enable PCI-MWI on 82558 devices
	
	cb_p[4] = 0;					// disable PCI transfer limits
	cb_p[5] = 0;
	
	cb_p[6] = CB_CB6_NON_DIRECT_DMA | CB_CB6_STD_TCB | CB_CB6_STD_STATS;

	cb_p[7] = CSR_FIELD(CB_CB7_UNDERRUN_RETRY, CB_CB7_UNDERRUN_RETRY_1) |
              CB_CB7_DISC_SHORT_FRAMES;

    if ((eeprom->getContents()->controllerType != I82558_CONTROLLER_TYPE) &&
		(phyAddr != PHY_ADDRESS_I82503))
		cb_p[8] = CB_CB8_CSMA_EN;

	cb_p[10] = CSR_FIELD(CB_CB10_PREAMBLE, CB_CB10_PREAMBLE_7_BYTES) |
			   CB_CB10_NSAI;

	cb_p[12] = CSR_FIELD(CB_CB12_IFS, CB_CB12_IFS_96_BIT_TIMES);

	cb_p[13] = CSR_FIELD(CB_CB13_FC_TYPE_LSB, CB_CB13_FC_TYPE_LSB_DEF);
	cb_p[14] = CSR_FIELD(CB_CB14_FC_TYPE_MSB, CB_CB14_FC_TYPE_MSB_DEF);

	cb_p[15] = ((cb_p[8] & CB_CB8_CSMA_EN) ? 0 : CB_CB15_CRS_CDT) |
                (promiscuousEnabled ? CB_CB15_PROMISCUOUS : 0);
	
	cb_p[16] = CSR_FIELD(CB_CB16_FC_DELAY_LSB, CB_CB16_FC_DELAY_LSB_DEF);
	cb_p[17] = CSR_FIELD(CB_CB17_FC_DELAY_MSB, CB_CB17_FC_DELAY_MSB_DEF);
	
	cb_p[18] = CB_CB18_PADDING;

#if 0	// XXX - need to fix this
    /* 
     * Force full duplex if there is a user override, or we are using Phy 0
     * and full duplex mode is enabled.  The FDX# pin is wired to Phy 1, 
     * which means that the 82557 can't autodetect the setting correctly.
     */
    if (forceFullDuplex || (phyAddr == PHY_ADDRESS_0 && fullDuplexMode))
		cb_p[19] = CB_CB19_FORCE_FDX;
#endif

	cb_p[19] = CB_CB19_AUTO_FDX;
	if (flowControl) {
		cb_p[19] |= ( CB_CB19_TX_FC         |
                      CB_CB19_RX_FC_RESTOP  |
				      CB_CB19_RX_FC_RESTART |
				      CB_CB19_REJECT_FC );
	}

	cb_p[20] = CSR_FIELD(CB_CB20_FC_ADDR_LSB, CB_CB20_FC_ADDR_LSB_DEF);

    return _polledCommand((cbHeader_t *) cfg_p, overlay_paddr);
}

//---------------------------------------------------------------------------
// Function: iaSetup
//
// Purpose:
//   Issue a polled IndividualAddressSETUP command to the NIC.
//
bool Intel82557::iaSetup()
{
	cb_iasetup_t * iaSetup_p = &overlay_p->iasetup;

    /*
     * Fill the IA-setup command block
     */
    bzero(iaSetup_p, sizeof(*iaSetup_p));

    WriteLE16(&iaSetup_p->header.command, CSR_FIELD(CB_CMD, CB_CMD_IASETUP) | 
                                          CB_EL);
	WriteLE32(&iaSetup_p->header.link, C_NULL);
    iaSetup_p->addr = myAddress;

    return _polledCommand((cbHeader_t *) iaSetup_p, overlay_paddr);
}

//---------------------------------------------------------------------------
// Function: mcSetup
//
// Purpose:
//   Issue a polled MultiCastSETUP command to the NIC. If 'fromData' is
//   true, then we ignore the addrs/count arguments and instead use the
//   multicast address list property in our interface client object.

bool Intel82557::mcSetup(IOEthernetAddress * addrs,
                         UInt32              count,
                         bool                update)
{
    bool                       cmdResult;
    cb_mcsetup_t *             mcSetup_ptr;
    IOPhysicalAddress          mcSetup_paddr;
    IOByteCount                mcSetup_len;
    IOBufferMemoryDescriptor * mcSetup_mem;

	if ( update )
    {
        //
        // mcSetup() was not called by the setMulticastList() function.
        // We should get the multicast list stored in the interface
        // object's property table.
        //
        // mcSetup() is always executed by the default workloop thread,
        // thus we don't have to worry about the address list being
        // changed while we go through it.
        //

        addrs = 0;
        count = 0;

        if (netif) {
            OSData * mcData = OSDynamicCast(OSData, 
                              netif->getProperty(kIOMulticastFilterData));
            if (mcData) {
                addrs = (IOEthernetAddress *) mcData->getBytesNoCopy();
                count = mcData->getLength() / sizeof(IOEthernetAddress);
                assert(addrs && count);
            }
        }
    }

    mcSetup_mem = IOBufferMemoryDescriptor::withOptions(
                                                kIOMemoryUnshared,
                                                PAGE_SIZE, PAGE_SIZE);
    if (!mcSetup_mem) {
		IOLog("%s: mcSetup memory allocation error\n", getName());
		return false;
    }

    mcSetup_ptr   = (cb_mcsetup_t *) mcSetup_mem->getBytesNoCopy();
    mcSetup_paddr = mcSetup_mem->getPhysicalSegment(0, &mcSetup_len);
    count         = min(count, (PAGE_SIZE - sizeof(cb_mcsetup_t)) /
                                sizeof(IOEthernetAddress));

	reserveDebuggerLock();

	do {
		cmdResult = false;

		WriteLE16(&mcSetup_ptr->header.status, 0);
		WriteLE16(&mcSetup_ptr->header.command,
                  CSR_FIELD(CB_CMD, CB_CMD_MCSETUP) | CB_EL);
		WriteLE32(&mcSetup_ptr->header.link, C_NULL);

		/* fill in the addresses (count may be zero) */
		for (UInt i = 0; i < count; i++)
			mcSetup_ptr->addrs[i] = addrs[i];

		/* Set the number of bytes in the MC list, if the count is zero,
		 * it is equivalent to disabling the multicast filtering mechanism.
		 */
		WriteLE16(&mcSetup_ptr->count, count * sizeof(IOEthernetAddress));

		if (!_polledCommand((cbHeader_t *) mcSetup_ptr, mcSetup_paddr)) {
			VPRINT("%s: MC-setup command failed 0x%x\n", getName(),
                   ReadLE16(&mcSetup_ptr->header.status));
			break;
		}

		cmdResult = (ReadLE16(&mcSetup_ptr->header.status) & CB_STATUS_OK) ?
                    true : false;
	} while (0);
	
	releaseDebuggerLock();

    mcSetup_mem->release();

    return cmdResult;
}

//---------------------------------------------------------------------------
// Function: _selfTest
//
// Purpose:
//   Issue a PORT self test command to the NIC and verify the results.

bool Intel82557::_selfTest()
{
    port_selftest_t * test_p = (port_selftest_t *) overlay_p;
	UInt32	results;

    WriteLE32(&test_p->signature, 0);
    WriteLE32(&test_p->results, ~0);
	sendPortCommand(portSelfTest_e, overlay_paddr);
 
    // Hardware needs to fetch the entire serial ROM contents which can
    // take some time.

    for (int i = 15; i != 0; i--)
    {
        IOSleep(10);
        results = ReadLE32(&test_p->results) & PORT_SELFTEST_MASK;
        if (0 == results) 
            break;
    }

    if (results) {		/* report errors from self test */
    	if (results & PORT_SELFTEST_ROM)
	    	IOLog("%s: Self test reports invalid ROM contents\n",
			getName());
    	if (results & PORT_SELFTEST_REGISTER)
			IOLog("%s: Self test reports internal register failure\n", 
			getName());
		if (results & PORT_SELFTEST_DIAGNOSE)
			IOLog("%s: Self test reports serial subsystem failure\n", 
			getName());
		if (results & PORT_SELFTEST_GENERAL)
			IOLog("%s: Self test failed\n", getName());
        return false;
	}
    
    // Device will perform a full internal reset after self-test is
    // complete, wait a bit before accessing hardware.

    IOSleep(10);    
    return true;
}

//---------------------------------------------------------------------------
// Function: sendPortCommand
//
// Purpose:
//   Issue an 82557 PORT command.
//
void Intel82557::sendPortCommand(port_command_t command, UInt arg)
{
	WriteLE32(&CSR_p->port, (arg & PORT_ADDRESS_MASK) |
                            CSR_FIELD(PORT_FUNCTION, command));
    return;
}

//---------------------------------------------------------------------------
// Function: enableAdapterInterrupts, disableAdapterInterrupts
//
// Purpose:
//   Turn on/off interrupts at the adapter.

void Intel82557::enableAdapterInterrupts()
{
	/*
	 * For 82558, mask (disable) the ER and FCP interrupts.
	 */
	UInt8	interruptByte;
	interruptEnabled = true;
	interruptByte = SCB_INTERRUPT_ER | SCB_INTERRUPT_FCP;
	WriteLE8(&CSR_p->interrupt, interruptByte);
}

void Intel82557::disableAdapterInterrupts()
{
	/*
	 * Write to memory followed by a read to defeat any PCI write-
	 * posting to mask all chip interrupt sources before clearing
	 * the interruptEnabled software flag.
	 */
	do {
		WriteLE8(&CSR_p->interrupt, SCB_INTERRUPT_M);
	} while ((ReadLE8(&CSR_p->interrupt) & SCB_INTERRUPT_M) == 0);
	interruptEnabled = false;
}

//---------------------------------------------------------------------------
// Function: _logCounters
//
// Purpose:
//   If Verbose is defined as yes, log extra information about errors that
//   have occurred.

static inline void
_logCounters(errorCounters_t * errorCounters_p)
{
    if (errorCounters_p->tx_good_frames)
		IOLog("tx_good_frames %ld\n",
			ReadLE32(&errorCounters_p->tx_good_frames));
    if (errorCounters_p->tx_maxcol_errors)
		IOLog("tx_maxcol_errors %ld\n",
			ReadLE32(&errorCounters_p->tx_maxcol_errors));
    if (errorCounters_p->tx_late_collision_errors)
		IOLog("tx_late_collision_errors %ld\n", 
			ReadLE32(&errorCounters_p->tx_late_collision_errors));
    if (errorCounters_p->tx_underrun_errors)
		IOLog("tx_underrun_errors %ld\n",
			ReadLE32(&errorCounters_p->tx_underrun_errors));
    if (errorCounters_p->tx_lost_carrier_sense_errors)
		IOLog("tx_lost_carrier_sense_errors %ld\n", 
			ReadLE32(&errorCounters_p->tx_lost_carrier_sense_errors));
    if (errorCounters_p->tx_deferred)
		IOLog("tx_deferred %ld\n", ReadLE32(&errorCounters_p->tx_deferred));
    if (errorCounters_p->tx_single_collisions)
		IOLog("tx_single_collisions %ld\n", 
			ReadLE32(&errorCounters_p->tx_single_collisions));
    if (errorCounters_p->tx_multiple_collisions)
		IOLog("tx_multiple_collisions %ld\n", 
			ReadLE32(&errorCounters_p->tx_multiple_collisions));
    if (errorCounters_p->tx_total_collisions)
		IOLog("tx_total_collisions %ld\n", 
			ReadLE32(&errorCounters_p->tx_total_collisions));
	if (errorCounters_p->rx_good_frames)
		IOLog("rx_good_frames %ld\n", 
			ReadLE32(&errorCounters_p->rx_good_frames));
	if (errorCounters_p->rx_crc_errors)
		IOLog("rx_crc_errors %ld\n",
			ReadLE32(&errorCounters_p->rx_crc_errors));
	if (errorCounters_p->rx_alignment_errors)
		IOLog("rx_alignment_errors %ld\n", 
			ReadLE32(&errorCounters_p->rx_alignment_errors));
	if (errorCounters_p->rx_resource_errors)
		IOLog("rx_resource_errors %ld\n",
			ReadLE32(&errorCounters_p->rx_resource_errors));
	if (errorCounters_p->rx_overrun_errors)
		IOLog("rx_overrun_errors %ld\n",
			ReadLE32(&errorCounters_p->rx_overrun_errors));
	if (errorCounters_p->rx_collision_detect_errors)
		IOLog("rx_collision_detect_errors %ld\n", 
			ReadLE32(&errorCounters_p->rx_collision_detect_errors));
	if (errorCounters_p->rx_short_frame_errors)
		IOLog("rx_short_frame_errors %ld\n", 
			ReadLE32(&errorCounters_p->rx_short_frame_errors));
    return;
}

//---------------------------------------------------------------------------
// Function: _dumpStatistics
//
// Purpose:
//   _dumpStatistics issues a new statistics dump command.  Every few seconds,
//   _updateStatistics is called from timeoutOccurred to check for updated
//   statistics.  If complete, update our counters, and issue a new dump
//   command.

bool Intel82557::_dumpStatistics()
{
	reserveDebuggerLock();

    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: _dumpStatistics: _waitSCBCommandClear failed\n", getName());
		return false;
    }

	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_CUC, SCB_CUC_DUMP_RESET_STAT));
	ReadLE16(&CSR_p->status);
	prevCUCommand = SCB_CUC_DUMP_RESET_STAT;

	releaseDebuggerLock();

    return true;
}

//---------------------------------------------------------------------------
// Function: _updateStatistics
//
// Purpose:
//   Gather statistics information from the adapter at regular intervals.

void Intel82557::_updateStatistics()
{
    if (ReadLE32(&errorCounters_p->_status) != DUMP_STATUS) {
		if (verbose)
			_logCounters(errorCounters_p);

		// Ethernet transmitter stats.
		//
		etherStats->dot3StatsEntry.singleCollisionFrames += 
			ReadLE32(&errorCounters_p->tx_single_collisions);
		
		etherStats->dot3StatsEntry.multipleCollisionFrames += 
			ReadLE32(&errorCounters_p->tx_multiple_collisions);
		
		etherStats->dot3StatsEntry.lateCollisions += 
			ReadLE32(&errorCounters_p->tx_late_collision_errors);
		
		etherStats->dot3StatsEntry.excessiveCollisions += 
			ReadLE32(&errorCounters_p->tx_maxcol_errors);

		etherStats->dot3StatsEntry.deferredTransmissions += 
			ReadLE32(&errorCounters_p->tx_deferred);

		etherStats->dot3StatsEntry.carrierSenseErrors += 
			ReadLE32(&errorCounters_p->tx_lost_carrier_sense_errors);

		etherStats->dot3TxExtraEntry.underruns += 
			ReadLE32(&errorCounters_p->tx_underrun_errors);

		// Ethernet receiver stats.
		//
		etherStats->dot3StatsEntry.alignmentErrors += 
			ReadLE32(&errorCounters_p->rx_alignment_errors);

		etherStats->dot3StatsEntry.fcsErrors += 
			ReadLE32(&errorCounters_p->rx_crc_errors);

		etherStats->dot3RxExtraEntry.resourceErrors += 
			ReadLE32(&errorCounters_p->rx_resource_errors);

		etherStats->dot3RxExtraEntry.overruns += 
			ReadLE32(&errorCounters_p->rx_overrun_errors);

		etherStats->dot3RxExtraEntry.collisionErrors += 
			ReadLE32(&errorCounters_p->rx_collision_detect_errors);			

		etherStats->dot3RxExtraEntry.frameTooShorts += 
			ReadLE32(&errorCounters_p->rx_short_frame_errors);			
		
		// Generic network stats. For the error counters, we assume
		// the Ethernet stats will never be cleared. Thus we derive the
		// error counters by summing the appropriate Ethernet error fields.
		//
		netStats->outputErrors =
			( etherStats->dot3StatsEntry.lateCollisions
			+ etherStats->dot3StatsEntry.excessiveCollisions
			+ etherStats->dot3StatsEntry.carrierSenseErrors
			+ etherStats->dot3TxExtraEntry.underruns
			+ etherStats->dot3TxExtraEntry.resourceErrors);

		netStats->inputErrors =
			( etherStats->dot3StatsEntry.fcsErrors
			+ etherStats->dot3StatsEntry.alignmentErrors
			+ etherStats->dot3RxExtraEntry.resourceErrors
			+ etherStats->dot3RxExtraEntry.overruns
			+ etherStats->dot3RxExtraEntry.collisionErrors
			+ etherStats->dot3RxExtraEntry.frameTooShorts);

		netStats->collisions += 
			ReadLE32(&errorCounters_p->tx_total_collisions);

		WriteLE32(&errorCounters_p->_status, DUMP_STATUS);
		_dumpStatistics();
    }
}

//---------------------------------------------------------------------------
// Function: allocatePageBlock
//
// Purpose:
//   Allocate a page of memory.

bool Intel82557::allocatePageBlock( pageBlock_t * p )
{
    UInt32 size = PAGE_SIZE;

    p->memory = IOBufferMemoryDescriptor::withOptions(kIOMemoryUnshared,
                                                      size, PAGE_SIZE);

    if (p->memory == 0) return false;

    if (p->memory->prepare() != kIOReturnSuccess)
    {
        p->memory->release();
        p->memory = 0;
        return false;
    }

	p->freeStart = p->memory->getBytesNoCopy();
	p->freeBytes = p->memory->getCapacity();
	bzero(p->freeStart, p->freeBytes);

	return true;
}

//---------------------------------------------------------------------------
// Function: freePageBlock
//
// Purpose:
//   Deallocate a page of memory.
//
void Intel82557::freePageBlock( pageBlock_t * p )
{
    if ( p->memory )
    {
        p->memory->complete();
        p->memory->release();
        p->memory = 0;
    }
}

//---------------------------------------------------------------------------
// Function: allocateMemoryFrom
//
// Purpose:
//   Allocate the next aligned chunk of memory in a page block.

void * Intel82557::allocateMemoryFrom( pageBlock_t *       p,
                                       UInt32              size,
                                       UInt32              align,
                                       IOPhysicalAddress * paddr )
{
    void *      allocPtr;
    IOByteCount segLength;

	if (align == 0)
		return 0;

	// Locate next alignment boundary.

	allocPtr = (void *)(((UInt32)p->freeStart + (align - 1)) & ~(align - 1));

	// Add alignment padding to the allocation size.

    size += (UInt32) allocPtr - (UInt32) p->freeStart;

    if (size > p->freeBytes)
		return 0;

    p->freeStart  = (void *)((UInt32) p->freeStart + size);
    p->freeBytes -= size;

    if ( paddr )
    {
        *paddr = p->memory->getPhysicalSegment(
                 (UInt32) allocPtr - (UInt32) p->memory->getBytesNoCopy(),
                 &segLength);
    }

    return allocPtr;
}

//---------------------------------------------------------------------------
// Function: getPhysAddress
//
// Purpose:
//   Get the physical address for a location described by a pageBlock.
//
bool Intel82557::getPhysAddress( pageBlock_t *       pageBlock,
                                 void *              virtAddress,
                                 IOPhysicalAddress * physAddress )
{
    IOPhysicalAddress paddr = 0;
    IOByteCount       segLength;

    if ( pageBlock && pageBlock->memory )
    {
        paddr = pageBlock->memory->getPhysicalSegment(
                    (UInt32) virtAddress -
                    (UInt32) pageBlock->memory->getBytesNoCopy(),
                    &segLength);
    }

    *physAddress = paddr;

    return (paddr != 0);
}

//---------------------------------------------------------------------------
// Function: hwInit
//
// Purpose:
//   Reset/configure the chip, detect the PHY.

bool Intel82557::hwInit( bool resetOnly )
{
	disableAdapterInterrupts();
	_resetChip();
	disableAdapterInterrupts();

    /* Command unit is idle after a chip reset */
    cuIsIdle = true;

    /* If only a chip reset is requested, then we're done. */
    if ( resetOnly )
    {
        return true;
    }

	/* disable early RX interrupt */
	WriteLE8(&CSR_p->earlyRxInterrupt, 0);

    /* load command unit base address */
    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: hwInit: CU _waitSCBCommandClear failed\n", getName());
		return false;
    }
    WriteLE32(&CSR_p->pointer, 0);	
	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_CUC, SCB_CUC_LOAD_BASE));
	ReadLE16(&CSR_p->status);
	prevCUCommand = SCB_CUC_LOAD_BASE;

    /* load receive unit base address */
    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: hwInit: RU _waitSCBCommandClear failed\n", getName());
		return false;
    }
    WriteLE32(&CSR_p->pointer, 0);	
	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_RUC, SCB_RUC_LOAD_BASE));
	ReadLE16(&CSR_p->status);

    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: hwInit: LOAD_DUMP_ADDR _waitSCBCommandClear failed\n",
               getName());
		return false;
    }
    WriteLE32(&errorCounters_p->_status, DUMP_STATUS);
	WriteLE32(&CSR_p->pointer, errorCounters_paddr);	
	WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_CUC, SCB_CUC_LOAD_DUMP_ADDR));
	ReadLE16(&CSR_p->status);
	prevCUCommand = SCB_CUC_LOAD_DUMP_ADDR;

    if (!_waitSCBCommandClear(CSR_p)) {
		VPRINT("%s: hwInit: intrACK _waitSCBCommandClear failed\n", getName());
		return false;
    }

	/* Setup flow-control threshold */
	WriteLE8(&CSR_p->flowControlThreshold, CSR_FIELD(FC_THRESHOLD, FC_THRESHOLD_512));

	_intrACK(CSR_p); /* ack any pending interrupts */

	_phyProbe();

	phyID = _phyGetID();
    VPRINT("%s: PHY model id is 0x%08lx\n", getName(), phyID);
    phyID &= PHY_MODEL_MASK;

    if (!config())
		return false;
    IOSleep(500);

    if (!iaSetup())
		return false;

	_intrACK(CSR_p); /* ack any pending interrupts */

    return true;
}

//---------------------------------------------------------------------------
// Function: coldInit
//
// Purpose:
//   One-time initialization code. This is called by start(), before we
//   attach any client objects.

bool Intel82557::coldInit()
{
	IOPhysicalAddress paddr;

    hwInit(true);

    /* allocate and initialize shared memory pointers */
	if (!allocatePageBlock(&shared)) {
		IOLog("%s: Can't allocate shared memory page\n", getName());
		goto failed;
    }
	if (!allocatePageBlock(&txRing)) {
		IOLog("%s: Can't allocate memory page for TX ring\n", getName());
		goto failed;
    }
	if (!allocatePageBlock(&rxRing)) {
		IOLog("%s: Can't allocate memory page for RX ring\n", getName());
		goto failed;
    }

    /* allocate memory for shared data structures
	 * self test needs to be
	 * 16 byte aligned
	 */	 
    overlay_p = (overlay_t *) allocateMemoryFrom( &shared,
                                                  sizeof(overlay_t), 
                                                  PARAGRAPH_ALIGNMENT,
                                                  &overlay_paddr );
    if (!overlay_p || !overlay_paddr)
        goto failed;

    tcbList_p = (tcb_t *) allocateMemoryFrom( &txRing, sizeof(tcb_t) *
                                              NUM_TRANSMIT_FRAMES,
                                              CACHE_ALIGNMENT );
	if (!tcbList_p)
        goto failed;

    KDB_tcb_p = (tcb_t *) allocateMemoryFrom( &shared, sizeof(tcb_t),
                                              CACHE_ALIGNMENT,
                                              &paddr );
    if (!KDB_tcb_p || !paddr)
        goto failed;
    KDB_tcb_p->_paddr = paddr;

    if (!getPhysAddress(&shared, &KDB_tcb_p->_tbds, &paddr))
        goto failed;
	WriteLE32(&KDB_tcb_p->tbdAddr, paddr);

    KDB_buf_p = allocateMemoryFrom( &shared, kIOEthernetMaxPacketSize,
                                    DWORD_ALIGNMENT, &KDB_buf_paddr );
	if (!KDB_buf_p || !KDB_buf_paddr)
        goto failed;

    errorCounters_p = (errorCounters_t *) allocateMemoryFrom(
                                          &shared, 
                                          sizeof(errorCounters_t),
                                          DWORD_ALIGNMENT,
                                          &errorCounters_paddr );

	if (!errorCounters_p || !errorCounters_paddr)
        goto failed;

    rfdList_p = (rfd_t *) allocateMemoryFrom( &rxRing, sizeof(rfd_t) *
                                              NUM_RECEIVE_FRAMES,
                                              CACHE_ALIGNMENT );
	if (!rfdList_p)
        goto failed;

    if (!_selfTest())
        goto failed;

    myAddress = eeprom->getContents()->addr;

    return true;

failed:
    return false;
}

//---------------------------------------------------------------------------
// Function: receiveInterruptOccurred
//
// Purpose:
//   Hand up rceived frames.

bool Intel82557::receiveInterruptOccurred()
{
	bool packetsQueued = false;

    while (ReadLE16(&headRfd->status) & RFD_STATUS_C) {
		rbd_count_t rbd_count = ReadLE32(&headRfd->_rbd.count);

		// rxCount does NOT include the Ethernet CRC (FCS).
		//
		UInt32 rxCount = CSR_VALUE(RBD_COUNT, rbd_count);

#if 0
		// When the receive unit runs out of resources, it will
		// skip over RFD/RBD, making them as complete, but the RBD will
		// have zero bytes and the EOF bit will not be set.
		// We just skip over those and allow them to be recycled.
		//
		// In those cases, the RFD->status word will be 0x8220.

		/* should have exactly 1 rbd per rfd */
		if (!(rbd_count & RBD_COUNT_EOF)) {
	    	IOLog("%s: more than 1 rbd, frame size %d\n", getName(), rxCount);
			
			IOLog("%s: RFD status: %04x\n", getName(), 
				ReadLE16(&headRfd->status));
			
			resetAdapter();
			return;
		}
#endif

		if ((!(ReadLE16(&headRfd->status) & RFD_STATUS_OK)) ||
			(rxCount < (kIOEthernetMinPacketSize - kIOEthernetCRCSize)) ||
			!enabledForNetif) {
			; /* bad or unwanted packet */
		}
		else {
	    	mbuf_t m = headRfd->_rbd._mbuf;
			mbuf_t m_in = 0;	// packet to pass up to inputPacket()
			bool replaced;

			packetsReceived = true;

			m_in = replaceOrCopyPacket(&m, rxCount, &replaced);
			if (!m_in) {
				etherStats->dot3RxExtraEntry.resourceErrors++;
				goto RX_INTR_ABORT;
			}

			if (replaced && (updateRFDFromMbuf(headRfd, m) == false)) {
				freePacket(m);	// free the new replacement mbuf.
				m_in = 0;		// pass up nothing.
				etherStats->dot3RxExtraEntry.resourceErrors++;
				IOLog("%s: updateRFDFromMbuf() error\n", getName());
				goto RX_INTR_ABORT;
			}

            netif->inputPacket(m_in, rxCount,
                               IONetworkInterface::kInputOptionQueuePacket);

			packetsQueued = true;
			netStats->inputPackets++;
	    }

RX_INTR_ABORT:
		/* clear fields in rfd */
		WriteNoSyncLE16(&headRfd->status, 0);
		WriteNoSyncLE16(&headRfd->command, (RFD_COMMAND_SF | RFD_COMMAND_EL));

        WriteNoSyncLE32(&headRfd->rbdAddr, C_NULL);
		WriteNoSyncLE32(&headRfd->misc, 0);

		/* clear fields in rbd */
		WriteNoSyncLE32(&headRfd->_rbd.count, 0);
		WriteLE32(&headRfd->_rbd.size,
                  CSR_FIELD(RBD_SIZE, MAX_BUF_SIZE) | RBD_SIZE_EL);

		/* adjust tail markers */
		WriteNoSyncLE32(&tailRfd->_rbd.size, CSR_FIELD(RBD_SIZE, MAX_BUF_SIZE));
		WriteLE16(&tailRfd->command, RFD_COMMAND_SF);

		tailRfd = headRfd;			// new tail
		headRfd = headRfd->_next;	// new head
    } /* while */

    return packetsQueued;
}

//---------------------------------------------------------------------------
// Function: transmitInterruptOccurred
//
// Purpose:
//   Free up packets associated with any completed TCB's.

void Intel82557::transmitInterruptOccurred()
{
	tcbQ_t * tcbQ_p = &tcbQ;
	tcb_t *  head;

	head = tcbQ_p->activeHead_p;
    while (tcbQ_p->numFree < tcbQ_p->numTcbs &&
          (ReadLE16(&head->status) & TCB_STATUS_C))
	{
		WriteNoSyncLE16(&head->status, 0);
		if (head->_mbuf) {
	    	freePacket(head->_mbuf);
	    	head->_mbuf = 0;
		}
		if (head->_interruptFlag && txPendingInterruptCount)
			txPendingInterruptCount--;
		head = tcbQ_p->activeHead_p = head->_next;
		tcbQ_p->numFree++;
    }
}

//---------------------------------------------------------------------------
// Function: interruptOccurred
//
// Purpose:
//   Field an interrupt.

void Intel82557::interruptOccurred(IOInterruptEventSource * src)
{
	scb_status_t  status;
	bool          flushInputQ = false;
	bool          doService   = false;

	if (interruptEnabled == false)
    {
		// Even with an interrupt filter frontend, it is still necessary
		// to check interruptEnabled flag since this driver never disables
		// its interrupt event source (may be shared). And the interface
		// may be disabled between the interrupt filter and this handler.
		return;
	}

	reserveDebuggerLock();

	/*
	 * Loop until the interrupt line becomes deasserted.
	 */
	while (1) {
		if ((status = _intrACK(CSR_p)) == 0)
			break;

		/*
		 * RX interrupt.
		 */
		if (status & (SCB_STATUS_FR | SCB_STATUS_RNR)) {
		
			flushInputQ = receiveInterruptOccurred() || flushInputQ;

			etherStats->dot3RxExtraEntry.interrupts++;

			if (status & SCB_STATUS_RNR) {
				etherStats->dot3RxExtraEntry.resets++;

				_abortReceive();
				_resetRfdList();

				if (!_startReceive()) {
					VPRINT("%s: Unable to restart receiver\n", getName());
					// resetAdapter();	/* shouldn't need to do this. */
				}
			}
		}

		/*
		 * TX interrupt.
		 */
		if (status & (SCB_STATUS_CX | SCB_STATUS_CNA)) {
			transmitInterruptOccurred();
			etherStats->dot3TxExtraEntry.interrupts++;
			doService = true;
		}
	}

	releaseDebuggerLock();

	if (enabledForNetif) {
		// Flush all packets received and pass them to the network stack.
		//
		if (flushInputQ)
			netif->flushInputQueue();

		// Call service() without holding the debugger lock to prevent a
		// deadlock when service() calls our outputPacket() function.
		//
		if (doService)
			transmitQueue->service();
	}
}

void Intel82557::interruptHandler( OSObject *               target,
                                   IOInterruptEventSource * src,
                                   int                      count )
{
	// C++ glue to eliminate compiler warnings
	((Intel82557 *) target)->interruptOccurred( src );
}

bool Intel82557::interruptFilter( OSObject * target,
                                  IOFilterInterruptEventSource * src )
{
	Intel82557 * me = (Intel82557 *) target;

	if (me->interruptEnabled &&
		(me->CSR_p->status & OSSwapHostToLittleInt16(SCB_STATUS_INT_MASK)))
		return true;
	else
		return false;
}

//---------------------------------------------------------------------------
// Function: updateTCBForMbuf
//
// Update the TxCB pointed by tcb_p to point to the mbuf chain 'm'.
// Returns the mbuf encoded onto the TxCB.

mbuf_t Intel82557::updateTCBForMbuf(tcb_t * tcb_p, mbuf_t m)
{
	// Set the invariant TCB fields.
	//
	WriteNoSyncLE16(&tcb_p->status, 0);

    if (++txCount == TRANSMIT_INT_DELAY) {
		WriteNoSyncLE16(&tcb_p->command, CSR_FIELD(TCB_COMMAND, CB_CMD_TRANSMIT) |
                        TCB_COMMAND_S  | TCB_COMMAND_SF | TCB_COMMAND_I);
		txCount = 0;
		tcb_p->_interruptFlag = 1;
	}
	else {
		WriteNoSyncLE16(&tcb_p->command, CSR_FIELD(TCB_COMMAND, CB_CMD_TRANSMIT) |
                        TCB_COMMAND_S | TCB_COMMAND_SF);
		tcb_p->_interruptFlag = 0;
	}

	WriteNoSyncLE8(&tcb_p->threshold, txThreshold8);
	WriteNoSyncLE16(&tcb_p->count, 0);	// all data are in the TBD's, none in TxCB

	// Since the format of a TBD closely matches the structure of an
	// 'struct IOPhysicalSegment', we shall have the cursor update the TBD list
	// directly.
	//
	UInt segments = txMbufCursor->getPhysicalSegmentsWithCoalesce(m,
                    (struct IOPhysicalSegment *) &tcb_p->_tbds[0],
                    TBDS_PER_TCB);

	if (!segments) {
		VPRINT("%s: getPhysicalSegments error, pkt len = %d\n", 
               getName(), mbuf_pkthdr_len(m));
		return 0;
	}

	// Update the TBD array size count.
	//
	WriteLE8(&tcb_p->number, segments);

	return m;
}

//---------------------------------------------------------------------------
// Function: outputPacket <IONetworkController>
//
// Purpose:
//   Transmit the packet handed by our IOOutputQueue.
//   TCBs have the suspend bit set, so that the CU goes into the suspend
//   state when done.  We use the CU_RESUME optimization that allows us to
//   issue CU_RESUMES without waiting for SCB command to clear.
//
UInt32 Intel82557::outputPacket(mbuf_t m, void * param)
{
	tcb_t * tcb_p;
	
    if (!enabledForNetif) {		// drop the packet.
		freePacket(m);
		return kIOReturnOutputDropped;
	}

	reserveDebuggerLock();

	if (tcbQ.numFree == 0) {	// retry when more space is available.
		releaseDebuggerLock();
		return kIOReturnOutputStall;
	}

    packetsTransmitted = true;	
	netStats->outputPackets++;

    tcb_p = tcbQ.freeHead_p;

	tcb_p->_mbuf = updateTCBForMbuf(tcb_p, m);
	if (tcb_p->_mbuf == 0) {
		etherStats->dot3TxExtraEntry.resourceErrors++;
		goto fail;
	}

    /* update the queue */
    tcbQ.numFree--;
    tcbQ.freeHead_p = tcbQ.freeHead_p->_next;

	/* The TCB is already setup and the suspend bit set. Now clear the
	 * suspend bit of the previous TCB.
	 */
    if (tcbQ.activeTail_p != tcb_p)
		ClearLE16(&tcbQ.activeTail_p->command, TCB_COMMAND_S);
    tcbQ.activeTail_p = tcb_p;

    /*
     * CUC_RESUME is optimized such that it is unnecessary to wait
     * for the CU to clear the SCB command word if the previous command
     * was a resume and the CU state is not idle.
     */
	if ( cuIsIdle ) 
	{
		if (!_waitSCBCommandClear(CSR_p)) {
	    	VPRINT("%s: outputPacket: _waitSCBCommandClear error\n", getName());
			etherStats->dot3TxExtraEntry.timeouts++;
	    	goto fail;
		}
		WriteLE32(&CSR_p->pointer, tcb_p->_paddr);
		WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_CUC, SCB_CUC_START));
		prevCUCommand = SCB_CUC_START;
        cuIsIdle = false;
    }
    else {
		if (prevCUCommand != SCB_CUC_RESUME) {
	    	if (!_waitSCBCommandClear(CSR_p)) {
				VPRINT("%s: outputPacket: _waitSCBCommandClear error\n", getName());
				etherStats->dot3TxExtraEntry.timeouts++;
				goto fail;
	    	}
		}
		WriteLE8(&CSR_p->command, CSR_FIELD(SCB_COMMAND_CUC, SCB_CUC_RESUME));
		prevCUCommand = SCB_CUC_RESUME;
    }

	if (tcb_p->_interruptFlag)
		txPendingInterruptCount++;

	releaseDebuggerLock();
    return kIOReturnOutputSuccess;

fail:
    freePacket(m);
    tcb_p->_mbuf = 0;
	releaseDebuggerLock();
    return kIOReturnOutputDropped;
}

//---------------------------------------------------------------------------
// Function: _receivePacket
//
// Purpose:
//   Part of kerneldebugger protocol.
//   Returns true if a packet was received successfully.
//
bool Intel82557::_receivePacket(void * pkt, UInt * len, UInt timeout)
{
	bool    processPacket = true;
	bool    ret           = false;
	UInt32  cmdstatus;

    timeout *= 1000;

    while ((ReadLE16(&headRfd->status) & RFD_STATUS_C) == 0) {
		if ((int) timeout <= 0) {
			processPacket = false;
			break;
		}
		IODelay(50);
		timeout -= 50;
    }

	if (processPacket) {
		if ((ReadLE16(&headRfd->status) & RFD_STATUS_OK) &&
		    (ReadLE32(&headRfd->_rbd.count) & RBD_COUNT_EOF))
	    {
			// Pass up good frames.
			//
			*len = CSR_VALUE(RBD_COUNT, ReadLE32(&headRfd->_rbd.count));
			*len = min(*len, kIOEthernetMaxPacketSize - kIOEthernetCRCSize);
			bcopy(mbuf_data(headRfd->_rbd._mbuf), pkt, *len);
			ret = true;
		}

		/* the head becomes the new tail */
		/* clear fields in rfd */
		WriteNoSyncLE16(&headRfd->status, 0);
		WriteNoSyncLE16(&headRfd->command, (RFD_COMMAND_SF | RFD_COMMAND_EL));
		WriteNoSyncLE32(&headRfd->rbdAddr, C_NULL);
		WriteNoSyncLE32(&headRfd->misc, 0);
	
		/* clear fields in rbd */
		WriteNoSyncLE32(&headRfd->_rbd.count, 0);
		WriteLE32(&headRfd->_rbd.size, CSR_FIELD(RBD_SIZE, MAX_BUF_SIZE) | 
                                       RBD_SIZE_EL);

		/* adjust tail markers */
		WriteNoSyncLE32(&tailRfd->_rbd.size, CSR_FIELD(RBD_SIZE, MAX_BUF_SIZE));
		WriteLE16(&tailRfd->command, RFD_COMMAND_SF);

		tailRfd = headRfd;			// new tail
		headRfd = headRfd->_next;	// new head
	}

    cmdstatus = ReadLE32(&CSR_p->status);

	if ( (cmdstatus != C_CSR_BAD) && (cmdstatus & SCB_STATUS_RNR) )
    {
		WriteLE16(&CSR_p->status, SCB_STATUS_RNR);	// ack RNR interrupt

#if DEBUG_KDP
		IOLog("%s::%s RUS:0x%x Index:%d\n", getName(), __FUNCTION__, 
			  CSR_VALUE(SCB_STATUS_RUS, ReadLE16(&CSR_p->status)),
              tailRfd - rfdList_p);
#endif

		_abortReceive();

#if DEBUG_KDP	// Display RFD/RBD fields
		for (int i = 0; i < NUM_RECEIVE_FRAMES; i++) {
			IOLog("   %02d: %04x %04x - %08x %08x\n", i,
				ReadLE16(&rfdList_p[i].command),
				ReadLE16(&rfdList_p[i].status),
				ReadLE32(&rfdList_p[i]._rbd.size),
				ReadLE32(&rfdList_p[i].misc));
		}
#endif

		_resetRfdList();
		_startReceive();
    }

    return ret;
}

//---------------------------------------------------------------------------
// Function: _sendPacket
//
// Purpose:
//   Part of kerneldebugger protocol.
//   Returns true if the packet was sent successfully.

bool Intel82557::_sendPacket(void * pkt, UInt len)
{
	tbd_t * tbd_p;

    // Set up the TCB and issue the command
	//
    WriteNoSyncLE16(&KDB_tcb_p->status, 0);
	WriteNoSyncLE32(&KDB_tcb_p->link, C_NULL);
	WriteNoSyncLE8( &KDB_tcb_p->threshold, txThreshold8);
	WriteNoSyncLE16(&KDB_tcb_p->command, CSR_FIELD(TCB_COMMAND, CB_CMD_TRANSMIT) |
                    TCB_COMMAND_EL | TCB_COMMAND_SF );
	WriteNoSyncLE16(&KDB_tcb_p->count, 0);	// all data are in the TBD's.
	WriteNoSyncLE8( &KDB_tcb_p->number, 1);	// 1 TBD only.

	// Copy the debugger packet to the pre-allocated buffer area.
	//
    len = min(len, kIOEthernetMaxPacketSize - kIOEthernetCRCSize);
    len = max(len, kIOEthernetMinPacketSize);
    bcopy(pkt, KDB_buf_p, len);

	// Update the TBD.
	//
	tbd_p = &KDB_tcb_p->_tbds[0];
	WriteNoSyncLE32(&tbd_p->addr, KDB_buf_paddr);
	WriteNoSyncLE32(&tbd_p->size, CSR_FIELD(TBD_SIZE, len));

	// Start up the command unit to send the packet.
	//
	return _polledCommand((cbHeader_t *) KDB_tcb_p, KDB_tcb_p->_paddr);
}
