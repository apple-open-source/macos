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

#ifndef __RASSCHEMADEFINITIONS_H__
#define __RASSCHEMADEFINITIONS_H__


// File names
#define	kRASServerPrefsFileName			SCSTR("com.apple.RemoteAccessServers.plist")

/*
 * Generic Keys
 */
#define kRASPropUserDefinedName                 kSCPropUserDefinedName			/* SCSTR("UserDefinedName")	CFString */
#define kRASRemoteAccessServer  		CFSTR("RemoteAccessServer")		/* RAS entity for Dynamic Store */	

/*
 * Top level entities
 */
#define kRASGlobals				CFSTR("Globals")			/*				CFDictionary */
#define	kRASActiveServers			CFSTR("ActiveServers")			/* 				CFArray */
#define kRASServers				CFSTR("Servers")			/*				CFDictionary */

/*
 * Remote Access Globals Keys
 */
#define KRASGlobPSKeyAccount			CFSTR("PSKeyAccount")			/*				CFString */
 
/*
 * Remote Access Server Entity Keys
 */
#define kRASEntDNS                             	kSCEntNetDNS				/* SCSTR("DNS") 		CFDictionary */
#define	kRASEntInterface			kSCEntNetInterface      		/* SCSTR("Interface") 		CFDictionary */
#define kRASEntIPv4				kSCEntNetIPv4				/* SCSTR("IPv4")		CFDictionary */
#define kRASEntIPv6				kSCEntNetIPv6				/* SCSTR("IPv6") 		CFDictionary */
#define kRASEntL2TP                          	kSCEntNetL2TP 				/* SCSTR("L2TP") 		CFDictionary */
#define kRASEntModem                         	kSCEntNetModem                         	/* SCSTR("Modem")		CFDictionary */
#define kRASEntPPP                           	kSCEntNetPPP                          	/* SCSTR("PPP")			CFDictionary */
#define kRASEntPPPoE                         	kSCEntNetPPPoE                          /* SCSTR("PPPoE")		CFDictionary */
#define kRASEntPPPSerial			kSCEntNetPPPSerial                      /* SCSTR("PPPSerial")		CFDictionary */
#define kRASEntPPTP				kSCEntNetPPTP                          	/* SCSTR("PPTP")		CFDictionary */
#define	kRASEntServer				SCSTR("Server")				/*				CFDictionary */
#define kRASEntDSACL				SCSTR("DSACL")				/*				CFDictionary */
#define kRASEntEAP				SCSTR("EAP")				/* CFDictionary - Prefix followed by protocol Number */

/*
 * kRASEntInterface Entity Keys
 */
#define kRASPropInterfaceType                 	kSCPropNetInterfaceType                 /* SCSTR("Type")		CFString */
#define kRASPropInterfaceSubType             	kSCPropNetInterfaceSubType              /* SCSTR("SubType")		CFString */

/*
 * kRASEntDNS Entity Keys
 */
#define kRASPropDNSServerAddresses             	kSCPropNetDNSServerAddresses		/* SCSTR("ServerAddresses")	CFArray[CFString] */
#define kRASPropDNSSearchDomains		kSCPropNetDNSSearchDomains		/* SCSTR("SearchDomains")	CFArray[CFString] */
#define kRASPropDNSOfferedServerAddresses	SCSTR("OfferedServerAddresses")		/* 				CFArray[CFString] */
#define kRASPropDNSOfferedSearchDomains		SCSTR("OfferedSearchDomains")		/* 				CFArray[CFString] */
#define kRASPropDNSOfferedSearchDomainServers	SCSTR("OfferedSearchDomainServers")	/*				CFArray[CFString] */

/*
 * kRASPropInterfaceType values
 */
#define kRASValInterfaceTypePPP                	kSCValNetInterfaceTypePPP        	/* SCSTR("PPP") */                 

/* kRASPropInterfaceSubType values (for PPP) */
#define kRASValInterfaceSubTypePPPoE           	kSCValNetInterfaceSubTypePPPoE          /* SCSTR("PPPoE") */            
#define kRASValInterfaceSubTypePPPSerial      	kSCValNetInterfaceSubTypePPPSerial      /* SCSTR("PPPSerial") */             
#define kRASValInterfaceSubTypePPTP            	kSCValNetInterfaceSubTypePPTP           /* SCSTR("PPTP") */          
#define kRASValInterfaceSubTypeL2TP            	kSCValNetInterfaceSubTypeL2TP           /* SCSTR("L2TP") */             

/*
 * kRASEntIPv4 Entity Keys
 */
#define kRASPropIPv4Addresses        		kSCPropNetIPv4Addresses			/* SCSTR("Addresses") 		CFArray[CFString] */
#define kRASPropIPv4SubnetMasks     		kSCPropNetIPv4SubnetMasks		/* SCSTR("SubnetMasks")		CFArray[CFString] */
#define kRASPropIPv4DestAddresses     		kSCPropNetIPv4DestAddresses		/* SCSTR("DestAddresses")	CFArray[CFString] */
#define kRASPropIPv4DestAddressRanges		SCSTR("DestAddressRanges")		/* 				CFArray[CFString] */
#define kRASPropIPv4RangeSubnetMasks         	SCSTR("RangeSubnetMasks")             	/* 				CFArray[CFString] */
#define kRASPropIPv4OfferedRouteAddresses	SCSTR("OfferedRouteAddresses")		/*				CFArray[CFString] */
#define kRASPropIPv4OfferedRouteMasks		SCSTR("OfferedRouteMasks")		/*				CFArray[CFString] */
#define kRASPropIPv4OfferedRouteTypes		SCSTR("OfferedRouteTypes")		/*				CFArray[CFString] */

/*
 * kRASPropIPv4OfferedRouteTypes values
 */
#define kRASValIPv4OfferedRouteTypesPrivate	SCSTR("Private")
#define kRASValIPv4OfferedRouteTypesPublic	SCSTR("Public")

/*
 * kRASEntIPv6 Entity Keys
 */
#define kRASPropIPv6Addresses         		kSCPropNetIPv6Addresses 		/* SCSTR("Addresses")		CFArray[CFString] */
#define kRASPropIPv6DestAddresses    		kSCPropNetIPv6DestAddresses		/* SCSTR("DestAddresses")	CFArray[CFString] */

/*
 * kRASEntPPP Entity Keys
 */
#define kRASPropPPPConnectTime			kSCPropNetPPPConnectTime         	/* SCSTR("ConnectTime")			CFNumber */
#define kRASPropPPPDisconnectOnIdle		kSCPropNetPPPDisconnectOnIdle		/* SCSTR("DisconnectOnIdle")		CFNumber (0 or 1) */
#define kRASPropPPPDisconnectOnIdleTimer	kSCPropNetPPPDisconnectOnIdleTimer	/* SCSTR("DisconnectOnIdleTimer") 	CFNumber */
#define kRASPropPPPDisconnectTime		kSCPropNetPPPDisconnectTime		/* SCSTR("DisconnectTime")		CFNumber */
#define kRASPropPPPLogfile			kSCPropNetPPPLogfile			/* SCSTR("Logfile")			CFString */
#define kRASPropPPPPlugins			kSCPropNetPPPPlugins			/* SCSTR("Plugins")			CFArray[CFString] */
#define kRASPropPPPSessionTimer			kSCPropNetPPPSessionTimer		/* SCSTR("SessionTimer")		CFNumber */
#define kRASPropPPPUseSessionTimer		kSCPropNetPPPUseSessionTimer		/* SCSTR("UseSessionTimer")		CFNumber (0 or 1) */
#define kRASPropPPPVerboseLogging		kSCPropNetPPPVerboseLogging		/* SCSTR("VerboseLogging")		CFNumber (0 or 1) */

/* Comm */
#define kRASPropPPPCommRemoteAddress            kSCPropNetPPPCommRemoteAddress          /* SCSTR("CommRemoteAddress")           CFString */

/* PPP Auth Plugins: */
#define kRASPropPPPAuthenticatorPlugins		SCSTR("AuthenticatorPlugins")		/*					CFArray[CFString] */
#define kRASPropPPPAuthenticatorACLPlugins	SCSTR("AuthenticatorACLPlugins")	/*					CFArray[CFString] */
#define kRASPropPPPAuthenticatorEAPPlugins	SCSTR("AuthenticatorEAPPlugins")	/*					CFArray[CFString] */

/* Auth: */
#define kRASPropPPPAuthPeerName                 SCSTR("AuthPeerName")                   /*                                      CFString */
#define kRASPropPPPAuthenticatorProtocol        SCSTR("AuthenticatorProtocol")  	/*  					CFArray[CFString] */

/* kRASPropPPPAuthProtocol values */
#define kRASValPPPAuthProtocolCHAP 		kSCValNetPPPAuthProtocolCHAP     	/* SCSTR("CHAP")			CFString */
#define kRASValPPPAuthProtocolPAP 		kSCValNetPPPAuthProtocolPAP             /* SCSTR("PAP")				CFString */
#define kRASValPPPAuthProtocolMSCHAP1 		SCSTR("MSCHAP1")             		/* 					CFString */
#define kRASValPPPAuthProtocolMSCHAP2 		SCSTR("MSCHAP2")             		/* 					CFString */
#define kRASValPPPAuthProtocolEAP 		SCSTR("EAP")             		/* 					CFString */

/* CCP: */
#define kRASPropPPPCCPEnabled     		kSCPropNetPPPCCPEnabled 		/* SCSTR("CCPEnabled") 			CFNumber (0 or 1) */
#define kRASPropPPPCCPProtocols			SCSTR("CCPProtocols")			/*					CFArray */

/* kRASPropPPPCCPProtocols values */
#define kRASValPPPCCPProtocolsMPPE		SCSTR("MPPE")	

/* MPPE option keys */
#define kRASPropPPPMPPEKeySize40		SCSTR("MPPEKeySize40")			/*					CFNumber */
#define kRASPropPPPMPPEKeySize128		SCSTR("MPPEKeySize128")			/*					CFNumber */		

/* IPCP: */
#define kRASPropPPPIPCPCompressionVJ  		kSCPropNetPPPIPCPCompressionVJ		/* SCSTR("IPCPCompressionVJ") 		CFNumber (0 or 1) */

/* LCP: */
#define kRASPropPPPLCPEchoEnabled            	kSCPropNetPPPLCPEchoEnabled 		/* SCSTR("LCPEchoEnabled")          	CFNumber (0 or 1) */
#define kRASPropPPPLCPEchoFailure            	kSCPropNetPPPLCPEchoFailure 		/* SCSTR("LCPEchoFailure")     		CFNumber */
#define kRASPropPPPLCPEchoInterval           	kSCPropNetPPPLCPEchoInterval 		/* SCSTR("LCPEchoInterval")  		CFNumber */
#define kRASPropPPPLCPCompressionACField     	kSCPropNetPPPLCPCompressionACField 	/* SCSTR("LCPCompressionACField")	CFNumber (0 or 1) */
#define kRASPropPPPLCPCompressionPField      	kSCPropNetPPPLCPCompressionPField 	/* SCSTR("LCPCompressionPField") 	CFNumber (0 or 1) */
#define kRASPropPPPLCPMRU                    	kSCPropNetPPPLCPMRU 			/* SCSTR("LCPMRU")      		CFNumber */
#define kRASPropPPPLCPMTU                    	kSCPropNetPPPLCPMTU 			/* SCSTR("LCPMTU")           		CFNumber */
#define kRASPropPPPLCPReceiveACCM            	kSCPropNetPPPLCPReceiveACCM 		/* SCSTR("LCPReceiveACCM")    		CFNumber */
#define kRASPropPPPLCPTransmitACCM           	kSCPropNetPPPLCPTransmitACCM 		/* SCSTR("LCPTransmitACCM")   		CFNumber */

/* ACSP: */
#define kRASPropPPPACSPEnabled			SCSTR("ACSPEnabled")			/* 					CFNumber */

/*
 * kRASEntPPPoE Entity Keys
 */
/* RESERVED FOR FUTURE USE */

/*
 * kRASEntPPPSerial Entity Keys
 */
/* RESERVED FOR FUTURE USE */

/*
 * kRASEntPPTP Entity Keys
 */
/* RESERVED FOR FUTURE USE */

/*
 * kRASEntL2TP Entity Keys
 */
#define kRASPropL2TPTransport           	kSCPropNetL2TPTransport			/* SCSTR("Transport") 			CFString */

/* kRASPropL2TPTransport values */
#define kRASValL2TPTransportIP               	kSCValNetL2TPTransportIP		/* SCSTR("IP") */                        
#define kRASValL2TPTransportIPSec           	kSCValNetL2TPTransportIPSec 		/* SCSTR("IPSec") */                       

#define kRASPropL2TPIPSecSharedSecret           SCSTR("IPSecSharedSecret") 		/* 					CFString */
#define kRASPropL2TPIPSecSharedSecretEncryption SCSTR("IPSecSharedSecretEncryption") 	/* 					CFString */

/* kRASPropL2TPIPSecSharedSecretEncryption values */
#define kRASValL2TPIPSecSharedSecretEncryptionKey	SCSTR("Key")                       
#define kRASValL2TPIPSecSharedSecretEncryptionKeychain	SCSTR("Keychain")                       

/*
 * kRASDSAccessControl Entity Keys
 */
#define kRASPropDSACLGroup			SCSTR("Group")				/* 					CFString */

/*
 * kRASEntServer Entity Keys
 */
#define kRASPropServerMaximumSessions		SCSTR("MaximumSessions")		/*					CFNumber */
#define kRASPropServerLogfile			SCSTR("Logfile")			/* 					CFString */
#define kRASPropServerVerboseLogging		SCSTR("VerboseLogging")			/* 					CFNumber (0 or 1) */

#endif


