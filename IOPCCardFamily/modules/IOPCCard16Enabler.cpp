/*======================================================================

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Contributor:  Apple Computer, Inc.  Portions © 2003 Apple Computer, 
    Inc. All rights reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
    Portions of this code are derived from:

	dummy_cs.c 1.28 2000/10/04 00:31:08

======================================================================*/


#include <IOKit/pccard/IOPCCard.h>

#ifdef PCMCIA_DEBUG
extern int ds_debug;
MODULE_PARM(ds_debug, "i");
#define DEBUG(n, args...) if (ds_debug>(n)) printk(KERN_DEBUG args)
#else
#define DEBUG(n, args...)
#endif

#define CardServices	device->cardServices

#define cs_error(handle, func, ret)			\
{							\
    error_info_t err = { (func), (ret) };		\
    CardServices(ReportError, (handle), &err);		\
}

#undef  super
#define super OSObject

OSDefineMetaClassAndStructors(IOPCCard16Enabler, OSObject)

OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  0);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  1);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  2);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  3);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  4);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  5);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  6);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  7);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  8);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler,  9);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 10);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 11);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 12);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 13);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 14);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 15);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 16);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 17);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 18);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 19);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 20);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 21);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 22);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 23);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 24);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 25);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 26);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 27);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 28);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 29);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 30);
OSMetaClassDefineReservedUnused(IOPCCard16Enabler, 31);

bool
IOPCCard16Enabler::init(IOPCCard16Device *provider)
{
    device = OSDynamicCast(IOPCCard16Device, provider);
    if (!device) return false;

    return super::init();
}

IOPCCard16Enabler *
IOPCCard16Enabler::withDevice(IOPCCard16Device *provider)
{
    IOPCCard16Enabler *me = new IOPCCard16Enabler;

    if (me && !me->init(provider)) {
        me->free();
        return 0;
    }

    return me;
}

bool
IOPCCard16Enabler::attach(IOPCCard16Device *provider)
{
    device = OSDynamicCast(IOPCCard16Device, provider);
    if (!device) return false;

    device->retain();

    handle = device->getCardServicesHandle();
    state = DEV_PRESENT;

    return true;
}

bool
IOPCCard16Enabler::detach(IOPCCard16Device *provider)
{
    DEBUG(0, "IOPCCard16Enabler::detach\n");

    bool success = unconfigure();

    device->release();

    return success;
}

void
IOPCCard16Enabler::free(void)
{
    for (unsigned i=0; i < tableEntryCount; i++) {
	kfree(configTable[i]);
    }

    super::free();
}

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************

int
IOPCCard16Enabler::eventHandler(cs_event_t event, int priority,
			        event_callback_args_t *args)
{
    DEBUG(1, "IOPCCard16Enabler::eventHandler(0x%06x, %d, 0x%p, 0x%p)\n",
	  event, priority, args->client_handle, args->client_data);

    return 0;
}

IOReturn
IOPCCard16Enabler::setPowerState(unsigned long powerState,
				 IOService * whatDevice)
{
    DEBUG(1, "IOPCCard16Enabler::setPowerState state=%d\n", powerState);
    
    if ((powerState == kIOPCCard16DeviceOnState) || (powerState == kIOPCCard16DeviceDozeState)) {
	if (!(state & DEV_SUSPEND)) return IOPMAckImplied;
	state &= ~DEV_SUSPEND;

    } else if (powerState == kIOPCCard16DeviceOffState) {
	if (state & DEV_SUSPEND) return IOPMAckImplied;
	state |= DEV_SUSPEND;
    }

    return IOPMAckImplied;
}

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn),args))!=0) goto cs_failed

#define CFG_CHECK(fn, args...) \
if (CardServices(fn, args) != 0) goto next_entry


bool
IOPCCard16Enabler::configure(UInt32 index /* = 0 */)
{
    bool success, success2;
    IOPCCardBridge * bridge;
    IODeviceMemory * ioMemory;

    DEBUG(0, "IOPCCard16Enabler::configure(0x%x)\n", index);

    OSArray * array = OSArray::withCapacity(1);
    if (success = !array) goto exit;

    /* Configure card */
    state |= DEV_CONFIG_PENDING;

    success = getConfigurations();
    if (!success) {
	state &= ~DEV_CONFIG_PENDING;
	goto exit;
    }

    if (index) {
	success = tryConfiguration(index);
    } else {
	sortConfigurations();

	for (UInt32 i=0; i < tableEntryCount; i++) {
	    success = tryConfiguration(configTable[i]->index);
	    if (success) break;
	}
    }
    if (!success) {
	state &= ~DEV_CONFIG_PENDING;
	goto exit;
    }
    
    // go for it
    state |= DEV_CONFIG;
    bridge = (IOPCCardBridge *)device->getProvider();

    success = bridge->configureSocket((IOService *)device, &configuration) == 0;
    state &= ~DEV_CONFIG_PENDING;
    if (!success) {
	state &= ~DEV_CONFIG;
	goto exit;
    }

    /* Finally, report what we've done */
    printk(KERN_INFO "IOPCCard16Enabler::configure using index 0x%02x: Vcc %d.%d",
	   configuration.ConfigIndex, configuration.Vcc/10, configuration.Vcc%10);
    if (configuration.Vpp1)
	printk(", Vpp1 %d.%d", configuration.Vpp1/10, configuration.Vpp1%10);
    if (configuration.Vpp2)
	printk(", Vpp2 %d.%d", configuration.Vpp2/10, configuration.Vpp2%10);
    if (configuration.Attributes & CONF_ENABLE_IRQ)
	printk(", irq %d", irq.AssignedIRQ);
	
    extern IORangeAllocator * gSharedMemoryRange;
    extern IORangeAllocator * gSharedIORange;

    memoryWindowCount = 0;
    for (int i=0; i < CISTPL_MEM_MAX_WIN; i++) {
	if (win[i]) {
	    printk(", mem 0x%06lx-0x%06lx", req[i].Base, req[i].Base+req[i].Size-1);

	    IODeviceMemory * range = IODeviceMemory::withRange(req[i].Base, req[i].Size);
	    if (success = !range) goto exit;
	    range->setTag(IOPCCARD16_MEMORY_WINDOW);
	    success = array->setObject(range);
	    range->release();
	    if (!success) goto exit;

	    success2 = gSharedMemoryRange->allocateRange(req[i].Base, req[i].Size);
	    if (!success2) IOLog("\nIOPCCard16Enabler: bad mem range %d(%08lx:%08lx)\n",
				req[i].Attributes, req[i].Base, req[i].Size);
	    memoryWindowCount++;
	}
    }

    ioMemory = device->ioDeviceMemory();
    if (!ioMemory) goto exit;	// this platform doesn't support i/o access

    ioWindowCount = 0;
    if (io.NumPorts1) {
	printk(", io 0x%04x-0x%04x", io.BasePort1, io.BasePort1+io.NumPorts1-1);

	IODeviceMemory * range = IODeviceMemory::withSubRange(ioMemory, io.BasePort1, io.NumPorts1);
	if (!range) goto exit;
	range->setTag(IOPCCARD16_IO_WINDOW);
	success = array->setObject(range);
	range->release();
	if (!success) goto exit;

	success2 = gSharedIORange->allocateRange(io.BasePort1, io.NumPorts1);
        if (!success2) IOLog("\nIOPCCard16Enabler: bad io range %d(%08lx:%08lx)\n",
			    io.Attributes1, io.BasePort1, io.NumPorts1);
	ioWindowCount++;
    }
    if (io.NumPorts2) {
	printk(" & 0x%04x-0x%04x", io.BasePort2, io.BasePort2+io.NumPorts2-1);

	IODeviceMemory * range = IODeviceMemory::withSubRange(ioMemory, io.BasePort2, io.NumPorts2);
	if (!range) goto exit;
	range->setTag(IOPCCARD16_IO_WINDOW);
	success = array->setObject(range);
	range->release();
	if (!success) goto exit;

	success2 = gSharedIORange->allocateRange(io.BasePort2, io.NumPorts2);
        if (!success2) IOLog("\nIOPCCard16Enabler: bad io range %d(%08lx:%08lx)\n",
			    io.Attributes2, io.BasePort2, io.NumPorts2);
	ioWindowCount++;
    }
    printk("\n");

    // set the nub's "IODeviceMemory" property, memory windows first, io windows second
    device->setDeviceMemory(array);

 exit:
    
    DEBUG(0, "IOPCCard16Enabler::configure(0x%x) was %ssuccessful.\n", index, success ? "" : "un");
    if (array) array->release();

    return success;
}


bool
IOPCCard16Enabler::unconfigure()
{
    DEBUG(0, "IOPCCard16Enabler::unconfigure\n");

    if (state & DEV_CONFIG) {
	IOPCCardBridge * bridge = (IOPCCardBridge *)device->getProvider();
	(void)bridge->unconfigureSocket((IOService *)device);
    }

    if (!(state & (DEV_CONFIG | DEV_CONFIG_PENDING))) return true;

    if (io.NumPorts1) CardServices(ReleaseIO, handle, &io);

    for (int i=0; i < CISTPL_MEM_MAX_WIN; i++) {
	if (win[i]) CardServices(ReleaseWindow, win[i]);
    }

    if (irq.AssignedIRQ) CardServices(ReleaseIRQ, handle, &irq);

    state &= ~DEV_CONFIG;

    return true;
}

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************

bool
IOPCCard16Enabler::getConfigurations(void)
{
    int last_fn, last_ret;
    tuple_t tuple;
    cisparse_t parse;
    u_char buf[64];
    u_int last_idx = 0;
    
    DEBUG(0, "IOPCCard16Enabler::getConfigurations(%p)\n", this);

    tableEntryCount = 0;

    if (!(state & DEV_CONFIG_PENDING)) return false;

    /*
       This reads the card's CONFIG tuple to find its configuration
       registers.
    */
    tuple.DesiredTuple = CISTPL_CONFIG;
    tuple.Attributes = 0;
    tuple.TupleData = buf;
    tuple.TupleDataMax = sizeof(buf);
    tuple.TupleOffset = 0;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    CS_CHECK(GetTupleData, handle, &tuple);
    CS_CHECK(ParseTuple, handle, &tuple, &parse);
    configuration.ConfigBase = parse.config.base;
    configuration.Present = parse.config.rmask[0];

    last_idx = parse.config.last_idx;
    if (last_idx == 0) goto cs_failed;

    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    while (tableEntryCount < kMaxConfigurations) {
	cistpl_cftable_entry_t dflt = { 0 }, *temp;
	cistpl_cftable_entry_t *cfg = &(parse.cftable_entry);
	CFG_CHECK(GetTupleData, handle, &tuple);
	CFG_CHECK(ParseTuple, handle, &tuple, &parse);

	if (cfg->flags & CISTPL_CFTABLE_DEFAULT) dflt = *cfg;
	if (cfg->index == 0) goto next_entry;

	temp = (cistpl_cftable_entry_t *)kmalloc(sizeof(cistpl_cftable_entry_t), GFP_KERNEL);
	*temp = dflt;

	temp->index = cfg->index;
	if (cfg->flags & CISTPL_CFTABLE_IF_PRESENT) {
	    temp->flags = cfg->flags;
	    temp->interface = cfg->interface;
	} 
	// if we haven't seen a TCPE_IF, use the implied values
	if (!(temp->flags & CISTPL_CFTABLE_IF_PRESENT)) {
	    temp->flags = CISTPL_CFTABLE_RDYBSY | CISTPL_CFTABLE_WP | CISTPL_CFTABLE_BVDS;
	    temp->interface = 0;
	}
	if (cfg->vcc.present) temp->vcc = cfg->vcc;
	if (cfg->vpp1.present) temp->vpp1 = cfg->vpp1;
	if (cfg->vpp2.present) temp->vpp2 = cfg->vpp2;
	// MACOSXXX timing ?
	if (cfg->io.nwin > 0) temp->io = cfg->io;
	if (cfg->irq.IRQInfo1) temp->irq = cfg->irq;
	if (cfg->mem.nwin > 0) temp->mem = cfg->mem;
	// MACOSXXX misc features ?
	// MACOSXXX additional subtuples (add a "hook" method for this?)

	configTable[tableEntryCount++] = temp;
	if (cfg->index == last_idx) break;
	
    next_entry:
	CS_CHECK(GetNextTuple, handle, &tuple);
    }

    DEBUG(1, "IOPCCard16Enabler::getConfigurations - successful, found %d configurations\n", tableEntryCount);
    return tableEntryCount > 0;

 cs_failed:
    cs_error(handle, last_fn, last_ret);

    if (!tableEntryCount && !last_idx) {
	DEBUG(1, "IOPCCard16Enabler::getConfigurations - failed to get config info, faking up a single memory window\n");

	cistpl_cftable_entry_t *temp = (cistpl_cftable_entry_t *)kmalloc(sizeof(cistpl_cftable_entry_t), GFP_KERNEL);

	temp->index = 1;
	temp->mem.flags = 0;
	temp->mem.nwin = 1;
	temp->mem.win[0].host_addr = 0;
	temp->mem.win[0].card_addr = 0;
	temp->mem.win[0].len = 0x1000;
	configTable[tableEntryCount++] = temp;

	return true;
    }

    DEBUG(1, "IOPCCard16Enabler::getConfigurations - had some problems, found %d configurations\n", tableEntryCount);
    return tableEntryCount > 0;
}

bool
IOPCCard16Enabler::sortConfigurations(void)
{
    if (!(state & DEV_CONFIG_PENDING)) return false;

    bool done = false;

    // repeat until there are no more swaps
    while (!done) {
	done = true;

	for (unsigned i=0; i < tableEntryCount - 1; i++) {
	    if ((!configTable[i]->io.nwin && (configTable[i+1]->io.nwin == 1)) ||
		((configTable[i]->io.nwin > 1) && !configTable[i+1]->io.nwin)) {
		cistpl_cftable_entry_t *temp = configTable[i];
		configTable[i] = configTable[i+1];
		configTable[i+1] = temp;
		done = false;
	    }
	}
    }

    return true;
}

bool
IOPCCard16Enabler::tryConfiguration(UInt32 index)
{
    unsigned i;
    int last_fn, last_ret;
    config_info_t conf;

    DEBUG(1, "IOPCCard16Enabler::tryConfiguration(0x%x)\n", index);
    if (!(state & DEV_CONFIG_PENDING)) return false;
    if (!index) return false;

    for (i=0; i < CISTPL_MEM_MAX_WIN; i++) win[i] = NULL;
    io.BasePort1 = io.NumPorts1 = io.BasePort2 = io.NumPorts2 = 0;

    cistpl_cftable_entry_t *cfg = 0;
    for (i=0; i < tableEntryCount; i++) {
	if (configTable[i]->index == index) {
	    cfg = configTable[i];
	    break;
	}
    }
    if (!cfg) {
	    DEBUG(1, "IOPCCard16Enabler::tryConfiguration(?) - can't find index 0x%x?\n", index);
	    return false;
    }

    configuration.ConfigIndex = cfg->index;
    configuration.IntType = INT_MEMORY;
	
    /* Does this card need audio output? */
    if (cfg->flags & CISTPL_CFTABLE_AUDIO) {
	configuration.Attributes |= CONF_ENABLE_SPKR;
	configuration.Status = CCSR_AUDIO_ENA;
    }

    /* Look up the current Vcc */
    CS_CHECK(GetConfigurationInfo, handle, &conf);
    configuration.Vcc = conf.Vcc;

    // MACOSXXX - changing the voltage like this doesn't work and
    // ignoring configuration entries with voltages that differ from
    // what the sense pins claim doesn't work either, some cards
    // really want to change.  for now, just ignore the whole issue.
    // whatever the power sense pins claim is what the card gets.
    // see radars 2776059 and 3239499

    /* Use power settings for Vcc and Vpp if present */
    /* Note that the CIS values need to be rescaled */
    if (cfg->vcc.present & (1<<CISTPL_POWER_VNOM)) {
	if (conf.Vcc != cfg->vcc.param[CISTPL_POWER_VNOM]/10000) {
	    DEBUG(1, "IOPCCard16Enabler::tryConfiguration(0x%x): requested Vcc %d != voltage sense pins %d\n",
		  index, cfg->vcc.param[CISTPL_POWER_VNOM]/10000, conf.Vcc);
#ifdef NOTNOW
#ifdef __MACOSX__
	    configuration.Vcc = cfg->vcc.param[CISTPL_POWER_VNOM]/10000;
#else
	    return false;
#endif
#endif    
	}
    }
	    
    if (cfg->vpp1.present & (1<<CISTPL_POWER_VNOM)) {
	configuration.Vpp1 = configuration.Vpp2 = 
	    cfg->vpp1.param[CISTPL_POWER_VNOM]/10000;
    }
	
    /* IO window settings */
    io.NumPorts1 = io.NumPorts2 = 0;
    if (cfg->io.nwin > 0) {
	cistpl_io_t *io_tpl = &cfg->io;
	io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
	if (!(io_tpl->flags & CISTPL_IO_8BIT))
	    io.Attributes1 = IO_DATA_PATH_WIDTH_16;
	if (!(io_tpl->flags & CISTPL_IO_16BIT))
	    io.Attributes1 = IO_DATA_PATH_WIDTH_8;
	io.IOAddrLines = io_tpl->flags & CISTPL_IO_LINES_MASK;
	io.BasePort1 = io_tpl->win[0].base;
	io.NumPorts1 = io_tpl->win[0].len;
	if (io_tpl->nwin > 1) {
	    io.Attributes2 = io.Attributes1;
	    io.BasePort2 = io_tpl->win[1].base;
	    io.NumPorts2 = io_tpl->win[1].len;
	}

	/* This reserves IO space but doesn't actually enable it */
	CS_CHECK(RequestIO, handle, &io);

	/* turn on IO */
	configuration.IntType = INT_MEMORY_AND_IO;
    }

    /*
	  Now set up a common memory windows, if needed.
    */

    for (i=0; i < cfg->mem.nwin; i++) {
	cistpl_mem_t *mem = &cfg->mem;
	req[i].Attributes = WIN_DATA_WIDTH_16 | WIN_MEMORY_TYPE_CM;
	req[i].Attributes |= WIN_ENABLE;
	req[i].Base = mem->win[i].host_addr;
	req[i].Size = mem->win[i].len;
	if (req[i].Size < 0x1000) req[i].Size = 0x1000;
	req[i].AccessSpeed = 0;
	win[i] = (window_handle_t)handle;
	CS_CHECK(RequestWindow, &win[i], &req[i]);
	map[i].Page = 0; map[i].CardOffset = mem->win[i].card_addr;
	CS_CHECK(MapMemPage, win[i], &map[i]);
    }

    /*
       Do we need to allocate an interrupt?
    */
    if (cfg->irq.IRQInfo1) {
	configuration.Attributes |= CONF_ENABLE_IRQ;
	CS_CHECK(RequestIRQ, handle, &irq);
    }
	
    /* If we got this far, we're cool! */
    DEBUG(1, "IOPCCard16Enabler::tryConfiguration(0x%x) was successful, \n", index);
    return true;
	
 cs_failed:
    DEBUG(1, "IOPCCard16Enabler::tryConfiguration(0x%x) had problems, \n", index);
    cs_error(handle, last_fn, last_ret);

    unconfigure();

    return false;
}

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************

u_int
IOPCCard16Enabler::getState(void)
{
    return state;
}

bool
IOPCCard16Enabler::getConfigurationInfo(config_info_t *config)
{
    config->Function = 0;
    int ret = CardServices(GetConfigurationInfo, handle, config);
    if (ret) {
	cs_error(handle, GetConfigurationInfo, ret);
	return false;
    }
    return true;
}

UInt32
IOPCCard16Enabler::getWindowCount(void)
{
    return memoryWindowCount + ioWindowCount;
}

UInt32
IOPCCard16Enabler::getWindowType(UInt32 index)
{
    if (index < memoryWindowCount) return IOPCCARD16_MEMORY_WINDOW;
    if (index < memoryWindowCount + ioWindowCount) return IOPCCARD16_IO_WINDOW;
    return IOPCCARD16_BAD_INDEX;
}

UInt32
IOPCCard16Enabler::getWindowSize(UInt32 index)
{
    if (index < memoryWindowCount) {
	return req[index].Size;
    }
    if (ioWindowCount && (index == memoryWindowCount)) {
	return io.NumPorts1;
    }
    if ((ioWindowCount == 2) && (index == (memoryWindowCount + 1))) {
	return io.NumPorts2;
    }
    return IOPCCARD16_BAD_INDEX;
}

bool
IOPCCard16Enabler::getWindowAttributes(UInt32 index, UInt32 *attributes)
{
    if (index < memoryWindowCount) {
	*attributes = req[index].Attributes;
	return true;
    }
    if (ioWindowCount && (index == memoryWindowCount)) {
	*attributes = io.Attributes1;
	return true;
    }
    if ((ioWindowCount == 2) && (index == (memoryWindowCount + 1))) {
	*attributes = io.Attributes2;
	return true;
    }
    *attributes = 0;
    return false;
}

bool
IOPCCard16Enabler::getWindowHandle(UInt32 index, window_handle_t *handle)
{
    if (index < memoryWindowCount) {
	*handle = win[index];
	return true;
    }
    return false;
}

bool
IOPCCard16Enabler::getWindowOffset(UInt32 index, UInt32 *offset)
{
    window_handle_t handle;
    
    if (getWindowHandle(index, &handle)) {
	memreq_t req;
	int ret = CardServices(GetMemPage, handle, &req);
	if (ret) {
	    cs_error(handle, GetMemPage, ret);
	    return false;
	}
	*offset = req.CardOffset;
	return true;
    }
    return false;
}

bool
IOPCCard16Enabler::setWindowOffset(UInt32 index, UInt32 newOffset)
{
    window_handle_t handle;
    
    if (getWindowHandle(index, &handle)) {
	memreq_t req;
	req.Page = 0;
	req.CardOffset = newOffset;
	int ret = CardServices(MapMemPage, handle, &req);
	if (ret) {
	    cs_error(handle, MapMemPage, ret);
	    return false;
	}
	return true;
    }
    return false;
}
