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
/*
 * IOSerialBSDClient.cpp
 *
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
#include "IOSerialSessionSync.h"
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
#define	TTY_DIALIN_FLAG		(0x01)	/* Device intended for dial in */
#define TTY_NUM_FLAGS		1

#define TTY_HIGH_HEADROOM	4	/* size error code + 1 */
#define TTY_HIGHWATER		(TTYHOG - TTY_HIGH_HEADROOM)
#define TTY_LOWWATER		((TTY_HIGHWATER * 7) / 8)

#define	IS_TTY_OUTWARD(dev)	(~minor(dev) &  TTY_DIALIN_FLAG)
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

class IOSerialBSDClientGlobals {
private:

    IOLock *fLock;
    unsigned int fMajor;
    unsigned int fLastMinor;
    IOSerialBSDClient **fTTYs;
    OSDictionary *fNames;

public:
    IOSerialBSDClientGlobals();
    ~IOSerialBSDClientGlobals();

    inline bool isValid();
    inline IOSerialBSDClient *getTTY(dev_t dev);

    dev_t createDevNode();
    bool registerTTY(dev_t dev, IOSerialBSDClient *tty);
    const OSSymbol *getUniqueTTYSuffix
        (const OSSymbol *inName, const OSSymbol *suffix, dev_t dev);
    void releaseUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix);
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

static const struct timeval dtrDownDelay = MUSEC2TIMEVALDECL(DTR_DOWN_DELAY);

/*
 * Map from Unix baud rate defines to <PortDevices> baud rate.  NB all
 * reference to bits used in a PortDevice are always 1 bit fixed point.
 * The extra bit is used to indicate 1/2 bits.
 */
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

    fMajor = (unsigned int) -1;
    fNames = OSDictionary::withCapacity(4);
    fLastMinor = 4;
    fTTYs = (IOSerialBSDClient **) IOMalloc(fLastMinor * sizeof(fTTYs[0]));
    fLock = IOLockAlloc();
    if (fLock && fTTYs && fNames) {
        bzero(fTTYs, fLastMinor * sizeof(fTTYs[0]));
        IOLockInit(fLock);
        fMajor = cdevsw_add(-1, &IOSerialBSDClient::devsw);
    }

    if (!fLock || !fTTYs || !fNames || fMajor == (unsigned int) -1)
        IOLog("IOSerialBSDClient didn't initialize");
}

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
    SAFE_RELEASE(fNames);
    if (fMajor != (unsigned int) -1)
        cdevsw_remove(fMajor, &IOSerialBSDClient::devsw);
    if (fTTYs)
        IOFree(fTTYs, fLastMinor * sizeof(fTTYs[0]));
    if (fLock)
        IOLockFree(fLock);
}

bool IOSerialBSDClientGlobals::isValid()
{
    return (fLock && fTTYs && fNames && fMajor != (unsigned int) -1);
}

dev_t IOSerialBSDClientGlobals::createDevNode()
{
    unsigned int i;

    IOTakeLock(fLock);

    for (i = 0; i < fLastMinor && fTTYs[i]; i++)
        ;

    if (i == fLastMinor)
    {
        unsigned int newLastMinor = fLastMinor + 4;
        IOSerialBSDClient **newTTYs;

        newTTYs = (IOSerialBSDClient **)
                    IOMalloc(newLastMinor * sizeof(fTTYs[0]));
        if (!newTTYs) {
            IOUnlock(fLock);
            return (dev_t) -1;
        }

        bzero(&newTTYs[fLastMinor], 4 * sizeof(fTTYs[0]));
        bcopy(fTTYs, newTTYs, fLastMinor * sizeof(fTTYs[0]));
        IOFree(fTTYs, fLastMinor * sizeof(fTTYs[0]));
        fLastMinor = newLastMinor;
        fTTYs = newTTYs;
    }

    dev_t dev = makedev(fMajor, i << TTY_NUM_FLAGS);
    fTTYs[i] = (IOSerialBSDClient *) -1;

    IOUnlock(fLock);

    return dev;
}

bool IOSerialBSDClientGlobals::
registerTTY(dev_t dev, IOSerialBSDClient *tty)
{
    bool ret = false;
    unsigned int i = TTY_UNIT(dev);

    IOTakeLock(fLock);

    assert(i < fLastMinor);
    if (i < fLastMinor) {
        assert(!tty || fTTYs[i] != (IOSerialBSDClient *) -1);
        if (tty && fTTYs[i] == (IOSerialBSDClient *) -1) {
            fTTYs[i] = tty;
            ret = true;
        }
    }

    IOUnlock(fLock);

    return ret;
}

IOSerialBSDClient *IOSerialBSDClientGlobals::getTTY(dev_t dev)
{
    return fTTYs[TTY_UNIT(dev)];
}

const OSSymbol *IOSerialBSDClientGlobals::
getUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix, dev_t dev)
{
    OSSet *suffixSet = 0;
    
    IOTakeLock(fLock);

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

    IOUnlock(fLock);

    return suffix;
}

void IOSerialBSDClientGlobals::
releaseUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix)
{
    OSSet *suffixSet;

    IOTakeLock(fLock);
    suffixSet = (OSSet *) fNames->getObject(inName);
    if (suffixSet)
        suffixSet->removeObject((OSObject *) suffix);
    IOUnlock(fLock);
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
        calloutNode = devfs_make_node(fBaseDev,
            DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
            calloutName->getCStringNoCopy() + sizeof(TTY_DEVFS_PREFIX) - 1);
        dialinNode = devfs_make_node(fBaseDev | TTY_DIALIN_FLAG,
            DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
            dialinName->getCStringNoCopy() + sizeof(TTY_DEVFS_PREFIX) - 1);
        if (!calloutNode || !dialinNode)
            break;

        // Always reset the name of our provider
        if (!setProperty(gIOTTYDeviceKey,     (OSObject *) deviceKey)
        ||  !setProperty(gIOCalloutDeviceKey, (OSObject *) calloutName)
        ||  !setProperty(gIODialinDeviceKey,  (OSObject *) dialinName))
            break;

        fCdevCalloutNode = calloutNode; calloutNode = 0;
        fCdevDialinNode  = dialinNode;  dialinNode = 0;
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

    do {

        fSession = IOSerialSessionSync::withStreamSync(fProvider);
        if (!fSession)
            break;

        fBaseDev = sBSDGlobals.createDevNode();
        if ((dev_t) -1 == fBaseDev)
            break;

        if (!createDevNodes())
            break;

        if (!setBaseTypeForDev())
            break;

        initState();

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
    return ((OSString *) self->getProperty(gIOTTYDeviceKey))
                ->getCStringNoCopy();
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
    if (fCdevCalloutNode)
        devfs_remove(fCdevCalloutNode);
    if (fCdevDialinNode)
        devfs_remove(fCdevDialinNode);
    if ((dev_t) -1 != fBaseDev) {
        sBSDGlobals.registerTTY(fBaseDev, 0);
        sBSDGlobals.releaseUniqueTTYSuffix(
                        (const OSSymbol *) getProperty(gIOTTYBaseNameKey),
                        (const OSSymbol *) getProperty(gIOTTYSuffixKey));
    }
    SAFE_RELEASE(fSession);

    super::stop(provider);
}

void IOSerialBSDClient::
initState()
{
    // Set the client pointer for this bsd tty client
    map.fClient = this;

    /*
     * We don't use all the flags from <sys/ttydefaults.h> since they
     * are only relevant for logins.  It's includeant to have echo off
     * initially so that the line doesn't start blathering before the
     * echo flag can be turned off.
     */
    fInitTermIn.c_iflag = 0;
    fInitTermIn.c_oflag = 0;
    fInitTermIn.c_cflag = TTYDEF_CFLAG;
    fInitTermIn.c_lflag = 0;
    fInitTermIn.c_ispeed = fInitTermIn.c_ospeed = TTYDEF_SPEED;
    termioschars(&fInitTermIn);

    fInitTermOut = fInitTermIn;
    bzero(&fDTRDownTime, sizeof(fDTRDownTime));
}

int IOSerialBSDClient::
iossopen(dev_t dev, int flags, int devtype, struct proc *p)
{
    IOSerialBSDClient *client = sBSDGlobals.getTTY(dev);
    struct tty *tp = &client->map.ftty;
    int error = 0;

    if (!client) {
        error = ENXIO;
        goto exitOpen;
    }

    /* Device has been opened exclusive by somebody */
    if (ISSET(tp->t_state, TS_XCLUDE) && !suser(p->p_ucred, &p->p_acflag)) {
        error = EBUSY;
        goto exitOpen;
    }

    if ( !IS_TTY_OUTWARD(dev) )
	client->fInOpensPending++;

checkbusy:
    error = client->acquireSession(dev);
    if (error) {
	if ( !IS_TTY_OUTWARD(dev) )
	    client->fInOpensPending--;
        goto exitOpen;
    }
    
    /*
     * Initialize Unix's tty struct,
     * set device parameters and RS232 state
     */
    if ( !ISSET(tp->t_state, TS_ISOPEN) ) {
        client->initSession();

        // Initialise the line state
        iossparam(tp, &tp->t_termios);
    }

    /*
     * Handle DCD:
     * If outgoing or not hw dcd or dcd is asserted, then continue.
     * Otherwise, block till dcd is asserted or open fPreempt.
     */
    if (IS_TTY_OUTWARD(dev)
    ||  ISSET(client->fSession->getState(), PD_RS232_S_CAR) ) {
        (*linesw[tp->t_line].l_modem)(tp, 1);
    }

    if (IS_TTY_PREEMPT(dev, tp->t_cflag)) {	/* Incoming and !CLOCAL */
        IOSerialSessionSync *audit = client->fSession; // Cache current fSession

	error = client->waitForDCD(flags);
        if (error == EBUSY) {	/* waitForDCD was fPreempted */
            if (tp->t_dev == dev) {
                tp->t_dev = 0;		// Preempted by outside request
            }
            else {
                audit->release();	// Preempted by OUTGOING tty call
            }

	    goto checkbusy;		// Preempted so try to get lock again.
	}
	else if (error == EINTR) {	/* waitForDCD was interrupted */
	    if (--client->fInOpensPending == 0)
		iossclose(dev, flags, devtype, p);	// shutdown
	    else {
		//
		// Interrupted by user request so clear lock and
		// wakeup the other sleeper before returning.
		//
		tp->t_dev = 0;
		wakeup((caddr_t) tp);
	    }
            goto exitOpen;
	}
	else {
	    assert(!error);
	}
    }

    // Open line discipline, make sure that the clocal state is maintained
    error = ((*linesw[(int) tp->t_line].l_open)(dev, tp));

    // launch the transmit and receive threads, if necessary
    if (!error)
        client->launchThreads();

    if ( !IS_TTY_OUTWARD(dev) )
	client->fInOpensPending--;

    // Allow any following opens to try to acquire fSession
    wakeup((caddr_t) tp);

exitOpen:
    return error;
}

int IOSerialBSDClient::
iossclose(dev_t dev, int flags, int devtype, struct proc *p)
{
    IOSerialBSDClient *client = sBSDGlobals.getTTY(dev);
    struct tty *tp = &client->map.ftty;
    IOReturn rtn;

    assert(client);

    /* Block anybody attempting to get fSession */
    client->fIsReleasing = true;

    /* We are closing, it doesn't matter now about holding back ... */
    CLR(tp->t_state, TS_TTSTOP);
    (void) client->fSession->executeEvent(PD_E_FLOW_CONTROL, 0);
    (void) client->fSession->setState(-1, PD_S_RX_ENABLE | PD_S_TX_ENABLE);

    // Clear any outstanding line breaks
    rtn = client->fSession->enqueueEvent(PD_RS232_E_LINE_BREAK, false, true);
    assert(!rtn);

    (*linesw[(int) tp->t_line].l_close)(tp, flags);

    if (ISSET(tp->t_cflag, HUPCL) || !ISSET(tp->t_state, TS_ISOPEN)
    || (IS_TTY_PREEMPT(dev, client->fInitTermIn.c_cflag)
        && !ISSET(client->fSession->getState(), PD_RS232_S_CAR)) ) {
        /*
         * XXX we will miss any carrier drop between here and the
         * next open.  Perhaps we should watch DCD even when the
         * port is closed; it is not sufficient to check it at
         * the next open because it might go up and down while
         * we're not watching.
         */
	(void) client->mctl(RS232_S_OFF, DMSET);
    }

    ttyclose(tp);

    assert(!tp->t_outq.c_cc);

    // Shut down the port, this will cause the RX && TX threads to terminate
    // Then wait for threads to terminate, this should be over very quickly.
    client->killThreads();

    // Release the fSession's lock
    client->fSession->releasePort();

    /* wakeup any sleeping incoming opens */
    tp->t_dev = 0;		/* Unlock the tty */
    client->fIsReleasing = false;
    wakeup((caddr_t) tp);

    return 0;
}

int IOSerialBSDClient::
iossread(dev_t dev, struct uio *uio, int ioflag)
{
    IOSerialBSDClient *client = sBSDGlobals.getTTY(dev);
    struct tty *tp = &client->map.ftty;
    int error = ENXIO;

    if (client) {
        error = (*linesw[(int) tp->t_line].l_read)(tp, uio, ioflag);
        if (client->frxBlocked && TTY_QUEUESIZE(tp) < TTY_LOWWATER)
            client->fSession->setState(PD_S_RX_EVENT, PD_S_RX_EVENT);
    }

    return error;
}

int IOSerialBSDClient::
iosswrite(dev_t dev, struct uio *uio, int ioflag)
{
    IOSerialBSDClient *client = sBSDGlobals.getTTY(dev);
    struct tty *tp = &client->map.ftty;
    int error = ENXIO;

    if (client)
        error = (*linesw[(int) tp->t_line].l_write)(tp, uio, ioflag);

    return error;
}

int IOSerialBSDClient::
iossselect(dev_t dev, int which, struct proc *p)
{
    int error = ENXIO;
    IOSerialBSDClient *client = sBSDGlobals.getTTY(dev);

    if (client)
        error = ttyselect(&client->map.ftty, which, p);

    return error;
}

void
IOSerialBSDClient::convertFlowCtrl(struct termios *t)
{
    IOReturn rtn;
    u_long flowCtrl;

    //
    // Have to reconstruct the flow control bits
    //
    rtn = fSession->requestEvent(PD_E_FLOW_CONTROL, &flowCtrl);
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

    out_b &= ( PD_RS232_S_DTR | PD_RS232_S_RFR | PD_RS232_S_CTS
	     | PD_RS232_S_CAR | PD_RS232_S_RNG | PD_RS232_S_BRK );
    return out_b;
}

int IOSerialBSDClient::
iossioctl(dev_t dev, u_long cmd, caddr_t data, int fflag,
                         struct proc *p)
{
    IOSerialBSDClient *client = sBSDGlobals.getTTY(dev);
    struct tty *tp = &client->map.ftty;
    int error = 0;

    if (!client)
    {
        error = ENXIO;
        goto exitIoctl;
    }

    /*
     * tty line disciplines return >= 0 if they could process this
     * ioctl request.  If so, simply return, we're done
     */
    error = (*linesw[(int) tp->t_line].l_ioctl)(tp, cmd, data, fflag, p);
    if (error >= 0) {
        client->optimiseInput(&tp->t_termios);
        goto exitIoctl;
    }

    if (TIOCGETA == cmd) {
        bcopy(&tp->t_termios, data, sizeof(struct termios));
        client->convertFlowCtrl((struct termios *) data);
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
        client->convertFlowCtrl(&tp->t_termios);

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

    default:
        break;
    }

    /* See if generic tty understands this. */
    if ((error = ttioctl(tp, cmd, data, fflag, p)) >= 0) {
        if (error > 0) {
            iossparam(tp, &tp->t_termios);	/* reestablish old state */
        }
        client->optimiseInput(&tp->t_termios);
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
        (void) client->mctl(PD_RS232_S_BRK, DMBIS);  break;
    case TIOCCBRK:
        (void) client->mctl(PD_RS232_S_BRK, DMBIC);  break;
    case TIOCSDTR:
        (void) client->mctl(PD_RS232_S_DTR, DMBIS);  break;
    case TIOCCDTR:
        (void) client->mctl(PD_RS232_S_DTR, DMBIC);  break;

    case TIOCMSET:
        (void) client->mctl(tiotors232(*(int *)data), DMSET);  break;
    case TIOCMBIS:
        (void) client->mctl(tiotors232(*(int *)data), DMBIS);  break;
    case TIOCMBIC:
        (void) client->mctl(tiotors232(*(int *)data), DMBIC);  break;
    case TIOCMGET:
        *(int *)data = rs232totio(client->mctl(0,     DMGET)); break;

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
    IOSerialBSDClient *client = ((ttyMap *) tp)->fClient;
    IOReturn rtn;

    if ( !client->fIstxEnabled && !ISSET(tp->t_state, TS_TTSTOP) ) {
        client->fIstxEnabled = true;
        client->fSession->setState(-1, PD_S_TX_ENABLE);
    }

    if  (tp->t_outq.c_cc) {
        // Notify the transmit thread of action to be performed
	rtn = client->fSession->setState(PD_S_TX_EVENT, PD_S_TX_EVENT);
	assert(!rtn);
    }
}

int IOSerialBSDClient::
iossstop(struct tty *tp, int rw)
{
    IOSerialBSDClient *client = ((ttyMap *) tp)->fClient;

    if ( ISSET(tp->t_state, TS_TTSTOP) ) {
	client->fIstxEnabled = false;
	client->fSession->setState(0, PD_S_TX_ENABLE);
    }

    if ( ISSET(rw, FWRITE) )
        client->fSession->executeEvent(PD_E_TXQ_FLUSH, 0);
    if ( ISSET(rw, FREAD) ) {
        client->fSession->executeEvent(PD_E_RXQ_FLUSH, 0);
        if (client->frxBlocked)	// wake up a blocked reader
            client->fSession->setState(PD_S_RX_ENABLE, PD_S_RX_ENABLE);
    }
    return 0;
}

/*
 * Parameter control functions
 */
int IOSerialBSDClient::
iossparam(struct tty *tp, struct termios *t)
{
    IOSerialBSDClient *client = ((ttyMap *) tp)->fClient;
    u_long data;
    int cflag, error = EINVAL;
    IOReturn rtn;

    if (ISSET(t->c_iflag, (IXOFF|IXON))
    && (t->c_cc[VSTART]==_POSIX_VDISABLE || t->c_cc[VSTOP]==_POSIX_VDISABLE)) {
        goto exitParam;
    }

    /* do historical conversions */
    if (t->c_ispeed == 0)
        t->c_ispeed = t->c_ospeed;

    /* check requested parameters */
    data = ttspeedtab(t->c_ospeed, iossspeeds);
    if (data < 0 || (data > 0 && t->c_ispeed != t->c_ospeed) )
        goto exitParam;

    rtn  = client->fSession->executeEvent(PD_E_DATA_RATE, data);
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
    rtn  = client->fSession->executeEvent(PD_E_DATA_SIZE, data);
    if (rtn)
        goto exitParam;

    data = PD_RS232_PARITY_NONE;
    if ( ISSET(cflag, PARENB) ) {
        if ( ISSET(cflag, PARODD) )
            data = PD_RS232_PARITY_ODD;
        else
            data = PD_RS232_PARITY_EVEN;
    }
    rtn = client->fSession->executeEvent(PD_E_DATA_INTEGRITY, data);
    if (rtn)
        goto exitParam;

    /* Set stop bits to 2 1/2 bits in length */
    if (ISSET(cflag, CSTOPB))
        data = 4;
    else
        data = 2;
    rtn = client->fSession->executeEvent(PD_RS232_E_STOP_BITS, data);
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
    CLR(t->c_iflag, IXON | IXOFF | IXANY);
    CLR(t->c_cflag, CRTS_IFLOW | CCTS_OFLOW);

    rtn = client->fSession->executeEvent(PD_E_FLOW_CONTROL, data);
    if (rtn)
        goto exitParam;
	
    //
    // Load the flow control start and stop characters.
    //
    rtn  = client->fSession->executeEvent(PD_RS232_E_XON_BYTE,  t->c_cc[VSTART]);
    rtn |= client->fSession->executeEvent(PD_RS232_E_XOFF_BYTE, t->c_cc[VSTOP]);
    if (rtn)
        goto exitParam;

    /* Always enable for transmission */
    client->fIstxEnabled = true;
    client->fSession->setState(PD_S_TX_ENABLE, PD_S_TX_ENABLE);

    /* Only enable reception if necessary */
    if ( ISSET(cflag, CREAD) )
        client->fSession->setState(-1, PD_S_RX_ENABLE);
    else
        client->fSession->setState( 0, PD_S_RX_ENABLE);

    error = 0;	/* If we got this far, indicate success */

exitParam:
    return error;
}

int IOSerialBSDClient::
acquireSession(dev_t dev)
{
    struct tty *tp = &map.ftty;
    int error = 0;
    bool goToSleep;
    IOSerialSessionSync *newSession;
    IOReturn rtn;

    //
    // The do loop below only allows one open through the gate if the device
    // isn't already opened, with the exception of an outgoing call fPreempting
    // an fPreemptable and pending open.
    //
    // Once the port has been sucessfully opened then EBUSY all further
    // attempts to open the port with different characteristics as determined
    // by (tp->t_dev != dev).
    //
    do {
        goToSleep = false;

        if (IS_TTY_PREEMPT(dev, tp->t_cflag) &&  fHasAuditSleeper) {
            // Already have a -[acquireAudit] sleeper so just sleep for
            // some change in the current state.
            goToSleep = true;
        }
        else if (tp->t_dev) {
            // Check for outstanding open request
            if (tp->t_dev == dev) {
                // Opening with exactly the same type.  Only
                // sleep if device isn't open or is in process of closing.
                goToSleep = !ISSET(tp->t_state, TS_ISOPEN) || fIsReleasing;
            }
            else if (IS_TTY_OUTWARD(tp->t_dev)
                 &&  IS_TTY_PREEMPT(dev, tp->t_cflag)) {
                // If open outward and current open is inward and HARDCAR
                // then go to sleep waiting for outgoing to terminate.
                goToSleep = true;
            }
            else if (IS_TTY_PREEMPT(tp->t_dev, tp->t_cflag)
                 &&  IS_TTY_OUTWARD(dev)) {
                // If current open is outward and we have a fPreemptable
                // request that is open then return BUSY.  If we haven't
                // already prempted the older request do so now, otherwise
                // sleep.
                if ( ISSET(tp->t_state, TS_ISOPEN) )
                    return EBUSY;
                else if (fPreempt)
                    goToSleep = true;
                else
                    fPreempt = true;
            }
            else
            {
                // Return BUSY if the bound state is different or can
                // not be fPreempt
                return EBUSY;
            }
        }

        if (goToSleep)
            tsleep((caddr_t) tp, TTIPRI, "ttyas", 0);	// Wait for wakeup.
    } while (goToSleep);

    if (!tp->t_dev) {
        // Nobody has bound the port yet so bind state then attempt
        // to acquire the appropriate type of lock.
        tp->t_dev = dev;
        if (!IS_TTY_PREEMPT(dev, tp->t_cflag)) {
            // Attempt to get an non-fPreemptable lock
            error = (fSession->acquirePort(false) == kIOReturnSuccess)? 0 : EBUSY;
        }
        else {
            //
            // As this process can sleep and later be fPreempted by an outgoing
            // call, we have to use a local fSession.  That way if we fail to
            // get the lock due to interuption or something we don't bugger
            // the port's new state up.  But if we do get the lock then
            // we have to release the old fSession and save the new one.
            //
            fHasAuditSleeper = true;
            newSession = IOSerialSessionSync::withStreamSync(fProvider);
            assert(newSession);

            rtn = newSession->acquireAudit(true);
            fHasAuditSleeper = false;
            if (rtn == kIOReturnSuccess) {
                // Who knows how long we have been asleep so
                // rebind the port and release the old fSession
                tp->t_dev = dev;
                fSession->release();
                fSession = newSession;
            }
            else {
                newSession->release();
                error = (rtn == kIOReturnIPCError) ? EINTR : ENXIO;
            }
        }
    }
    else if (fPreempt) {
        tp->t_dev = dev;	// Bind state

        newSession = IOSerialSessionSync::withStreamSync(fProvider);
        assert(newSession);

        rtn = newSession->acquirePort(false);
        fPreempt = false;
        if (rtn == kIOReturnSuccess) {
            //
            // Can't release old fSession here as it still has to wakeup
            // at some stage and clean itself up and probably wouldn't
            // appreciate talking to a released object.
            //
            fSession = newSession;	// Record new fSession
        }
        else {
            // The port was busy or failed for some other reason so
            // release the new fSession and return with an error.
            newSession->release();
            error = EBUSY;
        }
    }
    else {
        // Same bound state so go on
    }

    if (error) {
        tp->t_dev = 0;		// Unbind the state and ...
        wakeup((caddr_t) tp);	// ... Wake up other sleepers
    }

    return error;	// return error condition.
}

void IOSerialBSDClient::
initSession()
{
    struct tty *tp = &map.ftty;
    IOReturn rtn;

    tp->t_oproc = iossstart;
    tp->t_param = iossparam;
    tp->t_termios = IS_TTY_OUTWARD(tp->t_dev)? fInitTermOut : fInitTermIn;
    ttsetwater(tp);
    fSession->executeEvent(PD_E_TXQ_FLUSH, 0);
    fSession->executeEvent(PD_E_RXQ_FLUSH, 0);

    CLR(tp->t_state, TS_CARR_ON | TS_BUSY);

    fKillThreads = false;
    fDCDDelayTicks = MUSEC2TICK(DCD_DELAY);
#if DCD_DELAY
    if (fDCDDelayTicks < 1)
        fDCDDelayTicks = 1;
#endif /* DCD_DELAY */

    // Disable all flow control  & data movement initially
    rtn  = fSession->executeEvent(PD_E_FLOW_CONTROL, 0);
    rtn |= fSession->setState(0, PD_S_RX_ENABLE | PD_S_TX_ENABLE);
    assert(!rtn);

    /* Activate the port fSession */
    rtn = fSession->executeEvent(PD_E_ACTIVE, true);
    if (rtn)
        IOLog("ttyioss%04x: ACTIVE failed (%x)\n", tp->t_dev, rtn);

    /*
     * Cycle the PD_RS232_S_DTR line if necessary 
     */
    if ( !ISSET(fSession->getState(), PD_RS232_S_DTR) ) {
        struct timeval earliestUp;

        earliestUp = fDTRDownTime;
        timeradd(&earliestUp, &dtrDownDelay, &earliestUp);
        timersub(&earliestUp, &time, &earliestUp);
        if ( earliestUp.tv_sec >= 0 && timerisset(&earliestUp) ) {

            /* Convert earliest up time to relative msecs rounded to 10 ms. */
            earliestUp.tv_sec *= 1000;
            earliestUp.tv_sec += 10 * ((earliestUp.tv_usec + 5000) / 10000);
            if (earliestUp.tv_sec)
                IOSleep(earliestUp.tv_sec);
        }

	(void) mctl(RS232_S_ON, DMSET);
    }

    /* Raise RTS */
    rtn = fSession->setState( PD_RS232_S_RTS, PD_RS232_S_RTS);
    assert(!rtn);
}

int IOSerialBSDClient::
waitForDCD(int flag)
{
    struct tty *tp = &map.ftty;
    u_long pd_state;
    IOReturn rtn;

    /*
     * A fPreemptable open has only got an Audit lock on the port fSession
     * here we wait for either DCD going high, a fPreempt request or an
     * interrupt.  Once we notice DCD high we upgrade our lock.  Otherwise
     * we release the current fSession and either return with an interrupt
     * error or attempt to go through the checkbusy: gate again.
     */
    if ( !ISSET(flag, FNONBLOCK)
    &&   !ISSET(tp->t_state, TS_CARR_ON) && !ISSET(tp->t_cflag, CLOCAL) ) {

        /* Track DCD Transistion to high */
	pd_state = PD_RS232_S_CAR;
        rtn = fSession->watchState(&pd_state, PD_RS232_S_CAR);
	if (rtn == kIOReturnNotOpen || rtn == kIOReturnIPCError)
	    return (rtn == kIOReturnIPCError)? EINTR : EBUSY;

	assert(rtn == kIOReturnSuccess);
	assert(ISSET(pd_state, PD_RS232_S_CAR));
        (*linesw[tp->t_line].l_modem)(tp, 1);	// To be here we must have DCD
    }

    /* Upgrade Audit Lock to a full lock */
    rtn = fSession->acquirePort(false);
    assert(!rtn);

    return 0;
}

int IOSerialBSDClient::
mctl(u_int bits, int how)
{
    u_long oldBits, mbits;
    IOReturn rtn;

    bits &= RS232_S_OUTPUTS;
    oldBits = fSession->getState();

    if ( ISSET(bits, PD_RS232_S_BRK) && (how == DMBIS || how == DMBIC) ) {
	oldBits = (how == DMBIS);
	rtn = fSession->enqueueEvent(PD_RS232_E_LINE_BREAK, oldBits, true);
	if (oldBits)
	    rtn |= fSession->enqueueEvent(PD_E_DELAY, BRK_DELAY, true);
	assert(!rtn);
	return oldBits;
    }

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
    if ( ISSET(oldBits & ~mbits, PD_RS232_S_DTR) ) {
 	fDTRDownTime = *(timeval *) &time;
    }

    rtn = fSession->setState(mbits, RS232_S_OUTPUTS);
    if (rtn)
	IOLog("ttyioss%04x: mctl PD_E_FLOW_CONTROL failed %x\n",
	    map.ftty.t_dev, rtn);


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
    struct tty *tp = &map.ftty;
    bool cantByPass =
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
        (void) fSession->executeEvent( PD_E_SPECIAL_BYTE    , 0xc0);
        (void) fSession->executeEvent( PD_E_VALID_DATA_BYTE , 0x7e);
    }
    else if (tp->t_line == PPPDISC) {
        (void) fSession->executeEvent(PD_E_SPECIAL_BYTE    , 0x7e);
        (void) fSession->executeEvent(PD_E_VALID_DATA_BYTE , 0xc0);
    }
    else {
        (void) fSession->executeEvent(PD_E_VALID_DATA_BYTE, 0x7e);
        (void) fSession->executeEvent(PD_E_VALID_DATA_BYTE, 0xc0);
    }
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
getData()
{
    struct tty *tp = &map.ftty;
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

    rtn = fSession->dequeueData(rx_buf, bufferSize, &transferCount, minCount);
    if (rtn && rtn != kIOReturnIOError) {
        IOLog("ttyioss%04x: dequeueData ret %x\n", tp->t_dev, rtn);
        return;
    }

    if (!transferCount)
        return;

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
procEvent()
{
    struct tty *tp = &map.ftty;
    u_long event, data;
    IOReturn rtn;

    rtn = fSession->dequeueEvent(&event, &data, false);
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
    
    if (event)
	(*linesw[(int)tp->t_line].l_rint)(data, tp);
}

void IOSerialBSDClient::
rxFunc()
{
    int event;
    u_long wakeup_with;	// states
    IOReturn rtn;

    // Mark this thread as part of the BSD infrastructure.
    thread_funnel_set(kernel_flock, TRUE);

    frxBlocked = false;

    while ( !fKillThreads ) {
        if (frxBlocked) {
            wakeup_with = PD_S_RX_EVENT;
            rtn = fSession->watchState(&wakeup_with , PD_S_RX_EVENT);
            fSession->setState(0, PD_S_RX_EVENT);
            if (rtn == kIOReturnIOError && fKillThreads)
                break;	// Terminate thread loop
        }
	event = (fSession->nextEvent() & PD_E_MASK);
	if (event == PD_E_EOQ || event == VALID_DATA)
	    getData();
	else
	    procEvent();
    }

    // commit seppuku cleanly
    frxThread = NULL;
    wakeup((caddr_t) &frxThread);
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
txload(u_long *wait_mask)
{
    struct tty *tp = &map.ftty;
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
        rtn = fSession->requestEvent(PD_E_TXQ_AVAILABLE, &data);
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
        rtn = fSession->enqueueData(tx_buf, size, &cc, false);
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
iossdcddelay(void *vThis)
{
    IOSerialBSDClient *client = (IOSerialBSDClient *) vThis;
    struct tty *tp = &client->map.ftty;
    int   pd_state;	/* PortDevice state */

    if (client->fIsDCDTimer
    && ISSET(tp->t_state, TS_ISOPEN)) { // Check for race
	pd_state = ((client->fSession->getState() & PD_RS232_S_CAR) != 0);

	(void) (*linesw[(int) tp->t_line].l_modem)(tp, pd_state);
    }
    client->fIsDCDTimer = false;
}

void IOSerialBSDClient::
txFunc()
{
    struct tty *tp = &map.ftty;
    u_long waitfor, waitfor_mask, wakeup_with;	// states
    u_long interesting_bits;
    IOReturn rtn;

    // Mark this thread as part of the BSD infrastructure.
    thread_funnel_set(kernel_flock, TRUE);

    /*
     * Register interest in transitions to high of the
     *  PD_S_TXQ_LOW_WATER, PD_S_TXQ_EMPTY, PD_S_TX_EVENT status bits
     * and all other bit's being low
     */
    waitfor_mask = (PD_S_TX_EVENT | PD_S_TX_BUSY       | PD_RS232_S_CAR);
    waitfor      = (PD_S_TX_EVENT | PD_S_TXQ_LOW_WATER | PD_S_TXQ_EMPTY);

    // Get the current carrier state and toggle it
    SET(waitfor, (fSession->getState() & PD_RS232_S_CAR) ^ PD_RS232_S_CAR);

    for ( ;; ) {
	wakeup_with = waitfor;
	rtn  = fSession->watchState(&wakeup_with, waitfor_mask);
	if (rtn == kIOReturnIOError && fKillThreads) 
	    break;	// Terminate thread loop
	//
	// interesting_bits are set to true if the wait_for = wakeup_with
	// and we expressed an interest in the bit in waitfor_mask.
	//
	interesting_bits = waitfor_mask & (~waitfor ^ wakeup_with);

	// Has iossstart been trying to get out attention
	if ( ISSET(PD_S_TX_EVENT, interesting_bits) ) {
	    /* Clear PD_S_TX_EVENT bit in state register */
	    rtn = fSession->setState(0, PD_S_TX_EVENT); assert(!rtn);
	    txload(&waitfor_mask);
	}

	//
	// Now process the carriers current state if it has changed
	//
	if ( ISSET(PD_RS232_S_CAR, interesting_bits) ) {
	    waitfor ^= PD_RS232_S_CAR;		/* toggle value */

	    if (fIsDCDTimer) {
		/* Stop dcd timer interval was too short */
		fIsDCDTimer = false;
		untimeout(&IOSerialBSDClient::iossdcddelay, this);
	    }
	    else {
		fIsDCDTimer = true;
		timeout(&IOSerialBSDClient::iossdcddelay, this, fDCDDelayTicks);
	    }
	}

	//
	// Check to see if we can unblock the data transmission
	//
	if ( ISSET(PD_S_TXQ_LOW_WATER, interesting_bits) ) {
	    CLR(waitfor_mask, PD_S_TXQ_LOW_WATER); // Not interested any more
	    txload(&waitfor_mask);
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

    if (fIsDCDTimer) {
    	// Clear the DCD timeout
	fIsDCDTimer = false;
	untimeout(&IOSerialBSDClient::iossdcddelay, this);
    }

    ftxThread = NULL;
    wakeup((caddr_t) &ftxThread);

    IOExitThread();
}

void IOSerialBSDClient::
launchThreads()
{
    if (!frxThread)
	frxThread = 
            IOCreateThread((IOThreadFunc) &IOSerialBSDClient::rxFunc, this);
    if (!ftxThread)
	ftxThread =
            IOCreateThread((IOThreadFunc) &IOSerialBSDClient::txFunc, this);
}

void IOSerialBSDClient::
killThreads()
{
    if (ftxThread || frxThread) {
        fKillThreads = true;
        // Disable the chip
        fSession->executeEvent(PD_E_ACTIVE, false);

        while (ftxThread)
            tsleep((caddr_t) &ftxThread, TTOPRI, "ttytxd", 0);
        while (frxThread)
            tsleep((caddr_t) &frxThread, TTIPRI, "ttyrxd", 0);
    }
}


