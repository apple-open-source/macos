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

    Contributor:  Apple Computer, Inc.  Portions © 2000 Apple Computer, 
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

	cb_enabler.c 1.31 2000/06/12 21:29:36

======================================================================*/

#include <IOKit/pccard/IOPCCard.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOUserClient.h>

#ifdef PCMCIA_DEBUG
extern int ds_debug;
MODULE_PARM(ds_debug, "i");
#define DEBUG(n, args...) if (ds_debug>(n)) printk(KERN_DEBUG args)
#else
#define DEBUG(n, args...)
#endif

#define CardServices	cardServices

#define cs_error(handle, func, ret)		\
{						\
    error_info_t err = { func, ret };		\
    CardServices(ReportError, handle, &err);	\
}

#undef  super
#define super IOPCIDevice

OSDefineMetaClassAndStructors(IOCardBusDevice, IOPCIDevice);

OSMetaClassDefineReservedUnused(IOCardBusDevice,  0);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  1);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  2);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  3);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  4);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  5);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  6);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  7);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  8);
OSMetaClassDefineReservedUnused(IOCardBusDevice,  9);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 10);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 11);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 12);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 13);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 14);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 15);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 16);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 17);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 18);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 19);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 20);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 21);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 22);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 23);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 24);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 25);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 26);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 27);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 28);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 29);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 30);
OSMetaClassDefineReservedUnused(IOCardBusDevice, 31);

IOReturn
IOCardBusDevice::setPowerState(unsigned long powerState,
			       IOService * whatDevice)
{
    DEBUG(1, "IOCardBusDevice:setPowerState state=%d\n", powerState);

    if ((powerState == kIOPCIDeviceOnState) || (powerState == 1)) {
	if (!(state & DEV_SUSPEND)) return IOPMAckImplied;
	state &= ~DEV_SUSPEND;

	// restore basic pci config regs
	parent->setDevicePowerState(this, 1);

    } else if (powerState == kIOPCIDeviceOffState) {
	if (state & DEV_SUSPEND) return IOPMAckImplied;
	state |= DEV_SUSPEND;

	// save basic pci config regs
	parent->setDevicePowerState(this, 0);
    }

    return IOPMAckImplied;
}

IOReturn
IOCardBusDevice::setProperties(OSObject * properties)
{
    OSDictionary * dict;
    int rc;

//    if (IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeLocalUser)) {
//	IOLog("IOCardBusDevice::setProperties: failed, the user has insufficient privileges");
//	return kIOReturnNotPrivileged;
//    }

    dict = OSDynamicCast(OSDictionary, properties);
    if (dict) {

	if (dict->getObject("eject request")) {
	    rc = IOPCCardBridge::requestCardEjection(parent->getProvider());
	    if (rc) {
		IOLog("IOCardBusDevice::setProperties(eject request) failed with error = %d\n", rc);
		return kIOReturnError;
	    }
	    return kIOReturnSuccess;
	}
    }

    return kIOReturnBadArgument;
}

u_int
IOCardBusDevice::getState(void)
{
    return state;
}

client_handle_t
IOCardBusDevice::getCardServicesHandle(void)
{
    return handle;
}

int
IOCardBusDevice::cardServices(int func, void * arg1 /* = 0 */, void * arg2 /* = 0 */, void * arg3 /* = 0*/)
{
    return gIOPCCardWorkLoop->runAction((IOWorkLoop::Action)gCardServicesGate, NULL,
					(void *)func, arg1, arg2, arg3);
}

int
IOCardBusDevice::eventHandler(cs_event_t event, int priority,
			      event_callback_args_t *args)
{
    DEBUG(1, "IOCardBusDevice::eventHandler(0x%06x, %d, 0x%p)\n",
	  event, priority, args->client_handle);

    IOCardBusDevice *nub = OSDynamicCast(IOCardBusDevice, (OSObject *)args->client_data);
    if (!nub) {
	IOLog("IOCardBusDevice::eventHandler: bogus event?\n");
	return 0;
    }

    IOReturn ret = nub->messageClients(kIOPCCardCSEventMessage, (void *)event);
    if (ret && ret != kIOReturnUnsupported) {
	IOLog("IOCardBusDevice::eventHandler: messageClients returned %d\n", ret);
	return ret;
    } else {
	return 0;
    }
}

//MACOSXXX including bsd/sys/systm.h causes too much pain
__BEGIN_DECLS
int snprintf __P((char *, size_t, const char *, ...));
__END_DECLS

bool
IOCardBusDevice::bindCardServices(void)
{
    int ret;

    IOLog("IOCardBusDevice: binding socket %d function %d to card services.\n", socket, function);

    snprintf(bindName, sizeof(bindName), "Cardbus Expert");

    state = DEV_PRESENT | DEV_CONFIG_PENDING;

    bind_req_t bind_req;
    bind_req.Socket = socket;
    bind_req.Function = function;
    bind_req.dev_info = &bindName;
    ret = CardServices(BindDevice, &bind_req);
    if (ret) return false;

    client_reg_t client_reg;
    client_reg.dev_info = &bindName;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.event_handler = &IOCardBusDevice::eventHandler;
    client_reg.EventMask = 
	CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
	CS_EVENT_RESET_REQUEST  | CS_EVENT_EJECTION_REQUEST;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = (void *)this;
    ret = CardServices(RegisterClient, &handle, &client_reg);
    if (ret) return false;

    state |= DEV_CONFIG;

    if (function == 0) {
	/* Special hook: this configures all functions on the card */
	ret = CardServices(RequestIO, handle, NULL);
	if (ret) return false;
    
	memset(&configuration, 0, sizeof(struct config_req_t));
	configuration.Vcc = 33;
	configuration.IntType = INT_CARDBUS;
    
	ret = ((IOPCCardBridge *)parent)->configureSocket((IOService *)this, &configuration);
	if (ret) return false;
    }

    state &= ~DEV_CONFIG_PENDING;

    return true;
}

bool
IOCardBusDevice::unbindCardServices(void)
{
    int ret;

    IOLog("IOCardBusDevice: releasing socket %d function %d from card services.\n", socket, function);

    if (state & DEV_CONFIG) {

	if (function == 0) {

	    ret = ((IOPCCardBridge *)parent)->unconfigureSocket((IOService *)this);
	    if (ret) return false;
    
	    ret = CardServices(ReleaseIO, handle, NULL);
	    if (ret) return false;
	}
    
	ret = CardServices(DeregisterClient, handle);
	if (ret) return false;
    }
    state &= ~DEV_CONFIG;

    snprintf(bindName, sizeof(bindName), "not bound");

    return true;
}

bool
IOCardBusDevice::finalize(IOOptionBits options)
{
    unbindCardServices();
    return super::finalize(options);
}

static UInt64
getDebugFlags(OSDictionary * props)
{
    OSNumber *debugProp = OSDynamicCast(OSNumber, props->getObject(gIOKitDebugKey));
    if (debugProp) {
	return debugProp->unsigned64BitValue();
    }
    return 0;
}

static bool
compareVersionOneInfo(OSDictionary *catalog, OSDictionary *cis, bool debug)
{
    OSArray *version1 = OSDynamicCast(OSArray, catalog->getObject(kIOPCCardVersionOneMatchKey));
    OSArray *myversion1 = OSDynamicCast(OSArray, cis->getObject(kIOPCCardVersionOneMatchKey));

    if (!version1) {
	if (debug) IOLog("IOPCCardDevice: the VersionOneInfo match array is not of type OSArray?\n");
	return false;
    }
    if (!myversion1) {
	if (debug) IOLog("IOPCCardDevice: there is no VersionOneInfo CIS tuple to match against?\n");
	return false;
    }

    const OSString *wild = OSString::withCString("*");
    int max = version1->getCount();

    for (int i = 0; i < max; i++) {
	OSString *str = OSDynamicCast(OSString, version1->getObject(i));
	if (!str) {
	    IOLog("IOPCCardDevice: the VersionOneInfo[%d] in match array is not of type OSString?\n", i);
	    wild->release();
	    return false;
	}
	if (str->isEqualTo(wild)) continue;

	OSString *mystr = OSDynamicCast(OSString, myversion1->getObject(i));
	if (mystr && !mystr->isEqualTo(str)) {
	    if (debug && mystr) {
		IOLog("IOPCCardDevice: VersionOneInfo[%d], \"%s\"(match string) != \"%s\"(CIS string).\n",
			i, str->getCStringNoCopy(), mystr->getCStringNoCopy());
	    }
	    wild->release();
	    return false;
	}
    }
    wild->release();
    
    if (debug) IOLog("IOPCCardDevice: VersionOneInfo matched.\n");
    return true;
}

static bool
compareFunceTuples(OSDictionary *catalog, OSDictionary *cis, bool debug)
{
    OSNumber *funcid = OSDynamicCast(OSNumber, cis->getObject(kIOPCCardFunctionIDMatchKey));
    OSArray *funceArray = OSDynamicCast(OSArray, cis->getObject(kIOPCCardFunctionExtensionMatchKey));
    OSArray *matchArray = OSDynamicCast(OSArray, catalog->getObject(kIOPCCardFunctionExtensionMatchKey));

    if (!funcid && !funceArray) {
	if (debug) IOLog("IOPCCardDevice: card doesn't appear to have any FUNCE tuples.\n");
	return false;
    }
    int funceArraySize = funceArray->getCount();
    if (!funceArraySize) return false;

    if (!matchArray) {
	IOLog("IOPCCardDevice: FunctionExtensionMatch is not an array?\n");
	return false;
    }
    int matchArraySize = matchArray->getCount();
    if (!matchArraySize || matchArraySize & 1) {
	IOLog("IOPCCardDevice: FunctionExtensionMatch in match array is empty or has a odd count?\n");
	return false;
    }

    bool matched = false;
    bool matchAll = false;

    for (int i = 0; i < matchArraySize; i += 2) {
	OSData *pattern = OSDynamicCast(OSData, matchArray->getObject(i));
	OSData *mask = OSDynamicCast(OSData, matchArray->getObject(i+1));
	if (!pattern || !mask) {
	    IOLog("IOPCCardDevice: FunctionExtensionMatch[%d-%d] pair is bogus?\n", i, i+1);
	    continue;
	}

	int matchSize = pattern->getLength();
	if (matchSize != (int)mask->getLength()) {
	    IOLog("IOPCCardDevice: FunctionExtensionMatch[%d-%d] pair have different sizes?\n", i, i+1);
	    continue;
	}
	
	// the function extension match property from the driver
	// contains the function id as the first byte of the pattern,
	// we need both id and extension data to make a match, the
	// function id must always match

	// the first first byte of the mask data in the match pair is
	// used to control if that pair must match.  if it is nonzero
	// then that pair must match.
	
	matchSize--;

	for (int j=0; j < funceArraySize; j++) {
 
	    OSData *funce = OSDynamicCast(OSData, funceArray->getObject(j));

	    int funceSize = funce->getLength();
	    if (funceSize < matchSize) continue;

	    UInt8 *f = (UInt8 *)funce->getBytesNoCopy();
	    UInt8 *p = (UInt8 *)pattern->getBytesNoCopy();
	    UInt8 *m = (UInt8 *)mask->getBytesNoCopy();
	
	    // compare function id, ignore the mask
	    if (funcid->unsigned8BitValue() != *p) break;
	    
	    // get type of matching from first match mask 
	    matchAll = *m != 0;

	    // skip over function id
	    p++; m++;

	    matched = true;
	    for (int k=0; k < matchSize; k++) {

		if ((p[k] | m[k]) != m[k]) {
		    if (!j) IOLog("IOPCCardDevice: FunctionExtensionMatch[%d-%d] pair will never match?\n", i, i+1);
		    matched = false;
		    break;
		}
	    
		if ((f[k] & m[k]) != p[k]) {
		    matched = false;
		    break;
		}
	    }
	    //  stop looking, this pair has matched one of the function extension tuples
	    if (matched) break;
	}
	if (matchAll && !matched) break;
	
	if (!matchAll && matched) {
	    if (debug) IOLog("IOPCCardDevice: FunctionExtensionMatch[%d-%d] pair matched.\n", i, i+1);
	    return true;
	}
    }
    if (matched) {
	if (debug) IOLog("IOPCCardDevice: all FunctionExtensionMatch pairs matched against function extention tuples.\n");
	return true;
    }

    if (debug) IOLog("IOPCCardDevice: FUNCE tuples failed to match.\n");
    return false;
}

bool
IOCardBusDevice::matchPropertyTable(OSDictionary *table, SInt32 *score)
{
    // only look for properties that we care about
    // and only return false if they don't match
    // in all other cases return true
    bool matched = true;

    bool debug = getDebugFlags(table) & kIOLogStart != 0;
    if (debug) IOLog("IOCardBusDevice::matchPropertyTable entered.\n");

    if (table->getObject(kIOPCCardVersionOneMatchKey)) {
	matched = compareVersionOneInfo(table, getPropertyTable(), debug);
	return matched;
    }

    //MACOSXXX still need to do specific cis tuple matching/patching

    return super::matchPropertyTable(table, score);
}

IOService *
IOCardBusDevice::matchLocation(IOService * client)
{
    return this;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOService

OSDefineMetaClassAndStructors(IOPCCard16Device, IOService);

OSMetaClassDefineReservedUnused(IOPCCard16Device,  0);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  1);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  2);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  3);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  4);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  5);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  6);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  7);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  8);
OSMetaClassDefineReservedUnused(IOPCCard16Device,  9);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 10);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 11);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 12);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 13);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 14);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 15);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 16);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 17);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 18);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 19);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 20);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 21);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 22);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 23);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 24);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 25);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 26);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 27);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 28);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 29);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 30);
OSMetaClassDefineReservedUnused(IOPCCard16Device, 31);


bool
IOPCCard16Device::attach(IOService * provider)
{
    static IOPMPowerState powerStates[kIOPCCard16DevicePowerStateCount] = {
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    
    bridge = OSDynamicCast(IOPCCardBridge, provider);
    if (!bridge) return false;
    
    // initialize superclass variables
    PMinit();
    // register as controlling driver
    registerPowerDriver(this, (IOPMPowerState *) powerStates, kIOPCCard16DevicePowerStateCount);
    // join the tree
    provider->joinPMtree(this);

    if (!super::attach(provider)) {
	PMstop();
	return false;
    }

    return true;
}

void
IOPCCard16Device::detach(IOService * provider)
{
    PMstop();
    return super::detach(provider);
}

IOReturn
IOPCCard16Device::setPowerState(unsigned long powerState,
				IOService * whatDevice)
{
    DEBUG(1, "IOPCCard16Device:setPowerState state=%d\n", powerState);

    return enabler ? enabler->setPowerState(powerState, whatDevice) : IOPMAckImplied;
}

IOReturn
IOPCCard16Device::setProperties(OSObject * properties)
{
    OSDictionary * dict;
    int rc;

//    if (IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeLocalUser)) {
//	IOLog("IOCardBusDevice::setProperties: failed, the user has insufficient privileges");
//	return kIOReturnNotPrivileged;
//    }

    dict = OSDynamicCast(OSDictionary, properties);
    if (dict) {

	if (dict->getObject("eject request")) {
	    rc = IOPCCardBridge::requestCardEjection(bridge->getProvider());
	    if (rc) {
		IOLog("IOPCCard16Device::setProperties(eject request) failed with error = %d\n", rc);
		return kIOReturnError;
	    }
	    return kIOReturnSuccess;
	}
    }

    return kIOReturnBadArgument;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

u_int
IOPCCard16Device::getState(void)
{
    return enabler ? enabler->getState() : 0;
}

client_handle_t
IOPCCard16Device::getCardServicesHandle(void)
{
    return handle;
}

int
IOPCCard16Device::cardServices(int func, void * arg1 /* = 0 */, void * arg2 /* = 0 */, void * arg3 /* = 0 */)
{
    return gIOPCCardWorkLoop->runAction((IOWorkLoop::Action)gCardServicesGate, NULL,
					(void *)func, arg1, arg2, arg3);
}

int
IOPCCard16Device::eventHandler(cs_event_t event, int priority,
			       event_callback_args_t *args)
{
    DEBUG(1, "IOPCCard16Device::eventHandler(0x%06x, %d, 0x%p, 0x%p)\n",
	  event, priority, args->client_handle, args->client_data);

    IOPCCard16Device *nub = OSDynamicCast(IOPCCard16Device, (OSObject *)args->client_data);
    if (!nub) {
	IOLog("IOPCCard16Device::eventHandler: bogus event?\n");
	return 0;
    }

    // let the enabler take a first crack at looking at this event
    if (nub->enabler) {
	nub->enabler->eventHandler(event, priority, args);
    }

    IOReturn ret = nub->messageClients(kIOPCCardCSEventMessage, (void *)event);
    if (ret && ret != kIOReturnUnsupported) {
	IOLog("IOPCCard16Device::eventHandler: messageClients returned %d\n", ret);
	return ret;
    }
    
    return 0;
}

// this is to keep bind names unique
static int bindNameIndex = 0;

bool
IOPCCard16Device::bindCardServices(void)
{
    int ret;

    IOLog("IOPCCard16Device: binding socket %d function %d to card services.\n", socket, function);

    snprintf(bindName, sizeof(bindName), "PCCardExpert%d", bindNameIndex++);

    bind_req_t bind_req;
    bind_req.Socket = socket;
    bind_req.Function = function;
    bind_req.dev_info = &bindName;
    ret = CardServices(BindDevice, &bind_req);
    if (ret) return false;

    client_reg_t client_reg;
    client_reg.dev_info = &bindName;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.event_handler = &IOPCCard16Device::eventHandler;
    client_reg.EventMask = 
	CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
	CS_EVENT_RESET_REQUEST  | CS_EVENT_EJECTION_REQUEST;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = (void *)this;
    ret = CardServices(RegisterClient, &handle, &client_reg);
    if (ret) return false;

    return true;
}

bool
IOPCCard16Device::unbindCardServices(void)
{
    int ret;

    if (enabler) {
	enabler->detach(this);
	enabler->release();
	enabler = 0;
    }

    IOLog("IOPCCard16Device: releasing socket %d function %d from card services.\n", socket, function);
    
    ret = CardServices(DeregisterClient, handle);
    if (ret) return false;

    snprintf(bindName, sizeof(bindName), "not bound");

    return true;
}

bool
IOPCCard16Device::finalize(IOOptionBits options)
{
    unbindCardServices();

    return super::finalize(options);
}

bool
IOPCCard16Device::matchPropertyTable(OSDictionary *table, SInt32 *score)
{
    // only look for properties that we care about and only return 
    // false if they don't match, in all other cases return true
    bool matched = false;

    bool debug = getDebugFlags(table) & kIOLogStart != 0;
    if (debug) IOLog("IOPCCard16Device::matchPropertyTable entered.\n");

    if (table->getObject(kIOPCCardVersionOneMatchKey)) {
	matched = compareVersionOneInfo(table, getPropertyTable(), debug);
	if (!matched) return false;
    }

    if (table->getObject(kIOPCCardFunctionExtensionMatchKey)) {
	matched = compareFunceTuples(table, getPropertyTable(), debug);
	if (!matched) return false;
    }

    if (table->getObject(kIOPCCardMemoryDeviceNameMatchKey)) {
	matched = compareProperty(table, kIOPCCardMemoryDeviceNameMatchKey);
	if (debug) IOLog("MemoryDeviceName %s.\n", matched ? "matched" : "didn't match");
	if (!matched) return false;
    }

    if (table->getObject(kIOPCCardFunctionNameMatchKey)) {
	matched = compareProperty(table, kIOPCCardFunctionNameMatchKey);
	if (debug) IOLog("FunctionName %s.\n", matched ? "matched" : "didn't match");
	if (!matched) return false;
    }

    if (table->getObject(kIOPCCardFunctionIDMatchKey)) {
	matched = compareProperty(table, kIOPCCardFunctionIDMatchKey);
	if (debug) IOLog("FunctionID %s.\n", matched ? "matched" : "didn't match");
	if (!matched) return false;
    }

    //MACOSXXX still need to do specific cis tuple matching/patching

    if (debug) IOLog("IOPCCard16Device::matchPropertyTable %s a match.\n", matched ? "found" : "didn't find");
    return true;
}

IOService *
IOPCCard16Device::matchLocation(IOService * client)
{
    return this;
}

//*****************************************************************************

bool
IOPCCard16Device::installEnabler(IOPCCard16Enabler *customEnabler /* = 0 */)
{
    if (enabler) {
	if (!enabler->detach(this)) return false;
	enabler->release();
	enabler = 0;
    }

    if (customEnabler) {
	// use the specified custom enabler
	enabler = OSDynamicCast(IOPCCard16Enabler, customEnabler);
	if (!enabler) return false;
	enabler->retain();
    } else {
	// create a default enabler
	enabler = IOPCCard16Enabler::withDevice(this);
	if (!enabler) return false;
    }

    return enabler->attach(this);
}

bool
IOPCCard16Device::configure(UInt32 index /* = 0 */)
{
    if (!enabler) {
	if (!installEnabler()) return false;
    }

    return enabler->configure(index);
}

bool
IOPCCard16Device::unconfigure(void)
{
    bool success = true;

    if (enabler) {
	success = enabler->detach(this);
	enabler->release();
	enabler = 0;
    }

    return success;
}

bool
IOPCCard16Device::getConfigurationInfo(config_info_t *config)
{
    return enabler ? enabler->getConfigurationInfo(config) : false;
}

UInt32
IOPCCard16Device::getWindowCount(void)
{
    return enabler ? enabler->getWindowCount() : 0;
}

UInt32
IOPCCard16Device::getWindowType(UInt32 index)
{
    return enabler ? enabler->getWindowType(index) : 0;
}

UInt32
IOPCCard16Device::getWindowSize(UInt32 index)
{
    return enabler ? enabler->getWindowSize(index) : 0;
}

bool
IOPCCard16Device::getWindowAttributes(UInt32 index, UInt32 *attributes)
{
    return enabler ? enabler->getWindowAttributes(index, attributes) : false;
}

bool
IOPCCard16Device::getWindowHandle(UInt32 index, window_handle_t *handle)
{
    return enabler ? enabler->getWindowHandle(index, handle) : false;
}

bool
IOPCCard16Device::getWindowOffset(UInt32 index, UInt32 *offset)
{
    return enabler ? enabler->getWindowOffset(index, offset) : false;
}

bool
IOPCCard16Device::setWindowOffset(UInt32 index, UInt32 newOffset)
{
    return enabler ? enabler->setWindowOffset(index, newOffset) : false;
}

//*****************************************************************************


IODeviceMemory *
IOPCCard16Device::ioDeviceMemory(void)
{
    return bridge->ioDeviceMemory();
}

#ifdef __ppc__
UInt32
IOPCCard16Device::ioRead32( UInt16 offset, IOMemoryMap * map /* = 0 */)
{
    UInt32	value;

    if( 0 == map) {
	map = ioMap;
	if( 0 == map)
	    return( 0 );
    }

    value = OSReadSwapInt32( (volatile void *)map->getVirtualAddress(), offset);
    eieio();

    return( value );
}

UInt16
IOPCCard16Device::ioRead16( UInt16 offset, IOMemoryMap * map /* = 0 */)
{
    UInt16	value;

    if( 0 == map) {
	map = ioMap;
	if( 0 == map)
	    return( 0 );
    }

    value = OSReadSwapInt16( (volatile void *)map->getVirtualAddress(), offset);
    eieio();

    return( value );
}

UInt8
IOPCCard16Device::ioRead8( UInt16 offset, IOMemoryMap * map /* = 0 */ )
{
    UInt32	value;

    if( 0 == map) {
	map = ioMap;
	if( 0 == map)
	    return( 0 );
    }

    value = ((volatile UInt8 *) map->getVirtualAddress())[ offset ];
    eieio();

    return( value );
}

void
IOPCCard16Device::ioWrite32( UInt16 offset, UInt32 value,
			     IOMemoryMap * map /* = 0 */ )
{
    if( 0 == map) {
	map = ioMap;
	if( 0 == map)
	    return;
    }

    OSWriteSwapInt32( (volatile void *)map->getVirtualAddress(), offset, value);
    eieio();
}

void
IOPCCard16Device::ioWrite16( UInt16 offset, UInt16 value,
			     IOMemoryMap * map /* = 0 */ )
{
    if( 0 == map) {
	map = ioMap;
	if( 0 == map)
	    return;
    }

    OSWriteSwapInt16( (volatile void *)map->getVirtualAddress(), offset, value);
    eieio();
}

void
IOPCCard16Device::ioWrite8( UInt16 offset, UInt8 value,
			    IOMemoryMap * map /* = 0 */ )
{
    if( 0 == map) {
	map = ioMap;
	if( 0 == map)
	    return;
    }

    ((volatile UInt8 *) map->getVirtualAddress())[ offset ] = value;
    eieio();

}
#endif

//*****************************************************************************

#ifdef __i386__

// MACOSXXX this file was pulled from xnu, when it gets properly 
// exported from the kernel framework it should be pulled from 
// this project.

#include "pio.h"

UInt32
IOPCCard16Device::ioRead32( UInt16 offset, IOMemoryMap * map /* = 0 */ )
{
    UInt32	value;

    if( 0 == map)
	map = ioMap;

    value = inl( map->getPhysicalAddress() + offset );

    return( value );
}

UInt16
IOPCCard16Device::ioRead16( UInt16 offset, IOMemoryMap * map /* = 0 */)
{
    UInt16	value;

    if( 0 == map)
	map = ioMap;

    value = inw( map->getPhysicalAddress() + offset );

    return( value );
}

UInt8
IOPCCard16Device::ioRead8( UInt16 offset, IOMemoryMap * map /* = 0 */ )
{
    UInt32	value;

    if( 0 == map)
	map = ioMap;

    value = inb( map->getPhysicalAddress() + offset );

    return( value );
}

void
IOPCCard16Device::ioWrite32( UInt16 offset, UInt32 value,
			     IOMemoryMap * map /* = 0 */ )
{
    if( 0 == map)
	map = ioMap;

    outl( map->getPhysicalAddress() + offset, value );
}

void
IOPCCard16Device::ioWrite16( UInt16 offset, UInt16 value,
			     IOMemoryMap * map /* = 0 */ )
{
    if( 0 == map)
	map = ioMap;

    outw( map->getPhysicalAddress() + offset, value );
}

void
IOPCCard16Device::ioWrite8( UInt16 offset, UInt8 value,
			    IOMemoryMap * map /* = 0 */ )
{
    if( 0 == map)
	map = ioMap;

    outb( map->getPhysicalAddress() + offset, value );
}
#endif
