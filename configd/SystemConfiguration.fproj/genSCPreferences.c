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
 * 3 Nov 2000	Dieter Siegmund (dieter@apple)
 *		- created
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


#define REGULAR		0
#define COMMENT		1
#define END		2

#define STRING_MACRO_NAME	"STRING_DECL"

#define KEY_PREFIX	"kSC"

#define CACHE		"Cache"
#define COMP		"Comp"
#define PREF		"Pref"
#define PROP		"Prop"
#define PATH		"Path"
#define NETENT		"EntNet"
#define NETPROP		"PropNet"
#define NETVAL		"ValNet"
#define SETUPENT	"EntSetup"
#define SETUPPROP	"PropSetup"
#define SYSTEMENT	"EntSystem"
#define SYSTEMPROP	"PropSystem"
#define RESV		"Resv"
#define USERSENT	"EntUsers"
#define USERSPROP	"PropUsers"

#define CFNUMBER	"CFNumber"
#define CFSTRING	"CFString"
#define CFNUMBER_BOOL	"CFNumber (0 or 1)"
#define CFARRAY_CFSTRING "CFArray[CFString]"

#define ACTIVE			"Active"
#define ADDRESSES		"Addresses"
#define AIRPORT			"AirPort"
#define ALERT			"Alert"
#define ANYREGEX		"AnyRegex"
#define AUTOMATIC		"Automatic"
#define APPLETALK		"AppleTalk"
#define AUTH			"Auth"
#define BINDINGMETHODS		"BindingMethods"
#define BOOTP			"BOOTP"
#define BROADCAST		"Broadcast"
#define BROADCASTADDRESSES	"BroadcastAddresses"
#define BROADCASTSERVERTAG	"BroadcastServerTag"
#define COMM			"Comm"
#define COMPONENTSEPARATOR	"ComponentSeparator"
#define COMPUTERNAME		"ComputerName"
#define CONFIGMETHOD		"ConfigMethod"
#define CONSOLEUSER		"ConsoleUser"
#define CURRENTSET		"CurrentSet"
#define DEFAULTSERVERTAG	"DefaultServerTag"
#define DEFAULTZONE		"DefaultZone"
#define DESTADDRESSES		"DestAddresses"
#define DHCP			"DHCP"
#define DHCPCLIENTID		"DHCPClientID"
#define DEVICENAME		"DeviceName"
#define DIALMODE		"DialMode"
#define DNS			"DNS"
#define DOMAIN 			"Domain"
#define DOMAINNAME		"DomainName"
#define DOMAINSEPARATOR		"DomainSeparator"
#define DUPLEX			"Duplex"
#define ENCODING		"Encoding"
#define ENCRYPTION		"Encryption"
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
#define INACTIVE		"Inactive"
#define INCLUDEPRIVATENETS	"IncludePrivateNets"
#define INFORM			"INFORM"
#define INTERFACE		"Interface"
#define INTERFACES		"Interfaces"
#define IPCP			"IPCP"
#define IPV4			"IPv4"
#define IPV6			"IPv6"
#define LASTUPDATED		"LastUpdated"
#define LCP			"LCP"
#define LINK			"Link"
#define MACADDRESS		"MACAddress"
#define MANUAL			"Manual"
#define MEDIA			"Media"
#define MODEM			"Modem"
#define NAME			"Name"
#define NETINFO			"NetInfo"
#define NETWORK			"Network"
#define NETWORKSERVICES		"NetworkServices"
#define NETWORKID		"NetworkID"
#define NIS			"NIS"
#define NODE			"Node"
#define NODEID			"NodeID"
#define PASSWORD		"Password"
#define PLUGIN			"Plugin"
#define PORTNAME		"PortName"
#define PPP			"PPP"
#define PPPOE			"PPPoE"
#define PPPSERIAL		"PPPSerial"
#define PPPOVERRIDEPRIMARY	"PPPOverridePrimary"
#define PREFS			"Prefs"
#define PRIMARYINTERFACE	"PrimaryInterface"
#define PROTOCOL		"Protocol"
#define PROXIES			"Proxies"
#define ROOTSEPARATOR		"RootSeparator"
#define ROUTER			"Router"
#define RTSPENABLE		"RTSPEnable"
#define RTSPPORT		"RTSPPort"
#define RTSPPROXY		"RTSPProxy"
#define SEARCHDOMAINS		"SearchDomains"
#define SEEDNETWORKRANGE	"SeedNetworkRange"
#define SEEDROUTER		"SeedRouter"
#define SEEDZONES		"SeedZones"
#define SERVICE			"Service"
#define SERVERADDRESSES		"ServerAddresses"
#define SERVERTAGS		"ServerTags"
#define SERVICEORDER		"ServiceOrder"
#define SERVICEIDS		"ServiceIDs"
#define SETS			"Sets"
#define SETUP			"Setup"
#define SPEED			"Speed"
#define STATE			"State"
#define SOCKSENABLE		"SOCKSEnable"
#define SOCKSPORT		"SOCKSPort"
#define SOCKSPROXY		"SOCKSProxy"
#define SUBNETMASKS		"SubnetMasks"
#define SUBTYPE			"SubType"
#define SYSTEM			"System"
#define TYPE			"Type"
#define UID			"UID"
#define USERS			"Users"
#define USERDEFINEDNAME		"UserDefinedName"
#define VERBOSELOGGING		"VerboseLogging"

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
    { REGULAR, PROP, MACADDRESS, NULL, CFSTRING },
    { REGULAR, PROP, USERDEFINEDNAME, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Preference Keys\n */", NULL },
    { REGULAR, PREF, CURRENTSET, NULL, NULL },
    { REGULAR, PREF, HARDWARE, NULL, NULL },
    { REGULAR, PREF, NETWORKSERVICES, NULL, NULL },
    { REGULAR, PREF, SETS, NULL, NULL },
    { REGULAR, PREF, SYSTEM, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Component Keys\n */", NULL },
    { REGULAR, COMP, NETWORK, NULL, NULL },
    { REGULAR, COMP, SERVICE, NULL, NULL },
    { REGULAR, COMP, GLOBAL, NULL, NULL },
    { REGULAR, COMP, INTERFACE, NULL, NULL },
    { REGULAR, COMP, SYSTEM, NULL, NULL },
    { REGULAR, COMP, USERS, "users", NULL },	/* FIX ME! */
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Regex key which matches any component\n */", NULL },
    { REGULAR, COMP, ANYREGEX, "[^/]+", NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Network Entity Keys\n */", NULL },
    { REGULAR, NETENT, AIRPORT, NULL, NULL },
    { REGULAR, NETENT, APPLETALK, NULL, NULL },
    { REGULAR, NETENT, DNS, NULL, NULL },
    { REGULAR, NETENT, ETHERNET, NULL, NULL },
    { REGULAR, NETENT, INTERFACE, NULL, NULL },
    { REGULAR, NETENT, IPV4, NULL, NULL },
    { REGULAR, NETENT, IPV6, NULL, NULL },
    { REGULAR, NETENT, LINK, NULL, NULL },
    { REGULAR, NETENT, MODEM, NULL, NULL },
    { REGULAR, NETENT, NETINFO, NULL, NULL },
    { REGULAR, NETENT, NIS, NULL, NULL },
    { REGULAR, NETENT, PPP, NULL, NULL },
    { REGULAR, NETENT, PPPOE, NULL, NULL },
    { REGULAR, NETENT, PROXIES, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " NETWORK " Properties\n */", NULL },
    { REGULAR, NETPROP, SERVICEORDER, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP, PPPOVERRIDEPRIMARY, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " AIRPORT " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP AIRPORT, "PowerEnabled", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP AIRPORT, AUTH PASSWORD, NULL, CFSTRING },
    { REGULAR, NETPROP AIRPORT, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { REGULAR, NETPROP AIRPORT, "PreferredNetwork", NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " APPLETALK " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP APPLETALK, COMPUTERNAME, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, CONFIGMETHOD, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, DEFAULTZONE, NULL, CFSTRING },
    { REGULAR, NETPROP APPLETALK, NETWORKID, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, NODEID, NULL, CFNUMBER },
    { REGULAR, NETPROP APPLETALK, SEEDNETWORKRANGE, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP APPLETALK, SEEDZONES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP APPLETALK CONFIGMETHOD " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL APPLETALK CONFIGMETHOD, NODE, NULL, NULL },
    { REGULAR, NETVAL APPLETALK CONFIGMETHOD, ROUTER, NULL, NULL },
    { REGULAR, NETVAL APPLETALK CONFIGMETHOD, SEEDROUTER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " DNS " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP DNS, DOMAINNAME, NULL, CFSTRING },
    { REGULAR, NETPROP DNS, SEARCHDOMAINS, NULL, CFARRAY_CFSTRING},
    { REGULAR, NETPROP DNS, SERVERADDRESSES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " ETHERNET " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " INTERFACE " Entity Keys\n */", NULL },
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

    { COMMENT, "/*\n * " IPV4 " Entity Keys\n */", NULL, NULL, NULL },
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

    { COMMENT, "/*\n * " IPV6 " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP IPV6, ADDRESSES, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETPROP IPV6, CONFIGMETHOD, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " LINK " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP LINK, ACTIVE, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " MODEM " (Hardware) Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP MODEM, "ConnectionScript", NULL, CFSTRING },
    { REGULAR, NETPROP MODEM, DIALMODE, NULL, CFSTRING },
    { REGULAR, NETPROP MODEM, "PulseDial", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, "Speaker", NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP MODEM DIALMODE " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL MODEM DIALMODE, "IgnoreDialTone", NULL, NULL },
    { REGULAR, NETVAL MODEM DIALMODE, MANUAL, NULL, NULL },
    { REGULAR, NETVAL MODEM DIALMODE, "WaitForDialTone", NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " NETINFO " Entity Keys\n */", NULL, NULL, NULL },
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

    { COMMENT, "/*\n * " NIS " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP NIS, DOMAINNAME, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " PPP " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, "DialOnDemand", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "DisconnectOnIdle", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "DisconnectOnIdleTimer", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, "DisconnectOnLogout", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "IdleReminderTimer", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, "IdleReminder", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "Logfile", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, VERBOSELOGGING, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* " AUTH ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, AUTH NAME, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PASSWORD, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { REGULAR, NETPROP PPP, AUTH PROTOCOL, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "/* " KEY_PREFIX NETPROP PPP AUTH PROTOCOL " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL PPP AUTH PROTOCOL, "CHAP", NULL, CFSTRING },
    { REGULAR, NETVAL PPP AUTH PROTOCOL, "PAP", NULL, CFSTRING },

    { COMMENT, "\n/* " COMM ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, COMM "AlternateRemoteAddress", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, COMM "ConnectDelay", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM "DisplayTerminalWindow", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, COMM "RedialCount", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM "RedialEnabled", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, COMM "RedialInterval", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM "RemoteAddress", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, COMM "TerminalScript", NULL, CFSTRING },

    { COMMENT, "\n/* " IPCP ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, IPCP "CompressionVJ", NULL, CFNUMBER_BOOL },

    { COMMENT, "\n/* " LCP ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, LCP "EchoEnabled", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LCP "EchoFailure", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP "EchoInterval", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP "CompressionACField", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LCP "CompressionPField", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, LCP "MRU", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP "MTU", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP "ReceiveACCM", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, LCP "TransmitACCM", NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " PPPOE " Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " PPPSERIAL " Entity Keys\n */", NULL, NULL, NULL },
    { COMMENT, "/* RESERVED FOR FUTURE USE */", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " PROXIES " Entity Keys\n */", NULL, NULL, NULL },
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
    { REGULAR, NETPROP PROXIES, RTSPENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, RTSPPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, RTSPPROXY, NULL, CFSTRING },
    { REGULAR, NETPROP PROXIES, SOCKSENABLE, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PROXIES, SOCKSPORT, NULL, CFNUMBER },
    { REGULAR, NETPROP PROXIES, SOCKSPROXY, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Users Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, USERSENT, CONSOLEUSER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n " CONSOLEUSER " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, USERSPROP CONSOLEUSER, NAME, "username", CFSTRING },	/* FIX ME! */
    { REGULAR, USERSPROP CONSOLEUSER, UID, "uid", CFSTRING },		/* FIX ME! */
    { REGULAR, USERSPROP CONSOLEUSER, GID, "gid", CFSTRING },		/* FIX ME! */
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * " SYSTEM " Entity Keys\n */", NULL, NULL, NULL },
    { REGULAR, SYSTEMPROP, COMPUTERNAME, NULL, CFSTRING },
    { REGULAR, SYSTEMPROP, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/*\n * Configuration Cache Definitions\n */", NULL },
    { COMMENT, "/* domain prefixes */", NULL },
    { REGULAR, CACHE DOMAIN, FILE, "File:", NULL },
    { REGULAR, CACHE DOMAIN, PLUGIN, "Plugin:", NULL },
    { REGULAR, CACHE DOMAIN, SETUP, "Setup:", NULL },
    { REGULAR, CACHE DOMAIN, STATE, "State:", NULL },
    { REGULAR, CACHE DOMAIN, PREFS, "Prefs:", NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* Setup: properties */", NULL },
    { REGULAR, CACHE SETUPPROP, CURRENTSET, NULL, NULL },
    { REGULAR, CACHE SETUPPROP, LASTUPDATED, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "/* properties */", NULL },
    { REGULAR, CACHE NETPROP, INTERFACES, NULL, CFARRAY_CFSTRING },
    { REGULAR, CACHE NETPROP, PRIMARYINTERFACE, NULL, CFSTRING },
    { REGULAR, CACHE NETPROP, SERVICEIDS, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

// XXX OBSOLETE XXX
    { COMMENT, "/* OBSOLETE " NETPROP AIRPORT ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP AIRPORT, INCLUDEPRIVATENETS, NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP AIRPORT, "PreferredAirportNetwork", NULL, CFSTRING },

    { COMMENT, "/* OBSOLETE " NETPROP ETHERNET ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP ETHERNET, SPEED, NULL, CFNUMBER },
    { REGULAR, NETPROP ETHERNET, DUPLEX, NULL, CFSTRING },
    { REGULAR, NETPROP ETHERNET, "WakeOnSignal", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP ETHERNET, "WakeOnTraffic", NULL, CFNUMBER_BOOL },
    { COMMENT, "/* " KEY_PREFIX NETPROP ETHERNET DUPLEX " values */", NULL, NULL, NULL },
    { REGULAR, NETVAL ETHERNET DUPLEX, AUTOMATIC, NULL, NULL },
    { REGULAR, NETVAL ETHERNET DUPLEX, "FULL", NULL, NULL },
    { REGULAR, NETVAL ETHERNET DUPLEX, "HALF", NULL, NULL },

    { COMMENT, "/* OBSOLETE " NETPROP INTERFACE ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP INTERFACE, INTERFACE NAME, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, MACADDRESS, NULL, CFSTRING },
    { REGULAR, NETPROP INTERFACE, PORTNAME, NULL, CFSTRING },

    { COMMENT, "/* OBSOLETE " NETPROP MODEM ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP MODEM, "IgnoreDialTone", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, "InitString", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, "Port", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, PORTNAME, NULL, CFSTRING },
    { REGULAR, NETPROP MODEM, "RedialCount", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, "RedialEnabled", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, "RedialTimeout", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP MODEM, "Script", NULL, CFSTRING },
    { REGULAR, NETPROP MODEM, "SpeakerEnable", NULL, CFNUMBER },
    { REGULAR, NETPROP MODEM, SPEED, NULL, CFNUMBER },
    { REGULAR, NETPROP MODEM, "ToneDial", NULL, CFNUMBER },
    { REGULAR, NETPROP MODEM, "WaitForTone", NULL, CFNUMBER },

    { COMMENT, "/* OBSOLETE " NETPROP PPP ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPP, ALERT, NULL, CFARRAY_CFSTRING },
    { REGULAR, NETVAL PPP ALERT, "Password", NULL, CFSTRING },
    { REGULAR, NETVAL PPP ALERT, "Reminder", NULL, CFSTRING },
    { REGULAR, NETVAL PPP ALERT, "Status", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, "CompressionEnable", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "DeviceEntity", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, "HeaderCompression", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "IdleDisconnect", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "IdlePrompt", NULL, CFNUMBER_BOOL },
    { REGULAR, NETPROP PPP, "IdleTimeout", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, "PromptTimeout", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, "ReminderTimer", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, "SessionTimer", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, COMM "IdleTimer", NULL, CFNUMBER },
    { REGULAR, NETPROP PPP, IPCP "LocalAddress", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, IPCP "RemoteAddress", NULL, CFSTRING },
    { REGULAR, NETPROP PPP, IPCP "UseServerDNS", NULL, CFNUMBER_BOOL },

    { COMMENT, "/* OBSOLETE " NETPROP PPPOE ": */", NULL, NULL, NULL },
    { REGULAR, NETPROP PPPOE, PORTNAME, NULL, CFSTRING },

    { COMMENT, "", NULL, NULL, NULL },
// XXX OBSOLETE XXX

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
	    case REGULAR: {
		char buf[256];

		switch (type) {
		case gen_header_e:
		    snprintf(buf, sizeof(buf), KEY_PREFIX "%s%s;",
			     names[i].prefix, names[i].key);

		    if (names[i].type)
			printf(STRING_MACRO_NAME " %-40s /* %s */\n",
			       buf, names[i].type);
		    else
			printf(STRING_MACRO_NAME " %s\n", buf);
		    break;
		case gen_extern_e:
		    snprintf(buf, sizeof(buf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);

		    printf("volatile CFStringRef " KEY_PREFIX "%s%s = NULL;\n",
			   names[i].prefix, names[i].key);
		    break;
		case gen_init_e:
		    snprintf(buf, sizeof(buf), KEY_PREFIX "%s%s",
			     names[i].prefix, names[i].key);
		    if (names[i].value)
			printf("   *((void **)&%s) = (void *)CFSTR(\"%s\");\n",
			       buf, names[i].value);
		    else
			printf("   *((void **)&%s) = (void *)CFSTR(\"%s\");\n",
			       buf, names[i].key);
		    break;
		default:
		    break;
		}
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
	printf("#ifndef _SCPREFERENCES_H\n#define _SCPREFERENCES_H\n\n");
	//printf("#ifndef " STRING_MACRO_NAME "\n");
	printf("#ifndef __OBJC__\n");
	printf("#define " STRING_MACRO_NAME "\t\textern const CFStringRef\n");
	printf("#else\n");
	printf("#define " STRING_MACRO_NAME "\t\textern NSString *\n");
	printf("#endif\n");
	//printf("#endif " STRING_MACRO_NAME "\n");
	printf("\n");
	dump_names(gen_header_e);
	printf("#endif /* _SCPREFERENCES_H */\n");
    }
    else if (strcmp(type, "cfile") == 0) {
	printf("/*\n * This file is automatically generated\n * DO NOT EDIT!\n */\n\n");
	printf("\n#include <CoreFoundation/CFString.h>\n\n");
	dump_names(gen_extern_e);
	printf("\n\nvoid\n__private_extern__\n__Initialize(void)\n{\n");
	dump_names(gen_init_e);
	printf("}\n");
    }
    exit(0);
    return (0);
}

