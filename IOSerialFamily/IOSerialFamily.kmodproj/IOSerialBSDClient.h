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
 * IOSerialBSDClient.h
 *
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */


#ifndef _IOSERIALSERVER_H
#define _IOSERIALSERVER_H

#include <IOKit/IOLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOTypes.h>

#include <IOKit/IOService.h>

class IOSerialStreamSync;
class IOSerialSessionSync;
class IOSerialBSDClient : public IOService
{
    OSDeclareDefaultStructors(IOSerialBSDClient)

public:
    //
    // IOService overrides
    //
    virtual void free();

    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual bool matchPropertyTable(OSDictionary *table);

    // BSD TTY linediscipline stuff
    static struct cdevsw devsw;

protected:
    struct ttyMap {
        /*
        * Unix tty structure -- the glue between line disciplines and this
        * code.  Put at begining of structure so that sp and tp point to
        * same bit of memory.
        */
        struct tty ftty;
        IOSerialBSDClient *fClient;
    } map;

    IOSerialStreamSync *fProvider;
    dev_t fBaseDev;

    /*
     * Underlying device object state
     */
    IOSerialSessionSync *fSession; /* The session for the open connection */

    /*
     * Init and lock termios structures for both call in and call out
     * devices
     */
    struct termios fInitTermOut, fInitTermIn;

    struct timeval fDTRDownTime;

    int fInOpensPending;	/* Count of incoming opens waiting */
    int fDCDDelayTicks;

    /* state determined at time of open */
    boolean_t   fPreempt:1;	/* fPreempted audit lock holder */
    boolean_t   fIsReleasing:1;
    boolean_t   frxBlocked:1;	/* the rx_thread suspended */
    boolean_t   fHasAuditSleeper:1; /* A process is sleeping on audit */
    boolean_t   fKillThreads:1;	/* Threads must terminate */
    boolean_t   fIsTimersSet:1; /* Has frame timeout been set */
    boolean_t   fIstxEnabled:1; /* en/disabled due to flow control */
    boolean_t   fIsrxEnabled:1; /* TP_CREAD dependent */
    boolean_t   fIsDCDTimer:1;	/* DCD debounce flag */
    boolean_t   fIsDTRDelay:1;	/* Set during dtr down delay */

    void *fCdevCalloutNode;	// (character device's devfs node)
    void *fCdevDialinNode;      // (character device's devfs node)
    IOThread frxThread;		// Recieve data and event's thread
    IOThread ftxThread;		// Transmit data and state tracking thread

    /*
     * TTY glue layer private routines
     */
    virtual int open(dev_t dev, int flags, int devtype, struct proc *p);
    virtual void close(dev_t dev, int flags, int devtype, struct proc *p);

    virtual bool createDevNodes();
    virtual bool setBaseTypeForDev();
    virtual void initState();
    virtual int  acquireSession(dev_t dev);
    virtual int  waitForDCD(int flag);
    virtual void initSession();
    virtual void optimiseInput(struct termios *t);
    virtual void convertFlowCtrl(struct termios *t);

    /*
     * Modem control routines
     */
    virtual int  mctl(u_int bits, int how);

    /*
     * Receive and Transmit engine thread functions
     */
    virtual void getData();
    virtual void procEvent();
    virtual void txload(u_long *wait_mask);
    virtual void rxFunc();
    virtual void txFunc();

    virtual void launchThreads();
    virtual void killThreads();

private:
    // Unix character device switch table routines.
    static int  iossopen(dev_t dev, int flags, int devtype, struct proc *p);
    static int  iossclose(dev_t dev, int flags, int devtype, struct proc *p);
    static int  iossread(dev_t dev, struct uio *uio, int ioflag);
    static int  iosswrite(dev_t dev, struct uio *uio, int ioflag);
    static int  iossselect(dev_t dev, int which, void *wql, struct proc *p);
    static int  iossioctl(dev_t dev, u_long cmd, caddr_t data, int fflag,
                            struct proc *p);
    static int  iossstop(struct tty *tp, int rw);
    static void iossstart(struct tty *tp);	// assign to tp->t_oproc
    static int  iossparam(struct tty *tp, struct termios *t);
    static void iossdcddelay(void *vThis);
};

#endif /* ! _IOSERIALSERVER_H */
