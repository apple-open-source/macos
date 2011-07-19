/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add a generic socket support to pppd, instead of tty.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/uio.h>                     /* struct iovec */

#include <net/if.h>
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>

#define APPLE 1

#include "../../../Family/ppp_defs.h"
#include "../../../Family/if_ppp.h"
#include "../../../Family/ppp_domain.h"
#include "../../../Helpers/pppd/pppd.h"
#include "../../../Helpers/pppd/fsm.h"
#include "../../../Helpers/pppd/lcp.h"

#include <cclkeys.h>		// XX okay to #include Apple-specific header?
 

/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

#define DIR_MODEMS_USER         "/Library/Modem Scripts/"
#define DIR_MODEMS_SYS          "/System/Library/Modem Scripts/"
#define DIR_TERMINALS           "/Library/Terminal Scripts/"
#define DIR_TTYS		"/dev/"

#define SUFFIX_CCLENGINE	  	"/CCLEngine"
#define PATH_MINITERM	  	"/usr/libexec/MiniTerm.app"


// ppp serial error codes (bits 8..15 of last cause key)
#define EXIT_PPPSERIAL_NOCARRIER  	1
#define EXIT_PPPSERIAL_NONUMBER  	2
#define EXIT_PPPSERIAL_BUSY	  	3
#define EXIT_PPPSERIAL_NODIALTONE  	4
#define EXIT_PPPSERIAL_ERROR	  	5
#define EXIT_PPPSERIAL_NOANSWER	  	6
#define EXIT_PPPSERIAL_HANGUP	  	7
#define EXIT_PPPSERIAL_MODEMSCRIPTNOTFOUND  	8
#define EXIT_PPPSERIAL_BADSCRIPT  	9

/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */
void serial_check_options();
int serial_connect(int *errorcode);
void serial_process_extra_options();
void serial_connect_notifier(void *param, uintptr_t code);
void serial_lcpdown_notifier(void *param, uintptr_t code);
int serial_terminal_window(char *script, int infd, int outfd);

static int modemdict(char **argv);

/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */

extern char *serviceid;   			/* configuration service ID to publish */
extern CFStringRef serviceidRef;	/* configuration service ID to publish */
extern int	kill_link;

static CFBundleRef 	bundle = 0;		/* our bundle ref */
static CFURLRef    	url = 0;		/* our bundle url ref */

/* option variables */
static bool 	modemsound = 1;
static bool 	modemreliable = 1;
static bool 	modemcompress = 1;
static bool 	modempulse = 0;
static int	modemdialmode = 0; 
static u_char	fullmodemscript[1024] = { 0 };
static u_char	fullterminalscript[1024] = { 0 };
static u_char	connectcommand[1024] = { 0 };
static u_char	pathccl[1024] = { 0 };
static u_char	altconnectcommand[1024] = { 0 };
static u_char	disconnectcommand[1024] = { 0 };
static u_char	terminalcommand[1024] = { 0 };
static u_char	cancelstr[32] = { 0 };
static CFStringRef	cancelstrref = NULL;
static u_char	icstr[32] = { 0 };
static CFStringRef	icstrref = NULL;
static u_char	iconstr[1024] = { 0 };
static CFStringRef	iconstrref = NULL;
static u_char	*modemscript = NULL;	
static u_char	*terminalscript = NULL;	
static bool 	terminalwindow = 0;
void (*old_check_options) __P((void));
int (*old_connect) __P((int *));
void (*old_process_extra_options) __P((void));

CFDictionaryRef modemdictref = NULL;

static CFDataRef connectdataref = NULL;

static CFDataRef terminaldataref = NULL;

static CFDataRef altconnectdataref = NULL;

static CFDataRef disconnectdataref = NULL;

/* option descriptors */
option_t serial_options[] = {
    { "modemscript", o_string, &modemscript,
      "CCL to use" },
    { "modemsound", o_bool, &modemsound,
      "Turn modem sound on", 1 },
    { "nomodemsound", o_bool, &modemsound,
      "Turn modem sound off", 0 },
    { "modemreliable", o_bool, &modemreliable,
      "Turn modem error correction on", 1 },
    { "nomodemreliable", o_bool, &modemreliable,
      "Turn modem error correction off", 0 },
    { "modemcompress", o_bool, &modemcompress,
      "Turn modem data compression on", 1 },
    { "nomodemcompress", o_bool, &modemcompress,
      "Turn modem data compression off", 0 },
    { "modemtone", o_bool, &modempulse,
      "Use modem tone mode", 0 },
    { "modempulse", o_bool, &modempulse,
      "Use modem pulse tone", 1 },
    { "modemdialmode", o_int, &modemdialmode,
      "dialmode : 0 = normal, 1 = blind(ignoredialtone), 2 = manual" },
    { "terminalscript", o_string, &terminalscript,
      "Terminal CCL to use" },
    { "terminalwindow", o_bool, &terminalwindow,
      "Use terminal window", 1 },
    { "modemdict", o_special_cfarg, (void *)modemdict,
      "Serialized Modem Dictionary ", OPT_PRIV },
    { NULL }
};



    
/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */
int start(CFBundleRef ref)
{
    CFStringRef 	strref;
    CFURLRef 		urlref;
   
    bundle = ref;
    CFRetain(bundle);
    
    url = CFBundleCopyBundleURL(bundle);

    // hookup our handlers
    old_check_options = the_channel->check_options;
    the_channel->check_options = serial_check_options;
    
    old_connect = the_channel->connect;
    the_channel->connect = serial_connect;
    
    old_process_extra_options = the_channel->process_extra_options;
    the_channel->process_extra_options = serial_process_extra_options;

    add_notifier(&connect_fail_notify, serial_connect_notifier, 0);
    add_notifier(&lcp_lowerdown_notify, serial_lcpdown_notifier, 0);

    cancelstrref = CFBundleCopyLocalizedString(bundle, CFSTR("Cancel"), CFSTR("Cancel"), NULL);
    if (cancelstrref == 0) return 1;
    CFStringGetCString(cancelstrref, (char*)cancelstr, sizeof(cancelstr), kCFStringEncodingUTF8);
    
    icstrref = CFBundleCopyLocalizedString(bundle, CFSTR("Network Connection"), CFSTR("Network Connection"), NULL);
    if (icstrref == 0) return 1;
    CFStringGetCString(icstrref, (char*)icstr, sizeof(icstr), kCFStringEncodingUTF8);
    
    urlref = CFBundleCopyResourceURL(bundle, CFSTR("NetworkConnect.icns"), NULL, NULL);
    if (urlref == 0 || ((strref = CFURLGetString(urlref)) == 0)) {
		if (urlref)
            CFRelease(urlref);
        return 1;
    }
    CFStringGetCString(strref, (char*)iconstr, sizeof(iconstr), kCFStringEncodingUTF8);
	
	iconstrref = CFStringCreateCopy(NULL, strref);
    CFRelease(urlref);

	urlref = CFBundleCopyBuiltInPlugInsURL(bundle);
	if (urlref == 0 || ((CFURLGetFileSystemRepresentation(urlref, TRUE, pathccl, sizeof(pathccl))) == FALSE)) {
		if (urlref)
            CFRelease(urlref);
        return 1;
    }
    strlcat((char*)pathccl, SUFFIX_CCLENGINE, sizeof(pathccl));
    CFRelease(urlref);
    
    // add the socket specific options
    add_options(serial_options);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void serial_process_extra_options()
{
    char str[MAXPATHLEN];
    struct stat statbuf;

    if (device && !ptycommand) {
    
        // first, transform device name
        str[0] = 0;
        if (device[0] != '/') {
            strlcat(str, DIR_TTYS, sizeof(str));
            if ((device[0] != 't')
                    || (device[1] != 't')
                    || (device[2] != 'y')
                    || (device[3] != 'd'))
                    strlcat(str, "cu.", sizeof(str));
        }
        strlcat(str, device, sizeof(str));
        strlcpy(devnam, str, sizeof(devnam));
        default_device = 0;
            
        // then check if device is there
        if (stat(devnam, &statbuf) < 0) {
            if (errno == ENOENT) {
                option_error("Device '%s' does not exist", devnam);
                die(EXIT_DEVICE_ERROR);
            }
            else
                fatal("Couldn't stat device %s: %m", devnam);
        }
    }
    
    if (old_process_extra_options)
        (*old_process_extra_options)();
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void serial_check_options()
{
            
    //Fix me : we only get the 8 low bits return code from the wait_pid
    cancelcode = 136; /*cclErr_ScriptCancelled*/

    if (modemscript || modemdictref) {
        // actual command will be filled in at connection time
		connector_uid = 0;
		disconnector_uid = 0;
        connect_script = (char*)connectcommand;
        disconnect_script = (char*)disconnectcommand;
        if (altremoteaddress) {
            altconnect_script = (char*)altconnectcommand;
	    redialalternate = 1;
	}
        if (redialcount) 
            busycode = 122; /*cclErr_LineBusyErr*/
    }
    
    if (terminalwindow || terminalscript) {
        // actual command will be filled in at connection time
        terminal_script = (char*)terminalcommand;
    }

    if (old_check_options)
        (*old_check_options)();
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFDataRef Serialize(CFPropertyListRef obj, void **data, u_int32_t *dataLen)
{
    CFDataRef           	xml;
    
    xml = CFPropertyListCreateXMLData(NULL, obj);
    if (xml) {
        *data = (void*)CFDataGetBytePtr(xml);
        *dataLen = CFDataGetLength(xml);
    }
    return xml;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int serial_connect(int *errorcode)
{
    struct stat 	statbuf;
    int 		err;
	CFMutableDictionaryRef ccldict, moddict;
	int			val;
	CFNumberRef	numRef;
	CFStringRef	strRef;
	CFMutableDictionaryRef connectdict = NULL;
	CFMutableDictionaryRef terminaldict = NULL;
	
	*errorcode = 0;

    if (modemscript || modemdictref) {
		
		// ---------- connect and altconnect scripts ----------
		
		snprintf((char*)connectcommand, sizeof(connectcommand), "%s -l %s -x", 
		pathccl, serviceid);
		
		// duplicate that into the alternate script
		strlcpy((char*)altconnectcommand, (char*)connectcommand, sizeof(altconnectcommand));

		// ---------- disconnect script ----------
		snprintf((char*)disconnectcommand, sizeof(disconnectcommand), "%s -m 1 -l %s -x", 
			pathccl, serviceid);
		
		connectdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!connectdict) {
            option_error("Could't create the CCLEngine dictionary");
            status = EXIT_CONNECT_FAILED;
            return -1;
		}

		/* create the CCLEngine dictionary and add the keys */
		ccldict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!ccldict) {
            option_error("Could't create the CCLEngine dictionary");
            status = EXIT_CONNECT_FAILED;
			CFRelease(connectdict);
            return -1;
		}

		CFDictionaryAddValue(ccldict, kCCLEngineServiceIDKey, serviceidRef);
		CFDictionaryAddValue(ccldict, kCCLEngineBundlePathKey, CFURLGetString(url));

		val = log_to_fd >= 0 ? 1 : 0;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineLogToStdErrKey, numRef);
		CFRelease(numRef);

		val = debug ? 1 : 0;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineVerboseLoggingKey, numRef);
		CFRelease(numRef);

		val = LOG_NOTICE;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineSyslogLevelKey, numRef);
		CFRelease(numRef);

		val = LOG_PPP;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineSyslogFacilityKey, numRef);
		CFRelease(numRef);

		CFDictionaryAddValue(ccldict, kCCLEngineAlertNameKey, icstrref);
		CFDictionaryAddValue(ccldict, kCCLEngineIconPathKey, iconstrref);
		CFDictionaryAddValue(ccldict, kCCLEngineCancelNameKey, cancelstrref);

		CFDictionaryAddValue(ccldict, kCCLEngineModeKey, kCCLEngineModeConnect);

		CFDictionaryAddValue(connectdict, kCCLEngineDictKey, ccldict);

		// if a modem dictionary was given, use it 
		if (modemdictref) {
			/* create the Modem dictionary and add the keys */
			moddict = CFDictionaryCreateMutableCopy(NULL, 0, modemdictref);
			if (!moddict) {
				option_error("Could't create the Modem dictionary");
				status = EXIT_CONNECT_FAILED;
				CFRelease(ccldict);
				CFRelease(connectdict);
				return -1;
			}
		}
		// if a modem dictionary was not given, build one from arguments 
		else {
			/* create the Modem dictionary and add the keys */
			moddict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			if (!moddict) {
				option_error("Could't create the Modem dictionary");
				status = EXIT_CONNECT_FAILED;
				CFRelease(ccldict);
				CFRelease(connectdict);
				return -1;
			}

			if (modemscript) {
			   /* check for ccl */ 
				err = 0;
				if (modemscript[0] != '/') {
					snprintf((char*)fullmodemscript, sizeof(fullmodemscript), "%s%s", DIR_MODEMS_SYS, modemscript);
					if (stat((char*)fullmodemscript, &statbuf) < 0) {
						snprintf((char*)fullmodemscript, sizeof(fullmodemscript), "%s%s", DIR_MODEMS_USER, modemscript);
						err = stat((char*)fullmodemscript, &statbuf);
					}
				}
				else {
					strlcpy((char*)fullmodemscript, (char*)modemscript, sizeof(fullmodemscript));
					err = stat((char*)fullmodemscript, &statbuf);
				}

				if (err) {
					option_error("Could't find modem script '%s'", modemscript);
					devstatus = EXIT_PPPSERIAL_MODEMSCRIPTNOTFOUND;
					status = EXIT_CONNECT_FAILED;
					CFRelease(moddict);
					CFRelease(ccldict);
					CFRelease(connectdict);
					return -1;
				}
				strRef = CFStringCreateWithCString(NULL, (char*)fullmodemscript, kCFStringEncodingMacRoman);
				if (strRef) {
					CFDictionaryAddValue(moddict, kSCPropNetModemConnectionScript, strRef);
					CFRelease(strRef);
				}
			}

			val = modemsound;
			numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
			CFDictionaryAddValue(moddict, kSCPropNetModemSpeaker, numRef);
			CFRelease(numRef);

			val = modempulse;
			numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
			CFDictionaryAddValue(moddict, kSCPropNetModemPulseDial, numRef);
			CFRelease(numRef);

			val = modemcompress;
			numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
			CFDictionaryAddValue(moddict, kSCPropNetModemDataCompression, numRef);
			CFRelease(numRef);

			val = modemreliable;
			numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
			CFDictionaryAddValue(moddict, kSCPropNetModemErrorCorrection, numRef);
			CFRelease(numRef);

			CFDictionaryAddValue(moddict, kSCPropNetModemDialMode, modemdialmode == 1 ? kSCValNetModemDialModeIgnoreDialTone : (modemdialmode == 2 ? kSCValNetModemDialModeManual : kSCValNetModemDialModeWaitForDialTone) );
		}

		if (remoteaddress) {
			strRef = CFStringCreateWithCString(NULL, remoteaddress, kCFStringEncodingMacRoman);
			if (strRef) {
				CFDictionaryAddValue(moddict, kModemPhoneNumberKey, strRef);
				CFRelease(strRef);
			}
		}			

		CFDictionaryAddValue(connectdict, kSCEntNetModem, moddict);

		if (connectdataref) {
			CFRelease(connectdataref);
		}
		connectdataref = Serialize(connectdict, (void**)&connect_data, (uint32_t *)&connect_data_len);
	
		if (altremoteaddress) {
			strRef = CFStringCreateWithCString(NULL, altremoteaddress, kCFStringEncodingMacRoman);
			if (strRef) {
				CFDictionarySetValue(moddict, kModemPhoneNumberKey, strRef);
				CFRelease(strRef);
			}

			if (altconnectdataref) {
				CFRelease(altconnectdataref);
			}
			altconnectdataref = Serialize(connectdict, (void**)&altconnect_data, (uint32_t*)&altconnect_data_len);
		}

		CFDictionaryRemoveValue(moddict, kModemPhoneNumberKey);
		CFDictionarySetValue(ccldict, kCCLEngineModeKey, kCCLEngineModeDisconnect);
		if (disconnectdataref) {
			CFRelease(disconnectdataref);
		}
		disconnectdataref = Serialize(connectdict, (void**)&disconnect_data, (uint32_t*)&disconnect_data_len);

		CFRelease(ccldict);
		CFRelease(moddict);
		CFRelease(connectdict);
    }
        
    if (terminalwindow) {
        
        terminal_window_hook = serial_terminal_window;
        strlcpy((char*)terminalcommand, PATH_MINITERM, sizeof(terminalcommand));
    }

    if (terminalscript) {
        		
		snprintf((char*)terminalcommand, sizeof(terminalcommand), "%s -l %s -x", pathccl, serviceid);
		
		terminaldict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!terminaldict) {
            option_error("Could't create the Terminal Script dictionary");
            status = EXIT_CONNECT_FAILED;
            return -1;
		}

		/* create the CCLEngine dictionary and add the keys */
		ccldict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!ccldict) {
            option_error("Could't create the CCLEngine dictionary for Terminal script");
            status = EXIT_CONNECT_FAILED;
			CFRelease(terminaldict);
            return -1;
		}

		CFDictionaryAddValue(ccldict, kCCLEngineServiceIDKey, serviceidRef);
		CFDictionaryAddValue(ccldict, kCCLEngineBundlePathKey, CFURLGetString(url));

		val = log_to_fd >= 0 ? 1 : 0;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineLogToStdErrKey, numRef);
		CFRelease(numRef);

		val = debug ? 1 : 0;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineVerboseLoggingKey, numRef);
		CFRelease(numRef);

		val = LOG_NOTICE;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineSyslogLevelKey, numRef);
		CFRelease(numRef);

		val = LOG_PPP;
		numRef = CFNumberCreate(NULL, kCFNumberIntType, &val);
		CFDictionaryAddValue(ccldict, kCCLEngineSyslogFacilityKey, numRef);
		CFRelease(numRef);

		CFDictionaryAddValue(ccldict, kCCLEngineAlertNameKey, icstrref);
		CFDictionaryAddValue(ccldict, kCCLEngineIconPathKey, iconstrref);
		CFDictionaryAddValue(ccldict, kCCLEngineCancelNameKey, cancelstrref);

		CFDictionaryAddValue(ccldict, kCCLEngineModeKey, kCCLEngineModeConnect);

		CFDictionaryAddValue(terminaldict, kCCLEngineDictKey, ccldict);

		/* create the Modem dictionary and add the keys */
		moddict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!moddict) {
			option_error("Could't create the Modem dictionary for Terminal script");
			status = EXIT_CONNECT_FAILED;
			CFRelease(ccldict);
			CFRelease(terminaldict);
			return -1;
		}

	   /* check for ccl */ 
		snprintf((char*)fullterminalscript, sizeof(fullterminalscript), "%s%s", (terminalscript[0] == '/') ? "" : DIR_TERMINALS, terminalscript);
		err = stat((char*)fullterminalscript, &statbuf);
		if (err) {
			option_error("Could't find terminal script '%s'", terminalscript);
			devstatus = EXIT_PPPSERIAL_MODEMSCRIPTNOTFOUND;
			status = EXIT_CONNECT_FAILED;
			CFRelease(moddict);
			CFRelease(ccldict);
			CFRelease(terminaldict);
			return -1;
		}
		strRef = CFStringCreateWithCString(NULL, (char*)fullterminalscript, kCFStringEncodingMacRoman);
		if (strRef) {
			CFDictionaryAddValue(moddict, kSCPropNetModemConnectionScript, strRef);
			CFRelease(strRef);
		}

		
		strRef = CFStringCreateWithCString(NULL, user, kCFStringEncodingMacRoman);
		if (strRef) {
			CFDictionaryAddValue(moddict, kSCPropNetPPPAuthName, strRef);
			CFRelease(strRef);
		}
        
		strRef = CFStringCreateWithCString(NULL, passwd, kCFStringEncodingMacRoman);
		if (strRef) {
			CFDictionaryAddValue(moddict, kSCPropNetPPPAuthPassword, strRef);
			CFRelease(strRef);
		}

		CFDictionaryAddValue(terminaldict, kSCEntNetModem, moddict);

		if (terminaldataref) {
			CFRelease(terminaldataref);
		}
		terminaldataref = Serialize(terminaldict, (void**)&terminal_data, (uint32_t *)&terminal_data_len);

		CFRelease(ccldict);
		CFRelease(moddict);
		CFRelease(terminaldict);
    }
    
	if (remoteaddress)
		set_network_signature("Modem.RemoteAddress", remoteaddress, 0, 0);

    if (old_connect)
        return (*old_connect)(errorcode);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void serial_connect_notifier(void *param, uintptr_t code)
{
    
    switch (code) {
        // cclErr_BadScriptErr = -6028  	// Incorrect script for the modem.
        case 116:
            devstatus = EXIT_PPPSERIAL_BADSCRIPT;
            break;
        // cclErr_NoNumberErr = -6027  	// Can't connect because number is empty.
        case 117:
            devstatus = EXIT_PPPSERIAL_NONUMBER;
            break;
        // cclErr_NoAnswerErr = -6023	No answer.
        case 121:
            devstatus = EXIT_PPPSERIAL_NOANSWER;
            break;
        // cclErr_LineBusyErr = -6022	Line busy.
        case 122:
            devstatus = EXIT_PPPSERIAL_BUSY;
            break;
        // cclErr_NoCarrierErr = -6021	No carrier.
        case 123:
            devstatus = EXIT_PPPSERIAL_NOCARRIER;
            break;
        // cclErr_NoDialTone = -6020	No dial tone.
       case 124: // 
            devstatus = EXIT_PPPSERIAL_NODIALTONE;
            break;
        // cclErr_ModemErr = -6019  Modem error, modem not responding
        case 125:
            devstatus = EXIT_PPPSERIAL_ERROR;
            break;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void serial_lcpdown_notifier(void *param, uintptr_t code)
{

    if (status == EXIT_HANGUP)
        devstatus = EXIT_PPPSERIAL_HANGUP;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int start_listen (char *filestr)
{
    struct sockaddr_un	addr;
    int			err, s;
    mode_t		mask;

    if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
        goto fail;

    unlink(filestr);
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strlcpy(addr.sun_path, filestr, sizeof(addr.sun_path));
    mask = umask(0);
    err = bind(s, (struct sockaddr *)&addr, SUN_LEN(&addr));
    umask(mask);
    if (err) 
        goto fail;
    listen(s, 1);
    return s;
    
fail:
    if (s != -1) 
        close(s);
    return -1;
}

/* -----------------------------------------------------------------------------
 Pass a file descriptor to another process.
 If fd<0, then -fd is sent back instead as the error status. 
----------------------------------------------------------------------------- */
static int send_fd(int clifd, int fd)
{
    struct cmsg {
        struct cmsghdr 	hdr;
        int		fd;
    } cmsg;
    struct iovec	iov[1];
    struct msghdr   	msg;
    char		buf[2]; /* send_fd()/recv_fd() 2-byte protocol */

    iov[0].iov_base = buf;
    iov[0].iov_len = 2;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    
    if (fd < 0) {
        msg.msg_control = NULL;
	msg.msg_controllen = 0;
	buf[1] = -fd;   /* nonzero status means error */
	if (buf[1] == 0)
            buf[1] = 1;     /* -256, etc. would screw up protocol */
    } else {
	cmsg.hdr.cmsg_level = SOL_SOCKET;
	cmsg.hdr.cmsg_type = SCM_RIGHTS;
	cmsg.hdr.cmsg_len = sizeof(struct cmsg);
        cmsg.fd = fd;	/* the fd to pass */
	msg.msg_control = (caddr_t) &cmsg;
	msg.msg_controllen = sizeof(struct cmsg);
	buf[1] = 0;	/* zero status means OK */
    }
    buf[0] = 0;	/* null byte flag to recv_fd() */

    if (sendmsg(clifd, &msg, 0) != 2)
	return -1;
        
    return 0;
}

/* -----------------------------------------------------------------------------
 use launch services to launch an application
 return < 0 if the application cannot be launched
----------------------------------------------------------------------------- */
static int launch_app(char *app, char *params)
{
#ifdef HAVE_LAUNCHSERVICES

    CFURLRef 		urlref;
    LSLaunchURLSpec 	urlspec;
    OSStatus 		err;
#if 0
    OSErr		oserr;
    AEDesc		desc;
#endif

    urlref = CFURLCreateFromFileSystemRepresentation(NULL, (u_char*)app, strlen(app), FALSE);
    if (urlref == 0) 
        return -1;
    
#if 0
    oserr = AECreateDesc(typeChar, params, strlen(params), &desc);
    if (oserr != noErr) {
        CFRelease(urlref);
        return -1;
    }
#endif

    urlspec.appURL = urlref;
    urlspec.itemURLs = 0;
    urlspec.passThruParams = 0;
#if 0
    urlspec.passThruParams = &desc;
#endif 
    urlspec.launchFlags = kLSLaunchAsync + kLSLaunchDontAddToRecents 
                + kLSLaunchNewInstance + kLSLaunchNoParams;
    urlspec.asyncRefCon = 0;
        
    err = LSOpenFromURLSpec(&urlspec, NULL);
    if (err != 0) {
#if 0
        AEDisposeDesc(&desc);
#endif 
        CFRelease(urlref);
        return -2;
    }

#if 0
    AEDisposeDesc(&desc);
#endif 
    CFRelease(urlref);
	
#endif /* HAVE_LAUNCHSERVICES */
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int wait_accept(int fd)
{
    int			sacc = 0, nready, maxfd;
	socklen_t	len;
    fd_set	allset, rset;
    struct timeval 	timenow, timeout, timeend;
    struct sockaddr_un	addr;
    
    FD_ZERO(&allset);
    FD_SET(fd, &allset);
    maxfd = fd;
    
    getabsolutetime(&timeend);
    timeend.tv_sec += 30; // allow 30 seconds for contact
    
    // now wait for contact
    for ( ; ; ) {

        getabsolutetime(&timenow);
        timeout.tv_sec = timeend.tv_sec - timenow.tv_sec;
        timeout.tv_usec = timeend.tv_usec - timenow.tv_usec;
        if (timeout.tv_usec < 0) {
            timeout.tv_usec += 1000000;
            timeout.tv_sec -= 1;
        }
       
        if (timeout.tv_sec < 0)
            return -1; // time out expires

        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, &timeout);
        
        if (kill_link)
            return -1;
        
        if (FD_ISSET(fd, &rset)) {

            len = sizeof(addr);
            if ((sacc = accept(fd, (struct sockaddr *) &addr, &len)) == -1) {
                return -2; // contact failed
            }
            break;
        }
    }
    
    return sacc;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int readn(int ref, void *data, int len)
{
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = read(ref, p, left)) < 0) {
            if (kill_link)
                return 0;
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        else if (n == 0)
            break; /* EOF */
            
        left -= n;
        p += n;
    }
    return (len - left);
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int serial_terminal_window(char *script, int infd, int outfd)
{
    int 	slis, sacc = 0, n;
    char 	c;
    char 	str[32];

    //sprintf(str, "/var/run/pppd-%d", getpid());
    snprintf(str, sizeof(str), "/var/run/pppd-miniterm");
    
    slis = start_listen(str);
    if (slis == -1) {
        error("Cannot listen for terminal window application");
        status = EXIT_TERMINAL_FAILED;
        return -2;
    }

    if (launch_app(PATH_MINITERM, str) < 0) {
        error("Cannot launch terminal window application");
        status = EXIT_TERMINAL_FAILED;
        close(slis);
        return -2;
    }

    sacc = wait_accept(slis);
    close(slis);
    if (sacc < 0) {
        if (kill_link) 
            return 0;
        error("Cannot communicate with terminal window application.");
        status = EXIT_TERMINAL_FAILED;
        return -2;
    }
    
    send_fd(sacc, infd);
    
    n = readn(sacc, &c, 1);
    close(sacc);
    if (n != 1) {
        if (kill_link) 
            return 0;
        error("Cannot get status from terminal window application (error %m)");
        status = EXIT_TERMINAL_FAILED;
        return -2;
    }
        
    return (unsigned char)c;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
modemdict(argv)
    char **argv;
{
    CFDataRef          	xml;
    CFStringRef        	xmlError;
	u_int32_t			len;
    char *				ptr;

    len = strtoul(argv[0], &ptr, 0);

    xml = CFDataCreate(NULL, (u_char*)argv[1], len);
    if (xml) {
        modemdictref = CFPropertyListCreateFromXMLData(NULL,
                xml,  kCFPropertyListImmutable, &xmlError);
        CFRelease(xml);
    }

	return 1;
}
