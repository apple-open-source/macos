/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
// 45678901234567890123456789012345678901234567890123456789012345678901234567890
/*
 * IOSerialBSDClient.cpp
 *
 * 2002-04-19	dreece	moved device node removal from free() to didTerminate()
 * 2001-11-30	gvdl	open/close pre-emptible arbitration for termios
 *			IOSerialStreams.
 * 2001-09-02	gvdl	Fixed hot unplug code now terminate cleanly.
 * 2001-07-20	gvdl	Add new ioctl for DATA_LATENCY control.
 * 2001-05-11	dgl	Update iossparam function to recognize MIDI clock mode.
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */
#include <sys/cdefs.h>

__BEGIN_DECLS
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <miscfs/devfs/devfs.h>
#include <sys/systm.h>

#include <kern/thread.h>

extern int nulldev();
__END_DECLS

#include <IOKit/assert.h>
#include <IOKit/system.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOLib.h>

#include "IORS232SerialStreamSync.h"

#include "ioss.h"
#include "IOSerialKeys.h"

#include "IOSerialBSDClient.h"

#define super IOService

OSDefineMetaClassAndStructors(IOSerialBSDClient, IOService)

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))
#define SAFE_RELEASE(x) do { if (x) x->release(); x = 0; } while(0)

/*
 * Options and tunable parameters
 */
#define	TTY_DIALIN_INDEX	0
#define	TTY_CALLOUT_INDEX	1
#define TTY_NUM_FLAGS		1
#define TTY_NUM_TYPES		(1 << TTY_NUM_FLAGS)

#define TTY_HIGH_HEADROOM	4	/* size error code + 1 */
#define TTY_HIGHWATER		(TTYHOG - TTY_HIGH_HEADROOM)
#define TTY_LOWWATER		((TTY_HIGHWATER * 7) / 8)

#define	IS_TTY_OUTWARD(dev)	( minor(dev) &  TTY_CALLOUT_INDEX)
#define TTY_UNIT(dev)		( minor(dev) >> TTY_NUM_FLAGS)

#define TTY_QUEUESIZE(tp)	(tp->t_rawq.c_cc + tp->t_canq.c_cc)
#define IS_TTY_PREEMPT(dev, cflag)	\
    ( !IS_TTY_OUTWARD((dev)) && !ISSET((cflag), CLOCAL) )

#define TTY_DEVFS_PREFIX	"/dev/"
#define TTY_CALLOUT_PREFIX	TTY_DEVFS_PREFIX "cu."
#define TTY_DIALIN_PREFIX	TTY_DEVFS_PREFIX "tty."

/*
 * All times are in Micro Seconds
 */
#define MUSEC2TICK(x) \
            ((int) (((long long) (x) * hz + 500000) / 1000000))
#define MUSEC2TIMEVALDECL(x)	{ (x) / 1000000, ((x) % 1000000) }

#define	MAX_INPUT_LATENCY   40000	/* 40 ms */
#define	MIN_INPUT_LATENCY   10000	/* 10 ms */

#define	DTR_DOWN_DELAY	  2000000	/* DTR down time  2 seconds */
#define PREEMPT_IDLE	  DTR_DOWN_DELAY /* Same as close delay */
#define DCD_DELAY 	    10000 	/* Ignore DCD change of < 10ms */
#define BRK_DELAY 	   250000 	/* Minimum break  .25 sec */

#define	RS232_S_ON		(PD_RS232_S_RTS | PD_RS232_S_DTR)
#define	RS232_S_OFF		(0)

#define	RS232_S_INPUTS		(PD_RS232_S_CAR | PD_RS232_S_CTS)
#define	RS232_S_OUTPUTS		(PD_RS232_S_DTR | PD_RS232_S_RTS)

/* Default line state */
#define	ISPEED	B9600
#define	IFLAGS	(EVENP|ODDP|ECHO|CRMOD)

// External OSSymbol Cache, they have to be somewhere.
const OSSymbol *gIOSerialBSDServiceValue = 0;
const OSSymbol *gIOSerialBSDTypeKey = 0;
const OSSymbol *gIOSerialBSDAllTypes = 0;
const OSSymbol *gIOSerialBSDModemType = 0;
const OSSymbol *gIOSerialBSDRS232Type = 0;
const OSSymbol *gIOTTYDeviceKey = 0;
const OSSymbol *gIOTTYBaseNameKey = 0;
const OSSymbol *gIOTTYSuffixKey = 0;
const OSSymbol *gIOCalloutDeviceKey = 0;
const OSSymbol *gIODialinDeviceKey = 0;
const OSSymbol *gIOTTYWaitForIdleKey = 0;

class IOSerialBSDClientGlobals {
private:

    unsigned int fMajor;
    unsigned int fLastMinor;
    IOSerialBSDClient **fClients;
    OSDictionary *fNames;

public:
    IOSerialBSDClientGlobals();
    ~IOSerialBSDClientGlobals();

    inline bool isValid();
    inline IOSerialBSDClient *getClient(dev_t dev);

    dev_t assign_dev_t();
    bool registerTTY(dev_t dev, IOSerialBSDClient *tty);
    const OSSymbol *getUniqueTTYSuffix
        (const OSSymbol *inName, const OSSymbol *suffix, dev_t dev);
    void releaseUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix);
};

class AutoBase {
private:
    // Disable copy constructors of AutoBase based objects
    void operator =(AutoBase &src) { };
    AutoBase(AutoBase &src) { };
    static void *operator new(size_t size) { return 0; };

protected:
    AutoBase() { } ;
};

class AutoKernelFunnel : AutoBase {
    boolean_t fState;

public:
    AutoKernelFunnel()  { fState = thread_funnel_set(kernel_flock, TRUE); };
    ~AutoKernelFunnel() { thread_funnel_set(kernel_flock, fState); };
};

// Create an instance of the IOSerialBSDClientGlobals
// This runs the static constructor and destructor so that
// I can grab and release a lock as appropriate.
static IOSerialBSDClientGlobals sBSDGlobals;

struct cdevsw IOSerialBSDClient::devsw =
{
    /* d_open     */ IOSerialBSDClient::iossopen,
    /* d_close    */ IOSerialBSDClient::iossclose,
    /* d_read     */ IOSerialBSDClient::iossread,
    /* d_write    */ IOSerialBSDClient::iosswrite,
    /* d_ioctl    */ IOSerialBSDClient::iossioctl,
    /* d_stop     */ IOSerialBSDClient::iossstop,
    /* d_reset    */ nulldev,
    /* d_ttys     */ NULL,
    /* d_select   */ IOSerialBSDClient::iossselect,
    /* d_mmap     */ eno_mmap,
    /* d_strategy */ eno_strat,
    /* d_getc     */ eno_getc,
    /* d_putc     */ eno_putc,
    /* d_type     */ D_TTY
};

static const struct timeval kDTRDownDelay = MUSEC2TIMEVALDECL(DTR_DOWN_DELAY);
static const struct timeval kPreemptIdle  = MUSEC2TIMEVALDECL(PREEMPT_IDLE);
static const struct timeval kNever        = { 0, 0 };

/*
 * Map from Unix baud rate defines to <PortDevices> baud rate.  NB all
 * reference to bits used in a PortDevice are always 1 bit fixed point.
 * The extra bit is used to indicate 1/2 bits.
 */
#define IOSS_HALFBIT_BRD	1
#define IOSS_BRD(x)	((int) ((x) * 2.0))

static struct speedtab iossspeeds[] = {
    {       0,	               0    },
    {      50,	IOSS_BRD(     50.0) },
    {      75,	IOSS_BRD(     75.0) },
    {     110,	IOSS_BRD(    110.0) },
    {     134,	IOSS_BRD(    134.5) },	/* really 134.5 baud */
    {     150,	IOSS_BRD(    150.0) },
    {     200,	IOSS_BRD(    200.0) },
    {     300,	IOSS_BRD(    300.0) },
    {     600,	IOSS_BRD(    600.0) },
    {    1200,	IOSS_BRD(   1200.0) },
    {    1800,	IOSS_BRD(   1800.0) },
    {    2400,	IOSS_BRD(   2400.0) },
    {    4800,	IOSS_BRD(   4800.0) },
    {    9600,	IOSS_BRD(   9600.0) },
    {   19200,	IOSS_BRD(  19200.0) },
    {   38400,	IOSS_BRD(  38400.0) },
    {   57600,	IOSS_BRD(  57600.0) },
    {  115200,	IOSS_BRD( 115200.0) },
    {  230400,	IOSS_BRD( 230400.0) },
    {  460800,	IOSS_BRD( 460800.0) },
    {  921600,	IOSS_BRD( 921600.0) },
    { 1843200,	IOSS_BRD(1843200.0) },
    {   19000,	IOSS_BRD(  19200.0) },
    {   38000,	IOSS_BRD(  38400.0) },
    {   57000,	IOSS_BRD(  57600.0) },
    {  115000,	IOSS_BRD( 115200.0) },
    {  230000,	IOSS_BRD( 230400.0) },
    {  460000,	IOSS_BRD( 460800.0) },
    {  920000,	IOSS_BRD( 921600.0) },
    {  921000,	IOSS_BRD( 921600.0) },
    { 1840000,	IOSS_BRD(1843200.0) },
    {     -1,	              -1    }
};

static inline UInt64 getDebugFlagsTable(OSDictionary *props)
{
    OSNumber *debugProp;
    UInt64    debugFlags = gIOKitDebug;

    debugProp = OSDynamicCast(OSNumber, props->getObject(gIOKitDebugKey));
    if (debugProp)
	debugFlags = debugProp->unsigned64BitValue();

    return debugFlags;
}

#define getDebugFlags() (getDebugFlagsTable(getPropertyTable()));

#define IOLogCond(cond, x) do { if (cond) (IOLog x); } while (0)


//
// Static global data maintainence routines
//
bool IOSerialBSDClientGlobals::isValid()
{
    return (fClients && fNames && fMajor != (unsigned int) -1);
}

#define OSSYM(str) OSSymbol::withCStringNoCopy(str)
IOSerialBSDClientGlobals::IOSerialBSDClientGlobals()
{
    gIOSerialBSDServiceValue = OSSYM(kIOSerialBSDServiceValue);
    gIOSerialBSDTypeKey      = OSSYM(kIOSerialBSDTypeKey);
    gIOSerialBSDAllTypes     = OSSYM(kIOSerialBSDAllTypes);
    gIOSerialBSDModemType    = OSSYM(kIOSerialBSDModemType);
    gIOSerialBSDRS232Type    = OSSYM(kIOSerialBSDRS232Type);
    gIOTTYDeviceKey          = OSSYM(kIOTTYDeviceKey);
    gIOTTYBaseNameKey        = OSSYM(kIOTTYBaseNameKey);
    gIOTTYSuffixKey          = OSSYM(kIOTTYSuffixKey);
    gIOCalloutDeviceKey      = OSSYM(kIOCalloutDeviceKey);
    gIODialinDeviceKey       = OSSYM(kIODialinDeviceKey);
    gIOTTYWaitForIdleKey     = OSSYM(kIOTTYWaitForIdleKey);

    fMajor = (unsigned int) -1;
    fNames = OSDictionary::withCapacity(4);
    fLastMinor = 4;
    fClients = (IOSerialBSDClient **)
                        IOMalloc(fLastMinor * sizeof(fClients[0]));
    if (fClients && fNames) {
        bzero(fClients, fLastMinor * sizeof(fClients[0]));
        fMajor = cdevsw_add(-1, &IOSerialBSDClient::devsw);
    }

    if (!isValid())
        IOLog("IOSerialBSDClient didn't initialize");
}
#undef OSSYM

IOSerialBSDClientGlobals::~IOSerialBSDClientGlobals()
{
    SAFE_RELEASE(gIOSerialBSDServiceValue);
    SAFE_RELEASE(gIOSerialBSDTypeKey);
    SAFE_RELEASE(gIOSerialBSDAllTypes);
    SAFE_RELEASE(gIOSerialBSDModemType);
    SAFE_RELEASE(gIOSerialBSDRS232Type);
    SAFE_RELEASE(gIOTTYDeviceKey);
    SAFE_RELEASE(gIOTTYBaseNameKey);
    SAFE_RELEASE(gIOTTYSuffixKey);
    SAFE_RELEASE(gIOCalloutDeviceKey);
    SAFE_RELEASE(gIODialinDeviceKey);
    SAFE_RELEASE(gIOTTYWaitForIdleKey);
    SAFE_RELEASE(fNames);
    if (fMajor != (unsigned int) -1)
        cdevsw_remove(fMajor, &IOSerialBSDClient::devsw);
    if (fClients)
        IOFree(fClients, fLastMinor * sizeof(fClients[0]));
}

dev_t IOSerialBSDClientGlobals::assign_dev_t()
{
    AutoKernelFunnel funnel;	// Grab kernel funnel
    unsigned int i;

    for (i = 0; i < fLastMinor && fClients[i]; i++)
        ;

    if (i == fLastMinor)
    {
        unsigned int newLastMinor = fLastMinor + 4;
        IOSerialBSDClient **newClients;

        newClients = (IOSerialBSDClient **)
                    IOMalloc(newLastMinor * sizeof(fClients[0]));
        if (!newClients)
            return (dev_t) -1;

        bzero(&newClients[fLastMinor], 4 * sizeof(fClients[0]));
        bcopy(fClients, newClients, fLastMinor * sizeof(fClients[0]));
        IOFree(fClients, fLastMinor * sizeof(fClients[0]));
        fLastMinor = newLastMinor;
        fClients = newClients;
    }

    dev_t dev = makedev(fMajor, i << TTY_NUM_FLAGS);
    fClients[i] = (IOSerialBSDClient *) -1;

    return dev;
}

bool IOSerialBSDClientGlobals::
registerTTY(dev_t dev, IOSerialBSDClient *client)
{
    AutoKernelFunnel funnel;	// Grab kernel funnel

    bool ret = false;
    unsigned int i = TTY_UNIT(dev);

    assert(i < fLastMinor);
    if (i < fLastMinor) {
        assert(!client || fClients[i] != (IOSerialBSDClient *) -1);
        if (client && fClients[i] == (IOSerialBSDClient *) -1) {
            fClients[i] = client;
            ret = true;
        }
    }

    return ret;
}

// Assumes the caller has grabbed the funnel if necessary.
// Any call from UNIX is funnelled, I need to funnel any IOKit upcalls
// explicitly.
IOSerialBSDClient *IOSerialBSDClientGlobals::getClient(dev_t dev)
{
    return fClients[TTY_UNIT(dev)];
}

const OSSymbol *IOSerialBSDClientGlobals::
getUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix, dev_t dev)
{
    AutoKernelFunnel funnel;	// Grab kernel funnel

    OSSet *suffixSet = 0;
    
    do {
        // Do we have this name already registered?
        suffixSet = (OSSet *) fNames->getObject(inName);
        if (!suffixSet) {
            suffixSet = OSSet::withCapacity(4);
            if (!suffixSet) {
                suffix = 0;
                break;
            }

            suffixSet->setObject((OSObject *) suffix);
            if (!fNames->setObject(inName, suffixSet))
                suffix = 0;

            suffixSet->release();
            break;
        }

        // Have we seen this suffix before?
        if (!suffixSet->containsObject((OSObject *) suffix)) {
            // Nope so add it to the list of suffixes we HAVE seen.
            if (!suffixSet->setObject((OSObject *) suffix))
                suffix = 0;
            break;
        }

        // We have seen it before so we have to generate a new suffix
        // I'm going to use the unit as an unique index for this run
        // of the OS.
        char ind[8]; // 23 bits, 7 decimal digits + '\0'
        sprintf(ind, "%d", TTY_UNIT(dev));

        suffix = OSSymbol::withCString(ind);
        if (!suffix)
            break;

        // What about this suffix then?
        if (suffixSet->containsObject((OSObject *) suffix) // Been there before?
        || !suffixSet->setObject((OSObject *) suffix)) {
            suffix->release();	// Now what?
            suffix = 0;
        }
        if (suffix)
            suffix->release();	// Release the creation reference
    } while(false);

    return suffix;
}

void IOSerialBSDClientGlobals::
releaseUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix)
{
    AutoKernelFunnel funnel;	// Grab kernel funnel

    OSSet *suffixSet;

    suffixSet = (OSSet *) fNames->getObject(inName);
    if (suffixSet)
        suffixSet->removeObject((OSObject *) suffix);
}

bool IOSerialBSDClient::
createDevNodes()
{
    bool ret = false;
    OSData *tmpData;
    OSString *deviceKey = 0, *calloutName = 0, *dialinName = 0;
    void *calloutNode = 0, *dialinNode = 0;
    const OSSymbol *nubName, *suffix;

    // Convert the provider's base name to an OSSymbol if necessary
    nubName = (const OSSymbol *) fProvider->getProperty(gIOTTYBaseNameKey);
    if (!nubName || !OSDynamicCast(OSSymbol, (OSObject *) nubName)) {
        if (nubName)
            nubName = OSSymbol::withString((OSString *) nubName);
        else
            nubName = OSSymbol::withCString("");
        if (!nubName)
            return false;
        ret = fProvider->setProperty(gIOTTYBaseNameKey, (OSObject *) nubName);
        nubName->release();
        if (!ret)
            return false;
    }

    // Convert the provider's suffix to an OSSymbol if necessary
    suffix = (const OSSymbol *) fProvider->getProperty(gIOTTYSuffixKey);
    if (!suffix || !OSDynamicCast(OSSymbol, (OSObject *) suffix)) {
        if (suffix)
            suffix = OSSymbol::withString((OSString *) suffix);
        else
            suffix = OSSymbol::withCString("");
        if (!suffix)
            return false;
        ret = fProvider->setProperty(gIOTTYSuffixKey, (OSObject *) suffix);
        suffix->release();
        if (!ret)
            return false;
    }

    suffix = sBSDGlobals.getUniqueTTYSuffix(nubName, suffix, fBaseDev);
    if (!suffix)
        return false;
    setProperty(gIOTTYSuffixKey,   (OSObject *) suffix);
    setProperty(gIOTTYBaseNameKey, (OSObject *) nubName);

    do {
        int nameLen = nubName->getLength();
        int suffLen = suffix->getLength();
        int devLen  = nameLen + suffLen + 1;

        // Create the device key symbol
        tmpData = OSData::withCapacity(devLen);
        if (tmpData) {
            tmpData->appendBytes(nubName->getCStringNoCopy(), nameLen);
            tmpData->appendBytes(suffix->getCStringNoCopy(), suffLen + 1);
            deviceKey = OSString::
                withCString((char *) tmpData->getBytesNoCopy());
            tmpData->release();
        }
        if (!tmpData || !deviceKey)
            break;

        // Create the calloutName symbol
        tmpData = OSData::withCapacity(devLen + sizeof(TTY_CALLOUT_PREFIX));
        if (tmpData) {
            tmpData->appendBytes(TTY_CALLOUT_PREFIX,
                                 sizeof(TTY_CALLOUT_PREFIX)-1);
            tmpData->appendBytes(deviceKey->getCStringNoCopy(), devLen);
            calloutName = OSString::
                withCString((char *) tmpData->getBytesNoCopy());
            tmpData->release();
        }
        if (!tmpData || !calloutName)
            break;

        // Create the dialinName symbol
        tmpData = OSData::withCapacity(devLen + sizeof(TTY_DIALIN_PREFIX));
        if (tmpData) {
            tmpData->appendBytes(TTY_DIALIN_PREFIX,
                                 sizeof(TTY_DIALIN_PREFIX)-1);
            tmpData->appendBytes(deviceKey->getCStringNoCopy(), devLen);
            dialinName = OSString::
                withCString((char *) tmpData->getBytesNoCopy());
            tmpData->release();
        }
        if (!tmpData || !dialinName)
            break;

        // Create the device nodes
        calloutNode = devfs_make_node(fBaseDev | TTY_CALLOUT_INDEX,
            DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
            (char *) calloutName->getCStringNoCopy() + sizeof(TTY_DEVFS_PREFIX) - 1);
        dialinNode = devfs_make_node(fBaseDev | TTY_DIALIN_INDEX,
            DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
            (char *) dialinName->getCStringNoCopy() + sizeof(TTY_DEVFS_PREFIX) - 1);
        if (!calloutNode || !dialinNode)
            break;

        // Always reset the name of our provider
        if (!setProperty(gIOTTYDeviceKey,     (OSObject *) deviceKey)
        ||  !setProperty(gIOCalloutDeviceKey, (OSObject *) calloutName)
        ||  !setProperty(gIODialinDeviceKey,  (OSObject *) dialinName))
            break;


        fSessions[TTY_DIALIN_INDEX].fCDevNode  = dialinNode;  dialinNode = 0;
        fSessions[TTY_CALLOUT_INDEX].fCDevNode = calloutNode; calloutNode = 0;
        ret = true;
        
    } while(false);

    SAFE_RELEASE(deviceKey);
    SAFE_RELEASE(calloutName);
    SAFE_RELEASE(dialinName);
    if (dialinNode)
        devfs_remove(dialinNode);
    if (calloutNode)
        devfs_remove(calloutNode);

    return ret;
}

bool IOSerialBSDClient::
setBaseTypeForDev()
{
    const OSMetaClass *metaclass;
    const OSSymbol *name;
    static const char *streamTypeNames[] = {
        "IOSerialStream", "IORS232SerialStream", "IOModemSerialStream", 0
    };

    // Walk through the provider super class chain looking for an
    // interface but stop at IOService 'cause that aint a IOSerialStream.
    for (metaclass = fProvider->getMetaClass();
         metaclass && metaclass != IOService::metaClass;
         metaclass = metaclass->getSuperClass())
    {
        for (int i = 0; streamTypeNames[i]; i++) {
            const char *trial = streamTypeNames[i];

            // Check if class is prefixed by this name
            // Prefix 'cause IO...Stream & IO...StreamSync
            // should both match and if I just check for the prefix they will
            if (!strncmp(metaclass->getClassName(), trial, strlen(trial))) {
                bool ret = false;

                name = OSSymbol::withCStringNoCopy(trial);
                if (name) {
                    ret = setProperty(gIOSerialBSDTypeKey, (OSObject *) name);
                    name->release();
                }
                return ret;
            }
        }
    }
    return false;
}

bool IOSerialBSDClient::
start(IOService *provider)
{
    fBaseDev = -1;
    if (!super::start(provider))
        return false;

    if (!sBSDGlobals.isValid())
        return false;

    fProvider = OSDynamicCast(IOSerialStreamSync, provider);
    if (!fProvider)
        return false;

    /*
     * First initialise the dial in device.  
     * We don't use all the flags from <sys/ttydefaults.h> since they are
     * only relevant for logins.
     */
    fSessions[TTY_DIALIN_INDEX].fThis = this;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_iflag = 0;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_oflag = 0;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_cflag = TTYDEF_CFLAG;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_lflag = 0;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_ispeed
        = fSessions[TTY_DIALIN_INDEX].fInitTerm.c_ospeed = TTYDEF_SPEED;
    termioschars(&fSessions[TTY_DIALIN_INDEX].fInitTerm);

    // Now initialise the call out device
    fSessions[TTY_CALLOUT_INDEX].fThis = this;
    fSessions[TTY_CALLOUT_INDEX].fInitTerm
        = fSessions[TTY_DIALIN_INDEX].fInitTerm;

    do {
        fBaseDev = sBSDGlobals.assign_dev_t();
        if ((dev_t) -1 == fBaseDev)
            break;

        if (!createDevNodes())
            break;

        if (!setBaseTypeForDev())
            break;

        if (!sBSDGlobals.registerTTY(fBaseDev, this))
            break;

        // Let userland know that this serial port exists
        registerService();

        return true;
    } while (0);

    // Failure path
    stop(provider);	// Delete everything we have done till now
    return false;
}

static inline const char *devName(IORegistryEntry *self)
{
    OSString *devNameStr = ((OSString *) self->getProperty(gIOTTYDeviceKey));
    assert(devNameStr);

    return devNameStr->getCStringNoCopy();
}

bool IOSerialBSDClient::
matchPropertyTable(OSDictionary *table)
{
    bool matched;
    OSString *desiredType;
    OSObject *desiredTypeObj;
    const OSMetaClass *providerClass;
    unsigned int desiredLen;
    const char *desiredName;
    bool logMatch = (0 != (kIOLogMatch & getDebugFlagsTable(table)));

    if (!super::matchPropertyTable(table)) {
        IOLogCond(logMatch, ("TTY.%s: Failed superclass match\n",
                             devName(this)));
        return false;	// One of the name based matches has failed, thats it.
    }

    // Do some name matching
    matched = compareProperty(table, gIOTTYDeviceKey)
           && compareProperty(table, gIOTTYBaseNameKey)
           && compareProperty(table, gIOTTYSuffixKey)
           && compareProperty(table, gIOCalloutDeviceKey)
           && compareProperty(table, gIODialinDeviceKey);
    if (!matched) {
        IOLogCond(logMatch, ("TTY.%s: Failed non type based match\n",
                             devName(this)));
        return false;	// One of the name based matches has failed, thats it.
    }

    // The name matching is valid, so if we don't have a type based match
    // then we have no further matching to do and should return true.
    desiredTypeObj = table->getObject(gIOSerialBSDTypeKey);
    if (!desiredTypeObj)
        return true;

    // At this point we have to check for type based matching.
    desiredType = OSDynamicCast(OSString, desiredTypeObj);
    if (!desiredType) {
        IOLogCond(logMatch, ("TTY.%s: %s isn't an OSString?\n",
                             devName(this),
                             kIOSerialBSDTypeKey));
        return false;
    }
    desiredLen = desiredType->getLength();
    desiredName = desiredType->getCStringNoCopy();

    // Walk through the provider super class chain looking for an
    // interface but stop at IOService 'cause that aint a IOSerialStream.
    for (providerClass = fProvider->getMetaClass();
         providerClass && providerClass != IOService::metaClass;
         providerClass = providerClass->getSuperClass())
    {
        // Check if provider class is prefixed by desiredName
        // Prefix 'cause IOModemSerialStream & IOModemSerialStreamSync
        // should both match and if I just look for the prefix they will
        if (!strncmp(providerClass->getClassName(), desiredName, desiredLen))
            return true;
    }

    // Couldn't find the desired name in the super class chain
    // so report the failure and return false
    IOLogCond(logMatch, ("TTY.%s: doesn't have a %s interface\n",
            devName(this),
            desiredName));
    return false;
}

void IOSerialBSDClient::
stop(IOService *provider)
{
    {
	AutoKernelFunnel funnel;	// Take kernel funnel

        for (int i = 0; i < TTY_NUM_TYPES; i++) {
            Session *sp = &fSessions[i];
            struct tty *tp = &sp->ftty;
    
            // Now kill any stream that may currently be running
            sp->fErrno = ENXIO;
    
            // Enforce a zombie and unconnected state on the discipline
            SET(tp->t_state, TS_ZOMBIE);
            CLR(tp->t_state, TS_CONNECTED);
    
            // Flush out any sleeping threads
            ttyflush(tp, FREAD | FWRITE);
        }

        fActiveSession = 0;
        fKillThreads = true;
        fProvider->releasePort();
    }

    super::stop(provider);
}

void IOSerialBSDClient::free()
{
    {
	AutoKernelFunnel funnel;	// Take kernel funnel

	if ((dev_t) -1 != fBaseDev)
	    sBSDGlobals.registerTTY(fBaseDev, 0);
    }

    super::free();
}

bool IOSerialBSDClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    {
        AutoKernelFunnel funnel;	// Take kernel funnel
    
        if ((dev_t) -1 != fBaseDev) {
            sBSDGlobals.releaseUniqueTTYSuffix((const OSSymbol *) getProperty(gIOTTYBaseNameKey),
                                               (const OSSymbol *) getProperty(gIOTTYSuffixKey)	);
        }
    
        if (fSessions[TTY_CALLOUT_INDEX].fCDevNode)
            devfs_remove(fSessions[TTY_CALLOUT_INDEX].fCDevNode);
        if (fSessions[TTY_DIALIN_INDEX].fCDevNode)
            devfs_remove(fSessions[TTY_DIALIN_INDEX].fCDevNode);
    }  
    return super::didTerminate(provider, options, defer);
}

IOReturn IOSerialBSDClient::
setOneProperty(const OSSymbol *key, OSObject *value)
{
    if (key == gIOTTYWaitForIdleKey) {
        int error = waitForIdle();
        if (ENXIO == error)
            return kIOReturnOffline;
        else if (error)
            return kIOReturnAborted;
        else
            return kIOReturnSuccess;
    }
    
    return kIOReturnUnsupported;
}

IOReturn IOSerialBSDClient::
setProperties(OSObject *properties)
{
    IOReturn res = kIOReturnBadArgument;

    if (OSDynamicCast(OSString, properties)) {
        const OSSymbol *propSym =
            OSSymbol::withString((OSString *) properties);
        res = setOneProperty(propSym, 0);
        propSym->release();
    }
    else if (OSDynamicCast(OSDictionary, properties)) {
        const OSDictionary *dict = (const OSDictionary *) properties;
        OSCollectionIterator *keysIter;
        const OSSymbol *key;

        keysIter = OSCollectionIterator::withCollection(dict);
        if (!keysIter) {
            res = kIOReturnNoMemory;
            goto bail;
        }

        while ( (key = (const OSSymbol *) keysIter->getNextObject()) ) {
            res = setOneProperty(key, dict->getObject(key));
            if (res)
                break;
        }
        
        keysIter->release();
    }

bail:
    return res;		// Successfull just return now
}

// Bracket all open attempts with a reference on ourselves. 
int IOSerialBSDClient::
iossopen(dev_t dev, int flags, int devtype, struct proc *p)
{
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);

    if (!me || me->isInactive())
        return ENXIO;

    me->retain();
    int ret = me->open(dev, flags, devtype, p);
    me->release();

    return ret;
}

int IOSerialBSDClient::
iossclose(dev_t dev, int flags, int devtype, struct proc *p)
{
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);

    assert(me);

    me->close(dev, flags, devtype, p);

    // Remember this is the last close so we may have to delete ourselves
    // This reference is held just before we opened the line discipline
    // in open().
    me->release();

    return 0;
}

int IOSerialBSDClient::
iossread(dev_t dev, struct uio *uio, int ioflag)
{
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
    int error;
    struct tty *tp;
    Session *sp;

    assert(me);

    sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    tp = &sp->ftty;

    error = sp->fErrno;
    if (!error) {
        error = (*linesw[(int) tp->t_line].l_read)(tp, uio, ioflag);
        if (me->frxBlocked && TTY_QUEUESIZE(tp) < TTY_LOWWATER)
            me->sessionSetState(sp, PD_S_RX_EVENT, PD_S_RX_EVENT);
    }

    return error;
}

int IOSerialBSDClient::
iosswrite(dev_t dev, struct uio *uio, int ioflag)
{
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
    int error;
    struct tty *tp;
    Session *sp;

    assert(me);

    sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    tp = &sp->ftty;

    error = sp->fErrno;
    if (!error)
        error = (*linesw[(int) tp->t_line].l_write)(tp, uio, ioflag);

    return error;
}

int IOSerialBSDClient::
iossselect(dev_t dev, int which, void *wql, struct proc *p)
{
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
    int error;
    struct tty *tp;
    Session *sp;

    assert(me);

    sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    tp = &sp->ftty;

    error = sp->fErrno;
    if (!error)
        error = ttyselect(tp, which, wql, p);

    return error;
}

static inline int
tiotors232(int bits)
{
    int out_b = bits;

    out_b &= ( PD_RS232_S_DTR | PD_RS232_S_RFR | PD_RS232_S_CTS
	     | PD_RS232_S_CAR | PD_RS232_S_BRK );
    return out_b;
}

static inline int
rs232totio(int bits)
{
    u_long out_b = bits;

    out_b &= ( PD_RS232_S_DTR | PD_RS232_S_DSR
             | PD_RS232_S_RFR | PD_RS232_S_CTS
             | PD_RS232_S_BRK | PD_RS232_S_CAR  | PD_RS232_S_RNG);
    return out_b;
}

int IOSerialBSDClient::
iossioctl(dev_t dev, u_long cmd, caddr_t data, int fflag,
                         struct proc *p)
{
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
    int error = 0;
    struct tty *tp;
    Session *sp;

    assert(me);

    sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    tp = &sp->ftty;

    if (sp->fErrno) {
        error = sp->fErrno;
        goto exitIoctl;
    }

    /*
     * tty line disciplines return >= 0 if they could process this
     * ioctl request.  If so, simply return, we're done
     */
    error = (*linesw[(int) tp->t_line].l_ioctl)(tp, cmd, data, fflag, p);
    if (error >= 0) {
        me->optimiseInput(&tp->t_termios);
        goto exitIoctl;
    }

    // ...->l_ioctl may block so we need to check out state again
    if (sp->fErrno) {
        error = sp->fErrno;
        goto exitIoctl;
    }

    if (TIOCGETA == cmd) {
        bcopy(&tp->t_termios, data, sizeof(struct termios));
        me->convertFlowCtrl(sp, (struct termios *) data);
        error = 0;
        goto exitIoctl;
    }

    /* First pre-process and validate ioctl command */
    switch(cmd)
    {
    case TIOCSETA:
    case TIOCSETAW:
    case TIOCSETAF:
    {
        struct termios *dt = (struct termios *) data;

        /* XXX gvdl
         * The lock device code should go in here but the FreeBSD code is
         * bogus.  It can only set flags it can't clear them, because it
         * doesn't have the concept of a mask.  As a result I hove
         * hosen not to implement the lock device at this time.
         */

        /* Convert the PortSessionSync's flow control setting to termios */
        me->convertFlowCtrl(sp, &tp->t_termios);

        /*
         * Check to see if we are trying to disable either the start or
         * stop character at the same time as using the XON/XOFF character
         * based flow control system.  This is not implemented in the
         * current PortDevices protocol.
         */
        if (ISSET(dt->c_cflag, CIGNORE)
        &&  ISSET(tp->t_iflag, (IXON|IXOFF))
        && ( dt->c_cc[VSTART] == _POSIX_VDISABLE
                || dt->c_cc[VSTOP]  == _POSIX_VDISABLE ) )
        {
            error = EINVAL;
            goto exitIoctl;
        }
        break;
    }

    case TIOCEXCL:
        // Fore the TIOCEXCL ioctl to be atomic!
        if (ISSET(tp->t_state, TS_XCLUDE)) {
            error = EBUSY;
            goto exitIoctl;
        }
        break;

    default:
        break;
    }

    /* See if generic tty understands this. */
    if ((error = ttioctl(tp, cmd, data, fflag, p)) >= 0) {
        if (error > 0)
            iossparam(tp, &tp->t_termios);	/* reestablish old state */
        me->optimiseInput(&tp->t_termios);
	goto exitIoctl;        
    }

    // ttioctl may block so we need to check out state again
    if (sp->fErrno) {
        error = sp->fErrno;
        goto exitIoctl;
    }

    //
    // The generic ioctl handler doesn't know what the hell is going on
    // so try to interpret them here.
    //
    error = 0;	// clear the error condition for now.
    switch (cmd)
    {
    case TIOCSBRK:
        (void) me->mctl(PD_RS232_S_BRK, DMBIS);  break;
    case TIOCCBRK:
        (void) me->mctl(PD_RS232_S_BRK, DMBIC);  break;
    case TIOCSDTR:
        (void) me->mctl(PD_RS232_S_DTR, DMBIS);  break;
    case TIOCCDTR:
        (void) me->mctl(PD_RS232_S_DTR, DMBIC);  break;

    case TIOCMSET:
        (void) me->mctl(tiotors232(*(int *)data), DMSET);  break;
    case TIOCMBIS:
        (void) me->mctl(tiotors232(*(int *)data), DMBIS);  break;
    case TIOCMBIC:
        (void) me->mctl(tiotors232(*(int *)data), DMBIC);  break;
    case TIOCMGET:
        *(int *)data = rs232totio(me->mctl(0,     DMGET)); break;

    case IOSSDATALAT:
        (void) me->sessionExecuteEvent(sp, PD_E_DATA_LATENCY, *(UInt32 *) data);
        break;

    case IOSSPREEMPT:
        me->fPreemptAllowed = (bool) (*(int *) data);
        if (me->fPreemptAllowed)
            me->fLastUsedTime = kNever;
        else
            wakeup(&me->fPreemptAllowed);	// Wakeup any pre-empters
        break;

    default:
        error = ENOTTY; break;
    }

exitIoctl:
    /*
     * These flags functionality has been totally subsumed by the PortDevice
     * driver so make sure they always get cleared down before any serious
     * work is done.
     */
    CLR(tp->t_iflag, IXON | IXOFF | IXANY);
    CLR(tp->t_cflag, CRTS_IFLOW | CCTS_OFLOW);

    return error;
}


void IOSerialBSDClient::
iossstart(struct tty *tp)
{
    Session *sp = (Session *) tp;
    IOSerialBSDClient *me = sp->fThis;
    IOReturn rtn;

    assert(me);
    if (sp->fErrno)
	return;

    if ( !me->fIstxEnabled && !ISSET(tp->t_state, TS_TTSTOP) ) {
        me->fIstxEnabled = true;
        me->sessionSetState(sp, -1, PD_S_TX_ENABLE);
    }

    if  (tp->t_outq.c_cc) {
        // Notify the transmit thread of action to be performed
	rtn = me->sessionSetState(sp, PD_S_TX_EVENT, PD_S_TX_EVENT);
	assert(!rtn || rtn != kIOReturnOffline);
    }
}

int IOSerialBSDClient::
iossstop(struct tty *tp, int rw)
{
    Session *sp = (Session *) tp;
    IOSerialBSDClient *me = sp->fThis;

    assert(me);
    if (sp->fErrno)
	return 0;

    if ( ISSET(tp->t_state, TS_TTSTOP) ) {
	me->fIstxEnabled = false;
	me->sessionSetState(sp, 0, PD_S_TX_ENABLE);
    }

    if ( ISSET(rw, FWRITE) )
        me->sessionExecuteEvent(sp, PD_E_TXQ_FLUSH, 0);
    if ( ISSET(rw, FREAD) ) {
        me->sessionExecuteEvent(sp, PD_E_RXQ_FLUSH, 0);
        if (me->frxBlocked)	// wake up a blocked reader
            me->sessionSetState(sp, PD_S_RX_ENABLE, PD_S_RX_ENABLE);
    }
    return 0;
}

/*
 * Parameter control functions
 */
int IOSerialBSDClient::
iossparam(struct tty *tp, struct termios *t)
{
    Session *sp = (Session *) tp;
    IOSerialBSDClient *me = sp->fThis;
    u_long data;
    int cflag, error;
    IOReturn rtn = kIOReturnOffline;

    assert(me);

    if (sp->fErrno)
	goto exitParam;

    rtn = kIOReturnBadArgument;
    if (ISSET(t->c_iflag, (IXOFF|IXON))
    && (t->c_cc[VSTART]==_POSIX_VDISABLE || t->c_cc[VSTOP]==_POSIX_VDISABLE))
        goto exitParam;

    /* do historical conversions */
    if (t->c_ispeed == 0)
        t->c_ispeed = t->c_ospeed;

    /* First check to see if the requested speed is one of our valid ones */
    data = ttspeedtab(t->c_ospeed, iossspeeds);

    if ((int) data != -1 && t->c_ispeed == t->c_ospeed)
	rtn  = me->sessionExecuteEvent(sp, PD_E_DATA_RATE, data);
    else if ( (IOSS_HALFBIT_BRD & t->c_ospeed) ) {
	/*
	 * MIDI clock speed multipliers are used for externally clocked MIDI
	 * devices, and are evident by a 1 in the low bit of c_ospeed/c_ispeed
	 */
	data = (u_long) t->c_ospeed >> 1;	// set data to MIDI clock mode 
	rtn  = me->sessionExecuteEvent(sp, PD_E_EXTERNAL_CLOCK_MODE, data);
    }
    if (rtn)
        goto exitParam;

    /*
     * Setup SCC as for data and character len
     * Note: ttycharmask is anded with both transmitted and received
     * characters.
     */
    cflag = t->c_cflag;
    switch (cflag & CSIZE) {
    case CS5:       data = 5 << 1; break;
    case CS6:	    data = 6 << 1; break;
    case CS7:	    data = 7 << 1; break;
    default:	    /* default to 8bit setup */
    case CS8:	    data = 8 << 1; break;
    }
    rtn  = me->sessionExecuteEvent(sp, PD_E_DATA_SIZE, data);
    if (rtn)
        goto exitParam;


    data = PD_RS232_PARITY_NONE;
    if ( ISSET(cflag, PARENB) ) {
        if ( ISSET(cflag, PARODD) )
            data = PD_RS232_PARITY_ODD;
        else
            data = PD_RS232_PARITY_EVEN;
    }
    rtn = me->sessionExecuteEvent(sp, PD_E_DATA_INTEGRITY, data);
    if (rtn)
        goto exitParam;

    /* Set stop bits to 2 1/2 bits in length */
    if (ISSET(cflag, CSTOPB))
        data = 4;
    else
        data = 2;
    rtn = me->sessionExecuteEvent(sp, PD_RS232_E_STOP_BITS, data);
    if (rtn)
        goto exitParam;

    //
    // Reset the Flow Control values
    //
    data = 0;
    if ( ISSET(t->c_iflag, IXON) )
        SET(data, PD_RS232_A_TXO);
    if ( ISSET(t->c_iflag, IXANY) )
        SET(data, PD_RS232_A_XANY);
    if ( ISSET(t->c_iflag, IXOFF) )
        SET(data, PD_RS232_A_RXO);

    if ( ISSET(cflag, CRTS_IFLOW) )
        SET(data, PD_RS232_A_RFR);
    if ( ISSET(cflag, CCTS_OFLOW) )
        SET(data, PD_RS232_A_CTS);
    if ( ISSET(cflag, CDTR_IFLOW) )
        SET(data, PD_RS232_A_DTR);
    CLR(t->c_iflag, IXON | IXOFF | IXANY);
    CLR(t->c_cflag, CRTS_IFLOW | CCTS_OFLOW);
    rtn = me->sessionExecuteEvent(sp, PD_E_FLOW_CONTROL, data);
    if (rtn)
        goto exitParam;

    //
    // Load the flow control start and stop characters.
    //
    rtn  = me->sessionExecuteEvent(sp, PD_RS232_E_XON_BYTE,  t->c_cc[VSTART]);
    rtn |= me->sessionExecuteEvent(sp, PD_RS232_E_XOFF_BYTE, t->c_cc[VSTOP]);
    if (rtn)
        goto exitParam;

    /* Always enable for transmission */
    me->fIstxEnabled = true;
    rtn = me->sessionSetState(sp, PD_S_TX_ENABLE, PD_S_TX_ENABLE);
    if (rtn)
        goto exitParam;

    /* Only enable reception if necessary */
    if ( ISSET(cflag, CREAD) )
        rtn = me->sessionSetState(sp, -1, PD_S_RX_ENABLE);
    else
        rtn = me->sessionSetState(sp,  0, PD_S_RX_ENABLE);

exitParam:
    if (kIOReturnSuccess == rtn)
        error = 0;
    else if (kIOReturnOffline == rtn)
        error = sp->fErrno;
    else
        error = EINVAL;

    return error;
}


/*
 * Decision Tables for open semantic
 *
 * The Exact semantic to be used when open serial ports is very complicated.
 * We have nasty combinations of ports opened exclusively but pre-emptible while
 * a root user tries to open the port.  Anyway all of the states that are
 * interesting are listed below with pseudo code that implements the tables.
 *
 * The states across the top are for desired state.  Vertical for the current
 * open port's state, with State prefix:- ' ' true, '!' not true, 'x' dont care
 *
 *  Results
 * 	B => Block   E => Error Busy   S => Success   P => Pre-empt
 *
 * OPEN port was open and is not waiting for carrier
 * EXCL port is open and desires exclusivity
 * PREM port is open and is pre-emptible
 *
 * Callout Decision table
 *
 *         CALLOUT | 0 | 0 | 1 | 1 | 1 | 1 |
 *        NONBLOCK | 0 | 1 | 0 | 0 | 1 | 1 |
 *            ROOT | x | x | 0 | 1 | 0 | 1 |
 * -----------------------------------------
 *      !EXCL      | Bi| E | S | S | S | S |
 *       EXCL      | Bi| E | E | S | E | S |
 *
 * Is Callout Open
 *     if (wantCallout) {
 *         if (isExclusive && !wantSuser)
 *             return BUSY;
 *         else
 *             ; // Success;
 *     }
 *     else {
 * checkAndWaitForIdle:
 *         if (wantNonBlock)
 *             return BUSY;
 *         else {
 *             waitForIdle;
 *             goto checkBusy;
 *         }
 *     }
 *
 * Dial out Table
 *
 *         CALLOUT | 0 | 0 | 1 | 1 | 1 | 1 |
 *        NONBLOCK | 0 | 1 | 0 | 0 | 1 | 1 |
 *            ROOT | x | x | 0 | 1 | 0 | 1 |
 * ----------------------------------------
 * !OPENxEXCLxPREM | S | S | P | P | P | P |
 *  OPEN!EXCL!PREM | S | S | Bi| S | E | S |
 *  OPEN!EXCL PREM | S | S | P | P | P | P |
 *  OPEN EXCL!PREM | E | E | Bi| S | E | S |
 *  OPEN EXCL PREM | E | E | P | P | P | P |
 *
 * Is Dialout Waiting for carrier
 *     if (wantCallout)
 *         preempt;
 *     else 
 *         // Success Wait for carrier later on
 *
 * Is Dialout open
 *     if (wantCallout) {
 *         if (isPreempt)
 *             preempt;
 *         else if (!wantSuser)
 *             goto checkAndWaitForIdle;
 *         else
 *         	; // Success
 *     }
 *     else {
 *         if (isExclusive)
 *             return BUSY;
 *         else
 *         	; // Success
 *     }
 *
 */

int IOSerialBSDClient::
open(dev_t dev, int flags, int devtype, struct proc *p)
{
    Session *sp;
    struct tty *tp;
    int error = 0;
    bool wantNonBlock = flags & O_NONBLOCK;
    bool imPreempting = false;
    bool firstOpen = false;

checkBusy:
    if (isInactive()) {
        error = ENXIO;
        goto exitOpen;
    }

    // Check to see if the currently active device has been pre-empted.
    // If the device has been preempted then we have to wait for the
    // current owner to close the port.  And THAT means we have to return 
    // from this open otherwise UNIX doesn't deign to inform us when the
    // other process DOES close the port.  Welcome to UNIX being helpful.
    sp = &fSessions[IS_TTY_OUTWARD(dev)];
    if (sp->fErrno == EBUSY) {
        error = EBUSY;
        goto exitOpen;
    }

    // Can't call startConnectTransit as we need to make sure that
    // the device hasn't been hot unplugged while we were waiting.
    if (!imPreempting && fConnectTransit) {
        tsleep((caddr_t) this, TTIPRI, "ttyopn", 0);
        goto checkBusy;
    }
    fConnectTransit = true;

    // Check to see if the device is already open, which means we have an 
    // active session
    if (fActiveSession) {
        tp = &fActiveSession->ftty;

        bool isCallout    = IS_TTY_OUTWARD(tp->t_dev);
        bool isPreempt    = fPreemptAllowed;
        bool isExclusive  = ISSET(tp->t_state, TS_XCLUDE);
        bool isOpen       = ISSET(tp->t_state, TS_ISOPEN);
        bool wantCallout  = IS_TTY_OUTWARD(dev);
        bool wantSuser    = !suser(p->p_ucred, &p->p_acflag);

        if (isCallout) {
            // Is Callout and must be open
            if (wantCallout) {
                if (isExclusive && !wantSuser) {
                    //
                    // @@@ - UNIX doesn't allow us to block the open
                    // until the current session idles if they have the
                    // same dev_t.  The opens are reference counted
                    // this means that I must return an error and tell
                    // the users to use IOKit.
                    //
                    error = EBUSY;
                    goto exitOpen;
                }
                else
                    ; // Success - use current session
            }
            else {
checkAndWaitForIdle:
                if (wantNonBlock) {
                    error = EBUSY;
                    goto exitOpen;
                } else {
                    endConnectTransit();
                    error = waitForIdle();
                    if (error)
                        return error;	// No transition to clean up
                    goto checkBusy;
                }
            }
        }
        else if (isOpen) {
            // Is dial in and open
            if (wantCallout) {
                if (isPreempt) {
                    imPreempting = true;
                    preemptActive();
                    goto checkBusy;
                }
                else if (!wantSuser)
                    goto checkAndWaitForIdle;
                else
                    ; // Success - use current session (root override)
            }
            else {
                // Want dial in connection
                if (isExclusive) {
                    //
                    // @@@ - UNIX doesn't allow us to block the open
                    // until the current session idles if they have the
                    // same dev_t.  The opens are reference counted
                    // this means that I must return an error and tell
                    // the users to use IOKit.
                    //
                    error = EBUSY;
                    goto exitOpen;
                }
                else
                    ; // Success - use current session
            }
        }
        else {
            // Is dial in and blocking for carrier, i.e. not open
            if (wantCallout) {
                imPreempting = true;
                preemptActive();
                goto checkBusy;
            }
            else
                ; // Successful, will wait for carrier later
        }
    }

    // If we are here then we have successfully run the open gauntlet.
    tp = &sp->ftty;

    // If there is no active session that means that we have to acquire
    // the serial port.
    if (!fActiveSession) {
        IOReturn rtn = fProvider->acquirePort(/* sleep */ false);
        bool offline = isInactive();
        if (offline || kIOReturnSuccess != rtn) {
            error = (offline)? ENXIO : EBUSY;
            goto exitOpen;
        }
        else
            fActiveSession = sp;
    }

    /*
     * Initialize Unix's tty struct,
     * set device parameters and RS232 state
     */
    if ( !ISSET(tp->t_state, TS_ISOPEN) ) {
        initSession(sp);

        // Initialise the line state
        iossparam(tp, &tp->t_termios);
    }

    /*
     * Handle DCD:
     * If outgoing or not hw dcd or dcd is asserted, then continue.
     * Otherwise, block till dcd is asserted or open fPreempt.
     */
    if (IS_TTY_OUTWARD(dev)
    ||  ISSET(sessionGetState(sp), PD_RS232_S_CAR) ) {
        (*linesw[tp->t_line].l_modem)(tp, 1);
    }

    if (!IS_TTY_OUTWARD(dev) && !ISSET(flags, FNONBLOCK)
    &&  !ISSET(tp->t_state, TS_CARR_ON) && !ISSET(tp->t_cflag, CLOCAL)) {
        IOReturn rtn;
        UInt32 pd_state;

        /* Track DCD Transistion to high */
        fInOpensPending++;
        pd_state = PD_RS232_S_CAR;

        // Clear up the transit while we are waiting for carrier
        endConnectTransit();
        rtn = sessionWatchState(sp, &pd_state, PD_RS232_S_CAR);
        startConnectTransit(); 

        fInOpensPending--; wakeup(&fInOpensPending);

        // Check for an interrupt or a revoke
        if (kIOReturnIPCError == rtn || kIOReturnIOError == rtn) {
            if (!fInOpensPending) {
                fProvider->releasePort();
                fActiveSession = 0;
            }

            // End the connect transit lock and return error
            endConnectTransit();
            
            return EINTR;
        }
        
        // End non-preemptible code

        // Wait for the connection (open()/close()) engine to stabilise
        if (sp->fErrno) {
            sp->fErrno = 0;	// We have been pre-empted;
            endConnectTransit();
            goto checkBusy;
        }

        // Re-establish the connection transition state.
        (*linesw[tp->t_line].l_modem)(tp, 1);	// To be here we must have DCD
    }

    if ( !ISSET(tp->t_state, TS_ISOPEN) ) {
	clalloc(&tp->t_rawq, TTYCLSIZE, 1);
	clalloc(&tp->t_canq, TTYCLSIZE, 1);
	/* output queue doesn't need quoting */
	clalloc(&tp->t_outq, TTYCLSIZE, 0);
	retain();	// Hold a reference until the port is closed
        firstOpen = true;
    }

    // Open line discipline
    error = ((*linesw[(int) tp->t_line].l_open)(dev, tp));
    if (!error && firstOpen)
        launchThreads(); // launch the transmit and receive threads

    if (error) {
        release();
    	clfree(&tp->t_rawq);
	clfree(&tp->t_canq);
	clfree(&tp->t_outq);
    }

exitOpen:
    endConnectTransit();

    return error;
}

void IOSerialBSDClient::
close(dev_t dev, int flags, int devtype, struct proc *p)
{
    struct tty *tp;
    Session *sp;
    IOReturn rtn;

    startConnectTransit();

    sp = &fSessions[IS_TTY_OUTWARD(dev)];
    tp = &sp->ftty;

    if (!tp->t_dev && fInOpensPending) {
	retain();	// Hold a reference until the port is closed
        (void) fProvider->executeEvent(PD_E_ACTIVE, false);
        endConnectTransit();
        while (fInOpensPending)
            tsleep((caddr_t) &fInOpensPending, TTIPRI, "ttyrev", 0);
        return;
    }

    /* We are closing, it doesn't matter now about holding back ... */
    CLR(tp->t_state, TS_TTSTOP);
    
    if (!sp->fErrno) {
        (void) sessionExecuteEvent(sp, PD_E_FLOW_CONTROL, 0);
        (void) sessionSetState(sp, -1, PD_S_RX_ENABLE | PD_S_TX_ENABLE);

        // Clear any outstanding line breaks
        rtn = sessionEnqueueEvent(sp, PD_RS232_E_LINE_BREAK, false, true);
        assert(!rtn || rtn != kIOReturnOffline);
    }

    (*linesw[(int) tp->t_line].l_close)(tp, flags);

    if (!sp->fErrno) {
        if (ISSET(tp->t_cflag, HUPCL) || !ISSET(tp->t_state, TS_ISOPEN)
        || (IS_TTY_PREEMPT(dev, sp->fInitTerm.c_cflag)
            && !ISSET(sessionGetState(sp), PD_RS232_S_CAR)) ) {
            /*
            * XXX we will miss any carrier drop between here and the
            * next open.  Perhaps we should watch DCD even when the
            * port is closed; it is not sufficient to check it at
            * the next open because it might go up and down while
            * we're not watching.
            */
            (void) mctl(RS232_S_OFF, DMSET);
        }
    }

    ttyclose(tp);

    assert(!tp->t_outq.c_cc);

    // Free the data queues
    clfree(&tp->t_rawq);
    clfree(&tp->t_canq);
    clfree(&tp->t_outq);

    // Shut down the port, this will cause the RX && TX threads to terminate
    // Then wait for threads to terminate, this should be over very quickly.

    if (!sp->fErrno)
        killThreads(); // Disable the chip

    if (sp == fActiveSession)
    {
        fProvider->releasePort();
        fPreemptAllowed = false;
        fActiveSession = 0;
        wakeup(&fPreemptAllowed);	// Wakeup any pre-empters
    }

    sp->fErrno = 0;	/* Clear the error condition on last close */
    endConnectTransit();
}

void IOSerialBSDClient::
initSession(Session *sp)
{
    struct tty *tp = &sp->ftty;
    IOReturn rtn;

    tp->t_oproc = iossstart;
    tp->t_param = iossparam;
    tp->t_termios = sp->fInitTerm;
    ttsetwater(tp);

    /* Activate the session's port */
    rtn = sessionExecuteEvent(sp, PD_E_ACTIVE, true);
    if (rtn)
        IOLog("ttyioss%04x: ACTIVE failed (%x)\n", tp->t_dev, rtn);

    rtn  = sessionExecuteEvent(sp, PD_E_TXQ_FLUSH, 0);
    rtn |= sessionExecuteEvent(sp, PD_E_RXQ_FLUSH, 0);
    assert(!rtn || rtn != kIOReturnOffline);

    CLR(tp->t_state, TS_CARR_ON | TS_BUSY);

    fKillThreads = false;
    fDCDDelayTicks = MUSEC2TICK(DCD_DELAY);
#if DCD_DELAY
    if (fDCDDelayTicks < 1)
        fDCDDelayTicks = 1;
#endif /* DCD_DELAY */

    // Cycle the PD_RS232_S_DTR line if necessary 
    if ( !ISSET(fProvider->getState(), PD_RS232_S_DTR) ) {
        (void) waitOutDelay(0, &fDTRDownTime, &kDTRDownDelay);
        (void) mctl(RS232_S_ON, DMSET);
    }

    // Disable all flow control  & data movement initially
    rtn  = sessionExecuteEvent(sp, PD_E_FLOW_CONTROL, 0);
    rtn |= sessionSetState(sp, 0, PD_S_RX_ENABLE | PD_S_TX_ENABLE);
    assert(!rtn || rtn != kIOReturnOffline);

    /* Raise RTS */
    rtn = sessionSetState(sp,  PD_RS232_S_RTS, PD_RS232_S_RTS);
    assert(!rtn || rtn != kIOReturnOffline);
}

bool IOSerialBSDClient::
waitOutDelay(void *event,
             const struct timeval *start, const struct timeval *duration)
{
    struct timeval deltval, *delay = &deltval;

    timeradd(start, duration, delay);	// Delay Till = start + duration
    timersub(delay, &time, delay);	// Delay Duration = now - Delay Till
    if ( delay->tv_sec < 0 || !timerisset(delay) )
        return false;	// Delay expired
    else if (event) {
        unsigned int delayTicks;

        delayTicks = MUSEC2TICK(delay->tv_sec * 1000000 + delay->tv_usec);
        tsleep((caddr_t) event, TTIPRI, "ttydelay", delayTicks);
    }
    else {
        unsigned int delayMS;

        /* Calculate the required delay in milliseconds, rounded up */
        delayMS =  delay->tv_sec * 1000 + (delay->tv_usec + 999) / 1000;

        IOSleep(delayMS);
    }
    return true;	// We did sleep
}

int IOSerialBSDClient::
waitForIdle()
{
    AutoKernelFunnel funnel;	// Take kernel funnel

    while (fActiveSession || fConnectTransit) {
        if (isInactive())
            return ENXIO;

        int error = tsleep((caddr_t) this, TTIPRI | PCATCH, "ttyidl", 0);
        if (error)
            return error;
    }

    return 0;
}

void IOSerialBSDClient::
preemptActive()
{
    // 
    // We are not allowed to pre-empt if the current port has been
    // active recently.  So wait out the delay and if we sleep
    // then we will need to return to check the open conditions again.
    //
    if (waitOutDelay(&fPreemptAllowed, &fLastUsedTime, &kPreemptIdle))
        return;

    Session *sp = fActiveSession;
    struct tty *tp = &sp->ftty;

    sp->fErrno = EBUSY;
    fKillThreads = true;

    // This flag gets reset once we actually take over the session
    // this is done by the open code where it acquires the port
    // obviously we don't need to re-acquire the port as we didn't
    // release it in this case.
    fPreemptAllowed = false;

    // Enforce a zombie and unconnected state on the discipline
    SET(tp->t_state, TS_ZOMBIE);
    CLR(tp->t_state, TS_CONNECTED);

    // Flush out any sleeping threads
    ttyflush(tp, FREAD | FWRITE);

    // Shutdown the open connection
    killThreads();

    while (fInOpensPending)
        tsleep((caddr_t) &fInOpensPending, TTIPRI, "ttypre", 0);

    fActiveSession = 0;
    fProvider->releasePort();	// Release the old session
}

void IOSerialBSDClient::
startConnectTransit()
{
    // Wait for the connection (open()/close()) engine to stabilise
    while (fConnectTransit)
        tsleep((caddr_t) this, TTIPRI, "ttyctr", 0);
    fConnectTransit = true;
}

void IOSerialBSDClient::
endConnectTransit()
{
    // Clear up the transit while we are waiting for carrier
    fConnectTransit = false;
    wakeup(this);
}

void
IOSerialBSDClient::convertFlowCtrl(Session *sp, struct termios *t)
{
    IOReturn rtn;
    u_long flowCtrl;

    //
    // Have to reconstruct the flow control bits
    //
    rtn = sessionRequestEvent(sp, PD_E_FLOW_CONTROL, &flowCtrl);
    assert(!rtn);

    if ( ISSET(flowCtrl, PD_RS232_A_TXO) )
        SET(t->c_iflag, IXON);
    if ( ISSET(flowCtrl, PD_RS232_A_XANY) )
        SET(t->c_iflag, IXANY);
    if ( ISSET(flowCtrl, PD_RS232_A_RXO) )
        SET(t->c_iflag, IXOFF);

    if ( ISSET(flowCtrl, PD_RS232_A_RFR) )
        SET(t->c_cflag, CRTS_IFLOW);
    if ( ISSET(flowCtrl, PD_RS232_A_CTS) )
        SET(t->c_cflag, CCTS_OFLOW);
    if ( ISSET(flowCtrl, PD_RS232_A_DTR) )
        SET(t->c_cflag, CDTR_IFLOW);
}

// @@@ gvdl: Must only call when session is valid, check isInActive as well
int IOSerialBSDClient::
mctl(u_int bits, int how)
{
    u_long oldBits, mbits;
    IOReturn rtn;

    if ( ISSET(bits, PD_RS232_S_BRK) && (how == DMBIS || how == DMBIC) ) {
	oldBits = (how == DMBIS);
	rtn = fProvider->enqueueEvent(PD_RS232_E_LINE_BREAK, oldBits, true);
	if (!rtn && oldBits)
	    rtn = fProvider->enqueueEvent(PD_E_DELAY, BRK_DELAY, true);
	assert(!rtn || rtn != kIOReturnOffline);
	return oldBits;
    }

    bits &= RS232_S_OUTPUTS;
    oldBits = fProvider->getState();

    mbits = oldBits;
    switch (how)
    {
    case DMSET:
	mbits = bits | (mbits & RS232_S_INPUTS);
	break;

    case DMBIS:
	SET(mbits, bits);
	break;

    case DMBIC:
	CLR(mbits, bits);
	break;

    case DMGET:
	return mbits;
    }

    /* Check for a transition for DTR to low and record the down time */
    if ( ISSET(oldBits & ~mbits, PD_RS232_S_DTR) )
 	fDTRDownTime = *(timeval *) &time;

    rtn = fProvider->setState(mbits, RS232_S_OUTPUTS);
    if (rtn)
	IOLog("ttyioss%04x: mctl RS232_S_OUTPUTS failed %x\n",
	    fBaseDev, rtn);


    return mbits;
}

/*
 * Support routines
 */
#define NOBYPASS_IFLAG_MASK   (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON)
#define NOBYPASS_PAR_MASK     (IGNPAR | IGNBRK)
#define NOBYPASS_LFLAG_MASK   (ECHO | ICANON | IEXTEN | ISIG)

void IOSerialBSDClient::
optimiseInput(struct termios *t)
{
    Session *sp = fActiveSession;
    struct tty *tp = &sp->ftty;
    bool cantByPass;
    UInt32 slipEvent, pppEvent;

    cantByPass =
        (ISSET(t->c_iflag, NOBYPASS_IFLAG_MASK)
          || ( ISSET(t->c_iflag, BRKINT) && !ISSET(t->c_iflag, IGNBRK) )
          || ( ISSET(t->c_iflag, PARMRK)
               && ISSET(t->c_iflag, NOBYPASS_PAR_MASK) != NOBYPASS_PAR_MASK)
          || ISSET(t->c_lflag, NOBYPASS_LFLAG_MASK)
          || linesw[tp->t_line].l_rint != ttyinput);

    if (cantByPass)
        CLR(tp->t_state, TS_CAN_BYPASS_L_RINT);
    else
        SET(tp->t_state, TS_CAN_BYPASS_L_RINT);

    /*
     * Prepare to reduce input latency for packet
     * disciplines with a end of packet character.
     */
    if (tp->t_line == SLIPDISC) {
        slipEvent = PD_E_SPECIAL_BYTE;
        pppEvent  = PD_E_VALID_DATA_BYTE;
    }
    else if (tp->t_line == PPPDISC) {
        slipEvent = PD_E_VALID_DATA_BYTE;
        pppEvent  = PD_E_SPECIAL_BYTE;
    }
    else {
        slipEvent = PD_E_VALID_DATA_BYTE;
        pppEvent  = PD_E_VALID_DATA_BYTE;
    }

    (void) sessionExecuteEvent(sp, slipEvent, 0xc0);
    (void) sessionExecuteEvent(sp, pppEvent, 0xc0);
}

void IOSerialBSDClient::
iossdcddelay(void *vSession)
{
    Session *sp = (Session *) vSession;
    struct tty *tp = &sp->ftty;
    IOSerialBSDClient *me = sp->fThis;
    bool pd_state;	/* PortDevice state */

    AutoKernelFunnel funnel;	// Take kernel funnel

    if (!sp->fErrno && me->fDCDTimerDue
    && ISSET(tp->t_state, TS_ISOPEN)) { // Check for race
	pd_state = ISSET(me->sessionGetState(sp), PD_RS232_S_CAR);

	(void) (*linesw[(int) tp->t_line].l_modem)(tp, (int) pd_state);
    }
    me->fDCDTimerDue = false;
}


/*
 * The three functions below make up the recieve thread of the
 * Port Devices Line Discipline interface.
 *
 *	getData		// Main sleeper function
 *	procEvent	// Event processing
 *	rxFunc		// Thread main loop
*/

#define VALID_DATA (PD_E_VALID_DATA_BYTE & PD_E_MASK)

void IOSerialBSDClient::
getData(Session *sp)
{
    struct tty *tp = &sp->ftty;
    UInt32 transferCount, bufferSize, minCount;
    UInt8 rx_buf[1024];
    IOReturn rtn;

    if (fKillThreads)
        return;

    bufferSize = TTY_HIGHWATER - TTY_QUEUESIZE(tp);
    bufferSize = MIN(bufferSize, sizeof(rx_buf));
    if (bufferSize <= 0) {
        frxBlocked = true;	// No buffer space so block ourselves
        return;			// Will try again if data present
    }
    if (frxBlocked) {
        frxBlocked = false;
    }

    // minCount = (delay_usecs)? bufferSize : 1;
    minCount = 1;

    rtn = sessionDequeueData(sp, rx_buf, bufferSize, &transferCount, minCount);
    if (rtn) {
	if (kIOReturnOffline == rtn)
	    frxBlocked = true;	// Block ourselves?
	else if (rtn != kIOReturnIOError)
	    IOLog("ttyioss%04x: dequeueData ret %x\n", tp->t_dev, rtn);
        return;
    }

    if (!transferCount)
        return;

    // Track last in bound data time
    if (fPreemptAllowed)
        fLastUsedTime = *(timeval *) &time;

    /*
     * Avoid the grotesquely inefficient lineswitch routine
     * (ttyinput) in "raw" mode.  It usually takes about 450
     * instructions (that's without canonical processing or echo!).
     * slinput is reasonably fast (usually 40 instructions plus
     * call overhead).
     */
    if ( ISSET(tp->t_state, TS_CAN_BYPASS_L_RINT)
    &&  !ISSET(tp->t_lflag, PENDIN) ) {
        /* Update statistics */
        tk_nin += transferCount;
        tk_rawcc += transferCount;
        tp->t_rawcc += transferCount;

        /* update the rawq and tell recieve waiters to wakeup */
        (void) b_to_q(rx_buf, transferCount, &tp->t_rawq);
        ttwakeup(tp);
    }
    else {

        for (minCount = 0; minCount < transferCount; minCount++)
            (*linesw[(int) tp->t_line].l_rint)(rx_buf[minCount], tp);
    }
}

void IOSerialBSDClient::
procEvent(Session *sp)
{
    struct tty *tp = &sp->ftty;
    u_long event, data;
    IOReturn rtn;

    rtn = sessionDequeueEvent(sp, &event, &data, false);
    if (kIOReturnOffline == rtn)
	return;

    assert(!rtn && event != PD_E_EOQ && (event & PD_E_MASK) != VALID_DATA);

    switch(event) {
    case PD_E_SPECIAL_BYTE:
	break;	// Pass on the character to tty layer

    case PD_RS232_E_LINE_BREAK:	data  = 0;	   /* no_break */
    case PD_E_FRAMING_ERROR:	SET(data, TTY_FE);	break;
    case PD_E_INTEGRITY_ERROR:	SET(data, TTY_PE);	break;

    case PD_E_HW_OVERRUN_ERROR:
    case PD_E_SW_OVERRUN_ERROR:
	IOLog("ttyioss%04x: %sware Overflow\n", tp->t_dev,
	    (event == PD_E_SW_OVERRUN_ERROR) ? "Soft" : "Hard" );
	event = 0;
	break;

    case PD_E_DATA_LATENCY:
	/* no_break */

    case PD_E_FLOW_CONTROL:
    default:	/* Ignore */
	event = 0;
	break;
    }
    
    if (event) {
        // Track last in bound event time
        if (fPreemptAllowed)
            fLastUsedTime = *(timeval *) &time;

	(*linesw[(int)tp->t_line].l_rint)(data, tp);
    }
}

void IOSerialBSDClient::
rxFunc()
{
    Session *sp;
    int event;
    u_long wakeup_with;	// states
    IOReturn rtn;

    // Mark this thread as part of the BSD infrastructure.
    thread_funnel_set(kernel_flock, TRUE);

    sp = fActiveSession;

    frxThread = IOThreadSelf();
    frxThreadLaunched = true;
    wakeup((caddr_t) &frxThread);	// wakeup the thread launcher

    frxBlocked = false;

    while ( !fKillThreads ) {
        if (frxBlocked) {
            wakeup_with = PD_S_RX_EVENT;
            rtn = sessionWatchState(sp, &wakeup_with , PD_S_RX_EVENT);
            sessionSetState(sp, 0, PD_S_RX_EVENT);
            if ( (kIOReturnOffline == rtn)
	    ||   (kIOReturnIOError == rtn && fKillThreads) )
                break;	// Terminate thread loop
        }
	event = (sessionNextEvent(sp) & PD_E_MASK);
	if (event == PD_E_EOQ || event == VALID_DATA)
	    getData(sp);
	else
	    procEvent(sp);
    }

    // commit seppuku cleanly
    frxThread = NULL;
    frxThreadLaunched = false;
    wakeup((caddr_t) &frxThread);	// wakeup the thread killer

    IOExitThread();
}

/*
 * The three functions below make up the status monitoring and transmition
 * part of the Port Devices Line Discipline interface.
 *
 *	txload		// TX data download to Port Device
 *	dcddelay	// DCD callout function for DCD transitions
 *	txFunc		// Thread main loop and sleeper
 */

void IOSerialBSDClient::
txload(Session *sp, u_long *wait_mask)
{
    struct tty *tp = &sp->ftty;
    IOReturn rtn;
    UInt8 tx_buf[CBSIZE * 8];	// 1/2k buffer
    UInt32 data;
    UInt32 cc, size;

    if ( !tp->t_outq.c_cc )
	return;		// Nothing to do

    if ( !ISSET(tp->t_state, TS_BUSY) ) {
        SET(tp->t_state, TS_BUSY);
        SET(*wait_mask, PD_S_TXQ_EMPTY); // Start tracking PD_S_TXQ_EMPTY
	CLR(*wait_mask, PD_S_TX_BUSY);
    }

    while (cc = tp->t_outq.c_cc) {
        rtn = sessionRequestEvent(sp, PD_E_TXQ_AVAILABLE, &data);
	if (kIOReturnOffline == rtn || kIOReturnNotOpen == rtn)
	    return;

	assert(!rtn);

        size = data;
	if (size > 0)
	    size = MIN(size, sizeof(tx_buf));
	else {
	    SET(*wait_mask, PD_S_TXQ_LOW_WATER); // Start tracking low water
	    return;
	}
	
	size = q_to_b(&tp->t_outq, tx_buf, MIN(cc, size));
	assert(size);

	/* There was some data left over from the previous load */
        rtn = sessionEnqueueData(sp, tx_buf, size, &cc, false);
        if (fPreemptAllowed)
            fLastUsedTime = *(timeval *) &time;

	if (kIOReturnSuccess == rtn)
            ttwwakeup(tp);
        else
	    IOLog("ttyioss%04x: enqueueData rtn (%x)\n", tp->t_dev, rtn);
#ifdef DEBUG
        if ((u_int) cc != size)
            IOLog("ttyioss%04x: enqueueData didn't queue everything\n",
                  tp->t_dev);
#endif
    }
}

void IOSerialBSDClient::
txFunc()
{
    Session *sp;
    struct tty *tp;
    u_long waitfor, waitfor_mask, wakeup_with;	// states
    u_long interesting_bits;
    IOReturn rtn;

    // Mark this thread as part of the BSD infrastructure.
    thread_funnel_set(kernel_flock, TRUE);

    sp = fActiveSession;
    tp = &sp->ftty;

    ftxThread = IOThreadSelf();
    ftxThreadLaunched = true;
    wakeup((caddr_t) &ftxThread);	// wakeup the thread launcher

    /*
     * Register interest in transitions to high of the
     *  PD_S_TXQ_LOW_WATER, PD_S_TXQ_EMPTY, PD_S_TX_EVENT status bits
     * and all other bit's being low
     */
    waitfor_mask = (PD_S_TX_EVENT | PD_S_TX_BUSY       | PD_RS232_S_CAR);
    waitfor      = (PD_S_TX_EVENT | PD_S_TXQ_LOW_WATER | PD_S_TXQ_EMPTY);

    // Get the current carrier state and toggle it
    SET(waitfor, ISSET(sessionGetState(sp), PD_RS232_S_CAR) ^ PD_RS232_S_CAR);

    for ( ;; ) {
	wakeup_with = waitfor;
	rtn  = sessionWatchState(sp, &wakeup_with, waitfor_mask);
	if ( kIOReturnOffline == rtn || kIOReturnNotOpen == rtn
	||  (kIOReturnIOError == rtn && fKillThreads) )
	    break;	// Terminate thread loop

	//
	// interesting_bits are set to true if the wait_for = wakeup_with
	// and we expressed an interest in the bit in waitfor_mask.
	//
	interesting_bits = waitfor_mask & (~waitfor ^ wakeup_with);

	// Has iossstart been trying to get out attention
	if ( ISSET(PD_S_TX_EVENT, interesting_bits) ) {
	    /* Clear PD_S_TX_EVENT bit in state register */
	    rtn = sessionSetState(sp, 0, PD_S_TX_EVENT);
	    assert(!rtn || rtn != kIOReturnOffline);
	    txload(sp, &waitfor_mask);
	}

	//
	// Now process the carriers current state if it has changed
	//
	if ( ISSET(PD_RS232_S_CAR, interesting_bits) ) {
	    waitfor ^= PD_RS232_S_CAR;		/* toggle value */

	    if (fDCDTimerDue) {
		/* Stop dcd timer interval was too short */
		fDCDTimerDue = false;
		untimeout(&IOSerialBSDClient::iossdcddelay, sp);
	    }
	    else {
		fDCDTimerDue = true;
		timeout(&IOSerialBSDClient::iossdcddelay, sp, fDCDDelayTicks);
	    }
	}

	//
	// Check to see if we can unblock the data transmission
	//
	if ( ISSET(PD_S_TXQ_LOW_WATER, interesting_bits) ) {
	    CLR(waitfor_mask, PD_S_TXQ_LOW_WATER); // Not interested any more
	    txload(sp, &waitfor_mask);
	}

	//
	// 2 stage test for transmitter being no longer busy.
	// Stage 1: TXQ_EMPTY high, register interest in TX_BUSY bit
	//
	if ( ISSET(PD_S_TXQ_EMPTY, interesting_bits) ) {
	    CLR(waitfor_mask, PD_S_TXQ_EMPTY); /* Not interested */
	    SET(waitfor_mask, PD_S_TX_BUSY);   // But I want to know about chip
	}

	//
	// Stage 2 TX_BUSY dropping.
	// NB don't want to use interesting_bits as the TX_BUSY mask may
	// have just been set.  Instead here we simply check for a low.
	//
	if (PD_S_TX_BUSY & waitfor_mask & ~wakeup_with) {
	    CLR(waitfor_mask, PD_S_TX_BUSY); /* Not interested any more */
	    CLR(tp->t_state,  TS_BUSY);
            ttwwakeup(tp);		     /* Notify disc, not busy anymore */
	}
    }

    if (fDCDTimerDue) {
    	// Clear the DCD timeout
	fDCDTimerDue = false;
	untimeout(&IOSerialBSDClient::iossdcddelay, sp);
    }

    // Drop the carrier line and clear the BUSY bit
    (void) (*linesw[(int) tp->t_line].l_modem)(tp, false);

    ftxThread = NULL;
    ftxThreadLaunched = false;
    wakeup((caddr_t) &ftxThread); 	// wakeup the thread killer

    IOExitThread();
}

void IOSerialBSDClient::
launchThreads()
{
    // Clear the have launched flags
    ftxThreadLaunched = frxThreadLaunched = false;

    // Now launch the receive and transmitter threads
    IOCreateThread((IOThreadFunc) &IOSerialBSDClient::rxFunc, this);
    IOCreateThread((IOThreadFunc) &IOSerialBSDClient::txFunc, this);

    // Now wait for the threads to actually launch
    while (!frxThreadLaunched)
        tsleep((caddr_t) &frxThread, TTOPRI, "ttyrxl", 0);
    while (!ftxThreadLaunched)
        tsleep((caddr_t) &ftxThread, TTOPRI, "ttytxl", 0);
}

void IOSerialBSDClient::
killThreads()
{
    if (frxThread || ftxThread) {
        fKillThreads = true;
        fProvider->executeEvent(PD_E_ACTIVE, false);

        while (frxThread)
            tsleep((caddr_t) &frxThread, TTIPRI, "ttyrxd", 0);
        while (ftxThread)
            tsleep((caddr_t) &ftxThread, TTOPRI, "ttytxd", 0);
    }
}

// 
// session based accessors to Serial Stream Sync 
//
IOReturn IOSerialBSDClient::
sessionSetState(Session *sp, UInt32 state, UInt32 mask)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->setState(state, mask);
}

UInt32 IOSerialBSDClient::
sessionGetState(Session *sp)
{
    if (sp->fErrno)
        return 0;
    else
        return fProvider->getState();
}

IOReturn IOSerialBSDClient::
sessionWatchState(Session *sp, UInt32 *state, UInt32 mask)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->watchState(state, mask);
}

UInt32 IOSerialBSDClient::
sessionNextEvent(Session *sp)
{
    if (sp->fErrno)
        return PD_E_EOQ;
    else
        return fProvider->nextEvent();
}

IOReturn IOSerialBSDClient::
sessionExecuteEvent(Session *sp, UInt32 event, UInt32 data)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->executeEvent(event, data);
}

IOReturn IOSerialBSDClient::
sessionRequestEvent(Session *sp, UInt32 event, UInt32 *data)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->requestEvent(event, data);
}

IOReturn IOSerialBSDClient::
sessionEnqueueEvent(Session *sp, UInt32 event, UInt32 data, bool sleep)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->enqueueEvent(event, data, sleep);
}

IOReturn IOSerialBSDClient::
sessionDequeueEvent(Session *sp, UInt32 *event, UInt32 *data, bool sleep)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->dequeueEvent(event, data, sleep);
}

IOReturn IOSerialBSDClient::
sessionEnqueueData(Session *sp, UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->enqueueData(buffer, size, count, sleep);
}

IOReturn IOSerialBSDClient::
sessionDequeueData(Session *sp, UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->dequeueData(buffer, size, count, min);
}
