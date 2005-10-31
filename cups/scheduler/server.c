/*
 * "$Id: server.c,v 1.13.2.2 2005/07/27 21:58:45 jlovell Exp $"
 *
 *   Server start/stop routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   StartServer() - Start the server.
 *   StopServer()  - Stop the server.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include "cupsd.h"

#include <grp.h>

#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <pthread.h>

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>			/* for PSQUpdateQuota */
#endif /* HAVE_DLFCN_H */

/*
 * Constants...
 */

#define SYSEVENT_CANSLEEP	0x1	/* Decide whether to allow sleep or not */
#define SYSEVENT_WILLSLEEP	0x2	/* Computer will go to sleep */
#define SYSEVENT_WOKE		0x4	/* Computer woke from sleep */
#define SYSEVENT_NETCHANGED	0x8	/* Network changed */
#define SYSEVENT_NAMECHANGED	0x10	/* Computer name changed */


/* 
 * Structures... 
 */

typedef struct cups_sysevent_str	/*** System event data ****/
{
  unsigned char	event;			/* Event bit field */

  io_connect_t	powerKernelPort;	/* Power context data */
  long		powerNotificationID;	/* Power event data */
} cups_sysevent_t;


typedef struct cups_thread_data_str	/*** Thread context data  ****/
{
  cups_sysevent_t	sysevent;	/* Sys event */
  CFRunLoopTimerRef	timerRef;	/* Timer to delay some change notifications */
} cups_thread_data_t;


/* 
 * Globals... 
 */

static pthread_t	SysEventThread = NULL;		/* Thread to host a runloop */
static pthread_mutex_t	SysEventThreadMutex = { 0 };	/* Coordinates access to shared gloabals */ 
static pthread_cond_t	SysEventThreadCond = { 0 };	/* Thread initialization complete condition */
static CFRunLoopRef	SysEventRunloop = NULL;		/* The runloop. Access must be protected! */
static CFStringRef	ComputerNameKey = NULL,		/* Computer name key */
			NetworkGlobalKey = NULL,	/* Network global key */
			HostNamesKey = NULL,		/* Host name key */
			NetworkInterfaceKey = NULL;	/* Netowrk interface key */

#ifdef HAVE_DLFCN_H
static const char PSQLibPath[]	    = "/usr/lib/libPrintServiceQuota.dylib";
static const char PSQLibFuncName[] = "PSQUpdateQuota";
static void *PSQLibRef = NULL;		/* cached reference to libPrintServiceQuota.dylib */

void *PSQUpdateQuotaProc = NULL;	/* PSQUpdateQuota function pointer (exported) */
#endif /* HAVE_DLFCN_H */


/* 
 * Prototypes... 
 */

static void *sysEventThreadEntry();
static void sysEventPowerNotifier(void *context, io_service_t service, natural_t messageType, void *messageArgument);
static void sysEventConfigurationNotifier(SCDynamicStoreRef store, CFArrayRef changedKeys, void *context);
static void sysEventTimerNotifier(CFRunLoopTimerRef timer, void *context);
#endif	/* __APPLE__ */


/*
 * 'StartServer()' - Start the server.
 */

void
StartServer(void)
{
#ifdef HAVE_LIBSSL
  int		i;		/* Looping var */
  struct timeval curtime;	/* Current time in microseconds */
  unsigned char	data[1024];	/* Seed data */
#endif /* HAVE_LIBSSL */


#ifdef HAVE_LIBSSL
 /*
  * Initialize the encryption libraries...
  */

  SSL_library_init();
  SSL_load_error_strings();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

  gettimeofday(&curtime, NULL);
  srand(curtime.tv_sec + curtime.tv_usec);

  for (i = 0; i < sizeof(data); i ++)
    data[i] = rand(); /* Yes, this is a poor source of random data... */

  RAND_seed(&data, sizeof(data));
#elif defined(HAVE_GNUTLS)
 /*
  * Initialize the encryption libraries...
  */

  gnutls_global_init();
#endif /* HAVE_LIBSSL */

#ifdef __APPLE__
#ifdef HAVE_DLFCN_H
 /*
  * Load Print Service quota enforcement library (X Server only)
  */

  if (AppleQuotas)
  {
    if (!PSQLibRef)
      PSQLibRef = dlopen(PSQLibPath, RTLD_LAZY);

    if (PSQLibRef)
      PSQUpdateQuotaProc = dlsym(PSQLibRef, PSQLibFuncName);
  }
#endif /* HAVE_DLFCN_H */
#endif /* __APPLE__ */

 /*
  * Startup all the networking stuff...
  */

  StartListening();
  StartBrowsing();
  StartPolling();

 /*
  * Create a pipe for CGI processes...
  */

  if (cupsdOpenPipe(CGIPipes))
    LogMessage(L_ERROR, "StartServer: Unable to create pipes for CGI status!");
  else
  {
    LogMessage(L_DEBUG2, "StartServer: Adding fd %d to InputSet...", CGIPipes[0]);
    FD_SET(CGIPipes[0], InputSet);
  }

#ifdef __APPLE__
  StartSysEventMonitor();
#endif	/* __APPLE__ */
}


/*
 * 'StopServer()' - Stop the server.
 */

void
StopServer(void)
{
 /*
  * Close all network clients and stop all jobs...
  */

  CloseAllClients();
  StopListening();
  StopPolling();
  StopBrowsing();

#ifdef __APPLE__
  StopSysEventMonitor();

 /* 
  * Unload Print Service quota enforcement library (X Server only) 
  */

#ifdef HAVE_DLFCN_H
  PSQUpdateQuotaProc = NULL;
  if (PSQLibRef)
  {
    dlclose(PSQLibRef);
    PSQLibRef = NULL;
  }
#endif /* HAVE_DLFCN_H */
#endif	/* __APPLE__ */

#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
 /*
  * Free all of the certificates...
  */

  if (ServerCertificatesArray)
  {
    CFRelease(ServerCertificatesArray);
    ServerCertificatesArray = NULL;
  }
#endif /* HAVE_SSL && HAVE_CDSASSL */

 /*
  * Close the pipe for CGI processes...
  */

  if (CGIPipes[0] >= 0)
  {
    LogMessage(L_DEBUG2, "StopServer: Removing fd %d from InputSet...",
               CGIPipes[0]);

    FD_CLR(CGIPipes[0], InputFds);
    FD_CLR(CGIPipes[0], InputSet);

    cupsdClosePipe(CGIPipes);
  }

 /*
  * Close all log files...
  */

  if (AccessFile != NULL)
  {
    cupsFileClose(AccessFile);

    AccessFile = NULL;
  }

  if (ErrorFile != NULL)
  {
    cupsFileClose(ErrorFile);

    ErrorFile = NULL;
  }

  if (PageFile != NULL)
  {
    cupsFileClose(PageFile);

    PageFile = NULL;
  }
}


#ifdef __APPLE__

/*
 * 'StartSysEventMonitor()' - Start system event notifications
 */

void StartSysEventMonitor(void)
{
  int flags;

  if (pipe(SysEventPipes))
  {
    LogMessage(L_EMERG, "System event monitor pipe() failed - %s!", strerror(errno));
    return;
  }

  LogMessage(L_DEBUG2, "StartServer: Adding fd %d to InputSet...", SysEventPipes[0]);
  FD_SET(SysEventPipes[0], InputSet);

 /*
  * Set non-blocking mode on the descriptor we will be receiving notification events on.
  */

  flags = fcntl(SysEventPipes[0], F_GETFL, 0);
  fcntl(SysEventPipes[0], F_SETFL, flags | O_NONBLOCK);

 /*
  * Start the thread that runs the runloop...
  */

  pthread_mutex_init(&SysEventThreadMutex, NULL);
  pthread_cond_init(&SysEventThreadCond, NULL);
  pthread_create(&SysEventThread, NULL, sysEventThreadEntry, NULL);
}


/*
 * 'StopSysEventMonitor()' - Stop system event notifications
 */

void StopSysEventMonitor(void)
{
  CFRunLoopRef	rl;		/* The runloop */


  if (SysEventThread)
  {
   /*
    * Make sure the thread has completed it's initialization and
    * stored it's runloop reference in the shared global.
    */
		
    pthread_mutex_lock(&SysEventThreadMutex);
		
    if (SysEventRunloop == NULL)
      pthread_cond_wait(&SysEventThreadCond, &SysEventThreadMutex);
		
    rl = SysEventRunloop;
    SysEventRunloop = NULL;

    pthread_mutex_unlock(&SysEventThreadMutex);
		
    if (rl)
      CFRunLoopStop(rl);
		
    pthread_join(SysEventThread, NULL);
    pthread_mutex_destroy(&SysEventThreadMutex);
    pthread_cond_destroy(&SysEventThreadCond);
  }


  if (SysEventPipes[0] >= 0)
  {
    close(SysEventPipes[0]);
    close(SysEventPipes[1]);

    LogMessage(L_DEBUG2, "StopServer: Removing fd %d from InputSet...",
		SysEventPipes[0]);

    FD_CLR(SysEventPipes[0], InputFds);
    FD_CLR(SysEventPipes[0], InputSet);

    SysEventPipes[0] = -1;
    SysEventPipes[1] = -1;
  }
}


/*
 * 'UpdateSysEventMonitor()' - Handle power & network system events.
 */

void UpdateSysEventMonitor(void)
{
  cups_sysevent_t	sysevent;	/* The system event */
  printer_t		*p,		/* Printer information */
			*next;		/* Pointer to next printer in list */
  int			num_printers;	/* Number of printers */

 /*
  * Drain the event pipe...
  */

  while (read((int)SysEventPipes[0], &sysevent, sizeof(sysevent)) == sizeof(sysevent))
  {
    if ((sysevent.event & SYSEVENT_CANSLEEP))
    {
     /*
      * If there are any active printers cancel the sleep request...
      */

      for (p = Printers; p != NULL; p = p->next)
        if (p->job)
          break;

      if (p)
      {
        LogMessage(L_INFO, "System sleep canceled because printer %s is active", p->name);
        IOCancelPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
      }
      else
      {
	LogMessage(L_DEBUG, "System wants to sleep");
        IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
      }
    }


    if ((sysevent.event & SYSEVENT_WILLSLEEP))
    {
      LogMessage(L_INFO, "System going to sleep");
      Sleeping = 1;
      StopAllJobs();

      for (p = Printers; p != NULL; p = next)
      {
	next = p->next;

	if (p->type & CUPS_PRINTER_REMOTE)
	{
	 /*
	  * Deleting a printer that is part of an implicit class can also cause 
	  * a following class to be deleted (which our 'next' may be pointing at). 
	  * In this case reset 'next' to the head of the list...
	  */
    
	  num_printers = NumPrinters;

	  LogMessage(L_INFO, "Deleting remote destination \"%s\"", p->name);
	  DeletePrinter(p, 0);

	  if (NumPrinters != num_printers - 1)
	    next = Printers;
	}
	else
	{
	  LogMessage(L_DEBUG, "Deregistering local printer \"%s\"", p->name);
	  BrowseDeregisterPrinter(p, 0);
	}
      }
      IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
    }


    if ((sysevent.event & SYSEVENT_WOKE))
    {
      LogMessage(L_INFO, "System woke from sleep");
      IOAllowPowerChange(sysevent.powerKernelPort, sysevent.powerNotificationID);
      Sleeping = 0;
      CheckJobs();
    }

    if ((sysevent.event & SYSEVENT_NETCHANGED))
    {
      if (!Sleeping)
      {
        LogMessage(L_DEBUG, "System network configuration changed");

       /*
        *  Force update the list of network interfaces.
        */

        NetIFUpdate(TRUE);

       /*
        * Resetting browse_time before calling SendBrowseList causes remote 
        * printers to be deleted and browse packets sent for local shared printers.
        */

        for (p = Printers; p != NULL; p = p->next)
	  p->browse_time = 0;

        SendBrowseList();
      }
      else
        LogMessage(L_DEBUG, "System network configuration changed; ignored while sleeping");
    }

    if ((sysevent.event & SYSEVENT_NAMECHANGED))
    {
      if (!Sleeping)
      {
        LogMessage(L_DEBUG, "Computer name changed");

       /*
	* De-register the individual printers
	*/

	for (p = Printers; p != NULL; p = p->next)
	  BrowseDeregisterPrinter(p, 0);

       /*
	* Now re-register them
	*/

	for (p = Printers; p != NULL; p = p->next)
	  BrowseRegisterPrinter(p);
      }
      else
        LogMessage(L_DEBUG, "Computer name changed; ignored while sleeping");
    }
  }
}


/*
 * 'sysEventThreadEntry()' - A thread to run a runloop on. 
 *		       Receives power & computer name change notifications.
 */

static void *sysEventThreadEntry()
{
  io_object_t		powerNotifierObj;	/* Power notifier object */
  IONotificationPortRef powerNotifierPort;	/* Power notifier port */
  SCDynamicStoreRef	store    = NULL;	/* System Config dynamic store */
  CFRunLoopSourceRef	powerRLS = NULL,	/* Power runloop source */
			storeRLS = NULL;	/* System Config runloop source */
  CFStringRef		key[3],			/* System Config keys */
			pattern[1];		/* System Config patterns */
  CFArrayRef		keys = NULL,		/* System Config key array*/
			patterns = NULL;	/* System Config pattern array */
  SCDynamicStoreContext	storeContext;		/* Dynamic store context */
  CFRunLoopTimerContext timerContext;		/* Timer context */
  cups_thread_data_t	threadData;		/* Thread context data for the runloop notifiers */


  bzero(&threadData, sizeof(threadData));

 /*
  * Register for power state change notifications
  */

  threadData.sysevent.powerKernelPort = IORegisterForSystemPower(&threadData, &powerNotifierPort, sysEventPowerNotifier, &powerNotifierObj);
  if (threadData.sysevent.powerKernelPort)
  {
    powerRLS = IONotificationPortGetRunLoopSource(powerNotifierPort);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), powerRLS, kCFRunLoopDefaultMode);
  }
  else
    DEBUG_puts("runloopThread: error registering for system power notifications");


 /*
  * Register for system configuration change notifications
  */

  bzero(&storeContext,  sizeof(storeContext));
  storeContext.info = &threadData;

  store      = SCDynamicStoreCreate(NULL, CFSTR("cupsd"), sysEventConfigurationNotifier, &storeContext);

  if (!ComputerNameKey)	     ComputerNameKey	= SCDynamicStoreKeyCreateComputerName(NULL);
  if (!NetworkGlobalKey)     NetworkGlobalKey	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
  if (!HostNamesKey)	     HostNamesKey	= SCDynamicStoreKeyCreateHostNames(NULL);
  if (!NetworkInterfaceKey)  NetworkInterfaceKey= SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);

  if (store && ComputerNameKey && NetworkGlobalKey && HostNamesKey && NetworkInterfaceKey)
  {
    key[0]     = ComputerNameKey;
    key[1]     = NetworkGlobalKey;
    key[2]     = HostNamesKey;
    pattern[0] = NetworkInterfaceKey;

    keys     = CFArrayCreate(NULL, (const void **)key,     sizeof(key)/sizeof(key[0]),         &kCFTypeArrayCallBacks);
    patterns = CFArrayCreate(NULL, (const void **)pattern, sizeof(pattern)/sizeof(pattern[0]), &kCFTypeArrayCallBacks);

    if (keys && patterns && SCDynamicStoreSetNotificationKeys(store, keys, patterns))
    {
      if ((storeRLS = SCDynamicStoreCreateRunLoopSource(NULL, store, 0)) != NULL)
	CFRunLoopAddSource(CFRunLoopGetCurrent(), storeRLS, kCFRunLoopDefaultMode);
      else
	DEBUG_printf(("runloopThread: SCDynamicStoreCreateRunLoopSource failed: %s\n", SCErrorString(SCError())));
    }
    else
      DEBUG_printf(("runloopThread: SCDynamicStoreSetNotificationKeys failed: %s\n", SCErrorString(SCError())));
  }
  else
    DEBUG_printf(("runloopThread: SCDynamicStoreCreate failed: %s\n", SCErrorString(SCError())));

  if (keys)       CFRelease(keys);
  if (patterns)   CFRelease(patterns);


 /*
  * Set up a timer to delay the wake change notifications.
  * The initial time is set a decade or so into the future, we'll adjust this later.
  */

  bzero(&timerContext,  sizeof(timerContext));
  timerContext.info = &threadData;

  threadData.timerRef = CFRunLoopTimerCreate(NULL, 
				CFAbsoluteTimeGetCurrent() + (86400L * 365L * 10L), 
				86400L * 365L * 10L, 0, 0, sysEventTimerNotifier, &timerContext);
  CFRunLoopAddTimer(CFRunLoopGetCurrent(), threadData.timerRef, kCFRunLoopDefaultMode);


 /*
  * Store our runloop in a global so the main thread can
  * use it to stop us.
  */

  pthread_mutex_lock(&SysEventThreadMutex);

  SysEventRunloop = CFRunLoopGetCurrent();

  pthread_cond_signal(&SysEventThreadCond);
  pthread_mutex_unlock(&SysEventThreadMutex);


 /*
  * Disappear into the runloop until it's stopped by the main thread.
  */

  CFRunLoopRun();


 /*
  * Clean up before exiting.
  */

  if (threadData.timerRef)
  {
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), threadData.timerRef, kCFRunLoopDefaultMode);
    CFRelease(threadData.timerRef);
  }

  if (powerRLS)
  {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), powerRLS, kCFRunLoopDefaultMode);
    CFRunLoopSourceInvalidate(powerRLS);
    CFRelease(powerRLS);
  }

  if (threadData.sysevent.powerKernelPort)
  {
    IODeregisterForSystemPower(&powerNotifierObj);
    IOServiceClose(threadData.sysevent.powerKernelPort);
  }

  if (storeRLS)
  {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), storeRLS, kCFRunLoopDefaultMode);
    CFRunLoopSourceInvalidate(storeRLS);
    CFRelease(storeRLS);
  }

  if (store)
    CFRelease(store);

  pthread_exit(NULL);
}


/*
 * 'sysEventPowerNotifier()' - .
 */

static void sysEventPowerNotifier(void *context, io_service_t service, natural_t messageType, void *messageArgument)
{
  int			disposition = 1;	/* How shall we dispose of the event? */
						/* 0=ignore; 1=send now; 2=send later. Default is send now */
  cups_thread_data_t	*threadData;		/* Thread context data */

  threadData = (cups_thread_data_t *)context;

  (void)service;				/* anti-compiler-warning-code */

  switch (messageType)
  {
  case kIOMessageCanSystemPowerOff:
  case kIOMessageCanSystemSleep:
    threadData->sysevent.event |= SYSEVENT_CANSLEEP;
    break;

  case kIOMessageSystemWillRestart:
  case kIOMessageSystemWillPowerOff:
  case kIOMessageSystemWillSleep:
    threadData->sysevent.event |= SYSEVENT_WILLSLEEP;
    break;

  case kIOMessageSystemHasPoweredOn:
   /* 
    * Because powered on is followed by a net-changed event delay one second before sending it.
    */
    disposition = 2;
    threadData->sysevent.event |= SYSEVENT_WOKE;
    break;

  case kIOMessageSystemWillNotPowerOff:
  case kIOMessageSystemWillNotSleep:
  case kIOMessageSystemWillPowerOn:
  default:
    disposition = 0;
    break;
  }

  if (disposition == 0)
    IOAllowPowerChange(threadData->sysevent.powerKernelPort, (long)messageArgument);
  else
  {
    threadData->sysevent.powerNotificationID = (long)messageArgument;

    if (disposition == 1)
    {
     /* 
      * Send the event to the main thread now.
      */
      write((int)SysEventPipes[1], &threadData->sysevent, sizeof(threadData->sysevent));
      threadData->sysevent.event = 0;
    }
    else
    {
     /* 
      * Send the event to the main thread after 1 to 2 seconds.
      */
      CFRunLoopTimerSetNextFireDate(threadData->timerRef, CFAbsoluteTimeGetCurrent() + 2);
    }
  }
}


/*
 * 'sysEventConfigurationNotifier()' - Computer name changed notification callback.
 */

static void sysEventConfigurationNotifier(SCDynamicStoreRef store, CFArrayRef changedKeys, void *context)
{
  (void)store;		/* anti-compiler-warning-code */

  CFRange range = CFRangeMake(0, CFArrayGetCount(changedKeys));

  if (CFArrayContainsValue(changedKeys, range, ComputerNameKey))
    ((cups_thread_data_t *)context)->sysevent.event |= SYSEVENT_NAMECHANGED;

  if (CFArrayContainsValue(changedKeys, range, NetworkGlobalKey) ||
      CFArrayContainsValue(changedKeys, range, HostNamesKey) ||
      CFArrayContainsValue(changedKeys, range, NetworkInterfaceKey))
    ((cups_thread_data_t *)context)->sysevent.event |= SYSEVENT_NETCHANGED;

 /*
  * Because we registered for several different kinds of change notifications 
  * this callback usually gets called several times in a row. We use a timer to 
  * de-bounce these so we only end up generating one event for the main thread.
  */

  CFRunLoopTimerSetNextFireDate(((cups_thread_data_t *)context)->timerRef, 
  				CFAbsoluteTimeGetCurrent() + 2);
}


/*
 * 'sysEventTimerNotifier()' - .
 */

static void sysEventTimerNotifier(CFRunLoopTimerRef timer, void *context)
{
  cups_thread_data_t	*threadData;		/* Thread context data */

  threadData = (cups_thread_data_t *)context;

 /*
  * If an event is still pending send it to the main thread.
  */

  if (threadData->sysevent.event)
  {
    write((int)SysEventPipes[1], &threadData->sysevent, sizeof(threadData->sysevent));
    threadData->sysevent.event = 0;
  }
}

#endif	/* __APPLE__ */

/*
 * End of "$Id: server.c,v 1.13.2.2 2005/07/27 21:58:45 jlovell Exp $".
 */
