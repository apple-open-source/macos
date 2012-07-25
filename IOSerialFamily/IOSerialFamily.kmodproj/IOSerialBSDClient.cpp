/*
 * Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
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
 * 2007-07-12	dreece	fixed full-buffer hang
 * 2002-04-19	dreece	moved device node removal from free() to didTerminate()
 * 2001-11-30	gvdl	open/close pre-emptible arbitration for termios
 *			IOSerialStreams.
 * 2001-09-02	gvdl	Fixed hot unplug code now terminate cleanly.
 * 2001-07-20	gvdl	Add new ioctl for DATA_LATENCY control.
 * 2001-05-11	dgl	Update iossparam function to recognize MIDI clock mode.
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */
#include <sys/types.h>

__BEGIN_DECLS

#include <kern/thread.h>
#include <sys/time.h>

__END_DECLS

#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/ucred.h>
#include <sys/kernel.h>
#include <miscfs/devfs/devfs.h>
#include <sys/systm.h>
#include <sys/kauth.h>
#include <libkern/OSAtomic.h>
#include <pexpert/pexpert.h>

#include <IOKit/assert.h>
#include <IOKit/system.h>
#include <TargetConditionals.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOLib.h>

#include "IORS232SerialStreamSync.h"

#include "ioss.h"
#include "IOSerialKeys.h"

#include "IOSerialBSDClient.h"

#define super IOService

OSDefineMetaClassAndStructors(IOSerialBSDClient, IOService)

/*
 * Debugging assertions for tty locks
 */
#define TTY_DEBUG 1
#if TTY_DEBUG
#define	TTY_LOCK_OWNED(tp) do {lck_mtx_assert(&tp->t_lock, LCK_MTX_ASSERT_OWNED); } while (0)
#define	TTY_LOCK_NOTOWNED(tp) do {lck_mtx_assert(&tp->t_lock, LCK_MTX_ASSERT_NOTOWNED); } while (0)
#else
#define TTY_LOCK_OWNED(tp)
#define TTY_LOCK_NOTOWNED(tp)
#endif

/*
 * enable/disable kprint debugging
 */
#ifndef JLOG
#define JLOG 0
#endif


/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))
#define SAFE_RELEASE(x) do { if (x) x->release(); x = 0; } while(0)

/*
 * Options and tunable parameters
 *
 * must match what is in IOSerialBSDClient.h
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
 *
 * replacing hz from kern_clock with constant 100 (i.e. hz from kern_clock)
 * 
 * doing it with a #define so it will be easier to remember what to fix
 *
 * TODO: stop using tsleep
 */
#define LOCAL_HZ 100 

#define MUSEC2TICK(x) \
            ((int) (((long long) (x) * LOCAL_HZ + 500000) / 1000000))
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

# define IOSERIAL_DEBUG_INIT        (1<<0)
# define IOSERIAL_DEBUG_SETUP       (1<<1)
# define IOSERIAL_DEBUG_MISC        (1<<2)
# define IOSERIAL_DEBUG_CONTROL     (1<<3)  // flow control, stop bits, etc.
# define IOSERIAL_DEBUG_FLOW        (1<<4)
# define IOSERIAL_DEBUG_WATCHSTATE  (1<<5)
# define IOSERIAL_DEBUG_RETURNS     (1<<6)
# define IOSERIAL_DEBUG_BLOCK     (1<<7)
# define IOSERIAL_DEBUG_SLEEP     (1<<8)
# define IOSERIAL_DEBUG_DCDTRD    (1<<9)
# define IOSERIAL_DEBUG_MULTI	  (1<<10)

# define IOSERIAL_DEBUG_ERROR       (1<<15)
# define IOSERIAL_DEBUG_ALWAYS      (1<<16)

#ifdef DEBUG

#define IOSERIAL_DEBUG (IOSERIAL_DEBUG_ERROR | IOSERIAL_DEBUG_MULTI )

#else
// production debug output should be minimal
# define IOSERIAL_DEBUG (IOSERIAL_DEBUG_ERROR)
#endif



#ifdef IOSERIAL_DEBUG
# define REQUIRE(_expr)                                         \
        do {                                                    \
                if (!(_expr))                                   \
                        panic("%s:%s:%u: REQUIRE failed: %s",   \
                              __FILE__,                         \
                              __PRETTY_FUNCTION__,              \
                              __LINE__, #_expr);                \
        } while(0);

# define debug(fac, fmt, args...)                                                       \
do {                                                                                    \
        if (IOSERIAL_DEBUG_##fac & (IOSERIAL_DEBUG | IOSERIAL_DEBUG_ALWAYS))                     \
                kprintf("IOSerialFamily::%s: " fmt "\n", __FUNCTION__ , ##args);    \
} while(0)
#else
# define REQUIRE(_expr)                         \
        do {                                    \
                if (_expr) {                    \
                }                               \
        } while(0);

# define debug(fac, fmt, args...)       do { } while(0)
#endif

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
	IOLock * fFunnelLock;

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
	void takefFunnelLock();
	void releasefFunnelLock();
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
    /* d_reset    */ (reset_fcn_t *) &nulldev,
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
 * Idea for the line discipline routines came from FreeBSD 2004,
 * see src/sys/sys/linedisc.h
 */
static inline int bsdld_open(dev_t dev, struct tty *tp)
    { return (*linesw[tp->t_line].l_open)(dev, tp); }

static inline int bsdld_close(struct tty *tp, int flag)
    { return (*linesw[tp->t_line].l_close)(tp, flag); }

static inline int bsdld_read(struct tty *tp, struct uio *uio, int flag)
    { return (*linesw[tp->t_line].l_read)(tp, uio, flag); }

static inline int bsdld_write(struct tty *tp, struct uio *uio, int flag)
    { return (*linesw[tp->t_line].l_write)(tp, uio, flag); }

static inline int
bsdld_ioctl(struct tty *tp, u_long cmd, caddr_t data, int flag,
	    struct proc *p)
    { return (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p); }

static inline int bsdld_rint(int c, struct tty *tp)
    { return (*linesw[tp->t_line].l_rint)(c, tp); }

static inline void  bsdld_start(struct tty *tp)
    { (*linesw[tp->t_line].l_start)(tp); }

static inline int bsdld_modem(struct tty *tp, int flag)
    { return (*linesw[tp->t_line].l_modem)(tp, flag); }

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
    {    7200,	IOSS_BRD(   7200.0) },
    {    9600,	IOSS_BRD(   9600.0) },
    {   14400,	IOSS_BRD(  14400.0) },
    {   19200,	IOSS_BRD(  19200.0) },
    {   28800,	IOSS_BRD(  28800.0) },
    {   38400,	IOSS_BRD(  38400.0) },
    {   57600,	IOSS_BRD(  57600.0) },
    {   76800,	IOSS_BRD(  76800.0) },
    {  115200,	IOSS_BRD( 115200.0) },
    {  230400,	IOSS_BRD( 230400.0) },
    {  460800,	IOSS_BRD( 460800.0) },
    {  921600,	IOSS_BRD( 921600.0) },
    { 1843200,	IOSS_BRD(1843200.0) },
    {   19001,	IOSS_BRD(  19200.0) },	// Add some convenience mappings
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

#define IOLogCond(cond, args...) do { if (cond) kprintf(args); } while (0)

#define SAFE_PORTRELEASE(provider) do {			\
    if (fAcquired)					\
    	{ provider->releasePort(); fAcquired = false; }	\
} while (0)

//
// Static global data maintainence routines
//
static void
termios32to64(struct termios32 *in, struct user_termios *out)
{
	out->c_iflag = (user_tcflag_t)in->c_iflag;
	out->c_oflag = (user_tcflag_t)in->c_oflag;
	out->c_cflag = (user_tcflag_t)in->c_cflag;
	out->c_lflag = (user_tcflag_t)in->c_lflag;
    
	/* bcopy is OK, since this type is ILP32/LP64 size invariant */
	bcopy(in->c_cc, out->c_cc, sizeof(in->c_cc));
    
	out->c_ispeed = (user_speed_t)in->c_ispeed;
	out->c_ospeed = (user_speed_t)in->c_ospeed;
}

static void
termios64to32(struct user_termios *in, struct termios32 *out)
{
	out->c_iflag = (tcflag_t)in->c_iflag;
	out->c_oflag = (tcflag_t)in->c_oflag;
	out->c_cflag = (tcflag_t)in->c_cflag;
	out->c_lflag = (tcflag_t)in->c_lflag;
    
	/* bcopy is OK, since this type is ILP32/LP64 size invariant */
	bcopy(in->c_cc, out->c_cc, sizeof(in->c_cc));
    
	out->c_ispeed = (speed_t)in->c_ispeed;
	out->c_ospeed = (speed_t)in->c_ospeed;
}

bool IOSerialBSDClientGlobals::isValid()
{
    return (fFunnelLock && fClients && fNames && fMajor != (unsigned int) -1);
}

#define OSSYM(str) OSSymbol::withCStringNoCopy(str)
IOSerialBSDClientGlobals::IOSerialBSDClientGlobals()
{
#if JLOG
	debug(MULTI,"init");
#endif
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
        cdevsw_setkqueueok(fMajor, &IOSerialBSDClient::devsw, 0);
    }
	fFunnelLock = IOLockAlloc();
    if (!isValid())
        IOLog("IOSerialBSDClient didn't initialize");
}
#undef OSSYM

IOSerialBSDClientGlobals::~IOSerialBSDClientGlobals()
{
#if JLOG
	kprintf("IOSerialBSDClientGlobals::~IOSerialBSDClientGlobals\n");
#endif	
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
	if (fFunnelLock) {
		IOLockFree(fFunnelLock);
	}
}

void IOSerialBSDClientGlobals::takefFunnelLock() {
	IOLockLock(fFunnelLock);
}
void IOSerialBSDClientGlobals::releasefFunnelLock() {
	IOLockUnlock(fFunnelLock);
}
dev_t IOSerialBSDClientGlobals::assign_dev_t()
{
    unsigned int i;
    debug(MULTI,"begin");
	for (i = 0; i < fLastMinor && fClients[i]; i++)
        ;

    if (i == fLastMinor)
    {
        unsigned int newLastMinor = fLastMinor + 4;
        IOSerialBSDClient **newClients;

        newClients = (IOSerialBSDClient **)
                    IOMalloc(newLastMinor * sizeof(fClients[0]));
        if (!newClients) {
            debug(MULTI,"end not normal");
			return (dev_t) -1;
		}
        bzero(&newClients[fLastMinor], 4 * sizeof(fClients[0]));
        bcopy(fClients, newClients, fLastMinor * sizeof(fClients[0]));
        IOFree(fClients, fLastMinor * sizeof(fClients[0]));
        fLastMinor = newLastMinor;
        fClients = newClients;
    }

    dev_t dev = makedev(fMajor, i << TTY_NUM_FLAGS);
    fClients[i] = (IOSerialBSDClient *) -1;
    debug(MULTI,"end normal");
    return dev;
}

bool IOSerialBSDClientGlobals::
registerTTY(dev_t dev, IOSerialBSDClient *client)
{
    debug(MULTI,"begin");
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
	debug(MULTI,"end");
    return ret;
}

// Assumes the caller has grabbed the funnel if necessary.
// Any call from UNIX is funnelled, I need to funnel any IOKit upcalls
// explicitly.
IOSerialBSDClient *IOSerialBSDClientGlobals::getClient(dev_t dev)
{
    //debug(MULTI,"begin");
    return fClients[TTY_UNIT(dev)];
}

const OSSymbol *IOSerialBSDClientGlobals::
getUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix, dev_t dev)
{
    OSSet *suffixSet = 0;	
    debug(MULTI,"begin");
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
        snprintf(ind, sizeof (ind), "%d", TTY_UNIT(dev));

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
	debug(MULTI,"end");
    return suffix;
}

void IOSerialBSDClientGlobals::
releaseUniqueTTYSuffix(const OSSymbol *inName, const OSSymbol *suffix)
{
    OSSet *suffixSet;
    debug(MULTI,"begin");
    suffixSet = (OSSet *) fNames->getObject(inName);
    if (suffixSet)
        suffixSet->removeObject((OSObject *) suffix);
	
	debug(MULTI,"end");
}

bool IOSerialBSDClient::
createDevNodes()
{
    bool ret = false;
    OSData *tmpData;
    OSString *deviceKey = 0, *calloutName = 0, *dialinName = 0;
    void *calloutNode = 0, *dialinNode = 0;
    const OSSymbol *nubName, *suffix;
    debug(MULTI,"begin");

    // Convert the provider's base name to an OSSymbol if necessary
    nubName = (const OSSymbol *) fProvider->getProperty(gIOTTYBaseNameKey);
    if (!nubName || !OSDynamicCast(OSSymbol, (OSObject *) nubName)) {
        if (nubName)
            nubName = OSSymbol::withString((OSString *) nubName);
        else
            nubName = OSSymbol::withCString("");
        if (!nubName) {
			debug(MULTI,"no Nub for basename");
            return false;
		}
        ret = fProvider->setProperty(gIOTTYBaseNameKey, (OSObject *) nubName);
        nubName->release();
        if (!ret) {
			debug(MULTI,"no Nub for basename SET");
            return false;
		}
    }

    // Convert the provider's suffix to an OSSymbol if necessary
    suffix = (const OSSymbol *) fProvider->getProperty(gIOTTYSuffixKey);
    if (!suffix || !OSDynamicCast(OSSymbol, (OSObject *) suffix)) {
        if (suffix)
            suffix = OSSymbol::withString((OSString *) suffix);
        else
            suffix = OSSymbol::withCString("");
        if (!suffix) {
			debug(MULTI,"no suffix");
            return false;
		}
        ret = fProvider->setProperty(gIOTTYSuffixKey, (OSObject *) suffix);
        suffix->release();
        if (!ret) {
			debug(MULTI,"no suffix SET");
            return false;
		}
    }

    suffix = sBSDGlobals.getUniqueTTYSuffix(nubName, suffix, fBaseDev);
    if (!suffix) {
		debug(MULTI,"no UniqueTTYSuffix");
        return false;
	}
	
    setProperty(gIOTTYSuffixKey,   (OSObject *) suffix);
    fProvider->setProperty(gIOTTYSuffixKey,   (OSObject *) suffix);
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
        tmpData = OSData::withCapacity(devLen + (uint32_t)sizeof(TTY_CALLOUT_PREFIX));
        if (tmpData) {
            tmpData->appendBytes(TTY_CALLOUT_PREFIX,
                                 (uint32_t)sizeof(TTY_CALLOUT_PREFIX)-1);
            tmpData->appendBytes(deviceKey->getCStringNoCopy(), devLen);
            calloutName = OSString::
                withCString((char *) tmpData->getBytesNoCopy());
            tmpData->release();
        }
        if (!tmpData || !calloutName)
            break;

        // Create the dialinName symbol
        tmpData = OSData::withCapacity(devLen + (uint32_t)sizeof(TTY_DIALIN_PREFIX));
        if (tmpData) {
            tmpData->appendBytes(TTY_DIALIN_PREFIX,
                                 (uint32_t)sizeof(TTY_DIALIN_PREFIX)-1);
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
            (char *) calloutName->getCStringNoCopy() + (uint32_t)sizeof(TTY_DEVFS_PREFIX) - 1);
        dialinNode = devfs_make_node(fBaseDev | TTY_DIALIN_INDEX,
            DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
            (char *) dialinName->getCStringNoCopy() + (uint32_t)sizeof(TTY_DEVFS_PREFIX) - 1);
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
	debug(MULTI,"finish");
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
    debug(MULTI,"begin");
	

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
				debug(MULTI,"finish normal");
                return ret;
            }
        }
    }
	debug(MULTI,"finish error");
    return false;
}

bool IOSerialBSDClient::
start(IOService *provider)
{
    debug(MULTI,"begin");
	
	fBaseDev = -1;
    if (!super::start(provider))
        return false;

    if (!sBSDGlobals.isValid())
        return false;
	
	sBSDGlobals.takefFunnelLock();

    fProvider = OSDynamicCast(IOSerialStreamSync, provider);
    if (!fProvider)
        return false;

    fThreadLock = IOLockAlloc();
    if (!fThreadLock)
        return false;
	
	fOpenCloseLock = IOLockAlloc();
	if (!fOpenCloseLock)
		return false;
	
	fIoctlLock = IOLockAlloc();
	if (!fIoctlLock)
		return false;
		
	fisBlueTooth = false;
	fPreemptInProgress = false;
	fDCDThreadCall = 0;
	

    /*
     * First initialise the dial in device.  
     * We don't use all the flags from <sys/ttydefaults.h> since they are
     * only relevant for logins.
	 * 
	 * initialize the hotplug flag to zero (bsd hasn't attached so we are safe to unplug)
     */
    fSessions[TTY_DIALIN_INDEX].fThis = this;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_iflag = 0;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_oflag = 0;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_cflag = TTYDEF_CFLAG;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_lflag = 0;
    fSessions[TTY_DIALIN_INDEX].fInitTerm.c_ispeed
	= fSessions[TTY_DIALIN_INDEX].fInitTerm.c_ospeed
	= (gPESerialBaud == -1)? TTYDEF_SPEED : gPESerialBaud;
    termioschars(&fSessions[TTY_DIALIN_INDEX].fInitTerm);

    // Now initialise the call out device
	// 
	// initialize the hotplug flag to zero (bsd hasn't attached so we are safe to unplug)

    fSessions[TTY_CALLOUT_INDEX].fThis = this;
    fSessions[TTY_CALLOUT_INDEX].fInitTerm
        = fSessions[TTY_DIALIN_INDEX].fInitTerm;

    do {
		
		fBaseDev = sBSDGlobals.assign_dev_t();
        if ((dev_t) -1 == fBaseDev) {
            break;
		}
        if (!createDevNodes()) {
           break;
		}
        if (!setBaseTypeForDev()) {
           break;
		}
        if (!sBSDGlobals.registerTTY(fBaseDev, this)) {
            break;
		}
        // Let userland know that this serial port exists
        registerService();
		debug(MULTI," finished");
		sBSDGlobals.releasefFunnelLock();
        return true;
    } while (0);
    // Failure path
    debug(MULTI," Cleanup Resources");
    sBSDGlobals.releasefFunnelLock();
	cleanupResources();
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
        IOLogCond(logMatch, "TTY.%s: Failed superclass match\n",
                             devName(this));
        return false;	// One of the name based matches has failed, thats it.
    }

    // Do some name matching
    matched = compareProperty(table, gIOTTYDeviceKey)
           && compareProperty(table, gIOTTYBaseNameKey)
           && compareProperty(table, gIOTTYSuffixKey)
           && compareProperty(table, gIOCalloutDeviceKey)
           && compareProperty(table, gIODialinDeviceKey);
    if (!matched) {
        IOLogCond(logMatch, "TTY.%s: Failed non type based match\n",
                             devName(this));
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
        IOLogCond(logMatch, "TTY.%s: %s isn't an OSString?\n",
                             devName(this),
                             kIOSerialBSDTypeKey);
        return false;
    }
    desiredLen = desiredType->getLength();
    desiredName = desiredType->getCStringNoCopy();
	debug(FLOW, "desiredName is: %s", desiredName);

    // Walk through the provider super class chain looking for an
    // interface but stop at IOService 'cause that aint a IOSerialStream.
    for (providerClass = fProvider->getMetaClass();
		 
         providerClass && providerClass != IOService::metaClass;
         providerClass = providerClass->getSuperClass())
    {
        // Check if provider class is prefixed by desiredName
        // Prefix 'cause IOModemSerialStream & IOModemSerialStreamSync
        // should both match and if I just look for the prefix they will
        if (!strncmp(providerClass->getClassName(), desiredName, desiredLen)) {
			if (fProvider->metaCast("IOBluetoothSerialClientModemStreamSync") || fProvider->metaCast("IOBluetoothSerialClientSerialStreamSync")) {
				debug(FLOW,"ah hah, bluetooth");
				fisBlueTooth = true;
			}
			return true;
		}
    }

	
    // Couldn't find the desired name in the super class chain
    // so report the failure and return false
    IOLogCond(logMatch, "TTY.%s: doesn't have a %s interface\n",
            devName(this),
            desiredName);
    return false;
}

void IOSerialBSDClient::free()
{
    
#if JLOG
		kprintf("IOSerialBSDClient::free\n");
#endif
	sBSDGlobals.takefFunnelLock();
	if ((dev_t) -1 != fBaseDev) {
	    sBSDGlobals.registerTTY(fBaseDev, 0);
    }

	Session *sp = &fSessions[TTY_DIALIN_INDEX];
	if (sp->ftty) {
		ttyfree(sp->ftty);
		sp->ftty = NULL;
		debug(FLOW,"we free'd the ftty struct");
	}
	
    if (fThreadLock)
	IOLockFree(fThreadLock);
	
	if (fOpenCloseLock)
		IOLockFree(fOpenCloseLock);
	
	if (fIoctlLock)
		IOLockFree(fIoctlLock);
		
	if (fDCDThreadCall) {
		debug(DCDTRD, "DCDThread is freed in free");
		thread_call_cancel(fDCDThreadCall);
		thread_call_free(fDCDThreadCall);
		fDCDThreadCall = 0;
		fDCDTimerDue = false;
	}
	sBSDGlobals.releasefFunnelLock();
    super::free();
}

bool IOSerialBSDClient::
requestTerminate(IOService *provider, IOOptionBits options)
{
#if JLOG	
	kprintf("IOSerialBSDClient::requestTerminate\n");
#endif
    do {
		// Don't have anything to do, just a teardown synchronisation
		// for the isInactive() call.  We can't be made inactive in a 
		// funneled call anymore
			
		// ah, but we're not under the funnel anymore...
		// so we'll call out to the termination routine so we can still 
		// synchronize...
		
		if (super::requestTerminate(provider, options)) 
			return (true);		
			
		} while(1);
	// can't get here
	return(true);
}

bool IOSerialBSDClient::
didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    bool deferTerm;
    {
#if JLOG	
		kprintf("IOSerialBSDClient::didTerminate\n");
#endif
		cleanupResources();

        for (int i = 0; i < TTY_NUM_TYPES; i++) {
            Session *sp = &fSessions[i];
            struct tty *tp = sp->ftty;
			
            // Now kill any stream that may currently be running
            sp->fErrno = ENXIO;
			
			if (tp == NULL) // we found a session with no tty configured
				continue;
#if JLOG	
			kprintf("IOSerialBSDClient::didTerminate::we still have a session around...\n");
#endif
            // Enforce a zombie and unconnected state on the discipline
			tty_lock(tp);
			CLR(tp->t_cflag, CLOCAL);		// Fake up a carrier drop
			(void) bsdld_modem(tp, false);
			tty_unlock(tp);
        }
	fActiveSession = 0;
	deferTerm = (frxThread || ftxThread || fInOpensPending);
	if (deferTerm) {
	    fKillThreads = true;
	    fProvider->executeEvent(PD_E_ACTIVE, false);
	    fDeferTerminate = true;
	    *defer = true;	// Defer until the threads die
	}
	else
	    SAFE_PORTRELEASE(fProvider);
		
    }

    return deferTerm || super::didTerminate(provider, options, defer);
}

IOReturn IOSerialBSDClient::
setOneProperty(const OSSymbol *key, OSObject * /* value */)
{
#if JLOG	
	kprintf("IOSerialBSDClient::setOneProperty\n");
#endif
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
#if JLOG	
	kprintf("IOSerialBSDClient::setProperties\n");
#endif
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
    return res;		// Successful just return now
}

// Bracket all open attempts with a reference on ourselves. 
int IOSerialBSDClient::
iossopen(dev_t dev, int flags, int devtype, struct proc *p)
{
#if JLOG	
	kprintf("IOSerialBSDClient::iossopen\n");
#endif
	sBSDGlobals.takefFunnelLock();
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
	sBSDGlobals.releasefFunnelLock();
    if (!me || me->isInactive())
        return ENXIO;

    me->retain();
	
	// protect the open from the close
	IOLockLock(me->fOpenCloseLock);
	
    int ret = me->open(dev, flags, devtype, p);
	
	IOLockUnlock(me->fOpenCloseLock);
    me->release();

    return ret;
}

int IOSerialBSDClient::
iossclose(dev_t dev, int flags, int devtype, struct proc *p)
{
#if JLOG	
	kprintf("IOSerialBSDClient::iossclose enter\n");
#endif
	sBSDGlobals.takefFunnelLock();
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
	sBSDGlobals.releasefFunnelLock();
    if (!me)
        return ENXIO;

    Session *sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    struct tty *tp = sp->ftty;
	
	// protect the close from the open
	IOLockLock(me->fOpenCloseLock);

    if (!ISSET(tp->t_state, TS_ISOPEN)) {
		IOLockUnlock(me->fOpenCloseLock);
	    return EBADF;
	}
	
    me->close(dev, flags, devtype, p);
	IOLockUnlock(me->fOpenCloseLock);
	// Remember this is the last close so we may have to delete ourselves
	// This reference was held just before we opened the line discipline
    // in open().
    me->release();	
#if JLOG	
	kprintf("IOSerialBSDClient::iossclose exit\n");
#endif	
    return 0;
}

int IOSerialBSDClient::
iossread(dev_t dev, struct uio *uio, int ioflag)
{
#if JLOG	
	kprintf("IOSerialBSDClient::iossread\n");
#endif
	sBSDGlobals.takefFunnelLock();
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
	sBSDGlobals.releasefFunnelLock();
    int error;

    assert(me);

    Session *sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    struct tty *tp = sp->ftty;
	

    error = sp->fErrno;
    if (!error) {
		tty_lock(tp);
		error = bsdld_read(tp, uio, ioflag);
		tty_unlock(tp);

        if (me->frxBlocked && TTY_QUEUESIZE(tp) < TTY_LOWWATER) {
#if JLOG	
			kprintf("IOSerialBSDClient::iossread::TTY_QUEUESIZE(tp) < TTY_LOWWATER\n");
#endif			
            me->sessionSetState(sp, PD_S_RX_EVENT, PD_S_RX_EVENT);
		}
    }
	

    return error;
}

int IOSerialBSDClient::
iosswrite(dev_t dev, struct uio *uio, int ioflag)
{
	sBSDGlobals.takefFunnelLock();
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
	sBSDGlobals.releasefFunnelLock();
    int error;
	
#if JLOG
	kprintf("IOSerialBSDClient::iosswrite\n");
#endif
    assert(me);

    Session *sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    struct tty *tp = sp->ftty;

    error = sp->fErrno;
	
    if (!error) {
		tty_lock(tp);
        error = bsdld_write(tp, uio, ioflag);
		tty_unlock(tp);
	}
	
    return error;
}

int IOSerialBSDClient::
iossselect(dev_t dev, int which, void *wql, struct proc *p)
{
	sBSDGlobals.takefFunnelLock();
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
	sBSDGlobals.releasefFunnelLock();
    int error;
	
#if JLOG	
	kprintf("IOSerialBSDClient::iossselect\n");
#endif

    assert(me);

    Session *sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    struct tty *tp = sp->ftty;
	
    error = sp->fErrno;
    if (!error) {
		tty_lock(tp);
        error = ttyselect(tp, which, wql, p);
		tty_unlock(tp);
	}
    return error;
}

static inline int
tiotors232(int bits)
{
    UInt32 out_b = bits;
#if JLOG	
	kprintf("IOSerialBSDClient::tiotors232\n");
#endif
    out_b &= ( PD_RS232_S_DTR | PD_RS232_S_RFR | PD_RS232_S_CTS
	     | PD_RS232_S_CAR | PD_RS232_S_BRK );
    return out_b;
}

static inline int
rs232totio(int bits)
{
    UInt32 out_b = bits;
#if JLOG	
	kprintf("IOSerialBSDClient::rs232totio\n");
#endif

    out_b &= ( PD_RS232_S_DTR | PD_RS232_S_DSR
             | PD_RS232_S_RFR | PD_RS232_S_CTS
             | PD_RS232_S_BRK | PD_RS232_S_CAR  | PD_RS232_S_RNG);
    return out_b;
}

int IOSerialBSDClient::
iossioctl(dev_t dev, u_long cmd, caddr_t data, int fflag,				// XXX64
                         struct proc *p)
{
	sBSDGlobals.takefFunnelLock();
    IOSerialBSDClient *me = sBSDGlobals.getClient(dev);
	sBSDGlobals.releasefFunnelLock();
    int error = 0;
	
    debug(FLOW, "begin");
    IOLockLock(me->fIoctlLock);
    assert(me);

    Session *sp = &me->fSessions[IS_TTY_OUTWARD(dev)];
    struct tty *tp = sp->ftty;
	
    if (sp->fErrno) {
		debug(FLOW,"immediate error sp->fErrno: %d", sp->fErrno);
        error = sp->fErrno;
        goto exitIoctl;
    }

    /*
     * tty line disciplines return >= 0 if they could process this
     * ioctl request.  If so, simply return, we're done
     */
    error = bsdld_ioctl(tp, cmd, data, fflag, p);
    if (ENOTTY != error) {
		debug(FLOW,"got ENOTTY from BSD land");
        me->optimiseInput(&tp->t_termios);
        goto exitIoctl;
    }

    // ...->l_ioctl may block so we need to check our state again
    if (sp->fErrno) {
		debug(FLOW,"recheck sp->fErrno: %d", sp->fErrno);
        error = sp->fErrno;
        goto exitIoctl;
    }

	debug(CONTROL, "cmd: 0x%lx", cmd);
	
    /* First pre-process and validate ioctl command */
    switch(cmd)
    {
    case TIOCGETA_32:
    {
        debug(CONTROL,"TIOCGETA_32");
#ifdef __LP64__
        termios64to32((struct user_termios *)&tp->t_termios, (struct termios32 *)data);
#else
        bcopy(&tp->t_termios, data, sizeof(struct termios));
#endif
        me->convertFlowCtrl(sp, (struct termios *) data);
        error = 0;
        goto exitIoctl;
    }
    case TIOCGETA_64:
    {
        debug(CONTROL,"TIOCGETA_64");
#ifdef __LP64__
		bcopy(&tp->t_termios, data, sizeof(struct termios));
#else
		termios32to64((struct termios32 *)&tp->t_termios, (struct user_termios *)data);
#endif
        me->convertFlowCtrl(sp, (struct termios *) data);
        error = 0;
        goto exitIoctl;
    }
    case TIOCSETA_32:
    case TIOCSETAW_32:
    case TIOCSETAF_32:
    case TIOCSETA_64:
    case TIOCSETAW_64:
    case TIOCSETAF_64:
    {
		debug(CONTROL,"TIOCSETA_32/64/TIOCSETAW_32/64/TIOCSETAF_32/64");
		struct termios *dt = (struct termios *)data;
		struct termios lcl_termios;

#ifdef __LP64__
        if (cmd==TIOCSETA_32 || cmd==TIOCSETAW_32 || cmd==TIOCSETAF_32) {
            termios32to64((struct termios32 *)data, (struct user_termios *)&lcl_termios);
            dt = &lcl_termios;
        }
#else
        if (cmd==TIOCSETA_64 || cmd==TIOCSETAW_64 || cmd==TIOCSETAF_64) {
            termios64to32((struct user_termios *)data, (struct termios32 *)&lcl_termios);
            dt = &lcl_termios;
        }
#endif

        /* Convert the PortSessionSync's flow control setting to termios */
		tty_lock(tp);
        me->convertFlowCtrl(sp, &tp->t_termios);
		tty_unlock(tp);
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
		debug(CONTROL,"TIOCEXCL");
        // Force the TIOCEXCL ioctl to be atomic!
        if (ISSET(tp->t_state, TS_XCLUDE)) {
            error = EBUSY;
            goto exitIoctl;
        }
        break;

    default:
        break;
    }

    /* See if generic tty understands this. */
    if ( (error = ttioctl(tp, cmd, data, fflag, p)) != ENOTTY) {
		debug(CONTROL,"generic tty handled this");
        if (error) {
            iossparam(tp, &tp->t_termios);	/* reestablish old state */
		}
        me->optimiseInput(&tp->t_termios);
	goto exitIoctl;        
    }

    // ttioctl may block so we need to check our state again
    if (sp->fErrno) {
		debug(FLOW,"2nd recheck sp->fErrno: %d", sp->fErrno);
        error = sp->fErrno;
        goto exitIoctl;
    }

    //
    // The generic ioctl handlers don't know what is going on
    // so try to interpret them here.
    //
    error = 0;
    switch (cmd)
    {
    case TIOCSBRK:
		debug(CONTROL,"TIOCSBRK");			
        (void) me->mctl(PD_RS232_S_BRK, DMBIS);  break;

	case TIOCCBRK:
		debug(CONTROL,"TIOCCBRK");
        (void) me->mctl(PD_RS232_S_BRK, DMBIC);  break;

    case TIOCSDTR:
		debug(CONTROL,"TIOCSDTR");			
        (void) me->mctl(PD_RS232_S_DTR, DMBIS);  break;

    case TIOCCDTR:
		debug(CONTROL,"TIOCCDTR");			
        (void) me->mctl(PD_RS232_S_DTR, DMBIC);  break;

    case TIOCMSET:
		debug(CONTROL,"TIOCMSET");			
        (void) me->mctl(tiotors232(*(int *)data), DMSET);  break;

    case TIOCMBIS:
		debug(CONTROL,"TIOCMBIS");			
        (void) me->mctl(tiotors232(*(int *)data), DMBIS);  break;

    case TIOCMBIC:
		debug(CONTROL,"TIOCMBIC");			
        (void) me->mctl(tiotors232(*(int *)data), DMBIC);  break;

    case TIOCMGET:
		debug(CONTROL,"TIOCMGET");			
        *(int *)data = rs232totio(me->mctl(0,     DMGET)); break;

    case IOSSDATALAT_32:
    case IOSSDATALAT_64:
        // all users currently assume this data is a UInt32
		debug(CONTROL,"IOSSDATALAT");
        (void) me->sessionExecuteEvent(sp, PD_E_DATA_LATENCY, *(UInt32 *) data);
        break;

    case IOSSPREEMPT:
		debug(CONTROL,"IOSSPREEMPT");
        me->fPreemptAllowed = (bool) (*(int *) data);
        if (me->fPreemptAllowed) {
            me->fLastUsedTime = kNever;
            // initialize fPreemptInProgress in case we manage to 
            // call the Preemption ioctl before it is initialized elsewhere
            //me->fPreemptInProgress = false;
        }
        else
            wakeup(&me->fPreemptAllowed);	// Wakeup any pre-empters
        break;

    case IOSSIOSPEED_32:
    case IOSSIOSPEED_64:
    {
		debug(CONTROL,"IOSSIOSPEED_32/64");
		speed_t speed = *(speed_t *) data;

		// Remember that the speed is in half bits 
		IOReturn rtn = me->sessionExecuteEvent(sp, PD_E_DATA_RATE, speed << 1);
		debug(CONTROL, "IOSSIOSPEED_32 session execute return: 0x%x", rtn);
		if (kIOReturnSuccess != rtn) {
			error = (kIOReturnBadArgument == rtn)? EINVAL : EDEVERR;
			break;
		}
		tty_lock(tp);
		tp->t_ispeed = tp->t_ospeed = speed;
		ttsetwater(tp);
		tty_unlock(tp);
		break;
    }

    default: debug(CONTROL,"Unhandled ioctl"); error = ENOTTY; break;
    }

exitIoctl:
    /*
     * These flags functionality has been totally subsumed by the PortDevice
     * driver so make sure they always get cleared down before any serious
     * work is done.
     */
	debug(FLOW, "exiting");
	tty_lock(tp);
    CLR(tp->t_iflag, IXON | IXOFF | IXANY);
    CLR(tp->t_cflag, CRTS_IFLOW | CCTS_OFLOW);
	tty_unlock(tp);
	IOLockUnlock(me->fIoctlLock);
    return error;
}


void IOSerialBSDClient::
iossstart(struct tty *tp)
{
	Session *sp = (Session *)tp->t_iokit;
    IOSerialBSDClient *me = sp->fThis;
    IOReturn rtn;
	
#if JLOG	
	kprintf("IOSerialBSDClient::iossstart\n");
#endif

    assert(me);

    if (sp->fErrno)
	return;

    if ( !me->fIstxEnabled && !ISSET(tp->t_state, TS_TTSTOP) ) {
        me->fIstxEnabled = true;
#if JLOG	
		kprintf("iossstart calls sessionSetState to enable PD_S_TX_ENABLE\n");
#endif
        me->sessionSetState(sp, -1U, PD_S_TX_ENABLE);
    }

    if  (tp->t_outq.c_cc) {
        // Notify the transmit thread of action to be performed
#if JLOG	
		kprintf("iossstart calls sessionSetState to do the PD_S_TX_EVENT\n");
#endif
		rtn = me->sessionSetState(sp, PD_S_TX_EVENT, PD_S_TX_EVENT);
		assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);
    }
}

int IOSerialBSDClient::
iossstop(struct tty *tp, int rw)
{
	
    Session *sp = (Session *) tp->t_iokit;
    IOSerialBSDClient *me = sp->fThis;
#if JLOG	
	kprintf("IOSerialBSDClient::iossstop\n");
#endif

    assert(me);
    if (sp->fErrno)
	return 0;

    if ( ISSET(tp->t_state, TS_TTSTOP) ) {
		me->fIstxEnabled = false;
#if JLOG	
		kprintf("iossstop calls sessionSetState to disable PD_S_TX_EVENT\n");
#endif
		me->sessionSetState(sp, 0, PD_S_TX_ENABLE);
    }

    if ( ISSET(rw, FWRITE) ) {
#if JLOG	
		kprintf("iossstop calls sessionExecuteEvent to PD_E_TXQ_FLUSH\n");
#endif
        me->sessionExecuteEvent(sp, PD_E_TXQ_FLUSH, 0);
	}
    if ( ISSET(rw, FREAD) ) {
#if JLOG	
		kprintf("iossstop calls sessionExecuteEvent to PD_E_RXQ_FLUSH\n");
#endif
        me->sessionExecuteEvent(sp, PD_E_RXQ_FLUSH, 0);
        if (me->frxBlocked)	{ // wake up a blocked reader
#if JLOG	
			kprintf("iossstop calls sessionSetState to wake PD_S_RX_ENABLE\n");
#endif
            me->sessionSetState(sp, PD_S_RX_ENABLE, PD_S_RX_ENABLE);
		}
    }
    return 0;
}

/*
 * Parameter control functions
 * 
 */
int IOSerialBSDClient::
iossparam(struct tty *tp, struct termios *t)
{

    Session *sp = (Session *) tp->t_iokit;
    IOSerialBSDClient *me = sp->fThis;
    u_long data;
    int cflag, error;
    IOReturn rtn = kIOReturnOffline;
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam\n");
#endif

    assert(me);

    if (sp->fErrno)
	goto exitParam;

    rtn = kIOReturnBadArgument;
    if (ISSET(t->c_iflag, (IXOFF|IXON))
    && (t->c_cc[VSTART]==_POSIX_VDISABLE || t->c_cc[VSTOP]==_POSIX_VDISABLE))
        goto exitParam;

    /* do historical conversions */
    if (t->c_ispeed == 0) {
        t->c_ispeed = t->c_ospeed;
	}

    /* First check to see if the requested speed is one of our valid ones */
    data = ttspeedtab(t->c_ospeed, iossspeeds);

    if ((int) data != -1 && t->c_ispeed == t->c_ospeed) {
#if JLOG	
		kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_E_DATA_RATE, %d\n", (int)data);
#endif
	
	rtn  = me->sessionExecuteEvent(sp, PD_E_DATA_RATE, data); 
	}
    else if ( (IOSS_HALFBIT_BRD & t->c_ospeed) ) {
	/*
	 * MIDI clock speed multipliers are used for externally clocked MIDI
	 * devices, and are evident by a 1 in the low bit of c_ospeed/c_ispeed
	 */
	data = (u_long) t->c_ospeed >> 1;	// set data to MIDI clock mode 
#if JLOG	
		kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_E_EXTERNAL_CLOCK_MODE, %d\n", (int)data);
#endif
		
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
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_E_DATA_SIZE, %d\n", (int)data);
#endif
	
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
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_E_DATA_INTEGRITY, %d\n", (int)data);
#endif
	
    rtn = me->sessionExecuteEvent(sp, PD_E_DATA_INTEGRITY, data);
    if (rtn)
        goto exitParam;

    /* Set stop bits to 2 1/2 bits in length */
    if (ISSET(cflag, CSTOPB))
        data = 4;
    else
        data = 2;
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_RS232_E_STOP_BITS, %d\n", (int)data);
#endif
	
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
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_E_FLOW_CONTROL, %d\n", (int)data);
#endif
	
    rtn = me->sessionExecuteEvent(sp, PD_E_FLOW_CONTROL, data);
    if (rtn)
        goto exitParam;

    //
    // Load the flow control start and stop characters.
    //
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_RS232_E_XON_BYTE, t->c_cc[VSTART]\n");
#endif
    rtn  = me->sessionExecuteEvent(sp, PD_RS232_E_XON_BYTE,  t->c_cc[VSTART]);
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionExecuteEvent::PD_RS232_E_XOFF_BYTE, t->c_cc[VSTOP]\n");
#endif
    rtn |= me->sessionExecuteEvent(sp, PD_RS232_E_XOFF_BYTE, t->c_cc[VSTOP]);
    if (rtn)
        goto exitParam;

    /* Always enable for transmission */
    me->fIstxEnabled = true;
#if JLOG	
	kprintf("IOSerialBSDClient::iossparam::sessionSetState::PD_S_TX_ENABLE, PD_S_TX_ENABLE\n");
#endif
    rtn = me->sessionSetState(sp, PD_S_TX_ENABLE, PD_S_TX_ENABLE);
    if (rtn)
        goto exitParam;

    /* Only enable reception if necessary */
    if ( ISSET(cflag, CREAD) ) {
#if JLOG	
		kprintf("IOSerialBSDClient::iossparam::sessionSetState::-1U, PD_S_RX_ENABLE\n");
#endif	
        rtn = me->sessionSetState(sp, -1U, PD_S_RX_ENABLE);
	}
    else {
#if JLOG	
		kprintf("IOSerialBSDClient::iossparam::sessionSetState::0U, PD_S_RX_ENABLE\n");
#endif			
        rtn = me->sessionSetState(sp,  0U, PD_S_RX_ENABLE);
	}

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
open(dev_t dev, int flags, int /* devtype */, struct proc * /* p */)
{
    Session *sp;
    struct tty *tp;
    int error = 0;
    bool wantNonBlock = flags & O_NONBLOCK;
    bool imPreempting = false;
    bool firstOpen = false;
    // fPreemptInProgress is false at the beginning of every open
    // as Preemption can only occur later in the open process
    //fPreemptInProgress = false;
	
#if JLOG	
	kprintf("IOSerialBSDClient::open\n");
#endif
	
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
	if (sp->ftty == NULL) {
#if JLOG	
		kprintf("IOSerialBSDClient::open::ttymalloc'd\n");
#endif		
		sp->ftty = ttymalloc();
		sp->ftty->t_iokit = sp;
	}	
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
        tp = fActiveSession->ftty;

        bool isCallout    = IS_TTY_OUTWARD(tp->t_dev);
		fisCallout = isCallout;
        bool isPreempt    = fPreemptAllowed;
        bool isExclusive  = ISSET(tp->t_state, TS_XCLUDE);
        bool isOpen       = ISSET(tp->t_state, TS_ISOPEN);
        bool wantCallout  = IS_TTY_OUTWARD(dev);
		fwantCallout = wantCallout;
        // kauth_cred_issuser returns opposite of suser used in Leopard
        bool wantSuser    = kauth_cred_issuser(kauth_cred_get());

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
//                else
//                    ; // Success - use current session
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
//                else
//                    ; // Success - use current session (root override)
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
//                else
//                    ; // Success - use current session
            }
        }
        else {
            // Is dial in and blocking for carrier, i.e. not open
            if (wantCallout) {
                imPreempting = true;
                preemptActive();
                goto checkBusy;
            }
//            else
//                ; // Successful, will wait for carrier later
        }
    }

    // If we are here then we have successfully run the open gauntlet.
    tp = sp->ftty;

    // If there is no active session that means that we have to acquire
    // the serial port.
    if (!fActiveSession) {
        IOReturn rtn = fProvider->acquirePort(/* sleep */ false);
	fAcquired = (kIOReturnSuccess == rtn);

	// Check for a unplug while we blocked acquiring the port
	if (isInactive()) {
	    SAFE_PORTRELEASE(fProvider);
	    error = ENXIO;
	    goto exitOpen;
	}
	else if (kIOReturnSuccess != rtn) {
            error = EBUSY;
            goto exitOpen;
        }

	// We acquired the port successfully
	fActiveSession = sp;
    }

    /*
     * Initialize Unix's tty struct,
     * set device parameters and RS232 state
     */
    if ( !ISSET(tp->t_state, TS_ISOPEN) ) {
        initSession(sp);
        // racey, racey - and initSession doesn't return/set anything useful
        if (!fActiveSession || isInactive()) {
            SAFE_PORTRELEASE(fProvider);
            error = ENXIO;
            goto exitOpen;
        }
		
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
		tty_lock(tp);
        bsdld_modem(tp, true);
		tty_unlock(tp);
    }

    if (!IS_TTY_OUTWARD(dev) && !ISSET(flags, FNONBLOCK)
    &&  !ISSET(tp->t_state, TS_CARR_ON) && !ISSET(tp->t_cflag, CLOCAL)) {

        // Drop transit while we wait for the carrier
        fInOpensPending++;	// Note we are sleeping
        endConnectTransit();

        /* Track DCD Transistion to high */
        UInt32 pd_state = PD_RS232_S_CAR;
        IOReturn rtn = sessionWatchState(sp, &pd_state, PD_RS232_S_CAR);

	// Rely on the funnel for atomicicity
	int wasPreempted = (EBUSY == sp->fErrno);
        fInOpensPending--;
	if (!fInOpensPending)
	    wakeup(&fInOpensPending);

        startConnectTransit(); 	// Sync with the pre-emptor here
	if (wasPreempted)	{
	    endConnectTransit();
	    goto checkBusy;	// Try again
	}
	else if (kIOReturnSuccess != rtn) {

	    // We were probably interrupted
	    if (!fInOpensPending) {
		// clean up if we are the last opener
		SAFE_PORTRELEASE(fProvider);
                fActiveSession = 0;

		if (fDeferTerminate && isInactive()) {
		    bool defer = false;
		    super::didTerminate(fProvider, 0, &defer);
		}
            }

            // End the connect transit lock and return the error
            endConnectTransit();
	    if (isInactive())
		return ENXIO;
	    else switch (rtn) {
	    case kIOReturnAborted:
	    case kIOReturnIPCError:	return EINTR;

	    case kIOReturnNotOpen:
	    case kIOReturnIOError:
	    case kIOReturnOffline:	return ENXIO;

	    default:
					return EIO;
	    }
	}
        
		// To be here we must be transiting and have DCD
		tty_lock(tp);
		bsdld_modem(tp, true);
		tty_unlock(tp);
		
    }

    tty_lock(tp);
    if ( !ISSET(tp->t_state, TS_ISOPEN) ) {
        firstOpen = true;
    }
    error = bsdld_open(dev, tp);    // sets TS_ISOPEN
    if (error) {
	tty_unlock(tp);
    } else {
	tty_unlock(tp);
	if (firstOpen) {
	    retain();	// Hold a reference until the port is closed
		// because we can still get caught up if we get yanked late
		if (!fActiveSession || isInactive()) {
			SAFE_PORTRELEASE(fProvider);
            error = ENXIO;
            goto exitOpen;
		}		
            launchThreads(); // launch the transmit and receive threads
		// and we got caught in launchThreads once
		if (!fActiveSession || isInactive()) {
			SAFE_PORTRELEASE(fProvider);
            error = ENXIO;
            goto exitOpen;
		}		
	}
    }

exitOpen:
    endConnectTransit();

    return error;
}

void IOSerialBSDClient::
close(dev_t dev, int flags, int /* devtype */, struct proc * /* p */)
{
    struct tty *tp;
    Session *sp;
    IOReturn rtn;
#if JLOG	
	kprintf("IOSerialBSDClient::close\n");
#endif

    startConnectTransit();

    sp = &fSessions[IS_TTY_OUTWARD(dev)];
    tp = sp->ftty;

    if (!tp->t_dev && fInOpensPending) {
	// Never really opened - time to give up on this device
        (void) fProvider->executeEvent(PD_E_ACTIVE, false);
        endConnectTransit();
        while (fInOpensPending)
            tsleep((caddr_t) &fInOpensPending, TTIPRI, "ttyrev", 0);
		retain();	// Hold a reference for iossclose to release()
        return;
    }
    /* We are closing, it doesn't matter now about holding back ... */
	tty_lock(tp);
    CLR(tp->t_state, TS_TTSTOP);
	tty_unlock(tp);
    
    if (!sp->fErrno) {
        (void) sessionExecuteEvent(sp, PD_E_FLOW_CONTROL, 0);
        (void) sessionSetState(sp, -1U, PD_S_RX_ENABLE | PD_S_TX_ENABLE);

        // Clear any outstanding line breaks
        rtn = sessionEnqueueEvent(sp, PD_RS232_E_LINE_BREAK, false, true);
	assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);
    }
	tty_lock(tp);
    bsdld_close(tp, flags);
	tty_unlock(tp);
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
	tty_lock(tp);
    ttyclose(tp);	// Drops TS_ISOPEN flag
    assert(!tp->t_outq.c_cc);

    tty_unlock(tp);
    // Shut down the port, this will cause the RX && TX threads to terminate
    // Then wait for threads to terminate, this should be over very quickly.

    if (!sp->fErrno)
        killThreads(); // Disable the chip

    if (sp == fActiveSession)
    {
	SAFE_PORTRELEASE(fProvider);
        fPreemptAllowed = false;
        fActiveSession = 0;
        wakeup(&fPreemptAllowed);	// Wakeup any pre-empters
    }

    sp->fErrno = 0;	/* Clear the error condition on last close */
	
    endConnectTransit();
}
/*
 * no lock is assumed
 */
void IOSerialBSDClient::
initSession(Session *sp)
{
    struct tty *tp = sp->ftty;
    IOReturn rtn;
	
#if JLOG	
	kprintf("IOSerialBSDClient::initSession\n");
#endif
	
	tty_lock(tp);
    tp->t_oproc = iossstart;
    tp->t_param = iossparam;
    tp->t_termios = sp->fInitTerm;	
    ttsetwater(tp);
	tty_unlock(tp);
    /* Activate the session's port */
#if JLOG	
	kprintf("IOSerialBSDClient::initSession::sessionExecuteEvent::PD_E_ACTIVE\n");
#endif
	
    rtn = sessionExecuteEvent(sp, PD_E_ACTIVE, true);
    if (rtn)
        IOLog("ttyioss%04x: ACTIVE failed (%x)\n", tp->t_dev, rtn);
#if JLOG	
	kprintf("IOSerialBSDClient::initSession::sessionExecuteEvent::PD_E_TXQ_FLUSH, 0\n");
#endif
	
    rtn  = sessionExecuteEvent(sp, PD_E_TXQ_FLUSH, 0);
#if JLOG	
	kprintf("IOSerialBSDClient::initSession::sessionExecuteEvent::PD_E_RXQ_FLUSH, 0\n");
#endif
	
    rtn |= sessionExecuteEvent(sp, PD_E_RXQ_FLUSH, 0);
    assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);
	
	tty_lock(tp);
    CLR(tp->t_state, TS_CARR_ON | TS_BUSY);
	tty_unlock(tp);
	
    fKillThreads = false;
	
	if (!fDCDThreadCall) {
		debug(DCDTRD, "DCDThread is allocated");
		fDCDThreadCall =
			thread_call_allocate(&IOSerialBSDClient::iossdcddelay, this);
	}
    // racey again
    // if the early part of initSession takes too long to complete
    // we could have been unplugged (or reset) so we should check
    // we wait until here because this is the first place we're
    // touching the hardware semi-directly
    if(sp->fErrno || !fActiveSession || isInactive()) {
		debug(DCDTRD, "and then we return offline");
        rtn = kIOReturnOffline;
        return;
    }
    // Cycle the PD_RS232_S_DTR line if necessary 
    if ( !ISSET(fProvider->getState(), PD_RS232_S_DTR) ) {
        (void) waitOutDelay(0, &fDTRDownTime, &kDTRDownDelay);
        // racey, racey 
        if(sp->fErrno || !fActiveSession || isInactive()) {
            rtn = kIOReturnOffline;
            return;
        } else 
            (void) mctl(RS232_S_ON, DMSET);
	}

    // Disable all flow control  & data movement initially
#if JLOG	
	kprintf("IOSerialBSDClient::initSession::sessionExecuteEvent::PD_E_FLOW_CONTROL, 0\n");
#endif
    rtn  = sessionExecuteEvent(sp, PD_E_FLOW_CONTROL, 0);
#if JLOG	
	kprintf("IOSerialBSDClient::initSession::sessionSetState::0, PD_S_RX_ENABLE | PD_S_TX_ENABLE\n");
#endif
	
    rtn |= sessionSetState(sp, 0, PD_S_RX_ENABLE | PD_S_TX_ENABLE);
    assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);

    /* Raise RTS */
#if JLOG	
	kprintf("IOSerialBSDClient::initSession::sessionSetState::PD_RS232_S_RTS, PD_RS232_S_RTS\n");
#endif
	
    rtn = sessionSetState(sp,  PD_RS232_S_RTS, PD_RS232_S_RTS);

    assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);
}

bool IOSerialBSDClient::
waitOutDelay(void *event,
             const struct timeval *start, const struct timeval *duration)
{

    struct timeval delta;
#if JLOG	
	kprintf("IOSerialBSDClient::waitOutDelay\n");
#endif

    timeradd(start, duration, &delta); // Delay Till = start + duration

    {
	struct timeval now;

	microuptime(&now);
	timersub(&delta, &now, &delta);    // Delay Duration = Delay Till - now
    }

    if ( delta.tv_sec < 0 || !timerisset(&delta) )
        return false;	// Delay expired
    else if (event) {
        unsigned int delayTicks;

        delayTicks = MUSEC2TICK(delta.tv_sec * 1000000 + delta.tv_usec);
        tsleep((caddr_t) event, TTIPRI, "ttydelay", delayTicks);
    }
    else {
        unsigned int delayMS;

        /* Calculate the required delay in milliseconds, rounded up */
        delayMS =  delta.tv_sec * 1000 + (delta.tv_usec + 999) / 1000;

        IOSleep(delayMS);
    }
    return true;	// We did sleep
}

int IOSerialBSDClient::
waitForIdle()
{
#if JLOG	
	kprintf("IOSerialBSDClient::waitForIdle\n");
#endif

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
#if JLOG	
	kprintf("IOSerialBSDClient::preemptActive\n");
#endif
    // 
    // We are not allowed to pre-empt if the current port has been
    // active recently.  So wait out the delay and if we sleep
    // then we will need to return to check the open conditions again.
    //
    if (waitOutDelay(&fPreemptAllowed, &fLastUsedTime, &kPreemptIdle))
        return;

    Session *sp = fActiveSession;
    struct tty *tp = sp->ftty;

    sp->fErrno = EBUSY;

    // This flag gets reset once we actually take over the session
    // this is done by the open code where it acquires the port
    // obviously we don't need to re-acquire the port as we didn't
    // release it in this case.
    // 
    // setting fPreemptAllowed false here effectively locks other
    // preemption out...
    fPreemptAllowed = false;
    // set fPreemptInProgress to keep the EBUSY condition held high
    // during termination of the Preempted open
    // --- manifested in txfunc and rxfunc
    //
    // side effect of locking around the thread start/stop code 
    fPreemptInProgress = true;

    // Enforce a zombie and unconnected state on the discipline
	tty_lock(tp);
    CLR(tp->t_cflag, CLOCAL);		// Fake up a carrier drop
    (void) bsdld_modem(tp, false);
	tty_unlock(tp);
	
    // Wakeup all possible sleepers
    wakeup(TSA_CARR_ON(tp));
	tty_lock(tp);
    ttwakeup(tp);
	ttwwakeup(tp);
    tty_unlock(tp);

    killThreads();

    // Shutdown the open connection - complicated hand shaking
    if (fInOpensPending) {
	// Wait for the openers to finish up - still connectTransit
	while (fInOpensPending)
	    tsleep((caddr_t) &fInOpensPending, TTIPRI, "ttypre", 0);
	// Once the sleepers have all woken up it is safe to reset the
	// errno and continue on.
	sp->fErrno = 0;
    }
    // Preemption is over (it has occurred)
    fPreemptInProgress = false;
    fActiveSession = 0;
    SAFE_PORTRELEASE(fProvider);
}

void IOSerialBSDClient::
startConnectTransit()
{
#if JLOG	
	kprintf("IOSerialBSDClient::startConnectTransit\n");
#endif
    // Wait for the connection (open()/close()) engine to stabilise
    while (fConnectTransit)
        tsleep((caddr_t) this, TTIPRI, "ttyctr", 0);
    fConnectTransit = true;
}

void IOSerialBSDClient::
endConnectTransit()
{
#if JLOG	
	kprintf("IOSerialBSDClient::endConnectTransit\n");
#endif
    // Clear up the transit while we are waiting for carrier
    fConnectTransit = false;
    wakeup(this);
}
/*
 * convertFlowCtrl
 */
void
IOSerialBSDClient::convertFlowCtrl(Session *sp, struct termios *t)
{
    IOReturn rtn;
    UInt32 flowCtrl;
#if JLOG	
	kprintf("IOSerialBSDClient::convertFlowCtrl\n");
#endif
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

// XXX gvdl: Must only call when session is valid, check isInActive as well
/* 
 * mctl assumes lock isn't held
 */
int IOSerialBSDClient::
mctl(u_int bits, int how)
{
    u_long oldBits, mbits;
    IOReturn rtn;
#if JLOG	
	kprintf("IOSerialBSDClient::mctl\n");
#endif
    if ( ISSET(bits, PD_RS232_S_BRK) && (how == DMBIS || how == DMBIC) ) {
	oldBits = (how == DMBIS);
	rtn = fProvider->enqueueEvent(PD_RS232_E_LINE_BREAK, oldBits, true);
	if (!rtn && oldBits)
	    rtn = fProvider->enqueueEvent(PD_E_DELAY, BRK_DELAY, true);
	assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);
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

    /* Check for a transition of DTR to low and record the down time */
    if ( ISSET(oldBits & ~mbits, PD_RS232_S_DTR) )
	microuptime(&fDTRDownTime);

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
/* 
 * optimiseInput assumes lock is held 
 */
void IOSerialBSDClient::
optimiseInput(struct termios *t)
{
    Session *sp = fActiveSession;
#if JLOG	
	kprintf("IOSerialBSDClient::optimiseInput\n");
#endif
    if (!sp)	// Check for a hot unplug
	return;

    struct tty *tp = sp->ftty;
    UInt32 slipEvent, pppEvent;

    bool cantByPass =
        (ISSET(t->c_iflag, NOBYPASS_IFLAG_MASK)
          || ( ISSET(t->c_iflag, BRKINT) && !ISSET(t->c_iflag, IGNBRK) )
          || ( ISSET(t->c_iflag, PARMRK)
               && ISSET(t->c_iflag, NOBYPASS_PAR_MASK) != NOBYPASS_PAR_MASK)
          || ISSET(t->c_lflag, NOBYPASS_LFLAG_MASK)
          || linesw[tp->t_line].l_rint != ttyinput);
	
	tty_lock(tp);
    if (cantByPass) 
        CLR(tp->t_state, TS_CAN_BYPASS_L_RINT);
    else
        SET(tp->t_state, TS_CAN_BYPASS_L_RINT);
	tty_unlock(tp);

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
iossdcddelay(thread_call_param_t vSelf, thread_call_param_t vSp)
{
#if JLOG	
	kprintf("IOSerialBSDClient::iossdcddelay\n");
#endif

    IOSerialBSDClient *self = (IOSerialBSDClient *) vSelf;
    Session *sp = (Session *) vSp;
    struct tty *tp = sp->ftty;

    assert(self->fDCDTimerDue);
	
    if (!sp->fErrno && ISSET(tp->t_state, TS_ISOPEN)) {

	bool pd_state = ISSET(self->sessionGetState(sp), PD_RS232_S_CAR);
	tty_lock(tp);
	(void) bsdld_modem(tp, (int) pd_state);
	tty_unlock(tp);
    }

    self->fDCDTimerDue = false;
    self->release();
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
    struct tty *tp = sp->ftty;
    UInt32 transferCount, bufferSize, minCount;
    UInt8 rx_buf[1024];
    IOReturn rtn;

#if JLOG	
	kprintf("IOSerialBSDClient::getData\n");
#endif
    if (fKillThreads)
        return;

    bufferSize = TTY_HIGHWATER - TTY_QUEUESIZE(tp);
    bufferSize = MIN(bufferSize, (uint32_t)sizeof(rx_buf));
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
	if (rtn == kIOReturnOffline || rtn == kIOReturnNotOpen)
	    frxBlocked = true;	// Force a session condition check
	else if (rtn != kIOReturnIOError)
	    IOLog("ttyioss%04x: dequeueData ret %x\n", tp->t_dev, rtn);
        return;
    }

    if (!transferCount)
        return;

    // Track last in bound data time
    if (fPreemptAllowed)
	microuptime(&fLastUsedTime);

    /*
     * Avoid the grotesquely inefficient lineswitch routine
     * (ttyinput) in "raw" mode.  It usually takes about 450
     * instructions (that's without canonical processing or echo!).
     * slinput is reasonably fast (usually 40 instructions plus
     * call overhead).
     */
    if ( ISSET(tp->t_state, TS_CAN_BYPASS_L_RINT)
    &&  !ISSET(tp->t_lflag, PENDIN) ) {
		tty_lock(tp);
		/* Update statistics */
        tk_nin += transferCount;
        tk_rawcc += transferCount;
        tp->t_rawcc += transferCount;

        /* update the rawq and tell recieve waiters to wakeup */
		(void) b_to_q(rx_buf, transferCount, &tp->t_rawq);
        ttwakeup(tp);
		tty_unlock(tp);
    }
    else {

        for (minCount = 0; minCount < transferCount; minCount++) {
			tty_lock(tp);
            bsdld_rint(rx_buf[minCount], tp);
			tty_unlock(tp);
		}
    }
}

void IOSerialBSDClient::
procEvent(Session *sp)
{
    struct tty *tp = sp->ftty;
    UInt32 event, data;
    IOReturn rtn;
#if JLOG	
	kprintf("IOSerialBSDClient::procEvent\n");
#endif
    if (frxBlocked) {
        frxBlocked = false;
    }

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
		microuptime(&fLastUsedTime);
	tty_lock(tp);
	bsdld_rint(data, tp);
	tty_unlock(tp);
    }
}

void IOSerialBSDClient::
rxFunc()
{
    Session *sp;
    int event;
    UInt32 wakeup_with;	// states
    IOReturn rtn;
#if JLOG	
	kprintf("IOSerialBSDClient::rxFunc\n");
#endif
    sp = fActiveSession;
	struct tty *tp = sp->ftty;

    IOLockLock(fThreadLock);
    frxThread = IOThreadSelf();
    IOLockWakeup(fThreadLock, &frxThread, true);	// wakeup the thread launcher
    IOLockUnlock(fThreadLock);

    frxBlocked = false;

    while ( !fKillThreads ) {
        if (frxBlocked) {
            wakeup_with = PD_S_RX_EVENT;
            rtn = sessionWatchState(sp, &wakeup_with, PD_S_RX_EVENT);
            sessionSetState(sp, 0, PD_S_RX_EVENT);
            if ( kIOReturnOffline == rtn || kIOReturnNotOpen == rtn
	    ||   fKillThreads)
                break;	// Terminate thread loop
        }
	event = (sessionNextEvent(sp) & PD_E_MASK);
	if (event == PD_E_EOQ || event == VALID_DATA)
	    getData(sp);
	else
	    procEvent(sp);
    }

    // commit seppuku cleanly
#ifdef DEBUG
    debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif

    IOLockLock(fThreadLock);
    frxThread = NULL;
    IOLockWakeup(fThreadLock, &frxThread, true);	// wakeup the thread killer
	debug(FLOW, "fisCallout is: %d, fwantCallout is: %d, fisBlueTooth is: %d", fisCallout, fwantCallout, fisBlueTooth);
	debug(FLOW, "fPreemptAllowed is: %d", fPreemptAllowed);

    if (fDeferTerminate && !ftxThread && !fInOpensPending) {
	SAFE_PORTRELEASE(fProvider);

	bool defer = false;
	super::didTerminate(fProvider, 0, &defer);
    }
    else // we shouldn't go down this path if we've already released the port
         // (and didTerminate handles the rest of the issues anyway)
    {
        debug(FLOW, "we're killing our thread");
        // because bluetooth leaves its /dev/tty entries around
        // we need to tell the bsd side that carrier has dropped
        // when bluetooth tells us kIOReturnNotOpen (which it does correctly)
        // other tty like driver stacks would also ask us to remove the
        // /dev/tty entries which would terminate us cleanly
        // so... this check should be benign except for bluetooth
        // it should also be pointed out that it may be a limitation of the CLOCAL
        // handling in ppp that contributes to this problem
        // 
        // benign except for the preemption case - fixed...
        if (!ftxThread && !fInOpensPending && !fPreemptInProgress && fisBlueTooth)
        {
            // no transmit thread, we're about to kill the receive thread
            // tell the bsd side no more bytes (fErrno = 0)
            debug(FLOW, "no more threads, so we shouldn't be busy or have carrier");
            // Now kill any stream that may currently be running
            sp->fErrno = 0;

            // Enforce a zombie and unconnected state on the discipline
#ifdef DEBUG
            debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif
            debug(FLOW, "faking a CLOCAL drop");
            tty_lock(tp);
            CLR(tp->t_cflag, CLOCAL);  // Fake up a carrier drop
            debug(FLOW, "faked a CLOCAL drop, about to fake a carrier drop");
#ifdef DEBUG
            debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif
           
            (void) bsdld_modem(tp, false);
            tty_unlock(tp);
            debug(FLOW, "faked a carrier drop");
#ifdef DEBUG
            debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif

        }
    }
    debug(FLOW, "thread be dead");
    IOLockUnlock(fThreadLock);
    (void) thread_terminate(current_thread());
}

/*
 * The three functions below make up the status monitoring and transmition
 * part of the Port Devices Line Discipline interface.
 *
 *	txload		// TX data download to Port Device
 *	dcddelay	// DCD callout function for DCD transitions
 *	txFunc		// Thread main loop and sleeper
 *
 *  txload assumes the lock is not held when it is called...
 */

void IOSerialBSDClient::
txload(Session *sp, UInt32 *wait_mask)
{
    struct tty *tp = sp->ftty;
    IOReturn rtn;
    UInt8 tx_buf[CBSIZE * 8];	// 1/2k buffer
    UInt32 data;
    UInt32 cc, size;
#if JLOG	
	kprintf("IOSerialBSDClient::txload\n");
#endif
    if ( !tp->t_outq.c_cc )
		return;		// Nothing to do
    if ( !ISSET(tp->t_state, TS_BUSY) ) {
		tty_lock(tp);
        SET(tp->t_state, TS_BUSY);
        tty_unlock(tp);
		SET(*wait_mask, PD_S_TXQ_EMPTY); // Start tracking PD_S_TXQ_EMPTY
		CLR(*wait_mask, PD_S_TX_BUSY);
		
    }

    while ( (cc = tp->t_outq.c_cc) ) {
        rtn = sessionRequestEvent(sp, PD_E_TXQ_AVAILABLE, &data);
		if (kIOReturnOffline == rtn || kIOReturnNotOpen == rtn)
			return;

		assert(!rtn);

        size = data;
		if (size > 0)
			size = MIN(size, (uint32_t)sizeof(tx_buf));
		else {
			SET(*wait_mask, PD_S_TXQ_LOW_WATER); // Start tracking low water
			return;
		}
		tty_lock(tp);
		size = q_to_b(&tp->t_outq, tx_buf, MIN(cc, size));
		tty_unlock(tp);
		assert(size);

	/* There was some data left over from the previous load */
        rtn = sessionEnqueueData(sp, tx_buf, size, &cc, false);
        if (fPreemptAllowed)
	    microuptime(&fLastUsedTime);

		if (kIOReturnSuccess == rtn) {
			tty_lock(tp);
			bsdld_start(tp);
			tty_unlock(tp);
		}
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
    UInt32 waitfor, waitfor_mask, wakeup_with;	// states
    UInt32 interesting_bits;
    IOReturn rtn;
#if JLOG	
	kprintf("IOSerialBSDClient::txFunc\n");
#endif
    sp = fActiveSession;
    tp = sp->ftty;

    IOLockLock(fThreadLock);
    ftxThread = IOThreadSelf();
    IOLockWakeup(fThreadLock, &ftxThread, true);	// wakeup the thread launcher
    IOLockUnlock(fThreadLock);

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
		if ( rtn )
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
			assert(!rtn || rtn == kIOReturnOffline || rtn == kIOReturnNotOpen);
			txload(sp, &waitfor_mask);
		}

		//
		// Now process the carriers current state if it has changed
		//
		if ( ISSET(PD_RS232_S_CAR, interesting_bits) ) {
			waitfor ^= PD_RS232_S_CAR;		/* toggle value */

			if (fDCDTimerDue) {
				/* Stop dcd timer interval was too short */
				if (thread_call_cancel(fDCDThreadCall)) {
					debug(DCDTRD,"DCD thread canceled (interval too short)");
					release();
					fDCDTimerDue = false;
				}
			} else {
				AbsoluteTime dl;

				clock_interval_to_deadline(DCD_DELAY, kMicrosecondScale, &dl);
				thread_call_enter1_delayed(fDCDThreadCall, sp, dl);
				debug(DCDTRD,"DCD thread enter1 delayed");
				retain();
				fDCDTimerDue = true;
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
			CLR(waitfor_mask, PD_S_TX_BUSY); /* No longer interested */
			tty_lock(tp);
			CLR(tp->t_state,  TS_BUSY);

			/* Notify disc, not busy anymore */
			bsdld_start(tp);
			tty_unlock(tp);
		}
		
    }

    // Clear the DCD timeout
    if (fDCDTimerDue && thread_call_cancel(fDCDThreadCall)) {
		debug(DCDTRD,"DCD thread canceled (clear timeout)");
		release();
		fDCDTimerDue = false;
    }

    // Drop the carrier line and clear the BUSY bit
	if (!fActiveSession || isInactive()) {
		// we've been dropped via a hotplug
		// cleanup on aisle 5
		//
		// since there are 2 ways to die (sigh) if we died due to isInactive
		// notify upstream...
		if(fActiveSession) sp->fErrno = ENXIO;
		
		SAFE_PORTRELEASE(fProvider);
		
		bool defer = false;
		super::didTerminate(fProvider, 0, &defer);		
	}
	else // we're still gonna die, just cleanly
	{
	tty_lock(tp);
    (void) bsdld_modem(tp, false);
	tty_unlock(tp);

    IOLockLock(fThreadLock);
    ftxThread = NULL;
    IOLockWakeup(fThreadLock, &ftxThread, true);	// wakeup the thread killer
	debug(FLOW, "fisCallout is: %d, fwantCallout is: %d, fisBlueTooth is: %d", fisCallout, fwantCallout, fisBlueTooth);
	debug(FLOW, "fPreemptAllowed is: %d", fPreemptAllowed);

    if (fDeferTerminate && !frxThread && !fInOpensPending) {
		SAFE_PORTRELEASE(fProvider);

		bool defer = false;
		super::didTerminate(fProvider, 0, &defer);
    }
    else // we shouldn't go down this path if we've already released the port 
         // (and didTerminate handles the rest of the issues anyway)
    {
        debug(FLOW, "we're killing our thread");
        // because bluetooth leaves its /dev/tty entries around
        // we need to tell the bsd side that carrier has dropped
        // when bluetooth tells us kIOReturnNotOpen (which it does correctly)
        // other tty like driver stacks would also ask us to remove the
        // /dev/tty entries which would terminate us cleanly
        // so... this check should be benign except for bluetooth
        //
        // it should also be pointed out that it may be a limitation of the CLOCAL
        // handling in ppp that contributes to this problem
        //
        // benign except in the preemption case - fixed...
        if (!frxThread && !fInOpensPending && !fPreemptInProgress && fisBlueTooth)
        {
            // no receive thread, we're about to kill the transmit thread
            // tell the bsd side no more bytes (fErrno = 0)
            debug(FLOW, "no more threads, so we shouldn't be busy or have carrier");
            // Now kill any stream that may currently be running
            sp->fErrno = 0;

            // Enforce a zombie and unconnected state on the discipline
#ifdef DEBUG
            debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif
            debug(FLOW, "faking a CLOCAL drop");
            tty_lock(tp);
            CLR(tp->t_cflag, CLOCAL);  // Fake up a carrier drop
            debug(FLOW, "faked a CLOCAL drop, about to fake a carrier drop");
#ifdef DEBUG
            debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif
           
            (void) bsdld_modem(tp, false);
            tty_unlock(tp);
            debug(FLOW, "faked a carrier drop");
#ifdef DEBUG
            debug(CONTROL, "%s\n%s\n%s", state2StringPD(sessionGetState(sp)), state2StringTTY(tp->t_state), state2StringTermios((int)tp->t_cflag));
#endif
        }
    }
		IOLockUnlock(fThreadLock);
	}

    debug(FLOW, "thread be dead");
    (void) thread_terminate(current_thread());
}

void IOSerialBSDClient::
launchThreads()
{
    // Clear the have launched flags
#if JLOG	
	kprintf("IOSerialBSDClient::launchThreads\n");
#endif

    IOLockLock(fThreadLock);

    ftxThread = frxThread = 0;

    // Now launch the receive and transmitter threads
#if JLOG	
	kprintf("IOSerialBSDClient::createThread::rxFunc\n");
#endif
	
    createThread(
	OSMemberFunctionCast(IOThreadFunc, this, &IOSerialBSDClient::rxFunc),
	this);
#if JLOG	
	kprintf("IOSerialBSDClient::createThread::txFunc\n");
#endif
    createThread(
	OSMemberFunctionCast(IOThreadFunc, this, &IOSerialBSDClient::txFunc),
	this);

    // Now wait for the threads to actually launch
    while (!frxThread)
	IOLockSleep(fThreadLock, &frxThread, THREAD_UNINT);
    while (!ftxThread)
	IOLockSleep(fThreadLock, &ftxThread, THREAD_UNINT);

    IOLockUnlock(fThreadLock);
}

void IOSerialBSDClient::
killThreads()
{
#if JLOG	
	kprintf("IOSerialBSDClient::killThreads\n");
#endif
    if (frxThread || ftxThread || fInOpensPending) {
        fKillThreads = true;
	fProvider->executeEvent(PD_E_ACTIVE, false);

	IOLockLock(fThreadLock);
        while (frxThread)
	    IOLockSleep(fThreadLock, &frxThread, THREAD_UNINT);
        while (ftxThread)
	    IOLockSleep(fThreadLock, &ftxThread, THREAD_UNINT);
	IOLockUnlock(fThreadLock);
    }
#ifdef TARGET_OS_EMBEDDED
	// bluetooth, modem and fax team need to validate change
	// to remove this ifdef
	if (fDCDThreadCall) {
		debug(DCDTRD,"DCD Thread Freed in killThreads");
		thread_call_cancel(fDCDThreadCall);
		thread_call_free(fDCDThreadCall);
		fDCDTimerDue = false;
		fDCDThreadCall = 0;
	}	
#endif	
}

void IOSerialBSDClient::
cleanupResources()
{
#if JLOG	
	kprintf("IOSerialBSDClient::cleanupResources\n");
#endif
    // Remove our device name from the devfs
    if ((dev_t) -1 != fBaseDev) {
	sBSDGlobals.takefFunnelLock();
	sBSDGlobals.releaseUniqueTTYSuffix(
		(const OSSymbol *) getProperty(gIOTTYBaseNameKey),
		(const OSSymbol *) getProperty(gIOTTYSuffixKey));
	sBSDGlobals.releasefFunnelLock();
    }

    if (fSessions[TTY_CALLOUT_INDEX].fCDevNode)
	devfs_remove(fSessions[TTY_CALLOUT_INDEX].fCDevNode);
    if (fSessions[TTY_DIALIN_INDEX].fCDevNode)
	devfs_remove(fSessions[TTY_DIALIN_INDEX].fCDevNode);
}

// 
// session based accessors to Serial Stream Sync 
//
IOReturn IOSerialBSDClient::
sessionSetState(Session *sp, UInt32 state, UInt32 mask)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionSetState\n");
#endif
    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->setState(state, mask);
}

UInt32 IOSerialBSDClient::
sessionGetState(Session *sp)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionGetState\n");
#endif

    if (sp->fErrno)
        return 0;
    else
        return fProvider->getState();
}

IOReturn IOSerialBSDClient::
sessionWatchState(Session *sp, UInt32 *state, UInt32 mask)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionWatchState\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->watchState(state, mask);
}

UInt32 IOSerialBSDClient::
sessionNextEvent(Session *sp)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionNextEvent\n");
#endif

    if (sp->fErrno)
        return PD_E_EOQ;
    else
        return fProvider->nextEvent();
}

IOReturn IOSerialBSDClient::
sessionExecuteEvent(Session *sp, UInt32 event, UInt32 data)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionExecuteEvent\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->executeEvent(event, data);
}

IOReturn IOSerialBSDClient::
sessionRequestEvent(Session *sp, UInt32 event, UInt32 *data)
{	
#if JLOG	
	kprintf("IOSerialBSDClient::sessionRequestEvent\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->requestEvent(event, data);
}

IOReturn IOSerialBSDClient::
sessionEnqueueEvent(Session *sp, UInt32 event, UInt32 data, bool sleep)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionEnqueueEvent\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->enqueueEvent(event, data, sleep);
}

IOReturn IOSerialBSDClient::
sessionDequeueEvent(Session *sp, UInt32 *event, UInt32 *data, bool sleep)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionDequeueEvent\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->dequeueEvent(event, data, sleep);
}

IOReturn IOSerialBSDClient::
sessionEnqueueData(Session *sp, UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionEnqueueData\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->enqueueData(buffer, size, count, sleep);
}

IOReturn IOSerialBSDClient::
sessionDequeueData(Session *sp, UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
#if JLOG	
	kprintf("IOSerialBSDClient::sessionDequeueData\n");
#endif

    if (sp->fErrno)
        return kIOReturnOffline;
    else
        return fProvider->dequeueData(buffer, size, count, min);
}

IOThread IOSerialBSDClient::
createThread(IOThreadFunc fcn, void *arg)
{
	kern_return_t        result;
	thread_t                thread;
#if JLOG	
	kprintf("IOSerialBSDClient::createThread\n");
#endif

	result = kernel_thread_start((thread_continue_t)fcn, arg, &thread);
	if (result != KERN_SUCCESS)
		return (NULL);
	thread_deallocate(thread);
	return (thread);
}

#ifdef DEBUG
char * IOSerialBSDClient::state2StringTermios(UInt32 state)
    {
    static char stateDescription[1024];
    
    // Nulls the string:
    stateDescription[0] = 0;
    
    // reads the state and appends values:
    if (state & CIGNORE)
        strncat(stateDescription, "CIGNORE ", sizeof(stateDescription) - 9);


    if (state & CSIZE)
    strncat(stateDescription, "CSIZE ", sizeof(stateDescription) - 7);
    
    if (state & CSTOPB)
    strncat(stateDescription, "CSTOPB ", sizeof(stateDescription) - 8);
    
    if (state & CREAD)
    strncat(stateDescription, "CREAD ", sizeof(stateDescription) - 7);
    
    if (state & PARENB)
    strncat(stateDescription, "PARENB ", sizeof(stateDescription) - 8);
    
    if (state & PARODD)
    strncat(stateDescription, "PARODD ", sizeof(stateDescription) - 8);
    
    if (state & HUPCL)
    strncat(stateDescription, "HUPCL ", sizeof(stateDescription) - 7);
    
    if (state & CLOCAL)
    strncat(stateDescription, "CLOCAL ", sizeof(stateDescription) - 8);    
    
    if (state & CCTS_OFLOW)
    strncat(stateDescription, "CCTS_OFLOW ", sizeof(stateDescription) - 12);       
    
    if (state & CRTSCTS)
    strncat(stateDescription, "CRTSCTS ", sizeof(stateDescription) - 9);  
    
    if (state & CRTS_IFLOW)
    strncat(stateDescription, "CRTS_IFLOW ", sizeof(stateDescription) - 12);
    
    if (state & CDTR_IFLOW)
    strncat(stateDescription, "CDTR_IFLOW ", sizeof(stateDescription) - 12);        
    
    if (state & CDSR_OFLOW)
    strncat(stateDescription, "CDSR_OFLOW ", sizeof(stateDescription) - 12);
    
    if (state & CCAR_OFLOW)
    strncat(stateDescription, "CCAR_OFLOW ", sizeof(stateDescription) - 12);
    
    if (state & MDMBUF)
    strncat(stateDescription, "MDMBUF ", sizeof(stateDescription) - 8);

    return (stateDescription);
    }

char * IOSerialBSDClient::state2StringPD(UInt32 state)
    {
    static char stateDescription[1024];
    
    // Nulls the string:
    stateDescription[0] = 0;
    
    // reads the state and appends values:
    // reads the state and appends values:
    if (state & PD_RS232_S_CAR)
        strncat(stateDescription, "PD_RS232_S_CAR ", sizeof(stateDescription) - 16);
    else
        strncat(stateDescription, "^PD_RS232_S_CAR ", sizeof(stateDescription) - 17);

    if (state & PD_S_ACQUIRED)
    strncat(stateDescription, "PD_S_ACQUIRED ", sizeof(stateDescription) - 16);
    
    if (state & PD_S_ACTIVE)
    strncat(stateDescription, "PD_S_ACTIVE ", sizeof(stateDescription) - 13);
    
    if (state & PD_S_TX_ENABLE)
    strncat(stateDescription, "PD_S_TX_ENABLE ", sizeof(stateDescription) - 16);
    
    if (state & PD_S_TX_BUSY)
    strncat(stateDescription, "PD_S_TX_BUSY ", sizeof(stateDescription) - 14);
    
    if (state & PD_S_TX_EVENT)
    strncat(stateDescription, "PD_S_TX_EVENT ", sizeof(stateDescription) - 15);
    
    if (state & PD_S_TXQ_EMPTY)
    strncat(stateDescription, "PD_S_TXQ_EMPTY ", sizeof(stateDescription) - 16);
    
    if (state & PD_S_TXQ_LOW_WATER)
    strncat(stateDescription, "PD_S_TXQ_LOW_WATER ", sizeof(stateDescription) - 20);    
    
    if (state & PD_S_TXQ_HIGH_WATER)
    strncat(stateDescription, "PD_S_TXQ_HIGH_WATER ", sizeof(stateDescription) - 21);       
    
    if (state & PD_S_TXQ_FULL)
    strncat(stateDescription, "PD_S_TXQ_FULL ", sizeof(stateDescription) - 15);  
        
    if (state & PD_S_TXQ_MASK)
    strncat(stateDescription, "(PD_S_TXQ_MASK) ", sizeof(stateDescription) - 17);        
    
    if (state & PD_S_RX_ENABLE)
    strncat(stateDescription, "PD_S_RX_ENABLE ", sizeof(stateDescription) - 16);
    
    if (state & PD_S_RX_BUSY)
    strncat(stateDescription, "PD_S_RX_BUSY ", sizeof(stateDescription) - 14);
    
    if (state & PD_S_RX_EVENT)
    strncat(stateDescription, "PD_S_RX_EVENT ", sizeof(stateDescription) - 15);
    
    if (state & PD_S_RXQ_EMPTY)
    strncat(stateDescription, "PD_S_RXQ_EMPTY ", sizeof(stateDescription) - 16);
    
    if (state & PD_S_RXQ_LOW_WATER)
    strncat(stateDescription, "PD_S_RXQ_LOW_WATER ", sizeof(stateDescription) - 20);    
    
    if (state & PD_S_RXQ_HIGH_WATER)
    strncat(stateDescription, "PD_S_RXQ_HIGH_WATER ", sizeof(stateDescription) - 21);       
    
    if (state & PD_S_RXQ_FULL)
    strncat(stateDescription, "PD_S_RXQ_FULL ", sizeof(stateDescription) - 15);  
    
    if (state & PD_S_RXQ_MASK)
    strncat(stateDescription, "(PD_S_RXQ_MASK) ", sizeof(stateDescription) - 17);

    return (stateDescription);
    }

char * IOSerialBSDClient::state2StringTTY(UInt32 state)
    {
    static char stateDescription[2048];
    
    // Nulls the string:
    stateDescription[0] = 0;
    
    // reads the state and appends values:
    if (state & TS_SO_OLOWAT)
    strncat(stateDescription, "TS_SO_OLOWAT ", sizeof(stateDescription) - 14);

    if (state & TS_ASYNC)
    strncat(stateDescription, "TS_ASYNC ", sizeof(stateDescription) - 10);
    
    if (state & TS_BUSY)
    strncat(stateDescription, "TS_BUSY ", sizeof(stateDescription) - 9);
    
    if (state & TS_CARR_ON)
    strncat(stateDescription, "TS_CARR_ON ", sizeof(stateDescription) - 12);
    
    if (state & TS_FLUSH)
    strncat(stateDescription, "TS_FLUSH ", sizeof(stateDescription) - 10);
    
    if (state & TS_ISOPEN)
    strncat(stateDescription, "TS_ISOPEN ", sizeof(stateDescription) - 11);
    
    if (state & TS_TBLOCK)
    strncat(stateDescription, "TS_TBLOCK ", sizeof(stateDescription) - 11);
    
    if (state & TS_TIMEOUT)
    strncat(stateDescription, "TS_TIMEOUT ", sizeof(stateDescription) - 12);    
    
    if (state & TS_TTSTOP)
    strncat(stateDescription, "TS_TTSTOP ", sizeof(stateDescription) - 11);       
    
    // needs to pull in the notyet definition from tty.h
    //if (state & TS_WOPEN)
    //strncat(stateDescription, "TS_WOPEN ", sizeof(stateDescription) - 10);  
    
    if (state & TS_XCLUDE)
    strncat(stateDescription, "TS_XCLUDE ", sizeof(stateDescription) - 11);
    
    if (state & TS_LOCAL)
    strncat(stateDescription, "TS_LOCAL ", sizeof(stateDescription) - 10);        
    
    if (state & TS_ZOMBIE)
    strncat(stateDescription, "TS_ZOMBIE ", sizeof(stateDescription) - 11);
    
    if (state & TS_CONNECTED)
    strncat(stateDescription, "TS_CONNECTED ", sizeof(stateDescription) - 14);
    
    if (state & TS_CAN_BYPASS_L_RINT)
    strncat(stateDescription, "TS_CAN_BYPASS_L_RINT ", sizeof(stateDescription) - 22);

    if (state & TS_SNOOP)
    strncat(stateDescription, "TS_SNOOP ", sizeof(stateDescription) - 10);

    if (state & TS_SO_OCOMPLETE)
    strncat(stateDescription, "TS_SO_OCOMPLETE ", sizeof(stateDescription) - 17);

    if (state & TS_CAR_OFLOW)
    strncat(stateDescription, "TS_CAR_OFLOW ", sizeof(stateDescription) - 14);

    // needs to pull in the notyet definition from tty.h
    //if (state & TS_CTS_OFLOW)
    //strncat(stateDescription, "TS_CTS_OFLOW ", sizeof(stateDescription) - 14);

    // needs to pull in the notyet definition from tty.h
    //if (state & TS_DSR_OFLOW)
    //strncat(stateDescription, "TS_DSR_OFLOW ", sizeof(stateDescription) - 14);

    return (stateDescription);
    }
#endif
