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
 * genSCPreferences.c
 * - generates System Configuration header/cfile
 * - invoke with "header" to generate the header
 * - invoke with "cfile" to generate the cfile
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
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
#include <unistd.h>
#include <mach/boolean.h>

char copyright_string[] =
"/*\n"
" * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.\n"
" *\n"
" * @APPLE_LICENSE_HEADER_START@\n"
" * \n"
" * The contents of this file constitute Original Code as defined in and\n"
" * are subject to the Apple Public Source License Version 1.1 (the\n"
" * \"License\").  You may not use this file except in compliance with the\n"
" * License.  Please obtain a copy of the License at\n"
" * http://www.apple.com/publicsource and read it before using this file.\n"
" * \n"
" * This Original Code and all software distributed under the License are\n"
" * distributed on an \"AS IS\" basis, WITHOUT WARRANTY OF ANY KIND, EITHER\n"
" * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,\n"
" * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,\n"
" * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the\n"
" * License for the specific language governing rights and limitations\n"
" * under the License.\n"
" * \n"
" * @APPLE_LICENSE_HEADER_END@\n"
" */\n";


typedef enum {
	COMMENT,
	OBSOLETE,
	REGULAR,
	DEFINE ,
	FUTURE,
	END
} controlType;

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
#define CFDICTIONARY		"CFDictionary"
#define CFNUMBER		"CFNumber"
#define CFNUMBER_BOOL		"CFNumber (0 or 1)"
#define CFSTRING		"CFString"

#define ACTIVE			"Active"
#define ADDRESSES		"Addresses"
#define AIRPORT			"AirPort"
#define ALERT			"Alert"
#define ALTERNATEREMOTEADDRESS	"AlternateRemoteAddress"
#define ANYREGEX		"AnyRegex"
#define APPLETALK		"AppleTalk"
#define AUTH			"Auth"
#define AUTOMATIC		"Automatic"
#define BINDINGMETHODS		"BindingMethods"
#define BOOTP			"BOOTP"
#define BROADCAST		"Broadcast"
#define BROADCASTADDRESSES	"BroadcastAddresses"
#define BROADCASTSERVERTAG	"BroadcastServerTag"
#define CHAP			"CHAP"
#define COMM			"Comm"
#define COMPRESSIONACFIELD	"CompressionACField"
#define COMPRESSIONPFIELD	"CompressionPField"
#define COMPRESSIONVJ		"CompressionVJ"
#define COMPUTERNAME		"ComputerName"
#define CONFIGMETHOD		"ConfigMethod"
#define CONNECTDELAY		"ConnectDelay"
#define CONNECTIONSCRIPT	"ConnectionScript"
#define CONSOLEUSER		"ConsoleUser"
#define CURRENTSET		"CurrentSet"
#define DATACOMPRESSION		"DataCompression"
#define DEFAULTSERVERTAG	"DefaultServerTag"
#define DEFAULTZONE		"DefaultZone"
#define DESTADDRESSES		"DestAddresses"
#define DEVICENAME		"DeviceName"
#define DHCP			"DHCP"
#define DHCPCLIENTID		"DHCPClientID"
#define DIALMODE		"DialMode"
#define DIALONDEMAND		"DialOnDemand"
#define DISCONNECTONIDLE	"DisconnectOnIdle"
#define DISCONNECTONIDLETIMER	"DisconnectOnIdleTimer"
#define DISCONNECTONLOGOUT	"DisconnectOnLogout"
#define DISPLAYTERMINALWINDOW	"DisplayTerminalWindow"
#define DNS			"DNS"
#define DOMAIN 			"Domain"
#define DOMAINNAME		"DomainName"
#define DOMAINSEPARATOR		"DomainSeparator"
#define ECHOENABLED		"EchoEnabled"
#define ECHOFAILURE		"EchoFailure"
#define ECHOINTERVAL		"EchoInterval"
#define ENCODING		"Encoding"
#define ENCRYPTION		"Encryption"
#define ERRORCORRECTION		"ErrorCorrection"
#define ETHERNET		"Ethernet"
#define EXCEPTIONSLIST		"ExceptionsList"
#define FILE			"File"
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
#define IPCP			"IPCP"
#define IPV4			"IPv4"
#define IPV6			"IPv6"
#define LASTUPDATED		"LastUpdated"
#define LCP			"LCP"
#define LINK			"Link"
#define LOGFILE			"Logfile"
#define MACADDRESS		"MACAddress"
#define MANUAL			"Manual"
#define MEDIA			"Media"
#define MODEM			"Modem"
#define MRU			"MRU"
#define MTU			"MTU"
#define NAME			"Name"
#define NETINFO			"NetInfo"
#define NETWORK			"Network"
#define NETWORKID		"NetworkID"
#define NETWORKSERVICES		"NetworkServices"
#define NIS			"NIS"
#define NODE			"Node"
#define NODEID			"NodeID"
#define PAP			"PAP"
#define PASSWORD		"Password"
#define PLUGIN			"Plugin"
#define POWERENABLED		"PowerEnabled"
#define PPP			"PPP"
#define PPPOE			"PPPoE"
#define PPPOVERRIDEPRIMARY	"PPPOverridePrimary"
#define PPPSERIAL		"PPPSerial"
#define PREFERREDNETWORK	"PreferredNetwork"
#define PREFS			"Prefs"
#define PRIMARYINTERFACE	"PrimaryInterface"
#define PRIMARYSERVICE		"PrimaryService"
#define PROTOCOL		"Protocol"
#define PROXIES			"Proxies"
#define PULSEDIAL		"PulseDial"
#define RECEIVEACCM		"ReceiveACCM"
#define REDIALCOUNT		"RedialCount"
#define REDIALENABLED		"RedialEnabled"
#define REDIALINTERVAL		"RedialInterval"
#define REMOTEADDRESS		"RemoteAddress"
#define ROOTSEPARATOR		"RootSeparator"
#define ROUTER			"Router"
#define RTSPENABLE		"RTSPEnable"
#define RTSPPORT		"RTSPPort"
#define RTSPPROXY		"RTSPProxy"
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
#define SOCKSENABLE		"SOCKSEnable"
#define SOCKSPORT		"SOCKSPort"
#define SOCKSPROXY		"SOCKSProxy"
#define SORTLIST		"SortList"
#define SPEAKER			"Speaker"
#define SPEED			"Speed"
#define STATE			"State"
#define SUBNETMASKS		"SubnetMasks"
#define SUBTYPE			"SubType"
#define SYSTEM			"System"
#define TERMINALSCRIPT		"TerminalScript"
#define TRANSMITACCM		"TransmitACCM"
#define TYPE			"Type"
#define UID			"UID"
#define USERDEFINEDNAME		"UserDefinedName"
#define USERS			"Users"
#define VERBOSELOGGING		"VerboseLogging"
#define WAITFORDIALTONE		"WaitForDialTone"

struct {
    int				control;
    unsigned char *		prefix;
    unsigned char *		key;
    unsigned char *		value;
    unsigned char *		type;
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
    { REGULAR, NETENT, INTERFACE, NULL, CFDICTIONARY },
    { REGULAR, NETENT, IPV4, NULL, CFDICTIONARY },
    { REGULAR, NETENT, IPV6, NULL, CFDICTIONARY },
    { REGULAR, NETENT, LINK, NULL, CFDICTIONARY },
    { REGULAR, NETENT, MODEM, NULL, CFDICTIONARY },
    { REGULAR, NETENT, NETINFO, NULL, CFDICTIONARY },
    { FUTURE , NETENT, NIS, NULL, CFDICTIONARY },
    { REGULAR, NETENT, PPP, NULL, CFDICTIONARY },
    { REGULAR, NETENT, PPPOE, NULL, CFDICTIONARY },
    { REGULAR, NETENT, PROXIES, NULL, CFDICTIONARY },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX COMP NETWORK " Properties\n */", NULL },
    { REGULAR, NETPROP, SERVICEORDER, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP, PPPOVERRIDEPRIMARY, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT AIRPORT " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP AIRPORT, POWERENABLED, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP AIRPORT, AUTH PASSWORD, NULL, CFSTRING },
    { REGULAR, NETPROP AIRPORT, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { REGULAR, NETPROP AIRPORT, PREFERREDNETWORK, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT APPLETALK " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP APPLETALK, COMPUTERNAME, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, CONFIGMETHOD, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, DEFAULTZONE, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, NETWORKID, NULL, CFNUMBER },
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
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT INTERFACE " Entity Keys\n */", NULL },
    { REGULAR, NETPROP INTERFACE, DEVICENAME, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, HARDWARE, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, TYPE, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, SUBTYPE, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP INTERFACE TYPE " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL INTERFACE TYPE, ETHERNET, NULL, NULL },
    { REGULAR, NETVAL INTERFACE TYPE, PPP, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP SERVICE SUBTYPE " values (for " PPP ") */", NULL, NULL, NULL },
    { REGULAR, NETVAL INTERFACE SUBTYPE, PPPOE, NULL, NULL },
    { REGULAR, NETVAL INTERFACE SUBTYPE, PPPSERIAL, NULL, NULL },
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
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, MANUAL, NULL, NULL },
    { REGULAR, NETVAL IPV4 CONFIGMETHOD, PPP, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT IPV6 " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP IPV6, ADDRESSES, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP IPV6, CONFIGMETHOD, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT LINK " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP LINK, ACTIVE, NULL, CFBOOLEAN },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " KEY_PREFIX NETENT MODEM " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP MODEM, CONNECTIONSCRIPT, NULL, CFSTRING },
    { DEFINE , NETPROP MODEM, DATACOMPRESSION, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, DIALMODE, NULL, CFSTRING },
    { DEFINE , NETPROP MODEM, ERRORCORRECTION, NULL, CFNUMBER_BOOL },
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
    { REGULAR, NETPROP PPP, DIALONDEMAND, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, DISCONNECTONIDLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, DISCONNECTONIDLETIMER, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, DISCONNECTONLOGOUT, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, IDLEREMINDERTIMER, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, IDLEREMINDER, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LOGFILE, NULL, CFSTRING },
    { DEFINE , NETPROP PPP, SESSIONTIMER, NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, VERBOSELOGGING, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* " AUTH ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, AUTH NAME, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PASSWORD, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PROTOCOL, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP PPP AUTH PROTOCOL " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL PPP AUTH PROTOCOL, CHAP, NULL, CFSTRING },
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
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n " KEY_PREFIX COMP USERS " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, USERSENT, CONSOLEUSER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n " KEY_PREFIX USERSPROP CONSOLEUSER " Properties\n */", NULL, NULL, NULL },
    { REGULAR, USERSPROP CONSOLEUSER, NAME, NULL, CFSTRING },
    { REGULAR, USERSPROP CONSOLEUSER, UID, NULL, CFSTRING },
    { REGULAR, USERSPROP CONSOLEUSER, GID, NULL, CFSTRING },
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

    /* obsolete keys */
    { OBSOLETE, "Cache" DOMAIN, FILE, "File:", NULL },
    { OBSOLETE, "Cache" DOMAIN, PLUGIN, "Plugin:", NULL },
    { OBSOLETE, "Cache" DOMAIN, SETUP, "Setup:", NULL },
    { OBSOLETE, "Cache" DOMAIN, STATE, "State:", NULL },
    { OBSOLETE, "Cache" DOMAIN, PREFS, "Prefs:", NULL },
    { OBSOLETE, "Cache" SETUPPROP, CURRENTSET, NULL, CFSTRING },
    { OBSOLETE, "Cache" SETUPPROP, LASTUPDATED, NULL, NULL },
    { OBSOLETE, "Cache" NETPROP, INTERFACES, NULL, CFARRAY_CFSTRING },
    { OBSOLETE, "Cache" NETPROP, PRIMARYINTERFACE, NULL, CFSTRING },
    { OBSOLETE, "Cache" NETPROP, SERVICEIDS, NULL, CFARRAY_CFSTRING },

    { END, NULL, NULL, NULL, NULL },
};

enum {
    gen_extern_e,
    gen_init_e,
    gen_header_e,
};

void
dump_names(int type)
{
    int i;

    for (i = 0; TRUE; i++) {
	switch (names[i].control) {
	    case END: {
		goto done;
		break;
	    }
	    case COMMENT: {
		if (type != gen_extern_e && type != gen_init_e) {
		    if (names[i].prefix)
			printf("%s\n", names[i].prefix);
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
		case gen_extern_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    printf("volatile CFStringRef " KEY_PREFIX "%s%s = NULL;\n",
			   names[i].prefix, names[i].key);
		    break;
		case gen_init_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);
		    if (names[i].value)
			printf("   *((void **)&%s) = (void *)CFSTR(\"%s\");\n",
			       kbuf, names[i].value);
		    else
			printf("   *((void **)&%s) = (void *)CFSTR(\"%s\");\n",
			       kbuf, names[i].key);
		    break;
		default:
		    break;
		}
		break;
	    }
	    case OBSOLETE: {
		char kbuf[256];

		switch (type) {
		case gen_extern_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    printf("volatile CFStringRef " KEY_PREFIX "%s%s = NULL;\n",
			   names[i].prefix, names[i].key);
		    break;
		case gen_init_e:
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);
		    if (names[i].value)
			printf("   *((void **)&%s) = (void *)CFSTR(\"%s\");\n",
			       kbuf, names[i].value);
		    else
			printf("   *((void **)&%s) = (void *)CFSTR(\"%s\");\n",
			       kbuf, names[i].key);
		    break;
		default:
		    break;
		}
		break;
	    }
	    case FUTURE: {
		char kbuf[256];

		if (type == gen_header_e) {
		    snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    printf("/* #define %-37s %-40s /* %s */\n",
			   kbuf,
			   "SCSTR(\"???\") */",
			   "RESERVED FOR FUTURE USE");
		}
		break;
	    }
	    default: {
		break;
	    }
	}
    }
 done:
    return;
}

int
main(int argc, char * argv[])
{
    char * type = "";

    if (argc >= 2)
	type = argv[1];

    if (strcmp(type, "header") == 0) {
	printf("%s\n", copyright_string);
	printf("/*\n * This file is automatically generated\n * DO NOT EDIT!\n */\n\n");

	printf("/*\n");
	printf(" * Note: For Cocoa/Obj-C/Foundation programs accessing these preference\n");
	printf(" *       keys you may want to consider the following:\n");
	printf(" *\n");
	printf(" *       #define SCSTR(s) (NSString *)CFSTR(s)\n");
	printf(" *       #import <SystemConfiguration/SystemConfiguration.h>\n");
	printf(" */\n\n");

	printf("#ifndef _SCSCHEMADEFINITIONS_H\n#define _SCSCHEMADEFINITIONS_H\n\n");

	printf("#ifndef  SCSTR\n");
	printf("#include <CoreFoundation/CFString.h>\n");
	printf("#define  SCSTR(s) CFSTR(s)\n");
	printf("#endif\n");

	printf("\n");
	dump_names(gen_header_e);
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
	dump_names(gen_extern_e);
	printf("\n");
	printf("__private_extern__\nvoid\n__Initialize(void)\n");
	printf("{\n");
	printf("   static Boolean initialized = FALSE;\n");
	printf("\n");
	printf("   if (initialized)\n");
	printf("      return;\n");
	printf("\n");
	dump_names(gen_init_e);
	printf("\n");
	printf("   initialized = TRUE;\n");
	printf("   return;\n");
	printf("}\n");
    }
    exit(0);
    return (0);
}

