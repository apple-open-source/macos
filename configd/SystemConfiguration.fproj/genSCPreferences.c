/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * genSCPreferences.c
 * - generates System Configuration header/cfile
 * - invoke with "header" to generate the header
 * - invoke with "cfile" to generate the cfile
 */

/*
 * Modification History
 *
 * 16 July 2003			Allan Nathanson (ajn@apple.com)
 * - changes to facilitate cross-compilation to earlier releases
 *
 * 5 May 2003			Allan Nathanson (ajn@apple.com)
 * - switch back to "extern const CFStringRef ..."
 *
 * 1 June 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * 27 Apr 2001			Allan Nathanson (ajn@apple.com)
 * - switch from "extern const CFStringRef ..." to "#define ..."
 *
 * 3 Nov 2000			Dieter Siegmund (dieter@apple)
 * - created
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/boolean.h>

char copyright_string[] =
"/*\n"
" * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.\n"
" *\n"
" * @APPLE_LICENSE_HEADER_START@
" * 
" * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
" * 
" * This file contains Original Code and/or Modifications of Original Code
" * as defined in and that are subject to the Apple Public Source License
" * Version 2.0 (the 'License'). You may not use this file except in
" * compliance with the License. Please obtain a copy of the License at
" * http://www.opensource.apple.com/apsl/ and read it before using this
" * file.
" * 
" * The Original Code and all software distributed under the License are
" * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
" * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
" * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
" * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
" * Please see the License for the specific language governing rights and
" * limitations under the License.
" * 
" * @APPLE_LICENSE_HEADER_END@
" */\n";


typedef enum {
	COMMENT,
	OBSOLETE,
	REGULAR,
	DEFINE,
	FUTURE,
	END
} controlType;

#define SC_SCHEMA_DECLARATION	"SC_SCHEMA_DECLARATION"

#define KEY_PREFIX		"kSC"

#define COMP			"Comp"
#define DYNAMICSTORE		"DynamicStore"
#define PREF			"Pref"
#define PROP			"Prop"
#define PATH			"Path"
#define NETENT			"EntNet"
#define NETPROP			"PropNet"
#define NETVAL			"ValNet"
#define SETUPENT		"EntSetup"
#define SETUPPROP		"PropSetup"
#define SYSTEMENT		"EntSystem"
#define SYSTEMPROP		"PropSystem"
#define RESV			"Resv"
#define USERSENT		"EntUsers"
#define USERSPROP		"PropUsers"
#define VERSION			"Version"

#define CFARRAY_CFNUMBER	"CFArray[CFNumber]"
#define CFARRAY_CFSTRING	"CFArray[CFString]"
#define CFBOOLEAN		"CFBoolean"
#define CFDATA			"CFData"
#define CFDICTIONARY		"CFDictionary"
#define CFNUMBER		"CFNumber"
#define CFNUMBER_BOOL		"CFNumber (0 or 1)"
#define CFSTRING		"CFString"

#define ACSPENABLED		"ACSPEnabled"		// Apple Client Server Protocol
#define ACTIVE			"Active"
#define ADDRESSES		"Addresses"
#define AFTER			"After"
#define AIRPORT			"AirPort"
#define ALERT			"Alert"
#define ALLOWNETCREATION	"AllowNetCreation"
#define ALTERNATEREMOTEADDRESS	"AlternateRemoteAddress"
#define ANYREGEX		"AnyRegex"
#define APPLETALK		"AppleTalk"
#define AUTH			"Auth"
#define AUTOMATIC		"Automatic"
#define BEFORE			"Before"
#define BINDINGMETHODS		"BindingMethods"
#define BOOTP			"BOOTP"
#define BROADCAST		"Broadcast"
#define BROADCASTADDRESSES	"BroadcastAddresses"
#define BROADCASTSERVERTAG	"BroadcastServerTag"
#define CALLWAITINGAUDIBLEALERT	"CallWaitingAudibleAlert"
#define CCP			"CCP"
#define CHAP			"CHAP"
#define COMM			"Comm"
#define COMPRESSIONACFIELD	"CompressionACField"
#define COMPRESSIONPFIELD	"CompressionPField"
#define COMPRESSIONVJ		"CompressionVJ"
#define COMPUTERNAME		"ComputerName"
#define CONFIGMETHOD		"ConfigMethod"
#define CONNECTDELAY		"ConnectDelay"
#define CONNECTIONSCRIPT	"ConnectionScript"
#define CONNECTSPEED		"ConnectSpeed"
#define CONNECTTIME		"ConnectTime"
#define CONSOLEUSER		"ConsoleUser"
#define CURRENTSET		"CurrentSet"
#define DATACOMPRESSION		"DataCompression"
#define DEFAULTSERVERTAG	"DefaultServerTag"
#define DEFAULTZONE		"DefaultZone"
#define DESTADDRESSES		"DestAddresses"
#define DETACHING		"Detaching"
#define DEVICE			"Device"
#define DEVICENAME		"DeviceName"
#define DHCP			"DHCP"
#define DHCPCLIENTID		"DHCPClientID"
#define DIALMODE		"DialMode"
#define DIALONDEMAND		"DialOnDemand"
#define DISCONNECTONANSWER	"DisconnectOnAnswer"
#define DISCONNECTONIDLE	"DisconnectOnIdle"
#define DISCONNECTONIDLETIMER	"DisconnectOnIdleTimer"
#define DISCONNECTONLOGOUT	"DisconnectOnLogout"
#define DISCONNECTONSLEEP	"DisconnectOnSleep"
#define DISCONNECTTIME		"DisconnectTime"
#define DISPLAYTERMINALWINDOW	"DisplayTerminalWindow"
#define DNS			"DNS"
#define DOMAIN 			"Domain"
#define DOMAINNAME		"DomainName"
#define DOMAINSEPARATOR		"DomainSeparator"
#define EAP			"EAP"
#define ECHOENABLED		"EchoEnabled"
#define ECHOFAILURE		"EchoFailure"
#define ECHOINTERVAL		"EchoInterval"
#define ENABLED			"Enabled"
#define ENCODING		"Encoding"
#define ENCRYPTION		"Encryption"
#define ERRORCORRECTION		"ErrorCorrection"
#define ETHERNET		"Ethernet"
#define EXCEPTIONSLIST		"ExceptionsList"
#define FILE			"File"
#define FIREWIRE		"FireWire"
#define FLAGS			"Flags"
#define FTPENABLE		"FTPEnable"
#define FTPPASSIVE		"FTPPassive"
#define FTPPORT			"FTPPort"
#define FTPPROXY		"FTPProxy"
#define GID			"GID"
#define GLOBAL			"Global"
#define GOPHERENABLE		"GopherEnable"
#define GOPHERPORT		"GopherPort"
#define GOPHERPROXY		"GopherProxy"
#define HARDWARE		"Hardware"
#define HOLD			"Hold"
#define HOSTNAMES		"HostNames"
#define HTTPENABLE		"HTTPEnable"
#define HTTPPORT		"HTTPPort"
#define HTTPPROXY		"HTTPProxy"
#define HTTPSENABLE		"HTTPSEnable"
#define HTTPSPORT		"HTTPSPort"
#define HTTPSPROXY		"HTTPSProxy"
#define IDLEREMINDER		"IdleReminder"
#define IDLEREMINDERTIMER	"IdleReminderTimer"
#define IGNOREDIALTONE		"IgnoreDialTone"
#define INACTIVE		"Inactive"
#define INFORM			"INFORM"
#define INTERFACE		"Interface"
#define INTERFACENAME		"InterfaceName"
#define INTERFACES		"Interfaces"
#define IP			"IP"
#define IPCP			"IPCP"
#define IPV4			"IPv4"
#define IPV6			"IPv6"
#define IPSEC			"IPSec"
#define JOINMODE		"JoinMode"
#define KEYCHAIN		"Keychain"
#define L2TP			"L2TP"
#define LASTCAUSE		"LastCause"
#define LASTUPDATED		"LastUpdated"
#define LCP			"LCP"
#define LINK			"Link"
#define LINKLOCAL		"LinkLocal"
#define LOCALHOSTNAME		"LocalHostName"
#define LOGFILE			"Logfile"
#define MACADDRESS		"MACAddress"
#define MANUAL			"Manual"
#define MEDIA			"Media"
#define OPTIONS			"Options"
#define MODEM			"Modem"
#define MRU			"MRU"
#define MSCHAP1			"MSCHAP1"
#define MSCHAP2			"MSCHAP2"
#define MTU			"MTU"
#define NAME			"Name"
#define NETINFO			"NetInfo"
#define NETWORK			"Network"
#define NETWORKID		"NetworkID"
#define NETWORKRANGE		"NetworkRange"
#define NETWORKSERVICES		"NetworkServices"
#define NIS			"NIS"
#define NODE			"Node"
#define NODEID			"NodeID"
#define NOTE			"Note"
#define OVERRIDEPRIMARY		"OverridePrimary"
#define PAP			"PAP"
#define PASSWORD		"Password"
#define PLUGIN			"Plugin"
#define PLUGINS			"Plugins"
#define POWERENABLED		"PowerEnabled"
#define PPP			"PPP"
#define PPPOE			"PPPoE"
#define PPPOVERRIDEPRIMARY	"PPPOverridePrimary"
#define PPPSERIAL		"PPPSerial"
#define PPTP			"PPTP"
#define PREFERRED		"Preferred"
#define PREFERREDNETWORK	"PreferredNetwork"
#define PREFIXLENGTH		"PrefixLength"
#define PREFS			"Prefs"
#define PRIMARYINTERFACE	"PrimaryInterface"
#define PRIMARYSERVICE		"PrimaryService"
#define PROMPT			"Prompt"
#define PROTOCOL		"Protocol"
#define PROXIES			"Proxies"
#define PROXYAUTOCONFIGENABLE	"ProxyAutoConfigEnable"
#define PROXYAUTOCONFIGURLSTRING	"ProxyAutoConfigURLString"
#define PULSEDIAL		"PulseDial"
#define RECEIVEACCM		"ReceiveACCM"
#define RECENT			"Recent"
#define REDIALCOUNT		"RedialCount"
#define REDIALENABLED		"RedialEnabled"
#define REDIALINTERVAL		"RedialInterval"
#define RELAY			"Relay"
#define REMINDER		"Reminder"
#define REMINDERTIME		"ReminderTime"
#define REMOTEADDRESS		"RemoteAddress"
#define RETRYCONNECTTIME	"RetryConnectTime"
#define ROOTSEPARATOR		"RootSeparator"
#define ROUTER			"Router"
#define ROUTERADVERTISEMENT	"RouterAdvertisement"
#define RTSPENABLE		"RTSPEnable"
#define RTSPPORT		"RTSPPort"
#define RTSPPROXY		"RTSPProxy"
#define SAVEPASSWORDS		"SavePasswords"
#define SEARCHDOMAINS		"SearchDomains"
#define SEEDNETWORKRANGE	"SeedNetworkRange"
#define SEEDROUTER		"SeedRouter"
#define SEEDZONES		"SeedZones"
#define SERVERADDRESSES		"ServerAddresses"
#define SERVERTAGS		"ServerTags"
#define SERVICE			"Service"
#define SERVICEIDS		"ServiceIDs"
#define SERVICEORDER		"ServiceOrder"
#define SESSIONTIMER		"SessionTimer"
#define SETS			"Sets"
#define SETUP			"Setup"
#define	SHAREDSECRET		"SharedSecret"
#define SOCKSENABLE		"SOCKSEnable"
#define SOCKSPORT		"SOCKSPort"
#define SOCKSPROXY		"SOCKSProxy"
#define SORTLIST		"SortList"
#define SPEAKER			"Speaker"
#define SPEED			"Speed"
#define STATE			"State"
#define STATUS			"Status"
#define STF			"6to4"
#define STRONGEST		"Strongest"
#define SUBNETMASKS		"SubnetMasks"
#define SUBTYPE			"SubType"
#define SUPPORTSMODEMONHOLD	"SupportsModemOnHold"
#define SYSTEM			"System"
#define TERMINALSCRIPT		"TerminalScript"
#define TRANSMITACCM		"TransmitACCM"
#define TRANSPORT		"Transport"
#define TYPE			"Type"
#define UID			"UID"
#define USERDEFINEDNAME		"UserDefinedName"
#define USE			"Use"
#define USERS			"Users"
#define VERBOSELOGGING		"VerboseLogging"
#define WAITFORDIALTONE		"WaitForDialTone"

struct {
    int				control;
    const char *		prefix;
    const char *		key;
    const char *		value;
    const char *		type;
} names[] = {
    { COMMENT, "/*\n * Reserved Keys\n */", NULL, NULL },
    { REGULAR, RESV, LINK,	"__LINK__", CFSTRING },
    { REGULAR, RESV, INACTIVE,	"__INACTIVE__", NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Generic Keys\n */", NULL },
    { DEFINE , PROP, INTERFACENAME, NULL, CFSTRING },
    { REGULAR, PROP, MACADDRESS, NULL, CFSTRING },
    { REGULAR, PROP, USERDEFINEDNAME, NULL, CFSTRING },
    { DEFINE , PROP, VERSION, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Preference Keys\n */", NULL },
    { REGULAR, PREF, CURRENTSET, NULL, CFSTRING },
    { REGULAR, PREF, NETWORKSERVICES, NULL, CFDICTIONARY },
    { REGULAR, PREF, SETS, NULL, CFDICTIONARY },
    { REGULAR, PREF, SYSTEM, NULL, CFDICTIONARY },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Component Keys\n */", NULL },
    { REGULAR, COMP, NETWORK, NULL, NULL },
    { REGULAR, COMP, SERVICE, NULL, NULL },
    { REGULAR, COMP, GLOBAL, NULL, NULL },
    { DEFINE , COMP, HOSTNAMES, NULL, NULL },
    { REGULAR, COMP, INTERFACE, NULL, NULL },
    { REGULAR, COMP, SYSTEM, NULL, NULL },
    { REGULAR, COMP, USERS, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Regex key which matches any component\n */", NULL },
    { REGULAR, COMP, ANYREGEX, "[^/]+", NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Network Entity Keys\n */", NULL },
    { REGULAR, NETENT, AIRPORT, NULL, CFDICTIONARY },
    { REGULAR, NETENT, APPLETALK, NULL, CFDICTIONARY },
    { DEFINE , NETENT, DHCP, NULL, CFDICTIONARY },
    { REGULAR, NETENT, DNS, NULL, CFDICTIONARY },
    { REGULAR, NETENT, ETHERNET, NULL, CFDICTIONARY },
    { DEFINE , NETENT, FIREWIRE, NULL, CFDICTIONARY },
    { REGULAR, NETENT, INTERFACE, NULL, CFDICTIONARY },
    { REGULAR, NETENT, IPV4, NULL, CFDICTIONARY },
    { REGULAR, NETENT, IPV6, NULL, CFDICTIONARY },
    { DEFINE , NETENT, L2TP, NULL, CFDICTIONARY },
    { REGULAR, NETENT, LINK, NULL, CFDICTIONARY },
    { REGULAR, NETENT, MODEM, NULL, CFDICTIONARY },
    { REGULAR, NETENT, NETINFO, NULL, CFDICTIONARY },
    { FUTURE , NETENT, NIS, NULL, CFDICTIONARY },
    { REGULAR, NETENT, PPP, NULL, CFDICTIONARY },
    { REGULAR, NETENT, PPPOE, NULL, CFDICTIONARY },
    { DEFINE , NETENT, PPPSERIAL, NULL, CFDICTIONARY },
    { DEFINE , NETENT, PPTP, NULL, CFDICTIONARY },
    { REGULAR, NETENT, PROXIES, NULL, CFDICTIONARY },
    { DEFINE , NETENT, STF, NULL, CFDICTIONARY },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX COMP NETWORK " Properties\n */", NULL },
    { DEFINE , NETPROP, OVERRIDEPRIMARY, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP, SERVICEORDER, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP, PPPOVERRIDEPRIMARY, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX COMP NETWORK INTERFACE " Properties\n */", NULL },
    { DEFINE , NETPROP, INTERFACES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX COMP NETWORK HOSTNAMES " Properties\n */", NULL },
    { DEFINE , NETPROP, LOCALHOSTNAME, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT AIRPORT " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { DEFINE , NETPROP AIRPORT, ALLOWNETCREATION, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP AIRPORT, AUTH PASSWORD, NULL, CFDATA },
    { REGULAR, NETPROP AIRPORT, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { DEFINE , NETPROP AIRPORT, JOINMODE, NULL, CFSTRING },
    { REGULAR, NETPROP AIRPORT, POWERENABLED, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP AIRPORT, PREFERREDNETWORK, NULL, CFSTRING },
    { DEFINE , NETPROP AIRPORT, SAVEPASSWORDS, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP AIRPORT JOINMODE " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL AIRPORT JOINMODE, AUTOMATIC, NULL, NULL },
    { DEFINE , NETVAL AIRPORT JOINMODE, PREFERRED, NULL, NULL },
    { DEFINE , NETVAL AIRPORT JOINMODE, RECENT, NULL, NULL },
    { DEFINE , NETVAL AIRPORT JOINMODE, STRONGEST, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP AIRPORT PASSWORD ENCRYPTION " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL AIRPORT AUTH PASSWORD ENCRYPTION, KEYCHAIN, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT APPLETALK " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP APPLETALK, COMPUTERNAME, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, CONFIGMETHOD, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, DEFAULTZONE, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, NETWORKID, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, NETWORKRANGE, NULL, CFARRAY_CFNUMBER },
    { REGULAR, NETPROP APPLETALK, NODEID, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, SEEDNETWORKRANGE, NULL, CFARRAY_CFNUMBER },
    { REGULAR, NETPROP APPLETALK, SEEDZONES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP APPLETALK CONFIGMETHOD " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL APPLETALK CONFIGMETHOD, NODE, NULL, NULL },
    { REGULAR, NETVAL APPLETALK CONFIGMETHOD, ROUTER, NULL, NULL },
    { REGULAR, NETVAL APPLETALK CONFIGMETHOD, SEEDROUTER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT DNS " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP DNS, DOMAINNAME, NULL, CFSTRING },
    { REGULAR, NETPROP DNS, SEARCHDOMAINS, NULL, CFARRAY_CFSTRING},
    { REGULAR, NETPROP DNS, SERVERADDRESSES, NULL, CFARRAY_CFSTRING },
    { DEFINE , NETPROP DNS, SORTLIST, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT ETHERNET " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { DEFINE , NETPROP ETHERNET, MEDIA SUBTYPE, NULL, CFSTRING },
    { DEFINE , NETPROP ETHERNET, MEDIA OPTIONS, NULL, CFARRAY_CFSTRING },
    { DEFINE , NETPROP ETHERNET, MTU, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT FIREWIRE " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT INTERFACE " Entity Keys\n */", NULL },
    { REGULAR, NETPROP INTERFACE, DEVICENAME, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, HARDWARE, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, TYPE, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, SUBTYPE, NULL, CFSTRING },
    { DEFINE , NETPROP INTERFACE, SUPPORTSMODEMONHOLD, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP INTERFACE TYPE " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL INTERFACE TYPE, ETHERNET, NULL, NULL },
    { DEFINE , NETVAL INTERFACE TYPE, FIREWIRE, NULL, NULL },
    { REGULAR, NETVAL INTERFACE TYPE, PPP, NULL, NULL },
    { DEFINE , NETVAL INTERFACE TYPE, STF, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP SERVICE SUBTYPE " values (for " PPP ") */", NULL, NULL, NULL },
    { REGULAR, NETVAL INTERFACE SUBTYPE, PPPOE, NULL, NULL },
    { REGULAR, NETVAL INTERFACE SUBTYPE, PPPSERIAL, NULL, NULL },
    { DEFINE , NETVAL INTERFACE SUBTYPE, PPTP, NULL, NULL },
    { DEFINE , NETVAL INTERFACE SUBTYPE, L2TP, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT IPV4 " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP IPV4, ADDRESSES, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP IPV4, CONFIGMETHOD, NULL, CFSTRING },
    { REGULAR, NETPROP IPV4, DHCPCLIENTID, NULL, CFSTRING },
    { REGULAR, NETPROP IPV4, ROUTER, NULL, CFSTRING },
    { REGULAR, NETPROP IPV4, SUBNETMASKS, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP IPV4, DESTADDRESSES, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP IPV4, BROADCASTADDRESSES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP IPV4 CONFIGMETHOD " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, BOOTP, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, DHCP, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, INFORM, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, LINKLOCAL, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, MANUAL, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, PPP, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT IPV6 " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP IPV6, ADDRESSES, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP IPV6, CONFIGMETHOD, NULL, CFSTRING },
    { DEFINE , NETPROP IPV6, DESTADDRESSES, NULL, CFARRAY_CFSTRING },
    { DEFINE , NETPROP IPV6, FLAGS, NULL, CFNUMBER },
    { DEFINE , NETPROP IPV6, PREFIXLENGTH, NULL, CFARRAY_CFNUMBER },
    { DEFINE , NETPROP IPV6, ROUTER, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP IPV6 CONFIGMETHOD " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL IPV6 CONFIGMETHOD, AUTOMATIC, NULL, NULL },
    { DEFINE , NETVAL IPV6 CONFIGMETHOD, MANUAL, NULL, NULL },
    { DEFINE , NETVAL IPV6 CONFIGMETHOD, ROUTERADVERTISEMENT, NULL, NULL },
    { DEFINE , NETVAL IPV6 CONFIGMETHOD, STF, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT STF " Entity Keys\n */", NULL, NULL, NULL },
    { DEFINE , NETPROP STF, RELAY, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT LINK " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP LINK, ACTIVE, NULL, CFBOOLEAN },
    { DEFINE , NETPROP LINK, DETACHING, NULL, CFBOOLEAN },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT MODEM " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP MODEM, CONNECTIONSCRIPT, NULL, CFSTRING },
    { DEFINE , NETPROP MODEM, CONNECTSPEED, NULL, CFNUMBER },
    { DEFINE , NETPROP MODEM, DATACOMPRESSION, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, DIALMODE, NULL, CFSTRING },
    { DEFINE , NETPROP MODEM, ERRORCORRECTION, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP MODEM, HOLD CALLWAITINGAUDIBLEALERT, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP MODEM, HOLD DISCONNECTONANSWER, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP MODEM, HOLD ENABLED, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP MODEM, HOLD REMINDER, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP MODEM, HOLD REMINDERTIME, NULL, CFNUMBER },
    { DEFINE , NETPROP MODEM, NOTE, NULL, CFSTRING },
    { REGULAR, NETPROP MODEM, PULSEDIAL, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, SPEAKER, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, SPEED, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP MODEM DIALMODE " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL MODEM DIALMODE, IGNOREDIALTONE, NULL, NULL },
    { REGULAR, NETVAL MODEM DIALMODE, MANUAL, NULL, NULL },
    { REGULAR, NETVAL MODEM DIALMODE, WAITFORDIALTONE, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT NETINFO " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP NETINFO, BINDINGMETHODS, NULL, CFSTRING },
    { REGULAR, NETPROP NETINFO, SERVERADDRESSES, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP NETINFO, SERVERTAGS, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP NETINFO, BROADCASTSERVERTAG, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP NETINFO BINDINGMETHODS " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL NETINFO BINDINGMETHODS, BROADCAST, NULL, NULL },
    { REGULAR, NETVAL NETINFO BINDINGMETHODS, DHCP, NULL, NULL },
    { REGULAR, NETVAL NETINFO BINDINGMETHODS, MANUAL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP NETINFO BROADCASTSERVERTAG " default value */", NULL, NULL, NULL },
    { REGULAR, NETVAL NETINFO, DEFAULTSERVERTAG, "network", NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT NIS " Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT PPP " Entity Keys\n */", NULL, NULL, NULL },
    { DEFINE , NETPROP PPP, ACSPENABLED, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP PPP, CONNECTTIME, NULL, CFNUMBER },
    { DEFINE , NETPROP PPP, DEVICE LASTCAUSE, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, DIALONDEMAND, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, DISCONNECTONIDLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, DISCONNECTONIDLETIMER, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, DISCONNECTONLOGOUT, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP PPP, DISCONNECTONSLEEP, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP PPP, DISCONNECTTIME, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, IDLEREMINDERTIMER, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, IDLEREMINDER, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP PPP, LASTCAUSE, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LOGFILE, NULL, CFSTRING },
    { DEFINE , NETPROP PPP, PLUGINS, NULL, CFARRAY_CFSTRING },
    { DEFINE , NETPROP PPP, RETRYCONNECTTIME, NULL, CFNUMBER },
    { DEFINE , NETPROP PPP, SESSIONTIMER, NULL, CFNUMBER },
    { DEFINE , NETPROP PPP, STATUS, NULL, CFNUMBER },
    { DEFINE , NETPROP PPP, USE SESSIONTIMER, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, VERBOSELOGGING, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* " AUTH ": */", NULL, NULL, NULL },
    { DEFINE , NETPROP PPP, AUTH EAP PLUGINS, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP PPP, AUTH NAME, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PASSWORD, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { DEFINE , NETPROP PPP, AUTH PROMPT, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PROTOCOL, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP PPP AUTH PASSWORD ENCRYPTION " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL PPP AUTH PASSWORD ENCRYPTION, KEYCHAIN, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP PPP AUTH PROMPT " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL PPP AUTH PROMPT, BEFORE, NULL, CFSTRING },
    { DEFINE , NETVAL PPP AUTH PROMPT, AFTER, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP PPP AUTH PROTOCOL " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL PPP AUTH PROTOCOL, CHAP, NULL, CFSTRING },
    { DEFINE , NETVAL PPP AUTH PROTOCOL, EAP, NULL, CFSTRING },
    { DEFINE , NETVAL PPP AUTH PROTOCOL, MSCHAP1, NULL, CFSTRING },
    { DEFINE , NETVAL PPP AUTH PROTOCOL, MSCHAP2, NULL, CFSTRING },
    { REGULAR, NETVAL PPP AUTH PROTOCOL, PAP, NULL, CFSTRING },

    { COMMENT, "\n/* " COMM ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, COMM ALTERNATEREMOTEADDRESS, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, COMM CONNECTDELAY, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM DISPLAYTERMINALWINDOW, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, COMM REDIALCOUNT, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM REDIALENABLED, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, COMM REDIALINTERVAL, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM REMOTEADDRESS, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, COMM TERMINALSCRIPT, NULL, CFSTRING },
    { DEFINE , NETPROP PPP, COMM USE TERMINALSCRIPT, NULL, CFNUMBER_BOOL },

    { COMMENT, "\n/* " CCP ": */", NULL, NULL, NULL },
    { DEFINE , NETPROP PPP, CCP ENABLED, NULL, CFNUMBER_BOOL },

    { COMMENT, "\n/* " IPCP ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, IPCP COMPRESSIONVJ, NULL, CFNUMBER_BOOL },

    { COMMENT, "\n/* " LCP ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, LCP ECHOENABLED, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LCP ECHOFAILURE, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP ECHOINTERVAL, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP COMPRESSIONACFIELD, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LCP COMPRESSIONPFIELD, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LCP MRU, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP MTU, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP RECEIVEACCM, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP TRANSMITACCM, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT PPPOE " Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT PPPSERIAL " Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT PPTP " Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT L2TP " Entity Keys\n */", NULL, NULL, NULL },
    { DEFINE , NETPROP L2TP, IPSEC SHAREDSECRET, NULL, CFSTRING },
    { DEFINE , NETPROP L2TP, IPSEC SHAREDSECRET ENCRYPTION, NULL, CFSTRING },
    { DEFINE , NETPROP L2TP, TRANSPORT, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP L2TP IPSEC SHAREDSECRET ENCRYPTION " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL L2TP IPSEC SHAREDSECRET ENCRYPTION, KEYCHAIN, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP L2TP TRANSPORT " values */", NULL, NULL, NULL },
    { DEFINE , NETVAL L2TP TRANSPORT, IP, NULL, NULL },
    { DEFINE , NETVAL L2TP TRANSPORT, IPSEC, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT PROXIES " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP PROXIES, EXCEPTIONSLIST, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP PROXIES, FTPENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, FTPPASSIVE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, FTPPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, FTPPROXY, NULL, CFSTRING },
    { REGULAR, NETPROP PROXIES, GOPHERENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, GOPHERPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, GOPHERPROXY, NULL, CFSTRING },
    { REGULAR, NETPROP PROXIES, HTTPENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, HTTPPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, HTTPPROXY, NULL, CFSTRING },
    { DEFINE , NETPROP PROXIES, HTTPSENABLE, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP PROXIES, HTTPSPORT, NULL, CFNUMBER },
    { DEFINE , NETPROP PROXIES, HTTPSPROXY, NULL, CFSTRING },
    { REGULAR, NETPROP PROXIES, RTSPENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, RTSPPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, RTSPPROXY, NULL, CFSTRING },
    { REGULAR, NETPROP PROXIES, SOCKSENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, SOCKSPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, SOCKSPROXY, NULL, CFSTRING },
    { DEFINE , NETPROP PROXIES, PROXYAUTOCONFIGENABLE, NULL, CFNUMBER_BOOL },
    { DEFINE , NETPROP PROXIES, PROXYAUTOCONFIGURLSTRING, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n " KEY_PREFIX COMP USERS " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, USERSENT, CONSOLEUSER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX COMP SYSTEM " Properties\n */", NULL, NULL, NULL },
    { REGULAR, SYSTEMPROP, COMPUTERNAME, NULL, CFSTRING },
    { REGULAR, SYSTEMPROP, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Configuration Store Definitions\n */", NULL },
    { COMMENT, "/* domain prefixes */", NULL },
    { DEFINE , DYNAMICSTORE DOMAIN, FILE, "File:", NULL },
    { DEFINE , DYNAMICSTORE DOMAIN, PLUGIN, "Plugin:", NULL },
    { DEFINE , DYNAMICSTORE DOMAIN, SETUP, "Setup:", NULL },
    { DEFINE , DYNAMICSTORE DOMAIN, STATE, "State:", NULL },
    { DEFINE , DYNAMICSTORE DOMAIN, PREFS, "Prefs:", NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* " KEY_PREFIX DYNAMICSTORE DOMAIN SETUP " Properties */", NULL },
    { DEFINE , DYNAMICSTORE SETUPPROP, CURRENTSET, NULL, CFSTRING },
    { DEFINE , DYNAMICSTORE SETUPPROP, LASTUPDATED, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* Properties */", NULL },
    { DEFINE , DYNAMICSTORE NETPROP, INTERFACES, NULL, CFARRAY_CFSTRING },
    { DEFINE , DYNAMICSTORE NETPROP, PRIMARYINTERFACE, NULL, CFSTRING },
    { DEFINE , DYNAMICSTORE NETPROP, PRIMARYSERVICE, NULL, CFSTRING },
    { DEFINE , DYNAMICSTORE NETPROP, SERVICEIDS, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Obsolete schema definitions which will be removed \"soon\".\n */", NULL },
    { OBSOLETE, USERSPROP CONSOLEUSER, NAME, NULL, CFSTRING },
    { OBSOLETE, USERSPROP CONSOLEUSER, UID, NULL, CFNUMBER },
    { OBSOLETE, USERSPROP CONSOLEUSER, GID, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

    { END, NULL, NULL, NULL, NULL },
};

static inline void
setmax(int *max, char **maxstr, char *str)
{
    int l;

    l = strlen(str);
    if (l > *max) {
	if (*maxstr) free(*maxstr);
	*maxstr = strdup(str);
	*max = l;
    }
    return;
}

enum {
    gen_header_e,
    gen_hfile_e,
    gen_cfile_e,
};

void
dump_names(int type)
{
    int i;
    int maxkbuf = 0;
    char *maxkstr = NULL;
    int maxvbuf = 0;
    char *maxvstr = NULL;

    for (i = 0; TRUE; i++) {
	switch (names[i].control) {
	    case END: {
		goto done;
		break;
	    }
	    case COMMENT: {
		switch (type) {
		case gen_header_e:
		case gen_hfile_e:
		    if (names[i].prefix)
			printf("%s\n", names[i].prefix);
		    break;
		default:
		    break;
		}
		break;
	    }
	    case DEFINE: {
		char kbuf[256];
		char vbuf[256];

		switch (type) {
		case gen_header_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    if (names[i].value)
			snprintf(vbuf, sizeof(vbuf), "SCSTR(\"%s\")",
				 names[i].value);
		    else
			snprintf(vbuf, sizeof(vbuf), "SCSTR(\"%s\")",
				 names[i].key);

		    if (names[i].type)
			printf("#define %-40s %-40s /* %s */\n",
			       kbuf, vbuf, names[i].type);
		    else
			printf("#define %-40s %-40s\n",
			       kbuf, vbuf);
		    break;
		case gen_hfile_e:
		    snprintf(kbuf, sizeof(kbuf), "(" KEY_PREFIX "%s%s);",
			     names[i].prefix, names[i].key);
		    setmax(&maxkbuf, &maxkstr, kbuf);

		    snprintf(vbuf, sizeof(vbuf), "\"%s\"",
			     names[i].value ? names[i].value : names[i].key);
		    setmax(&maxvbuf, &maxvstr, vbuf);

		    printf("SC_SCHEMA_DECLARATION%-42s /* %-17s %-30s */\n",
			   kbuf,
			   names[i].type ? names[i].type : "",
			   vbuf);
		    break;
		case gen_cfile_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    if (names[i].value)
			printf("const CFStringRef %-45s = CFSTR(\"%s\");\n",
			       kbuf, names[i].value);
		    else
			printf("const CFStringRef %-45s = CFSTR(\"%s\");\n",
			       kbuf, names[i].key);
		    break;
		default:
		    break;
		}
		break;
	    }
	    case REGULAR: {
		char kbuf[256];
		char vbuf[256];

		switch (type) {
		case gen_header_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    if (names[i].value)
			snprintf(vbuf, sizeof(vbuf), "SCSTR(\"%s\")",
				 names[i].value);
		    else
			snprintf(vbuf, sizeof(vbuf), "SCSTR(\"%s\")",
				 names[i].key);

		    if (names[i].type)
			printf("#define %-40s %-40s /* %s */\n",
			       kbuf, vbuf, names[i].type);
		    else
			printf("#define %-40s %-40s\n",
			       kbuf, vbuf);
		    break;
		case gen_hfile_e:
		    snprintf(kbuf, sizeof(kbuf), "(" KEY_PREFIX "%s%s);",
			     names[i].prefix, names[i].key);
		    setmax(&maxkbuf, &maxkstr, kbuf);

		    snprintf(vbuf, sizeof(vbuf), "\"%s\"",
			     names[i].value ? names[i].value : names[i].key);
		    setmax(&maxvbuf, &maxvstr, vbuf);

		    printf("SC_SCHEMA_DECLARATION%-42s /* %-17s %-30s */\n",
			   kbuf,
			   names[i].type ? names[i].type : "",
			   vbuf);
		    break;
		case gen_cfile_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    if (names[i].value)
			printf("const CFStringRef %-45s = CFSTR(\"%s\");\n",
			       kbuf, names[i].value);
		    else
			printf("const CFStringRef %-45s = CFSTR(\"%s\");\n",
			       kbuf, names[i].key);
		    break;
		default:
		    break;
		}
		break;
	    }
	    case OBSOLETE: {
		static int nObsolete = 0;
		char kbuf[256];
		char vbuf[256];

		switch (type) {
		case gen_hfile_e:
		    if (nObsolete++ == 0) {
			printf("#ifndef  SCSTR\n");
			printf("#include <CoreFoundation/CFString.h>\n");
			printf("#define  SCSTR(s) CFSTR(s)\n");
			printf("#endif\n");
		    }

		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    if (names[i].value)
			snprintf(vbuf, sizeof(vbuf), "SCSTR(\"%s\")",
				 names[i].value);
		    else
			snprintf(vbuf, sizeof(vbuf), "SCSTR(\"%s\")",
				 names[i].key);

		    printf("#define %-40s %-40s /* %s */\n",
			   kbuf,
			   vbuf,
			   names[i].type ? names[i].type : "");
		    break;
		default:
		    break;
		}
		break;
	    }
	    case FUTURE: {
		char kbuf[256];

		switch (type) {
		case gen_header_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    printf("// #define %-37s %-40s /* %s */\n",
			   kbuf,
			   "SCSTR(\"???\") */",
			   "RESERVED FOR FUTURE USE");
		    break;
		case gen_hfile_e:
		    snprintf(kbuf, sizeof(kbuf), "(" KEY_PREFIX "%s%s);",
			     names[i].prefix, names[i].key);
		    setmax(&maxkbuf, &maxkstr, kbuf);

		    printf("// SC_SCHEMA_DECLARATION%-39s /* %s */\n",
			   kbuf, "RESERVED FOR FUTURE USE");
		    break;
		default:
		    break;
		}
		break;
	    }
	    default: {
		break;
	    }
	}
    }
 done:
    switch (type) {
    case gen_hfile_e:
	fprintf(stderr, "max key: length = %2d, string = %s\n", maxkbuf, maxkstr);
	fprintf(stderr, "max val: length = %2d, string = %s\n", maxvbuf, maxvstr);
	break;
    }
    return;
}

int
main(int argc, char * argv[])
{
    char * type = "";

    if (argc >= 2)
	type = argv[1];

    if (strcmp(type, "header-x") == 0) {
	printf("%s\n", copyright_string);
	printf("/*\n * This file is automatically generated\n * DO NOT EDIT!\n */\n\n");

	printf("/*\n");
	printf(" * Note: For Cocoa/Obj-C/Foundation programs accessing these preference\n");
	printf(" *       keys you may want to consider the following:\n");
	printf(" *\n");
	printf(" *       #define SCSTR(s) (NSString *)CFSTR(s)\n");
	printf(" *       #import <SystemConfiguration/SystemConfiguration.h>\n");
	printf(" */\n\n");

	printf("#ifndef _SCSCHEMADEFINITIONS_10_1_H\n#define _SCSCHEMADEFINITIONS_10_1_H\n\n");

	printf("#ifndef  SCSTR\n");
	printf("#include <CoreFoundation/CFString.h>\n");
	printf("#define  SCSTR(s) CFSTR(s)\n");
	printf("#endif\n");

	printf("\n");
	dump_names(gen_header_e);
	printf("#endif /* _SCSCHEMADEFINITIONS_10_1_H */\n");
    }
    else if (strcmp(type, "header") == 0) {
	printf("%s\n", copyright_string);
	printf("/*\n * This file is automatically generated\n * DO NOT EDIT!\n */\n\n");

	printf("/*\n");
	printf(" * Note: For Cocoa/Obj-C/Foundation programs accessing these preference\n");
	printf(" *       keys you may want to consider the following:\n");
	printf(" *\n");
	printf(" *       #define " SC_SCHEMA_DECLARATION "(x)\t\textern NSString * x\n");
	printf(" *       #import <SystemConfiguration/SystemConfiguration.h>\n");
	printf(" */\n\n");

	printf("#ifndef _SCSCHEMADEFINITIONS_H\n#define _SCSCHEMADEFINITIONS_H\n\n");

	printf("#ifndef SC_SCHEMA_DECLARATION\n");
	printf("#ifndef SCSTR\n");
	printf("#include <CoreFoundation/CFString.h>\n");
	printf("#define " SC_SCHEMA_DECLARATION "(x)\textern const CFStringRef x\n");
	printf("#else\n");
	printf("#import <Foundation/NSString.h>\n");
	printf("#define " SC_SCHEMA_DECLARATION "(x)\textern NSString * x\n");
	printf("#endif\n");
	printf("#endif\n");

	printf("\n");
	dump_names(gen_hfile_e);

	printf("#include <AvailabilityMacros.h>\n");
	printf("#if MAC_OS_X_VERSION_10_3 > MAC_OS_X_VERSION_MIN_REQUIRED\n");
	printf("  #if MAC_OS_X_VERSION_10_1 <= MAC_OS_X_VERSION_MIN_REQUIRED\n");
	printf("    #include <SystemConfiguration/SCSchemaDefinitions_10_1.h>\n");
	printf("  #endif\n");
	printf("#endif\n\n");

	printf("#endif /* _SCSCHEMADEFINITIONS_H */\n");
    }
    else if (strcmp(type, "cfile") == 0) {
	printf("/*\n");
	printf(" * This file is automatically generated\n");
	printf(" * DO NOT EDIT!\n");
	printf(" */\n");
	printf("\n");
	printf("#include <CoreFoundation/CFString.h>\n");
	printf("\n");
	dump_names(gen_cfile_e);
    }
    exit(0);
    return (0);
}

