/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/*
 * mslpd.c : Minimal SLP v2 Service Agent
 *
 *   This is the main module for the mslpd server.  This service provides
 *   SA services by reading a registration file which is either supplied
 *   by the command line or by the configuration file.  The configuration
 *   file is called mslpd.conf and is either in the current working directory
 *   specified in the command line.
 * 
 *   Command line:
 *      mslpd <regfile> [-f <configfile> ]
 *
 *   Terminate mslpd using:
 *      SIGINT or CTL-C
 *
 *   The configuration file is optional.  The configuration elements used
 *   in mslpd (the SA) functions are as followed:
 *
 *    Configuration parameter:     Data type:   Default:   Meaning:
 *    ============================ ============ ========== ===================
 *    net.slp.locale               String       "en"       Language tag to use
 *    net.slp.useScopes            String List  "DEFAULT"  SA scopes to use
 *    net.slp.DAAddresses          DA List      <none>     Use these DAs
 *    net.slp.interfaces           Addr List    <default>  Mcast interface to
 *                                                         send and receive
 *                                                         SLP messages from
 *    net.slp.isBroadcastOnly      Boolean      False      Use Bcast not Mcast
 *    net.slp.multicastMaximumWait Integer      15000      Milliseconds wait
 *    net.slp.MTU                  Integer      1400       Max # bytes to send
 *    net.slp.multicastTTL         Integer      255        Max range of Mcast
 *    net.slp.traceDrops           Boolean      False      Trace dropped msgs
 *    com.sun.slp.traceAll         Boolean      False      Turn on all traces
 *    com.sun.slp.noSA             Boolean      False      Testing: do not use
 *                                                         mcast/convergence
 *    com.sun.slp.noDA             Boolean      False      Testing: do not use
 *                                                         DAs
 *    com.sun.slp.regfile          String       <default>  This file holds the
 *                                                         currently advertised
 *                                                         services while mslpd
 *                                                         is running.
 *    com.sun.slp.tempfile         String       <default>  This file is used to
 *                                                         update or add reg-
 *                                                         istrations while
 *                                                         mslpd is running.
 *    net.slp.serializedRegURL     String       <none>     A "file:" URL which
 *                                                         points to the reg
 *                                                         file to proxy ad-
 *                                                         vertise when mslpd
 *                                                         starts up.
 *
 *	 com.apple.slp.isDA			   Boolean      False      This deamon is to be
 *														   used both as an SA
 *														   and a DA
 *
 *	 com.apple.slp.daScopeList		String List	<none>	   These are the scopes
 *														   the DA is configured with
 *
 *   com.apple.slp.defaultRegistrationScope String List "DEFAULT" SA Scope(s) to 
 *															register services in
 *
 *   Configuration parameters starting with "net.slp" are part of the API
 *   published by the SVRLOC WG.  Configuration parameters starting with 
 *   "com.sun.slp" are private configuration parameters to this
 *   implementation.
 *
 *   A boolean configuration parameter is only true if it is present and
 *   it is set to "true".
 *
 *   Configuration parameters in a configuration file are supplied on a
 *   single line with an attribute value pair.  For instance, the following
 *   registration file would set the scopes of the SA to one, two and three
 *   and turn on all trace features.
 *
 *      net.slp.useScopes=one,two,three
 *      com.sun.slp.traceAll=true
 *   
 * Version: 1.16
 * Date:    10/06/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */
#include <mach/mach.h>
#include <mach/mach_error.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <IOKit/IOMessage.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "SLPSystemConfiguration.h"
#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslplib.h"
#include "mslpd.h"

#include "slpipc.h"

#include "SLPRegistrar.h"

/*
 * Locally defined functions
 */
static int		InitializeListeners( SAState* psa );

static int      assign_defaults(SAState *psa, struct sockaddr_in *psin,
				int argc, char *argv[]);

static void     exit_handler(int signo);

#ifdef EXTRA_MSGS
static SLPInternalError fcopy(const char *pc1, const char *pc2);
#endif  /* EXTRA_MSGS */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 * GLOBAL :     This data structure holds references to resources
 *              which need to be cleaned up on program termination.
 *              This global is to be used when exit_handler is
 *              called - and NOWHERE ELSE.
 */
static MslpdResources	global_resources;
static bool				_Terminated = false;
static int				_Signal = 0;

bool IsProcessTerminated( void )
{
    return _Terminated;
}

SLPBoolean AreWeADirectoryAgent( void )
{
    SLPBoolean	isDA = SLP_FALSE;
    
    if ( SLPGetProperty("com.apple.slp.isDA") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.isDA"),"true") )
        isDA = SLP_TRUE;
    
    return isDA;
}

SLPInternalError reset_slpd( int argc, char *pcArgv[], struct sockaddr_in *psin, SAState* psa )
{
    const char * pcFile;

    SLP_LOG( SLP_LOG_STATE, "*** slpd reset ***" );
#ifdef EXTRA_MSGS
    if ((psa->pvMutex = SDGetMutex(MSLP_SERVER)) == NULL)
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"slpd: could not init mutex",SLP_INTERNAL_SYSTEM_ERROR);
    }

    global_resources.pvMutex = psa->pvMutex; // to free on exit! 

    SDLock(psa->pvMutex);
#endif

    if ( argc > 4 || (argc > 1 && pcArgv[1][0] == '?') )
    {
        fprintf( stderr,
            "Usage: %s [<filename>] [-f <config-file>]\n"
            "  <filename> is a serialized registration file. (optional)\n"
            "  <config-file> is a configuration file. (optional)\n",
            pcArgv[0] );
        return SLP_PARSE_ERROR;
    }

    if ( assign_defaults( psa, psin, argc, pcArgv ) != SLP_OK )
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"Could not assign defaults - abort",SLP_INTERNAL_SYSTEM_ERROR);
        return SLP_INTERNAL_SYSTEM_ERROR;
    }

    InitializeSLPdSystemConfigurator();
    DeleteRegFileIfFirstStartupSinceBoot();
    
    /*
    *  process the registration file, indicated on the command line or param
    */
    if ((pcFile = SLPGetProperty("net.slp.serializedRegURL")) != NULL)
    {
        pcFile = &pcFile[strlen("file:")]; /* skip past the 'file:' scheme */
    }

    if (pcFile)
    {
        if (process_regfile(&(psa->store), pcFile) < 0)
        {
            SLP_LOG(SLP_LOG_ERR,"mslpd: could not process regfile",SLP_INTERNAL_SYSTEM_ERROR);
            exit(-2);
        }
    }

#ifdef EXTRA_MSGS
    /*
    * Copy the value of the registration file to file used by both
    * mslpd and all services which advertise themselves using libslp.
    */
    if ( SLPGetProperty("com.sun.slp.regfile") )
        SLP_LOG(SLP_LOG_DEBUG, "reset_slpd, handling regfile: %s", SLPGetProperty("com.sun.slp.regfile"));
    else
        SLP_LOG(SLP_LOG_DEBUG, "reset_slpd, regfile property is NULL!");
        
    if ( pcFile )
    {
        fcopy(pcFile,SLPGetProperty("com.sun.slp.regfile"));
    }
    else
    {
        FILE *fp = fopen(SLPGetProperty("com.sun.slp.regfile"),"r+");

        if (!fp)
            fp = fopen(SLPGetProperty("com.sun.slp.regfile"),"w+");		// the file may not exist, so create it
            
        if (!fp)
        {
            SLP_LOG(SLP_LOG_ERR,"mslpd: could not initialize regfile %s",strerror(errno));
            return SLP_PREFERENCES_ERROR;
        }
        else
        {
            if ( getc(fp) == EOF )		// is this a new file?
            {
	            if (fprintf(fp,"# initial reg file automatically generated\n") < 0)
	            {
	                fclose(fp);
                    SLP_LOG(SLP_LOG_ERR,"mslpd: could not write to regfile %s",strerror(errno));
                    return SLP_PREFERENCES_ERROR;
	            }
                
	            if (SDchmod_writable(SLPGetProperty("com.sun.slp.regfile")) < 0)
	            {
	                fclose(fp);
                    SLP_LOG(SLP_LOG_ERR,"mslpd: could not change file permissions",strerror(errno));
                    return SLP_PREFERENCES_ERROR;
	            }
            }
            else if (process_regfile(&(psa->store), SLPGetProperty("com.sun.slp.regfile")) < 0)  // This file was already there, we should process it
            {
                fclose(fp);
                LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"mslpd: could not process regfile",SLP_INTERNAL_SYSTEM_ERROR);
            }

            if (fclose(fp) < 0)
            {
                SLP_LOG(SLP_LOG_ERR,"mslpd: could not close reg file",strerror(errno));
                return SLP_PREFERENCES_ERROR;
            }
        }
    }

    SDUnlock(psa->pvMutex);
#endif /* EXTRA_MSGS */

    SLP_LOG( SLP_LOG_DEBUG, "reset_slpd finished" );
    
    return SLP_OK;
}

int InitializeListeners( SAState* psa )
{
    int	err = 0;
    
    err = InitializeInternalProcessListener( psa );
    
    if ( !err )
        err = InitializeTCPListener( psa );
        
    if ( !err )
        err = InitializeUDPListener( psa );
    
    return err;
}

enum
{
	kSignalMessage		= 1000
};

typedef struct SignalMessage
{
	mach_msg_header_t	header;
	mach_msg_body_t		body;
	int					signum;
	mach_msg_trailer_t	trailer;
} SignalMessage;



static void SignalHandler(int signum);
static void SignalMessageHandler(CFMachPortRef port,SignalMessage *msg,CFIndex size,void *info);
static mach_port_t					gSignalPort = MACH_PORT_NULL;


void SignalHandler(int signum)
{
	SignalMessage	msg;
	
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0);
	msg.header.msgh_size = sizeof(msg) - sizeof(mach_msg_trailer_t);
	msg.header.msgh_remote_port = gSignalPort;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_id = kSignalMessage;
	msg.body.msgh_descriptor_count = 0;
	msg.signum = signum;
	
	mach_msg(&msg.header,(MACH_SEND_MSG | MACH_SEND_TIMEOUT),
			 msg.header.msgh_size,0,MACH_PORT_NULL,0,MACH_PORT_NULL);
}



void SignalMessageHandler(CFMachPortRef port,SignalMessage *msg,CFIndex size,void *info)
{
	CFRunLoopStop(CFRunLoopGetCurrent());
}


/*
 * For now, this requires a single argument, which is the service file.
 * The service file is formatted as described in the SLP API specification.
 */
int main(int argc, char *pcArgv[])
{
    SAState 			sa;
    struct sockaddr_in	sin; /* for active da discovery */
    int 				err = 0;
    const char *		pcScopes;
	struct rlimit 		rlim;
	rlim_t				i;

// first just see if they are only checking the version...
    if ( argc > 1 && strcmp(pcArgv[1], "-v") == 0 )
    {
        // they just want the version number
        fprintf( stdout, "%s\n", SLPD_VERSION );
        exit(0);
    }
    
    SLP_LOG( SLP_LOG_STATE, "*** slpd started ***" );
	
// we need to close all the open file descriptors of who ever launched us!

	if ( getrlimit(RLIMIT_NOFILE, &rlim) < 0 )
	{
		LOG(SLP_LOG_ERR, "Failed to getrlimit!");
		rlim.rlim_cur = FD_SETSIZE;
	}
	
	for ( i=rlim.rlim_cur; i>=0; i-- )
		close(i);

	// Ignore SIGPIPE's mysteriously generated by AFP/ServerControl.
	struct sigaction	sSigAction, sSigOldAction ;

	sigemptyset (&sSigAction.sa_mask) ;
	sSigAction.sa_handler = (void (*) (int)) SIG_IGN ;
	::sigaction (SIGPIPE, &sSigAction, &sSigOldAction) ;


	mach_port_limits_t	limits = { 1 };
	CFMachPortRef		port;
	
	port = CFMachPortCreate(NULL,(CFMachPortCallBack)SignalMessageHandler,NULL,NULL);
	CFRunLoopAddSource(CFRunLoopGetCurrent(),CFMachPortCreateRunLoopSource(NULL,port,0),kCFRunLoopCommonModes);
	
	gSignalPort = CFMachPortGetPort(port);
	mach_port_set_attributes(mach_task_self(),gSignalPort,MACH_PORT_LIMITS_INFO,
							 (mach_port_info_t)&limits,sizeof(limits) / sizeof(natural_t));
	
	signal(SIGINT,SignalHandler);
    
	if ( IsNetworkSetToTriggerDialup() )
	{
		LOG(SLP_LOG_ERR, "slpd: Network is set to auto dial ppp, - abort");
		return 0;
	}
	
    OPEN_NETWORKING();		// have to call this to intialize our lock mutex
    InitSLPRegistrar();

    memset( &sa, 0, sizeof(SAState) );

    err = reset_slpd( argc, pcArgv, &sin, &sa );		// do initialization and process config file
   
    if ( err )
    {
        LOG(SLP_LOG_ERR, "slpd: could not do initial setup - abort");
        return err;
    }
    
    SDatexit(exit_handler);

	/*
     *	time to daemonize
     */

    /*
    *  initialize network stuff
    */
    if (mslpd_init_network(&sa) != SLP_OK)
    {
    	LOG(SLP_LOG_FAIL, "slpd: could not init networking - abort");
        return -3;
    }

	/*
    * save socket handles so we can free them on exit.
    */
    global_resources.sdUDP = sa.sdUDP;
    global_resources.sdTCP = sa.sdTCP;

    if ((pcScopes = SLPGetProperty("net.slp.useScopes"))==NULL)
    {
        pcScopes = SLP_DEFAULT_SCOPE;
    }
    
    InitializeListeners( &sa );
    
	StartSLPUDPListener( &sa );
	StartSLPTCPListener( &sa );
    
    if ( AreWeADirectoryAgent() )
	{
        StartSLPDAAdvertiser( &sa );
		SLPRegistrar::TheSLPR()->EnableRAdminNotification();
	}
	
    /*
    *  discover DAs actively
    *  each time one is found, local registration is immediately forward
    *
    *  NOTE:  This is very simple-minded.  If there are many DAs or any of
    *  them are slow - the forwarding will take too long and the active
    *  discovery will time out before all DAs are found.  A better way to
    *  do this would be to simply go through all DAs after this round and
    *  forward to them sequentially.
    */
	InitializeSLPDARegisterer( &sa );

    StartSLPDALocator( (void *)mslpd_daadvert_callback, CFRunLoopGetCurrent(), &sa );

    sa.pdat = GetGlobalDATable();

    /*
    * The main loop is such that it simply launches handlers.  These could
    * be done in a separate thread.  Note that the SAState is read only
    * except for the fd_set, which should only be set in this thread.
    * If the mslpd accepts register/deregister ipc commands in the future,
    * the SAStore.store field will require locks.
    */

    SLP_LOG( SLP_LOG_MSG, "slpd: initialization finished");
    
    err = RunSLPInternalProcessListener( &sa );		// this is just going to listen for IPC communications
    
    CFRunLoopRun();		// this will run forever until interrupted from the command line or CFRunLoopStop
    
    SLP_LOG( SLP_LOG_MSG, "slpd: exiting");
    
    
    return _Signal;
}

/*
 * assign_defaults
 *
 *   Initialization routine for all state specific to the mslpd.
 *   This is the routine which processes the command line inputs
 *   as well.  After basic properties are set, the configuration
 *   file is read in (if one is specified on the command line.)
 *
 */
static int assign_defaults(SAState *psa, struct sockaddr_in *psin,
                           int argc, char *argv[]) 
{

    char* 		daScopeListTemp = NULL;
    SLPBoolean 	needToSetOverflow = SLP_FALSE;
        
	psa->tvTimeout.tv_sec =  1L;
	psa->tvTimeout.tv_usec = 0L;
	memset(psin,0,sizeof(struct sockaddr_in));
	
	/* set up default configuration */
	SLPSetProperty("com.sun.slp.isSA","true");
	#ifdef MAC_OS_X
	SLPSetProperty("com.apple.slp.isDA", "false");
	#endif
	SLPSetProperty("net.slp.useScopes",SLP_DEFAULT_SCOPE);							// this is our current list of scopes
    SLPSetProperty("com.apple.slp.defaultRegistrationScope",SLP_DEFAULT_SCOPE);		// use this for registrations
    SLPSetProperty("com.apple.slp.daScopeList", SLP_DEFAULT_SCOPE);					// this only get's used if we are a da
	SLPSetProperty("net.slp.multicastTTL",MCAST_TTL);
	SLPSetProperty("net.slp.MTU",SENDMTU);
    if (!SLPGetProperty("com.apple.slp.port"))
        SLPSetProperty("com.apple.slp.port","427");
	SLPSetProperty("net.slp.locale","en");
	SLPSetProperty("com.sun.slp.minRefreshInterval","10800");
    SLPSetProperty("com.apple.slp.logfile",LOG_FILE);
    SLPSetProperty("com.apple.slp.identity", "slpd");

    /*
	* If there is a configuration file on the command line, read it in.
	* Ensure that net.slp.serializedRegURL is set properly at the end.
	*/
	if (argc == 4 && !strcmp(argv[2], "-f")) 
	{
		char buf[1029];
		SLPReadConfigFile(argv[3]);
		sprintf(buf,"file:%s",argv[1]);
		/* the command line takes precedence over the property file. */
		SLPSetProperty("net.slp.serializedRegURL",buf);
	} 
	else if (argc == 3 && !strcmp(argv[1],"-f")) 
	{
		SLPReadConfigFile(argv[2]);    
	} 

    // Now we need to check if our scope list is too long and needs to be trimmed
    if ( SLPGetProperty( "com.apple.slp.daScopeList" ) && strlen(SLPGetProperty( "com.apple.slp.daScopeList" ) ) > kMaxScopeListLenForUDP )
    {
        daScopeListTemp = (char*)malloc( strlen(SLPGetProperty( "com.apple.slp.daScopeList" ) ) +1 );
        strcpy( daScopeListTemp, SLPGetProperty( "com.apple.slp.daScopeList" ) );

        while ( strlen( daScopeListTemp ) > kMaxScopeListLenForUDP )	// as long as we have a scope list that is too long
        {
            // message is too long
            char* tempPtr = strrchr( daScopeListTemp, ',' );
            
            needToSetOverflow = SLP_TRUE;
            
            if ( !tempPtr )
            {
                free( daScopeListTemp );
                SLP_LOG( SLP_LOG_FAIL, "we can't fit a single scope in our advertisement!" );			// bad error
                break;
            }
            
            if ( tempPtr && *tempPtr == ',' )
                *tempPtr = '\0';				// chop off here and try again
        }
    }
    
    if ( needToSetOverflow == SLP_TRUE )
    {
        SLPSetProperty( "com.apple.slp.daPrunedScopeList", daScopeListTemp );
        SLP_LOG( SLP_LOG_RADMIN, "We have a scope list that is longer than can be advertised via multicasting.  Some SLP implementations may see a truncated list." );
    }
    else
        SLPSetProperty( "com.apple.slp.daPrunedScopeList", NULL );
    
    if ( daScopeListTemp )
        free( daScopeListTemp );
    
#ifndef NDEBUG

	if ( AreWeADirectoryAgent() )
        mslplog(SLP_LOG_DEBUG,"slpd as a DA started with scopes", SLPGetProperty("com.apple.slp.daScopeList"));
    else
        mslplog(SLP_LOG_DEBUG,"slpd as an SA started with scopes", SLPGetProperty("net.slp.useScopes"));
  
#endif /* NDEBUG */

	/* set up the address for da discovery */
	psin->sin_family      = AF_INET;
	psin->sin_port        = htons(SLP_PORT);
	
	if (SLPGetProperty("net.slp.isBroadcastOnly") != NULL &&
		!(SDstrcasecmp(SLPGetProperty("net.slp.isBroadcastOnly"),"true"))) 
	{
		psin->sin_addr.s_addr = BROADCAST;
	} 
	else 
	{
		psin->sin_addr.s_addr = SLP_MCAST;
	}
  
    psa->pdat = GetGlobalDATable();

#ifdef EXTRA_MSGS
	if (!SLPGetProperty("com.sun.slp.regfile"))
		SLPSetProperty("com.sun.slp.regfile",SDDefaultRegfile());
	if (!SLPGetProperty("com.sun.slp.tempfile"))
		SLPSetProperty("com.sun.slp.tempfile",SDDefaultTempfile());
#endif /* EXTRA_MSGS */
  
	return SLP_OK;
}


/*
 * propogate_all_advertisements
 *
 *   This routine is used at the end of active discovery and when rereg-
 *   istration is required to register all services with the DAs that have
 *   the appropriate scopes.
 *
 */
			   
void propogate_all_advertisements(SAState *psa) 
{
    int 			i, needToCheckDAList=0;
    DATable*		pdat = GetGlobalDATable();
    SLPInternalError		err = SLP_OK;
    
    if ( !psa || !pdat )
        return;
        
    LockGlobalDATable();
    for (i = 0; i < pdat->iSize; i++) 
    {
        if ( pdat->pDAE[i].iStrikes <= kNumberOfStrikesAllowed )
        {
            err = propogate_registrations(psa,pdat->pDAE[i].sin, pdat->pDAE[i].pcScopeList);
        
            if ( err )
            {
                SLP_LOG( SLP_LOG_DA, "Error trying to propogate a registration to DA: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr) );		// just log this
                
                pdat->pDAE[i].iStrikes++;		// and give em a strike.
                
                needToCheckDAList = 1;		// look for any DAs that should be struck out
            }
        }
    }  
    UnlockGlobalDATable();
    
    if ( needToCheckDAList )
        dat_boot_off_struck_out_das();
}


/*
 * propogate_registration
 *
 *   This routine is used  to register all services with the DAs that have
 *   the appropriate scopes.
 *
 */
			   
void propogate_registration( SAState *psa, const char* lang, const char* srvtype, const char* url, const char* scopeList, const char* attrlist, int life ) 
{
    int 			i, needToCheckDAList=0;
    SLPInternalError		err = SLP_OK;
    DATable*		pdat = GetGlobalDATable();
    const char*		pcSL = GetEncodedScopeToRegisterIn();
    
    if ( !psa || !pdat )
        return;
        
    if ( scopeList && scopeList[0] != '\0' )
        pcSL = scopeList;

    LockGlobalDATable();
    for (i = 0; i < pdat->iSize; i++) 
    {
        if ( list_intersection(pdat->pDAE[i].pcScopeList, pcSL ) )
        {
            if ( pdat->pDAE[i].iStrikes <= kNumberOfStrikesAllowed )
            {
                err = propogate_registration_with_DA(psa,pdat->pDAE[i].sin, lang, url, srvtype, pcSL, attrlist, life);
                
                if ( err )
                {
                    SLP_LOG( SLP_LOG_DA, "Error trying to propogate a registration to DA: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr) );		// just log this
                    
                    pdat->pDAE[i].iStrikes++;		// and give em a strike.
                    
                    needToCheckDAList = 1;		// look for any DAs that should be struck out
                }
            }
        }
        else
        {
            SLP_LOG( SLP_LOG_DEBUG, "Skipping registration propigation with DA: %s, DA ScopeList: %s, service ScopeList: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr), pdat->pDAE[i].pcScopeList, pcSL );
        }
        
        if ( err )
        {
            SLP_LOG( SLP_LOG_DA, "Error trying to propogate a registration to DA: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr) );		// just log this
        }
    }  
    UnlockGlobalDATable();
    
    if ( needToCheckDAList )
        dat_boot_off_struck_out_das();
}


/*
 * propogate_deregistration
 *
 *   This routine is used  to deregister all services with the DAs that have
 *   the appropriate scopes.
 *
 */
			   
void propogate_deregistration( SAState *psa, const char* lang, const char* srvtype, const char* url, const char* scopeList, const char* attrlist, int life ) 
{
    int 			i, needToCheckDAList=0;
    SLPInternalError		err = SLP_OK;
    DATable*		pdat = GetGlobalDATable();
    const char*		pcSL = GetEncodedScopeToRegisterIn();
    
    if ( !psa || !pdat )
        return;
        
    if ( scopeList && scopeList[0] != '\0' )
        pcSL = scopeList;

    LockGlobalDATable();
    for (i = 0; i < pdat->iSize; i++) 
    {
        if ( list_intersection(pdat->pDAE[i].pcScopeList, pcSL ) )
        {
            err = propogate_deregistration_with_DA(psa,pdat->pDAE[i].sin, lang, url, srvtype, pcSL, attrlist, life);
                
            if ( err )
            {
                SLP_LOG( SLP_LOG_DA, "Error trying to propogate a registration to DA: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr) );		// just log this
                
                pdat->pDAE[i].iStrikes++;		// and give em a strike.
                
                needToCheckDAList = 1;		// look for any DAs that should be struck out
            }
        }
        else
        {
            SLP_LOG( SLP_LOG_DEBUG, "Skipping deregistration propigation with DA: %s, DA ScopeList: %s, service ScopeList: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr), pdat->pDAE[i].pcScopeList, pcSL );
        }
        
        if ( err )
        {
            SLP_LOG( SLP_LOG_DA, "Error trying to propogate a deregistration to DA: %s", inet_ntoa(pdat->pDAE[i].sin.sin_addr) );		// just log this
        }
    }  
    UnlockGlobalDATable();
    
    if ( needToCheckDAList )
        dat_boot_off_struck_out_das();
}



#ifdef EXTRA_MSGS

static SLPInternalError fcopy(const char *pc1, const char *pc2)
{
    struct stat st1;
    char buf[100];
    FILE *fpSrc = NULL, *fpDest = NULL;
    int total;
    
    if (stat(pc1, &st1) < 0 ||
        (fpSrc = fopen(pc1,"rb")) == NULL ||
        (fpDest = fopen(pc2,"wb")) == NULL) 
    {
        if (fpSrc)
            fclose(fpSrc);
            
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR, "fcopy: could not open both src and dest to copy", SLP_PARAMETER_BAD);
    }
    else
    { /* copy file 1 to file 2 */
        total = st1.st_size;
        while (total > 0)
        {
            int xfer = (total > 100) ? 100 : total;
            int got;
            int wrote;
            if ((got = read(fileno(fpSrc),buf,xfer)) != xfer ||
                (wrote = write(fileno(fpDest),buf,xfer)) != xfer)
            {
                fclose(fpSrc);
                fclose(fpDest);
                LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR, "fcopy: could not read temp or write reg file", SLP_INTERNAL_SYSTEM_ERROR);
            }
            
            total -= xfer;
        } // while (total > 0)
                
        if (SDchmod_writable(SLPGetProperty("com.sun.slp.regfile")) < 0) 
        {
            SLP_LOG(SLP_LOG_ERR,"fcopy: could not change file permissions %s",strerror(errno));
            fclose(fpSrc);
            fclose(fpDest);
            return SLP_PREFERENCES_ERROR;
        }
        
        if (fclose(fpSrc) || fclose(fpDest)) 
        {
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR, "fcopy: could not close temp or reg file", SLP_INTERNAL_SYSTEM_ERROR);
        }
    } // else
    
    return SLP_OK;
}
#endif /* EXTRA_MSGS */

static void exit_handler(int signo) 
{
  /* free resources ! */
    SLP_LOG( SLP_LOG_STATE, "*** slpd exit has been called: (%d) ***", signo );

    close(global_resources.sdUDP);
    remove(SLPGetProperty("com.sun.slp.tempfile"));

#ifdef SLPTCP
    close(global_resources.sdTCP);
#endif /* SLPTCP */
  
    if ( signo == SIGHUP )
    {
        // we want to relaunch.  Question is are we a child of watchdog?
        SLP_LOG( SLP_LOG_SIGNAL, "slpd's parent pid is %d", getppid() );
    }
  
    exit(0);
}

