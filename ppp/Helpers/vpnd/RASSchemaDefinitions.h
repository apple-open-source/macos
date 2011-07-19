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
#define	kRASServerPrefsFileName			CFSTR("com.apple.RemoteAccessServers.plist")

/*
 * Generic Keys
 */
#define kRASPropUserDefinedName			kSCPropUserDefinedName			/* CFSTR("UserDefinedName")	CFString */
#define kRASRemoteAccessServer			CFSTR("RemoteAccessServer")		/* RAS entity for Dynamic Store */	

/*
 * Top level entities
 */
#define kRASGlobals						CFSTR("Globals")			/*				CFDictionary */
#define	kRASActiveServers				CFSTR("ActiveServers")		/* 				CFArray */
#define kRASServers						CFSTR("Servers")			/*				CFDictionary */

/*
 * Remote Access Globals Keys
 */
#define KRASGlobPSKeyAccount			CFSTR("PSKeyAccount")		/*				CFString */
 
/*
 * Remote Access Server Entity Keys
 */
#define kRASEntDNS						kSCEntNetDNS				/* CFSTR("DNS") 		CFDictionary */
#define	kRASEntInterface				kSCEntNetInterface			/* CFSTR("Interface")   CFDictionary */
#define kRASEntIPv4						kSCEntNetIPv4				/* CFSTR("IPv4")		CFDictionary */
#define kRASEntIPv6						kSCEntNetIPv6				/* CFSTR("IPv6") 		CFDictionary */
#define kRASEntL2TP						kSCEntNetL2TP 				/* CFSTR("L2TP") 		CFDictionary */
#define kRASEntModem					kSCEntNetModem				/* CFSTR("Modem")		CFDictionary */
#define kRASEntPPP						kSCEntNetPPP				/* CFSTR("PPP")			CFDictionary */
#define kRASEntPPPoE					kSCEntNetPPPoE				/* CFSTR("PPPoE")		CFDictionary */
#define kRASEntPPPSerial				kSCEntNetPPPSerial			/* CFSTR("PPPSerial")   CFDictionary */
#define kRASEntPPTP						kSCEntNetPPTP				/* CFSTR("PPTP")		CFDictionary */
#define	kRASEntServer					CFSTR("Server")				/*						CFDictionary */
#define kRASEntDSACL					CFSTR("DSACL")				/*						CFDictionary */
#define kRASEntRadius					CFSTR("Radius")				/*						CFDictionary */
#define kRASEntEAP						CFSTR("EAP")				/* CFDictionary - Prefix followed by protocol Number */
#define kRASEntIPSec					CFSTR("IPSec")				/* CFDictionary */

/*
 * kRASEntInterface Entity Keys
 */
#define kRASPropInterfaceType			kSCPropNetInterfaceType				/* CFSTR("Type")		CFString */
#define kRASPropInterfaceSubType		kSCPropNetInterfaceSubType			/* CFSTR("SubType")		CFString */

/*
 * kRASEntDNS Entity Keys
 */
#define kRASPropDNSServerAddresses				kSCPropNetDNSServerAddresses		/* CFSTR("ServerAddresses")	CFArray[CFString] */
#define kRASPropDNSSearchDomains				kSCPropNetDNSSearchDomains			/* CFSTR("SearchDomains")	CFArray[CFString] */
#define kRASPropDNSOfferedServerAddresses		CFSTR("OfferedServerAddresses")		/*							CFArray[CFString] */
#define kRASPropDNSOfferedSearchDomains			CFSTR("OfferedSearchDomains")		/*							CFArray[CFString] */
#define kRASPropDNSOfferedSearchDomainServers	CFSTR("OfferedSearchDomainServers")	/*							CFArray[CFString] */

/*
 * kRASPropInterfaceType values
 */
#define kRASValInterfaceTypePPP                	kSCValNetInterfaceTypePPP        	/* CFSTR("PPP") */                 
#define kRASValInterfaceTypeIPSec				CFSTR("IPSec")

/* kRASPropInterfaceSubType values (for PPP) */
#define kRASValInterfaceSubTypePPPoE           	kSCValNetInterfaceSubTypePPPoE          /* CFSTR("PPPoE") */            
#define kRASValInterfaceSubTypePPPSerial      	kSCValNetInterfaceSubTypePPPSerial      /* CFSTR("PPPSerial") */             
#define kRASValInterfaceSubTypePPTP            	kSCValNetInterfaceSubTypePPTP           /* CFSTR("PPTP") */          
#define kRASValInterfaceSubTypeL2TP            	kSCValNetInterfaceSubTypeL2TP           /* CFSTR("L2TP") */             

/*
 * kRASEntIPv4 Entity Keys
 */
#define kRASPropIPv4Addresses        		kSCPropNetIPv4Addresses			/* CFSTR("Addresses") 		CFArray[CFString] */
#define kRASPropIPv4SubnetMasks     		kSCPropNetIPv4SubnetMasks		/* CFSTR("SubnetMasks")		CFArray[CFString] */
#define kRASPropIPv4DestAddresses     		kSCPropNetIPv4DestAddresses		/* CFSTR("DestAddresses")	CFArray[CFString] */
#define kRASPropIPv4DestAddressRanges		CFSTR("DestAddressRanges")		/*							CFArray[CFString] */
#define kRASPropIPv4RangeSubnetMasks		CFSTR("RangeSubnetMasks")		/*							CFArray[CFString] */
#define kRASPropIPv4OfferedRouteAddresses	CFSTR("OfferedRouteAddresses")  /*							CFArray[CFString] */
#define kRASPropIPv4OfferedRouteMasks		CFSTR("OfferedRouteMasks")		/*							CFArray[CFString] */
#define kRASPropIPv4OfferedRouteTypes		CFSTR("OfferedRouteTypes")		/*							CFArray[CFString] */

/*
 * kRASPropIPv4OfferedRouteTypes values
 */
#define kRASValIPv4OfferedRouteTypesPrivate	CFSTR("Private")
#define kRASValIPv4OfferedRouteTypesPublic	CFSTR("Public")

/*
 * kRASEntIPv6 Entity Keys
 */
#define kRASPropIPv6Addresses         		kSCPropNetIPv6Addresses 		/* CFSTR("Addresses")		CFArray[CFString] */
#define kRASPropIPv6DestAddresses    		kSCPropNetIPv6DestAddresses		/* CFSTR("DestAddresses")	CFArray[CFString] */

/*
 * kRASEntPPP Entity Keys
 */
#define kRASPropPPPConnectTime				kSCPropNetPPPConnectTime         	/* CFSTR("ConnectTime")				CFNumber */
#define kRASPropPPPDisconnectOnIdle			kSCPropNetPPPDisconnectOnIdle		/* CFSTR("DisconnectOnIdle")		CFNumber (0 or 1) */
#define kRASPropPPPDisconnectOnIdleTimer	kSCPropNetPPPDisconnectOnIdleTimer	/* CFSTR("DisconnectOnIdleTimer") 	CFNumber */
#define kRASPropPPPDisconnectTime			kSCPropNetPPPDisconnectTime			/* CFSTR("DisconnectTime")			CFNumber */
#define kRASPropPPPLogfile					kSCPropNetPPPLogfile				/* CFSTR("Logfile")					CFString */
#define kRASPropPPPPlugins					kSCPropNetPPPPlugins				/* CFSTR("Plugins")					CFArray[CFString] */
#define kRASPropPPPSessionTimer				kSCPropNetPPPSessionTimer			/* CFSTR("SessionTimer")			CFNumber */
#define kRASPropPPPUseSessionTimer			kSCPropNetPPPUseSessionTimer		/* CFSTR("UseSessionTimer")			CFNumber (0 or 1) */
#define kRASPropPPPVerboseLogging			kSCPropNetPPPVerboseLogging			/* CFSTR("VerboseLogging")			CFNumber (0 or 1) */
#define kRASPropPPPInterceptDHCP			CFSTR("InterceptDHCP")				/*									CFNumber (0 or 1) */

/* Comm */
#define kRASPropPPPCommRemoteAddress		kSCPropNetPPPCommRemoteAddress		/* CFSTR("CommRemoteAddress")		CFString */

/* PPP Auth Plugins: */
#define kRASPropPPPAuthenticatorPlugins		CFSTR("AuthenticatorPlugins")		/*									CFArray[CFString] */
#define kRASPropPPPAuthenticatorACLPlugins	CFSTR("AuthenticatorACLPlugins")	/*									CFArray[CFString] */
#define kRASPropPPPAuthenticatorEAPPlugins	CFSTR("AuthenticatorEAPPlugins")	/*									CFArray[CFString] */

/* Auth: */
#define kRASPropPPPAuthPeerName				CFSTR("AuthPeerName")				/*									CFString */
#define kRASPropPPPAuthenticatorProtocol	CFSTR("AuthenticatorProtocol")  	/*									CFArray[CFString] */

/* kRASPropPPPAuthProtocol values */
#define kRASValPPPAuthProtocolCHAP			kSCValNetPPPAuthProtocolCHAP     	/* CFSTR("CHAP")					CFString */
#define kRASValPPPAuthProtocolPAP			kSCValNetPPPAuthProtocolPAP			/* CFSTR("PAP")						CFString */
#define kRASValPPPAuthProtocolMSCHAP1 		CFSTR("MSCHAP1")             		/*									CFString */
#define kRASValPPPAuthProtocolMSCHAP2 		CFSTR("MSCHAP2")             		/*									CFString */
#define kRASValPPPAuthProtocolEAP			CFSTR("EAP")						/*									CFString */

/* CCP: */
#define kRASPropPPPCCPEnabled				kSCPropNetPPPCCPEnabled				/* CFSTR("CCPEnabled")				CFNumber (0 or 1) */
#define kRASPropPPPCCPProtocols				CFSTR("CCPProtocols")				/*									CFArray */

/* kRASPropPPPCCPProtocols values */
#define kRASValPPPCCPProtocolsMPPE			CFSTR("MPPE")	

/* MPPE option keys */
#define kRASPropPPPMPPEKeySize40			CFSTR("MPPEKeySize40")				/*									CFNumber */
#define kRASPropPPPMPPEKeySize128			CFSTR("MPPEKeySize128")				/*									CFNumber */		

/* IPCP: */
#define kRASPropPPPIPCPCompressionVJ  		kSCPropNetPPPIPCPCompressionVJ		/* CFSTR("IPCPCompressionVJ") 		CFNumber (0 or 1) */

/* LCP: */
#define kRASPropPPPLCPEchoEnabled			kSCPropNetPPPLCPEchoEnabled 		/* CFSTR("LCPEchoEnabled")			CFNumber (0 or 1) */
#define kRASPropPPPLCPEchoFailure			kSCPropNetPPPLCPEchoFailure 		/* CFSTR("LCPEchoFailure")     		CFNumber */
#define kRASPropPPPLCPEchoInterval			kSCPropNetPPPLCPEchoInterval 		/* CFSTR("LCPEchoInterval")  		CFNumber */
#define kRASPropPPPLCPCompressionACField	kSCPropNetPPPLCPCompressionACField 	/* CFSTR("LCPCompressionACField")	CFNumber (0 or 1) */
#define kRASPropPPPLCPCompressionPField		kSCPropNetPPPLCPCompressionPField 	/* CFSTR("LCPCompressionPField") 	CFNumber (0 or 1) */
#define kRASPropPPPLCPMRU					kSCPropNetPPPLCPMRU					/* CFSTR("LCPMRU")					CFNumber */
#define kRASPropPPPLCPMTU					kSCPropNetPPPLCPMTU					/* CFSTR("LCPMTU")           		CFNumber */
#define kRASPropPPPLCPReceiveACCM			kSCPropNetPPPLCPReceiveACCM 		/* CFSTR("LCPReceiveACCM")    		CFNumber */
#define kRASPropPPPLCPTransmitACCM			kSCPropNetPPPLCPTransmitACCM 		/* CFSTR("LCPTransmitACCM")   		CFNumber */

/* ACSP: */
#define kRASPropPPPACSPEnabled				CFSTR("ACSPEnabled")				/*									CFNumber */

/*
 * kRASEntPPPoE Entity Keys
 */
#define kRASPropPPPoEDeviceName           	CFSTR("DeviceName")					/*									CFString */
#define kRASPropPPPoEServiceName           	CFSTR("ServiceName")				/*									CFString */
#define kRASPropPPPoEAccessConcentratorName CFSTR("AccessConcentratorName")		/*									CFString */


/*
 * kRASEntRadius Entity Keys
 */
#define kRASPropRadiusServers				CFSTR("Servers")					/*									CFArray */
#define kRASPropRadiusNASIPAddress			CFSTR("NASIPAddress")				/*									CFString */
#define kRASPropRadiusNASIdentifier			CFSTR("NASIdentifier")				/*									CFString */
#define kRASPropRadiusNASPortType			CFSTR("NASPortType")				/*									Integer */
#define kRASPropRadiusTunnelType			CFSTR("TunnelType")					/*									Integer */


#define kRASPropRadiusServerAddress			CFSTR("Address")					/*									CFString */
#define kRASPropRadiusServerSharedSecret	CFSTR("SharedSecret")				/*									CFString */
#define kRASPropRadiusServerSharedSecretEncryption CFSTR("SharedSecretEncryption")	/*								CFString */
#define kRASPropRadiusServerPort			CFSTR("Port")						/*									CFNumber */
#define kRASPropRadiusServerTimeout			CFSTR("Timeout")					/*									CFNumber */
#define kRASPropRadiusServerRetries			CFSTR("Retries")					/*									CFNumber */
#define kRASPropRadiusServerAuthProtocol	CFSTR("AuthProtocol")				/*									CFArray */
/* kRASPropRadiusServerAuthProtocols values are listed in kRASPropPPPAuthProtocol */

/* kRASPropRadiusSharedSecretEncryption values */
#define kRASValRadiusSharedSecretEncryptionKey		CFSTR("Key")                       
#define kRASValRadiusSharedSecretEncryptionKeychain	CFSTR("Keychain")                       

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
#define kRASPropL2TPTransport           	kSCPropNetL2TPTransport				/* CFSTR("Transport")				CFString */

/* kRASPropL2TPTransport values */
#define kRASValL2TPTransportIP				kSCValNetL2TPTransportIP			/* CFSTR("IP") */                        
#define kRASValL2TPTransportIPSec			kSCValNetL2TPTransportIPSec 		/* CFSTR("IPSec") */                       

#define kRASPropL2TPIPSecSharedSecret           CFSTR("IPSecSharedSecret")				/*							CFString */
#define kRASPropL2TPIPSecSharedSecretEncryption CFSTR("IPSecSharedSecretEncryption") 	/*							CFString */

/* kRASPropL2TPIPSecSharedSecretEncryption values */
#define kRASValL2TPIPSecSharedSecretEncryptionKey		CFSTR("Key")                       
#define kRASValL2TPIPSecSharedSecretEncryptionKeychain	CFSTR("Keychain")                       

/*
 * kRASEntIPSec Entity Keys
 */

/* Phase 1 */
#define kRASPropIPSecExchangeMode				CFSTR("ExchangeMode")			/*									CFArray */
#define kRASPropIPSecProposalsBehavior			CFSTR("ProposalsBehavior") 		/*									CFString */
#define kRASPropIPSecLocalIdentifier			CFSTR("LocalIdentifier") 		/*									CFString */
#define kRASPropIPSecRemoteIdentifier			CFSTR("RemoteIdentifier") 		/*									CFString */
#define kRASPropIPSecLocalAddress				CFSTR("LocalAddress")			/*									CFString */
#define kRASPropIPSecRemoteAddress				CFSTR("RemoteAddress")			/*									CFString */
#define kRASPropIPSecSharedSecret				CFSTR("SharedSecret")			/*									CFString */
#define kRASPropIPSecSharedSecretEncryption		CFSTR("SharedSecretEncryption") /*									CFString */
#define kRASPropIPSecNonceSize					CFSTR("NonceSize")				/*									CFNumber */
#define kRASPropIPSecIdentifierVerification		CFSTR("IdentifierVerification") /*									CFString */
#define kRASPropIPSecLocalCertificate			CFSTR("LocalCertificate") 		/*									CFData */
#define kRASPropIPSecNattMultipleUsersEnabled	CFSTR("NattMultipleUsersEnabled")	/*						CFNumber (0 or 1) */
#define kRASPropIPSecNattKeepAliveEnabled		CFSTR("NattKeepAliveEnabled")	/*							CFNumber (0 or 1) */
#define kRASPropIPSecNattKeepAliveTimer			CFSTR("NattKeepAliveTimer")		/*									CFNumber */
#define kRASPropIPSecXAuthEnabled				CFSTR("XAuthEnabled")			/*							CFNumber (0 or 1) */
#define kRASPropIPSecXAuthName					CFSTR("XAuthName")				/*								CFString */
#define kRASPropIPSecXAuthPassword				CFSTR("XAuthPassword")				/*								CFString */
#define kRASPropIPSecModeConfigEnabled			CFSTR("ModeConfigEnabled")		/*							CFNumber (0 or 1) */
#define kRASPropIPSecAuthenticationMethod		CFSTR("AuthenticationMethod")   /*									CFString */
#define kRASPropIPSecDeadPeerDetectionEnabled	CFSTR("DeadPeerDetectionEnabled")	/*						CFNumber (0 or 1) */
#define kRASPropIPSecDeadPeerDetectionDelay		CFSTR("DeadPeerDetectionDelay")	/*									CFNumber */
#define kRASPropIPSecDeadPeerDetectionRetry		CFSTR("DeadPeerDetectionRetry")	/*									CFNumber */
#define kRASPropIPSecDeadPeerDetectionMaxFail	CFSTR("DeadPeerDetectionMaxFail")	/*								CFNumber */
#define kRASPropIPSecBlackHoleDetectionEnabled	CFSTR("BlackHoleDetectionEnabled") 	/*						CFNumber (0 or 1) */
#define kRASPropIPSecDisconnectOnIdle			CFSTR("DisconnectOnIdle")		/*							CFNumber (0 or 1) */
#define kRASPropIPSecDisconnectOnIdleTimer		CFSTR("DisconnectOnIdleTimer") 	/*									CFNumber */

/* kRASPropIPSecExchangeMode values */
#define kRASValIPSecExchangeModeMain				CFSTR("Main")                       
#define kRASValIPSecExchangeModeBase				CFSTR("Base")                       
#define kRASValIPSecExchangeModeAggressive			CFSTR("Aggressive")                       

/* kRASPropIPSecIdentifierVerification values */
#define kRASValIPSecIdentifierVerificationNone							CFSTR("None")                       
#define kRASValIPSecIdentifierVerificationGenerateFromRemoteAddress		CFSTR("GenerateFromRemoteAddress")                       
#define kRASValIPSecIdentifierVerificationUseRemoteIdentifier			CFSTR("UseRemoteIdentifier")                       
#define kRASValIPSecIdentifierVerificationUseOpenDirectory				CFSTR("UseOpenDirectory")                       

/* kRASPropIPSecProposalsBehavior values */
#define kRASValIPSecProposalsBehaviorClaim			CFSTR("Claim")                       
#define kRASValIPSecProposalsBehaviorObey			CFSTR("Obey")                       
#define kRASValIPSecProposalsBehaviorStrict			CFSTR("Strict")                       
#define kRASValIPSecProposalsBehaviorExact			CFSTR("Exact")                
       
/* kRASPropIPSecSharedSecretEncryption values */
#define kRASValIPSecSharedSecretEncryptionKey		CFSTR("Key")                       
#define kRASValIPSecSharedSecretEncryptionKeychain	CFSTR("Keychain")                       

/* kRASPropIPSecAuthenticationMethod values */
#define kRASValIPSecAuthenticationMethodSharedSecret	CFSTR("SharedSecret")                       
#define kRASValIPSecAuthenticationMethodCertificate		CFSTR("Certificate")                       
#define kRASValIPSecAuthenticationMethodHybrid			CFSTR("Hybrid")                       


/* Phase 1 ISAKMP Proposals*/
#define kRASPropIPSecProposals						CFSTR("Proposals")					/*						CFArray */

#define kRASPropIPSecProposalLifetime					CFSTR("Lifetime")				/*						CFNumber */
#define kRASPropIPSecProposalDHGroup					CFSTR("DHGroup")				/*						CFNumber */
#define kRASPropIPSecProposalEncryptionAlgorithm		CFSTR("EncryptionAlgorithm")	/*						CFString */
#define kRASPropIPSecProposalHashAlgorithm				CFSTR("HashAlgorithm")			/*						CFString */

/* kRASPropIPSecProposalEncryptionAlgorithm values */
#define kRASValIPSecProposalEncryptionAlgorithmDES		CFSTR("DES")                       
#define kRASValIPSecProposalEncryptionAlgorithm3DES		CFSTR("3DES")                       
#define kRASValIPSecProposalEncryptionAlgorithmAES		CFSTR("AES")                       
#define kRASValIPSecProposalEncryptionAlgorithmAES256	CFSTR("AES256")                       

/* kRASPropIPSecProposalHashAlgorithm values */
#define kRASValIPSecProposalHashAlgorithmMD5			CFSTR("MD5")                       
#define kRASValIPSecProposalHashAlgorithmSHA1			CFSTR("SHA1")                       


/* Policies */
#define kRASPropIPSecPolicies					CFSTR("Policies")			/*										CFArray */

#define kRASPropIPSecPolicyLocalAddress			CFSTR("LocalAddress") 		/*										CFString */
#define kRASPropIPSecPolicyLocalPrefix			CFSTR("LocalPrefix") 		/*										CFNumber */
#define kRASPropIPSecPolicyLocalPort			CFSTR("LocalPort")			/*										CFNumber */
#define kRASPropIPSecPolicyRemoteAddress		CFSTR("RemoteAddress") 		/*										CFString */
#define kRASPropIPSecPolicyRemotePrefix			CFSTR("RemotePrefix") 		/*										CFNumber */
#define kRASPropIPSecPolicyRemotePort			CFSTR("RemotePort") 		/*										CFNumber */
#define kRASPropIPSecPolicyLevel				CFSTR("Level")				/*										CFString */
#define kRASPropIPSecPolicyMode					CFSTR("Mode")				/*										CFString */
#define kRASPropIPSecPolicyProtocol				CFSTR("Protocol")			/*										CFNumber */
#define kRASPropIPSecPolicyDirection			CFSTR("Direction")			/*										CFSring */

/* in kRASPropIPSecPolicyLevel is kRASValIPSecPolicyLevelRequire */
#define kRASPropIPSecPolicyLifetime					CFSTR("Lifetime")				/*						CFNumber */
#define kRASPropIPSecPolicyPFSGroup					CFSTR("PFSGroup")				/*						CFNumber */
#define kRASPropIPSecPolicyEncryptionAlgorithm		CFSTR("EncryptionAlgorithm")	/*						CFArray */
#define kRASPropIPSecPolicyCompressionAlgorithm		CFSTR("CompressionAlgorithm")   /*						CFArray */
#define kRASPropIPSecPolicyHashAlgorithm			CFSTR("HashAlgorithm")			/*						CFArray */

/* kRASPropIPSecPolicyLevel values */
#define kRASValIPSecPolicyLevelNone				CFSTR("None")                       
#define kRASValIPSecPolicyLevelRequire			CFSTR("Require")                       
#define kRASValIPSecPolicyLevelDiscard			CFSTR("Discard")                       
#define kRASValIPSecPolicyLevelUnique			CFSTR("Unique")                       

/* kRASPropIPSecPolicyMode values */
#define kRASValIPSecPolicyModeTransport			CFSTR("Transport")                       
#define kRASValIPSecPolicyModeTunnel			CFSTR("Tunnel")                       

/* kRASPropIPSecPolicyDirection values */
#define kRASValIPSecPolicyDirectionIn			CFSTR("In")                       
#define kRASValIPSecPolicyDirectionOut			CFSTR("Out")                       
#define kRASValIPSecPolicyDirectionInOut		CFSTR("InOut")                       

/* kRASPropIPSecPolicyEncryptionAlgorithm values */
#define kRASValIPSecPolicyEncryptionAlgorithmDES		CFSTR("DES")                       
#define kRASValIPSecPolicyEncryptionAlgorithm3DES		CFSTR("3DES")                       
#define kRASValIPSecPolicyEncryptionAlgorithmAES		CFSTR("AES")                       
#define kRASValIPSecPolicyEncryptionAlgorithmAES256		CFSTR("AES256")

/* kRASPropIPSecPolicyCompressionAlgorithm values */
#define kRASValIPSecPolicyCompressionAlgorithmDeflate	CFSTR("Deflate")                       

/* kRASPropIPSecPolicyHashAlgorithm values */
#define kRASValIPSecPolicyHashAlgorithmSHA1				CFSTR("SHA1")                       
#define kRASValIPSecPolicyHashAlgorithmMD5				CFSTR("MD5")                       


/*
 * kRASDSAccessControl Entity Keys
 */
#define kRASPropDSACLGroup			CFSTR("Group")							/* 					CFString */

/*
 * kRASEntServer Entity Keys
 */
#define kRASPropServerMaximumSessions		CFSTR("MaximumSessions")		/*					CFNumber */
#define kRASPropServerLogfile				CFSTR("Logfile")				/* 					CFString */
#define kRASPropServerVerboseLogging		CFSTR("VerboseLogging")			/* 					CFNumber (0 or 1) */
#define kRASPropServerLoadBalancingEnabled	CFSTR("LoadBalancingEnabled")	/* 					CFNumber (0 or 1) */
#define kRASPropServerLoadBalancingAddress	CFSTR("LoadBalancingAddress")	/* 					CFString */
//#define kRASPropServerLoadBalancingInterface	CFSTR("LoadBalancingInterface")	/* 					CFString */
//#define kRASPropServerLoadBalancingPriority	CFSTR("LoadBalancingPriority")	/* 					CFNumber (1 or 10) */
#define kRASPropServerLoadBalancingPort		CFSTR("LoadBalancingPort")		/*					CFNumber */

#endif


