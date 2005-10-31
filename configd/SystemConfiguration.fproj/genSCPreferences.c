/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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
 * genSCPreferences.c
 * - generates System Configuration header/cfile
 * - invoke with "header" to generate the header
 * - invoke with "cfile" to generate the cfile
 */

/*
 * Modification History
 *
 * 4 March 2004			Allan Nathanson (ajn@apple.com)
 * - an alternate scheme to help facilitate access to the schema
 *   definitions for cross-compilation to earlier releases AND
 *   access to CFM applications.
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
" * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.\n"
" *\n"
" * @APPLE_LICENSE_HEADER_START@\n"
" * \n"
" * This file contains Original Code and/or Modifications of Original Code\n"
" * as defined in and that are subject to the Apple Public Source License\n"
" * Version 2.0 (the 'License'). You may not use this file except in\n"
" * compliance with the License. Please obtain a copy of the License at\n"
" * http://www.opensource.apple.com/apsl/ and read it before using this\n"
" * file.\n"
" * \n"
" * The Original Code and all software distributed under the License are\n"
" * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER\n"
" * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,\n"
" * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,\n"
" * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.\n"
" * Please see the License for the specific language governing rights and\n"
" * limitations under the License.\n"
" * \n"
" * @APPLE_LICENSE_HEADER_END@\n"
" */\n";


typedef enum {
	COMMENT,
	GROUP,
	SC_10_1,
	SC_10_1_10_4,	// deprecated in 10.4
	SC_10_2,
	SC_10_3,
	SC_10_4,
	END
} controlType;

#define SC_SCHEMA_DECLARATION	"SC_SCHEMA_DECLARATION"
#define SC_SCHEMA_KV		"SC_SCHEMA_KV"

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

#define ACSP			"ACSP"			// Apple Client Server Protocol
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
#define AUTOCONFIG		"AutoConfig"
#define AUTODISCOVERY		"AutoDiscovery"
#define AUTOMATIC		"Automatic"
#define BEFORE			"Before"
#define BINDINGMETHODS		"BindingMethods"
#define BOOTP			"BOOTP"
#define BROADCAST		"Broadcast"
#define CALLWAITINGAUDIBLEALERT	"CallWaitingAudibleAlert"
#define CAUSE			"Cause"
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
#define DEFAULT			"Default"
#define DEFAULTZONE		"DefaultZone"
#define DEST			"Dest"
#define DETACHING		"Detaching"
#define DEVICE			"Device"
#define DEVICENAME		"DeviceName"
#define DHCP			"DHCP"
#define DHCPCLIENTID		"DHCPClientID"
#define DIALMODE		"DialMode"
#define DIALONDEMAND		"DialOnDemand"
#define DISCONNECTONANSWER	"DisconnectOnAnswer"
#define DISCONNECTONFASTUSERSWITCH	"DisconnectOnFastUserSwitch"
#define DISCONNECTONIDLE	"DisconnectOnIdle"
#define DISCONNECTONIDLETIMER	"DisconnectOnIdleTimer"
#define DISCONNECTONLOGOUT	"DisconnectOnLogout"
#define DISCONNECTONSLEEP	"DisconnectOnSleep"
#define DISCONNECTTIME		"DisconnectTime"
#define DISPLAYTERMINALWINDOW	"DisplayTerminalWindow"
#define DNS			"DNS"
#define DOMAIN 			"Domain"
#define DOMAINS			"Domains"
#define EAP			"EAP"
#define ECHO			"Echo"
#define ECHOFAILURE		"EchoFailure"
#define ECHOINTERVAL		"EchoInterval"
#define ENABLE			"Enable"
#define ENABLED			"Enabled"
#define ENCODING		"Encoding"
#define ENCRYPTION		"Encryption"
#define ERRORCORRECTION		"ErrorCorrection"
#define ETHERNET		"Ethernet"
#define EXCEPTIONSLIST		"ExceptionsList"
#define EXCLUDESIMPLEHOSTNAMES	"ExcludeSimpleHostnames"
#define FILE			"File"
#define FIREWIRE		"FireWire"
#define FIRST			"First"
#define FLAGS			"Flags"
#define FTP			"FTP"
#define GID			"GID"
#define GLOBAL			"Global"
#define GOPHER			"Gopher"
#define HARDWARE		"Hardware"
#define HOLD			"Hold"
#define HOSTNAMES		"HostNames"
#define HTTP			"HTTP"
#define HTTPS			"HTTPS"
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
#define LAST			"Last"
#define LCP			"LCP"
#define LINK			"Link"
#define LINKLOCAL		"LinkLocal"
#define LOCALHOSTNAME		"LocalHostName"
#define LOGFILE			"Logfile"
#define MACADDRESS		"MACAddress"
#define MANUAL			"Manual"
#define MATCH			"Match"
#define MEDIA			"Media"
#define OPTIONS			"Options"
#define MODEM			"Modem"
#define MPPE40			"MPPE40"
#define MPPE128			"MPPE128"
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
#define NODE			"Node"
#define NODEID			"NodeID"
#define NOTE			"Note"
#define ORDER			"Order"
#define ORDERS			"Orders"
#define OVERRIDEPRIMARY		"OverridePrimary"
#define PAP			"PAP"
#define PASSIVE			"Passive"
#define PASSWORD		"Password"
#define PEERDNS			"PeerDNS"
#define PLUGIN			"Plugin"
#define PLUGINS			"Plugins"
#define POWER			"Power"
#define PORT			"Port"
#define PPP			"PPP"
#define PPPOE			"PPPoE"
#define PPPSERIAL		"PPPSerial"
#define PPTP			"PPTP"
#define PREFERRED		"Preferred"
#define PREFIXLENGTH		"PrefixLength"
#define PREFS			"Prefs"
#define PRIMARYINTERFACE	"PrimaryInterface"
#define PRIMARYSERVICE		"PrimaryService"
#define PROMPT			"Prompt"
#define PROTOCOL		"Protocol"
#define PROXIES			"Proxies"
#define PROXY			"Proxy"
#define PULSEDIAL		"PulseDial"
#define RECEIVEACCM		"ReceiveACCM"
#define RECENT			"Recent"
#define REDIALCOUNT		"RedialCount"
#define REDIAL			"Redial"
#define REDIALINTERVAL		"RedialInterval"
#define RELAY			"Relay"
#define REMINDER		"Reminder"
#define REMINDERTIME		"ReminderTime"
#define REMOTEADDRESS		"RemoteAddress"
#define RETRYCONNECTTIME	"RetryConnectTime"
#define ROOTSEPARATOR		"RootSeparator"
#define ROUTER			"Router"
#define ROUTERADVERTISEMENT	"RouterAdvertisement"
#define RTSP			"RTSP"
#define SAVEPASSWORDS		"SavePasswords"
#define SEARCH			"Search"
#define SEEDNETWORKRANGE	"SeedNetworkRange"
#define SEEDROUTER		"SeedRouter"
#define SEEDZONES		"SeedZones"
#define SERVER			"Server"
#define SERVICE			"Service"
#define SERVICEIDS		"ServiceIDs"
#define SESSIONTIMER		"SessionTimer"
#define SETS			"Sets"
#define SETUP			"Setup"
#define	SHAREDSECRET		"SharedSecret"
#define SOCKS			"SOCKS"
#define SORTLIST		"SortList"
#define SPEAKER			"Speaker"
#define SPEED			"Speed"
#define STATE			"State"
#define STATUS			"Status"
#define STF			"6to4"
#define STRONGEST		"Strongest"
#define SUBNETMASKS		"SubnetMasks"
#define SUBTYPE			"SubType"
#define SUPPLEMENTAL		"Supplemental"
#define SUPPORTSMODEMONHOLD	"SupportsModemOnHold"
#define SYSTEM			"System"
#define TAG			"Tag"
#define TAGS			"Tags"
#define TERMINALSCRIPT		"TerminalScript"
#define TIMEOUT			"Timeout"
#define TRANSMITACCM		"TransmitACCM"
#define TRANSPORT		"Transport"
#define TYPE			"Type"
#define UID			"UID"
#define UPDATED			"Updated"
#define URLSTRING		"URLString"
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

  { GROUP, NULL, "Reserved Keys", NULL, NULL },

    { SC_10_1, RESV, LINK,	"__LINK__", CFSTRING },
    { SC_10_1, RESV, INACTIVE,	"__INACTIVE__", NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NULL, "Generic Keys", NULL, NULL },

    { SC_10_1, PROP, INTERFACENAME, NULL, CFSTRING },
    { SC_10_1, PROP, MACADDRESS, NULL, CFSTRING },
    { SC_10_1, PROP, USERDEFINEDNAME, NULL, CFSTRING },
    { SC_10_1, PROP, VERSION, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, PREF, "Preference Keys", NULL, NULL },

    { SC_10_1, PREF, CURRENTSET, NULL, CFSTRING },
    { SC_10_1, PREF, NETWORKSERVICES, NULL, CFDICTIONARY },
    { SC_10_1, PREF, SETS, NULL, CFDICTIONARY },
    { SC_10_1, PREF, SYSTEM, NULL, CFDICTIONARY },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, COMP, "Component Keys", NULL, NULL },

    { SC_10_1, COMP, NETWORK, NULL, NULL },
    { SC_10_1, COMP, SERVICE, NULL, NULL },
    { SC_10_1, COMP, GLOBAL, NULL, NULL },
    { SC_10_2, COMP, HOSTNAMES, NULL, NULL },
    { SC_10_1, COMP, INTERFACE, NULL, NULL },
    { SC_10_1, COMP, SYSTEM, NULL, NULL },
    { SC_10_1, COMP, USERS, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "--- Regex pattern which matches any component ---", NULL },
    { SC_10_1, COMP, ANYREGEX, "[^/]+", NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETENT, "Network Entity Keys", NULL, NULL },

    { SC_10_1, NETENT, AIRPORT, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, APPLETALK, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, DHCP, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, DNS, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, ETHERNET, NULL, CFDICTIONARY },
    { SC_10_3, NETENT, FIREWIRE, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, INTERFACE, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, IPV4, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, IPV6, NULL, CFDICTIONARY },
    { SC_10_3, NETENT, L2TP, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, LINK, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, MODEM, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, NETINFO, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, PPP, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, PPPOE, NULL, CFDICTIONARY },
    { SC_10_3, NETENT, PPPSERIAL, NULL, CFDICTIONARY },
    { SC_10_3, NETENT, PPTP, NULL, CFDICTIONARY },
    { SC_10_1, NETENT, PROXIES, NULL, CFDICTIONARY },
    { SC_10_3, NETENT, STF, NULL, CFDICTIONARY },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP AIRPORT, KEY_PREFIX COMP NETWORK " Properties", NULL, NULL },

    { SC_10_2, NETPROP, OVERRIDEPRIMARY, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP, SERVICE ORDER, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP, PPP OVERRIDEPRIMARY, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP AIRPORT, KEY_PREFIX COMP NETWORK INTERFACE " Properties", NULL, NULL },

    { SC_10_2, NETPROP, INTERFACES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP AIRPORT, KEY_PREFIX COMP NETWORK HOSTNAMES " Properties", NULL, NULL },

    { SC_10_2, NETPROP, LOCALHOSTNAME, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP AIRPORT, KEY_PREFIX NETENT AIRPORT " (Hardware) Entity Keys", NULL, NULL },

    { SC_10_2, NETPROP AIRPORT, ALLOWNETCREATION, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP AIRPORT, AUTH PASSWORD, NULL, CFDATA },
    { SC_10_1, NETPROP AIRPORT, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { SC_10_2, NETPROP AIRPORT, JOINMODE, NULL, CFSTRING },
    { SC_10_1, NETPROP AIRPORT, POWER ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP AIRPORT, PREFERRED NETWORK, NULL, CFSTRING },
    { SC_10_2, NETPROP AIRPORT, SAVEPASSWORDS, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP AIRPORT JOINMODE " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL AIRPORT JOINMODE, AUTOMATIC, NULL, NULL },
    { SC_10_2, NETVAL AIRPORT JOINMODE, PREFERRED, NULL, NULL },
    { SC_10_2, NETVAL AIRPORT JOINMODE, RECENT, NULL, NULL },
    { SC_10_2, NETVAL AIRPORT JOINMODE, STRONGEST, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP AIRPORT PASSWORD ENCRYPTION " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL AIRPORT AUTH PASSWORD ENCRYPTION, KEYCHAIN, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP APPLETALK, KEY_PREFIX NETENT APPLETALK " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP APPLETALK, COMPUTERNAME, NULL, CFSTRING },
    { SC_10_1, NETPROP APPLETALK, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { SC_10_1, NETPROP APPLETALK, CONFIGMETHOD, NULL, CFSTRING },
    { SC_10_1, NETPROP APPLETALK, DEFAULTZONE, NULL, CFSTRING },
    { SC_10_1, NETPROP APPLETALK, NETWORKID, NULL, CFNUMBER },
    { SC_10_2, NETPROP APPLETALK, NETWORKRANGE, NULL, CFARRAY_CFNUMBER },
    { SC_10_1, NETPROP APPLETALK, NODEID, NULL, CFNUMBER },
    { SC_10_1, NETPROP APPLETALK, SEEDNETWORKRANGE, NULL, CFARRAY_CFNUMBER },
    { SC_10_1, NETPROP APPLETALK, SEEDZONES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP APPLETALK CONFIGMETHOD " values ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL APPLETALK CONFIGMETHOD, NODE, NULL, NULL },
    { SC_10_1, NETVAL APPLETALK CONFIGMETHOD, ROUTER, NULL, NULL },
    { SC_10_1, NETVAL APPLETALK CONFIGMETHOD, SEEDROUTER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP DNS, KEY_PREFIX NETENT DNS " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP DNS, DOMAIN NAME, NULL, CFSTRING },
    { SC_10_4, NETPROP DNS, OPTIONS, NULL, CFSTRING },
    { SC_10_1, NETPROP DNS, SEARCH DOMAINS, NULL, CFARRAY_CFSTRING},
    { SC_10_4, NETPROP DNS, SEARCH ORDER, NULL, CFNUMBER},
    { SC_10_1, NETPROP DNS, SERVER ADDRESSES, NULL, CFARRAY_CFSTRING },
    { SC_10_4, NETPROP DNS, SERVER PORT, NULL, CFNUMBER },
    { SC_10_4, NETPROP DNS, SERVER TIMEOUT, NULL, CFNUMBER },
    { SC_10_1, NETPROP DNS, SORTLIST, NULL, CFARRAY_CFSTRING },
    { SC_10_4, NETPROP DNS, SUPPLEMENTAL MATCH DOMAINS, NULL, CFARRAY_CFSTRING},
    { SC_10_4, NETPROP DNS, SUPPLEMENTAL MATCH ORDERS, NULL, CFARRAY_CFNUMBER},
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP ETHERNET, KEY_PREFIX NETENT ETHERNET " (Hardware) Entity Keys", NULL, NULL },

    { SC_10_2, NETPROP ETHERNET, MEDIA SUBTYPE, NULL, CFSTRING },
    { SC_10_2, NETPROP ETHERNET, MEDIA OPTIONS, NULL, CFARRAY_CFSTRING },
    { SC_10_2, NETPROP ETHERNET, MTU, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP FIREWIRE, KEY_PREFIX NETENT FIREWIRE " (Hardware) Entity Keys", NULL, NULL },

    { COMMENT, "* RESERVED FOR FUTURE USE *", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP INTERFACE, KEY_PREFIX NETENT INTERFACE " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP INTERFACE, DEVICENAME, NULL, CFSTRING },
    { SC_10_1, NETPROP INTERFACE, HARDWARE, NULL, CFSTRING },
    { SC_10_1, NETPROP INTERFACE, TYPE, NULL, CFSTRING },
    { SC_10_1, NETPROP INTERFACE, SUBTYPE, NULL, CFSTRING },
    { SC_10_2, NETPROP INTERFACE, SUPPORTSMODEMONHOLD, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP INTERFACE TYPE " values ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL INTERFACE TYPE, ETHERNET, NULL, NULL },
    { SC_10_3, NETVAL INTERFACE TYPE, FIREWIRE, NULL, NULL },
    { SC_10_1, NETVAL INTERFACE TYPE, PPP, NULL, NULL },
    { SC_10_3, NETVAL INTERFACE TYPE, STF, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP SERVICE SUBTYPE " values (for " PPP ") ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL INTERFACE SUBTYPE, PPPOE, NULL, NULL },
    { SC_10_1, NETVAL INTERFACE SUBTYPE, PPPSERIAL, NULL, NULL },
    { SC_10_2, NETVAL INTERFACE SUBTYPE, PPTP, NULL, NULL },
    { SC_10_3, NETVAL INTERFACE SUBTYPE, L2TP, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP IPV4, KEY_PREFIX NETENT IPV4 " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP IPV4, ADDRESSES, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP IPV4, CONFIGMETHOD, NULL, CFSTRING },
    { SC_10_1, NETPROP IPV4, DHCPCLIENTID, NULL, CFSTRING },
    { SC_10_1, NETPROP IPV4, ROUTER, NULL, CFSTRING },
    { SC_10_1, NETPROP IPV4, SUBNETMASKS, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP IPV4, DEST ADDRESSES, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP IPV4, BROADCAST ADDRESSES, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP IPV4 CONFIGMETHOD " values ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL IPV4 CONFIGMETHOD, BOOTP, NULL, NULL },
    { SC_10_1, NETVAL IPV4 CONFIGMETHOD, DHCP, NULL, NULL },
    { SC_10_1, NETVAL IPV4 CONFIGMETHOD, INFORM, NULL, NULL },
    { SC_10_2, NETVAL IPV4 CONFIGMETHOD, LINKLOCAL, NULL, NULL },
    { SC_10_1, NETVAL IPV4 CONFIGMETHOD, MANUAL, NULL, NULL },
    { SC_10_1, NETVAL IPV4 CONFIGMETHOD, PPP, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP IPV6, KEY_PREFIX NETENT IPV6 " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP IPV6, ADDRESSES, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP IPV6, CONFIGMETHOD, NULL, CFSTRING },
    { SC_10_3, NETPROP IPV6, DEST ADDRESSES, NULL, CFARRAY_CFSTRING },
    { SC_10_3, NETPROP IPV6, FLAGS, NULL, CFNUMBER },
    { SC_10_3, NETPROP IPV6, PREFIXLENGTH, NULL, CFARRAY_CFNUMBER },
    { SC_10_3, NETPROP IPV6, ROUTER, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP IPV6 CONFIGMETHOD " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL IPV6 CONFIGMETHOD, AUTOMATIC, NULL, NULL },
    { SC_10_3, NETVAL IPV6 CONFIGMETHOD, MANUAL, NULL, NULL },
    { SC_10_3, NETVAL IPV6 CONFIGMETHOD, ROUTERADVERTISEMENT, NULL, NULL },
    { SC_10_3, NETVAL IPV6 CONFIGMETHOD, STF, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP STF, KEY_PREFIX NETENT STF " Entity Keys", NULL, NULL },

    { SC_10_3, NETPROP STF, RELAY, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP LINK, KEY_PREFIX NETENT LINK " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP LINK, ACTIVE, NULL, CFBOOLEAN },
    { SC_10_2, NETPROP LINK, DETACHING, NULL, CFBOOLEAN },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP MODEM, KEY_PREFIX NETENT MODEM " (Hardware) Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP MODEM, CONNECTIONSCRIPT, NULL, CFSTRING },
    { SC_10_2, NETPROP MODEM, CONNECTSPEED, NULL, CFNUMBER },
    { SC_10_1, NETPROP MODEM, DATACOMPRESSION, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP MODEM, DIALMODE, NULL, CFSTRING },
    { SC_10_1, NETPROP MODEM, ERRORCORRECTION, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP MODEM, HOLD CALLWAITINGAUDIBLEALERT, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP MODEM, HOLD DISCONNECTONANSWER, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP MODEM, HOLD ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP MODEM, HOLD REMINDER, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP MODEM, HOLD REMINDERTIME, NULL, CFNUMBER },
    { SC_10_2, NETPROP MODEM, NOTE, NULL, CFSTRING },
    { SC_10_1, NETPROP MODEM, PULSEDIAL, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP MODEM, SPEAKER, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP MODEM, SPEED, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP MODEM DIALMODE " values ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL MODEM DIALMODE, IGNOREDIALTONE, NULL, NULL },
    { SC_10_1, NETVAL MODEM DIALMODE, MANUAL, NULL, NULL },
    { SC_10_1, NETVAL MODEM DIALMODE, WAITFORDIALTONE, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP NETINFO, KEY_PREFIX NETENT NETINFO " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP NETINFO, BINDINGMETHODS, NULL, CFSTRING },
    { SC_10_1, NETPROP NETINFO, SERVER ADDRESSES, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP NETINFO, SERVER TAGS, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP NETINFO, BROADCAST SERVER TAG, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP NETINFO BINDINGMETHODS " values ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL NETINFO BINDINGMETHODS, BROADCAST, NULL, NULL },
    { SC_10_1, NETVAL NETINFO BINDINGMETHODS, DHCP, NULL, NULL },
    { SC_10_1, NETVAL NETINFO BINDINGMETHODS, MANUAL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP NETINFO BROADCAST SERVER TAG " default value ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL NETINFO, DEFAULT SERVER TAG, "network", NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP PPP, KEY_PREFIX NETENT PPP " Entity Keys", NULL, NULL },

    { SC_10_3, NETPROP PPP, ACSP ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP PPP, CONNECTTIME, NULL, CFNUMBER },
    { SC_10_2, NETPROP PPP, DEVICE LAST CAUSE, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, DIALONDEMAND, NULL, CFNUMBER_BOOL },
    { SC_10_4, NETPROP PPP, DISCONNECTONFASTUSERSWITCH, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, DISCONNECTONIDLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, DISCONNECTONIDLETIMER, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, DISCONNECTONLOGOUT, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP PPP, DISCONNECTONSLEEP, NULL, CFNUMBER_BOOL },
    { SC_10_3, NETPROP PPP, DISCONNECTTIME, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, IDLEREMINDERTIMER, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, IDLEREMINDER, NULL, CFNUMBER_BOOL },
    { SC_10_2, NETPROP PPP, LAST CAUSE, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, LOGFILE, NULL, CFSTRING },
    { SC_10_2, NETPROP PPP, PLUGINS, NULL, CFARRAY_CFSTRING },
    { SC_10_3, NETPROP PPP, RETRYCONNECTTIME, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, SESSIONTIMER, NULL, CFNUMBER },
    { SC_10_2, NETPROP PPP, STATUS, NULL, CFNUMBER },
    { SC_10_2, NETPROP PPP, USE SESSIONTIMER, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, VERBOSELOGGING, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "--- " AUTH ": ---", NULL, NULL, NULL },
    { SC_10_3, NETPROP PPP, AUTH EAP PLUGINS, NULL, CFARRAY_CFSTRING },
    { SC_10_1, NETPROP PPP, AUTH NAME, NULL, CFSTRING },
    { SC_10_1, NETPROP PPP, AUTH PASSWORD, NULL, CFSTRING },
    { SC_10_1, NETPROP PPP, AUTH PASSWORD ENCRYPTION, NULL, CFSTRING },
    { SC_10_3, NETPROP PPP, AUTH PROMPT, NULL, CFSTRING },
    { SC_10_1, NETPROP PPP, AUTH PROTOCOL, NULL, CFARRAY_CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP PPP AUTH PASSWORD ENCRYPTION " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL PPP AUTH PASSWORD ENCRYPTION, KEYCHAIN, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP PPP AUTH PROMPT " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL PPP AUTH PROMPT, BEFORE, NULL, CFSTRING },
    { SC_10_3, NETVAL PPP AUTH PROMPT, AFTER, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP PPP AUTH PROTOCOL " values ---", NULL, NULL, NULL },
    { SC_10_1, NETVAL PPP AUTH PROTOCOL, CHAP, NULL, CFSTRING },
    { SC_10_3, NETVAL PPP AUTH PROTOCOL, EAP, NULL, CFSTRING },
    { SC_10_3, NETVAL PPP AUTH PROTOCOL, MSCHAP1, NULL, CFSTRING },
    { SC_10_3, NETVAL PPP AUTH PROTOCOL, MSCHAP2, NULL, CFSTRING },
    { SC_10_1, NETVAL PPP AUTH PROTOCOL, PAP, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "--- " COMM ": ---", NULL, NULL, NULL },
    { SC_10_1, NETPROP PPP, COMM ALTERNATEREMOTEADDRESS, NULL, CFSTRING },
    { SC_10_1, NETPROP PPP, COMM CONNECTDELAY, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, COMM DISPLAYTERMINALWINDOW, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, COMM REDIALCOUNT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, COMM REDIAL ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, COMM REDIALINTERVAL, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, COMM REMOTEADDRESS, NULL, CFSTRING },
    { SC_10_1, NETPROP PPP, COMM TERMINALSCRIPT, NULL, CFSTRING },
    { SC_10_2, NETPROP PPP, COMM USE TERMINALSCRIPT, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "--- " CCP ": ---", NULL, NULL, NULL },
    { SC_10_2, NETPROP PPP, CCP ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_4, NETPROP PPP, CCP MPPE40 ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_4, NETPROP PPP, CCP MPPE128 ENABLED, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "--- " IPCP ": ---", NULL, NULL, NULL },
    { SC_10_1, NETPROP PPP, IPCP COMPRESSIONVJ, NULL, CFNUMBER_BOOL },
    { SC_10_4, NETPROP PPP, IPCP USE PEERDNS, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

    { COMMENT, "--- " LCP ": ---", NULL, NULL, NULL },
    { SC_10_1, NETPROP PPP, LCP ECHO ENABLED, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, LCP ECHOFAILURE, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, LCP ECHOINTERVAL, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, LCP COMPRESSIONACFIELD, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, LCP COMPRESSIONPFIELD, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PPP, LCP MRU, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, LCP MTU, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, LCP RECEIVEACCM, NULL, CFNUMBER },
    { SC_10_1, NETPROP PPP, LCP TRANSMITACCM, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP PPPOE, KEY_PREFIX NETENT PPPOE " Entity Keys", NULL, NULL },

    { COMMENT, "* RESERVED FOR FUTURE USE *", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP PPPSERIAL, KEY_PREFIX NETENT PPPSERIAL " Entity Keys", NULL, NULL },

    { COMMENT, "* RESERVED FOR FUTURE USE *", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP PPTP, KEY_PREFIX NETENT PPTP " Entity Keys", NULL, NULL },

    { COMMENT, "* RESERVED FOR FUTURE USE *", NULL, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP L2TP, KEY_PREFIX NETENT L2TP " Entity Keys", NULL, NULL },

    { SC_10_3, NETPROP L2TP, IPSEC SHAREDSECRET, NULL, CFSTRING },
    { SC_10_3, NETPROP L2TP, IPSEC SHAREDSECRET ENCRYPTION, NULL, CFSTRING },
    { SC_10_3, NETPROP L2TP, TRANSPORT, NULL, CFSTRING },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP L2TP IPSEC SHAREDSECRET ENCRYPTION " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL L2TP IPSEC SHAREDSECRET ENCRYPTION, KEYCHAIN, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },
    { COMMENT, "--- " KEY_PREFIX NETPROP L2TP TRANSPORT " values ---", NULL, NULL, NULL },
    { SC_10_3, NETVAL L2TP TRANSPORT, IP, NULL, NULL },
    { SC_10_3, NETVAL L2TP TRANSPORT, IPSEC, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, NETPROP PROXIES, KEY_PREFIX NETENT PROXIES " Entity Keys", NULL, NULL },

    { SC_10_1, NETPROP PROXIES, EXCEPTIONSLIST, NULL, CFARRAY_CFSTRING },
    { SC_10_4, NETPROP PROXIES, EXCLUDESIMPLEHOSTNAMES, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, FTP ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, FTP PASSIVE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, FTP PORT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PROXIES, FTP PROXY, NULL, CFSTRING },
    { SC_10_1, NETPROP PROXIES, GOPHER ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, GOPHER PORT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PROXIES, GOPHER PROXY, NULL, CFSTRING },
    { SC_10_1, NETPROP PROXIES, HTTP ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, HTTP PORT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PROXIES, HTTP PROXY, NULL, CFSTRING },
    { SC_10_1, NETPROP PROXIES, HTTPS ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, HTTPS PORT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PROXIES, HTTPS PROXY, NULL, CFSTRING },
    { SC_10_1, NETPROP PROXIES, RTSP ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, RTSP PORT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PROXIES, RTSP PROXY, NULL, CFSTRING },
    { SC_10_1, NETPROP PROXIES, SOCKS ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_1, NETPROP PROXIES, SOCKS PORT, NULL, CFNUMBER },
    { SC_10_1, NETPROP PROXIES, SOCKS PROXY, NULL, CFSTRING },
    { SC_10_4, NETPROP PROXIES, PROXY AUTOCONFIG ENABLE, NULL, CFNUMBER_BOOL },
    { SC_10_4, NETPROP PROXIES, PROXY AUTOCONFIG URLSTRING, NULL, CFSTRING },
    { SC_10_4, NETPROP PROXIES, PROXY AUTODISCOVERY ENABLE, NULL, CFNUMBER_BOOL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, USERSENT CONSOLEUSER, KEY_PREFIX COMP USERS " Entity Keys", NULL, NULL },

    { SC_10_1, USERSENT, CONSOLEUSER, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, SYSTEMPROP COMPUTERNAME, KEY_PREFIX COMP SYSTEM " Properties", NULL, NULL },

    { SC_10_1, SYSTEMPROP, COMPUTERNAME, NULL, CFSTRING },
    { SC_10_1, SYSTEMPROP, COMPUTERNAME ENCODING, NULL, CFNUMBER },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, DYNAMICSTORE DOMAIN, "SCDynamicStore \"domain\" prefixes", NULL, NULL },

    { SC_10_1, DYNAMICSTORE DOMAIN, FILE, "File:", NULL },
    { SC_10_1, DYNAMICSTORE DOMAIN, PLUGIN, "Plugin:", NULL },
    { SC_10_1, DYNAMICSTORE DOMAIN, SETUP, "Setup:", NULL },
    { SC_10_1, DYNAMICSTORE DOMAIN, STATE, "State:", NULL },
    { SC_10_1, DYNAMICSTORE DOMAIN, PREFS, "Prefs:", NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, DYNAMICSTORE SETUPPROP, "Preference (\"location\") Keys", NULL, NULL },

    { SC_10_1, DYNAMICSTORE SETUPPROP, CURRENTSET, NULL, CFSTRING },
    { SC_10_1, DYNAMICSTORE SETUPPROP, LAST UPDATED, NULL, NULL },
    { COMMENT, "", NULL, NULL, NULL },

  { GROUP, DYNAMICSTORE NETPROP, "Common/shared Keys", NULL, NULL },

    { SC_10_1, DYNAMICSTORE NETPROP, INTERFACES, NULL, CFARRAY_CFSTRING },
    { SC_10_1, DYNAMICSTORE NETPROP, PRIMARYINTERFACE, NULL, CFSTRING },
    { SC_10_1, DYNAMICSTORE NETPROP, PRIMARYSERVICE, NULL, CFSTRING },
    { SC_10_1, DYNAMICSTORE NETPROP, SERVICEIDS, NULL, CFARRAY_CFSTRING },
//  { COMMENT, "", NULL, NULL, NULL },

//{ GROUP, "DEPRECATED", "Deprecated schema definition keys", NULL, NULL },

    { SC_10_1_10_4, USERSPROP CONSOLEUSER, NAME, NULL, CFSTRING },
    { SC_10_1_10_4, USERSPROP CONSOLEUSER, UID, NULL, CFNUMBER },
    { SC_10_1_10_4, USERSPROP CONSOLEUSER, GID, NULL, CFNUMBER },
//  { COMMENT, "", NULL, NULL, NULL },

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
    gen_comments_e,
    gen_headerdoc_e,
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
		        break;
		    case gen_comments_e:
		        if (names[i].prefix)
			    printf(" *   %s\n", names[i].prefix);
		        break;
		    case gen_hfile_e:
//		        if (names[i].prefix)
//			    printf("%s\n", names[i].prefix);
		        break;
		    default:
		        break;
		}
		break;
	    }

	    case GROUP: {
		switch (type) {
		    case gen_header_e:
		        break;
		    case gen_comments_e:
		        if (names[i].key)
			    printf(" * %s\n *\n", names[i].key);
		        break;
		    case gen_headerdoc_e:
		        if (names[i].prefix)
			    printf("\n/*!\n  @group %s\n */\n", names[i].key);
		        break;
		    default:
		        break;
		}
		break;
	    }

	    default: {
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

		        printf("#define %-50s %s\n",
			       kbuf, vbuf);
		        break;
		    case gen_comments_e:
		        switch (names[i].control) {
			    case SC_10_1_10_4:
			        // don't report deprecated keys
			        break;
			    default:
			        snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
				         names[i].prefix, names[i].key);

			        snprintf(vbuf, sizeof(vbuf), "\"%s\"",
				         names[i].value ? names[i].value : names[i].key);

			        if (names[i].type)
				    printf(" *   %-50s %-30s %s\n",
				           kbuf, vbuf, names[i].type);
			        else
				    printf(" *   %-50s %s\n",
				           kbuf, vbuf);
			        break;
			}
			break;
		    case gen_headerdoc_e:
			snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
				 names[i].prefix, names[i].key);
			setmax(&maxkbuf, &maxkstr, kbuf);

			snprintf(vbuf, sizeof(vbuf), "\"%s\"",
				 names[i].value ? names[i].value : names[i].key);
			setmax(&maxvbuf, &maxvstr, vbuf);

			printf("\n");

			printf("/*!\n");
			printf("  @const %s\n", kbuf);
			switch (names[i].control) {
			    case SC_10_1:
				printf("  @availability Introduced in Mac OS X 10.1.\n");
				break;
			    case SC_10_2:
				printf("  @availability Introduced in Mac OS X 10.2.\n");
				break;
			    case SC_10_1_10_4:
				printf("  @availability Introduced in Mac OS X 10.1, but later deprecated in Mac OS X 10.4.\n");
				break;
			    case SC_10_3:
				printf("  @availability Introduced in Mac OS X 10.3.\n");
				break;
			    case SC_10_4:
				printf("  @availability Introduced in Mac OS X 10.4.\n");
				break;
			}
			printf(" */\n");
			printf("extern const CFStringRef %s;\n", kbuf);

		        break;
		    case gen_hfile_e:
			snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
				 names[i].prefix, names[i].key);
			setmax(&maxkbuf, &maxkstr, kbuf);

			snprintf(vbuf, sizeof(vbuf), "\"%s\"",
				 names[i].value ? names[i].value : names[i].key);
			setmax(&maxvbuf, &maxvstr, vbuf);

			printf("\n");

			switch (names[i].control) {
			    case SC_10_1:
				printf("#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030\n");
				printf("  " SC_SCHEMA_DECLARATION "(%s, AVAILABLE_MAC_OS_X_VERSION_10_1_AND_LATER)\n", kbuf);
				printf("#endif\n");
				break;
			    case SC_10_2:
				printf("#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030\n");
				printf("  " SC_SCHEMA_DECLARATION "(%s, AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER)\n", kbuf);
				printf("#endif\n");
				break;
			    case SC_10_3:
				printf("#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030\n");
				printf("  " SC_SCHEMA_DECLARATION "(%s, AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER)\n", kbuf);
				printf("#endif\n");
				break;
			    case SC_10_1_10_4:
				printf("#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030\n");
				printf("  " SC_SCHEMA_DECLARATION "(%s, AVAILABLE_MAC_OS_X_VERSION_10_1_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4)\n", kbuf);
				printf("#endif\n");
				break;
			    case SC_10_4:
				printf("#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040\n");
				printf("  " SC_SCHEMA_DECLARATION "(%s, AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER)\n", kbuf);
				printf("#endif\n");
				break;
			    default:
				printf("#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030\n");
				printf("  " SC_SCHEMA_DECLARATION "(%s,)\n", kbuf);
				printf("#endif\n");
				break;
			}

			switch (names[i].control) {
			    case SC_10_1:
			    case SC_10_1_10_4:
				printf("#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1010) || (MAC_OS_X_VERSION_MAX_ALLOWED >= 1010)\n");
				break;
			    case SC_10_2:
				printf("#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1020) || (MAC_OS_X_VERSION_MAX_ALLOWED >= 1020)\n");
				break;
			    case SC_10_3:
				printf("#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1030) || (MAC_OS_X_VERSION_MAX_ALLOWED >= 1030)\n");
				break;
			    case SC_10_4:
				printf("#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1040) || (MAC_OS_X_VERSION_MAX_ALLOWED >= 1040)\n");
				break;
			}

			printf("  #define %-48s              \\\n",
			       kbuf);
			printf("          " SC_SCHEMA_KV "(%-48s \\\n",
			       kbuf);
			printf("                      ,%-48s \\\n",
			       vbuf);
			printf("                      ,%-48s )\n",
			       names[i].type ? names[i].type : "");

			switch (names[i].control) {
			    case SC_10_1:
			    case SC_10_1_10_4:
			    case SC_10_2:
			    case SC_10_3:
			    case SC_10_4:
				printf("#endif\n");
				break;
			}

			break;
		    case gen_cfile_e:
			snprintf(kbuf, sizeof(kbuf), KEY_PREFIX "%s%s",
				 names[i].prefix, names[i].key);

			if (names[i].value)
			    printf("const CFStringRef %-48s = CFSTR(\"%s\");\n",
				   kbuf, names[i].value);
			else
			    printf("const CFStringRef %-48s = CFSTR(\"%s\");\n",
				   kbuf, names[i].key);
			break;
		    default:
			break;
		}
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

	printf("#warning USE OF THIS HEADER HAS BEEN DEPRECATED\n");

	printf("#ifndef _SCSCHEMADEFINITIONS_H\n");
	printf("#warning Please #include <SystemConfiguration/SystemConfiguration.h> instead\n");
	printf("#warning of including this file directly.\n");
	printf("#include <SystemConfiguration/SCSchemaDefinitions.h>\n");
	printf("#endif\n\n");

//	printf("#ifndef  SCSTR\n");
//	printf("#include <CoreFoundation/CFString.h>\n");
//	printf("#define  SCSTR(s) CFSTR(s)\n");
//	printf("#endif\n\n");
//
//	dump_names(gen_header_e);
//	printf("\n");

	printf("#endif /* _SCSCHEMADEFINITIONS_10_1_H */\n");
    }
    else if (strcmp(type, "header") == 0) {
	printf("%s\n", copyright_string);
	printf("/*\n * This file is automatically generated\n * DO NOT EDIT!\n */\n\n");

	printf("/*\n");
	dump_names(gen_comments_e);
	printf(" */\n\n\n");

	printf("/*\n");
	printf(" * Note: The MACOSX_DEPLOYMENT_TARGET environment variable should be used\n");
	printf(" *       when building an application targeted for an earlier version of\n");
	printf(" *       Mac OS X.  Please reference Technical Note TN2064 for more details.\n");
	printf(" */\n\n");

	printf("/*\n");
	printf(" * Note: For Cocoa/Obj-C/Foundation applications accessing these preference\n");
	printf(" *       keys you may want to consider the following :\n");
	printf(" *\n");
	printf(" *       #define " SC_SCHEMA_DECLARATION "(k,q)\textern NSString * k;\n");
	printf(" *       #import <SystemConfiguration/SystemConfiguration.h>\n");
	printf(" */\n\n");

	printf("/*\n");
	printf(" * Note: For CFM applications using these schema keys you may want to\n");
	printf(" *       consider the following :\n");
	printf(" *\n");
	printf(" *       #define " SC_SCHEMA_DECLARATION "(k,q)\n");
	printf(" *       #define " SC_SCHEMA_KV "(k,v,t)\tlookup_SC_key( CFSTR( #k ) )\n");
	printf(" *       #include <SystemConfiguration/SystemConfiguration.h>\n");
	printf(" *\n");
	printf(" *       CFStringRef lookup_SC_key(CFStringRef key)\n");
	printf(" *       {\n");
	printf(" *         // this function should [dynamically, on-demand] load the\n");
	printf(" *         // SystemConfiguration.framework, look up the provided key,\n");
	printf(" *         // and return the associated value.\n");
	printf(" *       }\n");
	printf(" */\n\n");

	printf("/*\n");
	printf(" * Note: Earlier versions of this header file defined a \"SCSTR\" macro\n");
	printf(" *       which helped to facilitate Obj-C development. Use of this macro\n");
	printf(" *       has been deprecated (in Mac OS X 10.4) in favor of the newer\n");
	printf(" *       \"" SC_SCHEMA_DECLARATION "\" and \"" SC_SCHEMA_KV "\" macros\n");
	printf(" */\n\n\n");

	printf("#ifndef _SCSCHEMADEFINITIONS_H\n#define _SCSCHEMADEFINITIONS_H\n\n");

	printf("/* -------------------- Macro declarations -------------------- */\n\n");

	printf("#include <AvailabilityMacros.h>\n\n");

	printf("/*\n");
	printf(" * let's \"do the right thing\" for those wishing to build for\n");
	printf(" * Mac OS X 10.1.0 ... 10.2.x\n");
	printf(" */\n");

	printf("#if MAC_OS_X_VERSION_MIN_REQUIRED <= 1020\n");
	printf("    #ifndef SCSTR\n");
	printf("      #include <CoreFoundation/CFString.h>\n");
	printf("      #define SCSTR(s) CFSTR(s)\n");
	printf("    #endif\n");
	printf("  #ifndef " SC_SCHEMA_DECLARATION "\n");
	printf("    #define " SC_SCHEMA_DECLARATION "(k,q)\textern const CFStringRef k q;\n");
	printf("    #endif\n");
	printf("  #ifndef " SC_SCHEMA_KV "\n");
	printf("    #define " SC_SCHEMA_KV "(k,v,t)\tSCSTR( v )\n");
	printf("  #endif\n");
	printf("#endif\n\n");

	printf("/*\n");
	printf(" * Define a schema key/value/type tuple\n");
	printf(" */\n");
	printf("#ifndef " SC_SCHEMA_KV "\n");
	printf("  #define " SC_SCHEMA_KV "(k,v,t)\tk\n");
	printf("#endif\n\n");

	printf("/*\n");
	printf(" * Provide an \"extern\" for the key/value\n");
	printf(" */\n");
	printf("#ifndef " SC_SCHEMA_DECLARATION "\n");
	printf("  #ifndef SCSTR\n");
	printf("    #include <CoreFoundation/CFString.h>\n");
	printf("    #define " SC_SCHEMA_DECLARATION "(k,q)\textern const CFStringRef k q;\n");
	printf("  #else\n");
	printf("    #import <Foundation/NSString.h>\n");
	printf("    #define " SC_SCHEMA_DECLARATION "(k,q)\textern NSString * k q;\n");
	printf("  #endif\n");
	printf("#endif\n");

	// The SCSTR() macro should only be availble for Mac OS X 10.1.0 ... 10.4.x
	printf("#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1010) && (MAC_OS_X_VERSION_MAX_ALLOWED <= 1040)\n");
	printf("    #ifndef SCSTR\n");
	printf("      #include <CoreFoundation/CFString.h>\n");
	printf("      #define SCSTR(s) CFSTR(s)\n");
	printf("    #endif\n");
	printf("#endif\n\n\n");

	printf("/* -------------------- HeaderDoc comments -------------------- */\n\n\n");
	printf("#if\t0\n");
	printf("/*!\n");
	printf(" *\t@header SCSchemaDefinitions\n");
	printf(" */\n");
	dump_names(gen_headerdoc_e);
	printf("\n");
	printf("#endif\t/* 0 */\n\n\n");

	printf("/* -------------------- Schema declarations -------------------- */\n\n");
	dump_names(gen_hfile_e);
	printf("\n");

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

