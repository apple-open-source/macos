/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// netparameters - ParameterSource keys for network protocol parameters
//
#ifndef _H_NETPARAMETERS
#define _H_NETPARAMETERS

#include "parameters.h"


namespace Security {
namespace Network {


enum {
    // generic (potentially applies to all protocols)
    kNetworkGenericUsername			= PARAMKEY(0x00001,string), // username for simple auth
    kNetworkGenericPassword			= PARAMKEY(0x00002,string),	// password for simple auths
    kNetworkRestartPosition			= PARAMKEY(0x00003,integer), // byte position to restart from
    
    // generic but for proxy use only (all proxy protocols)
    kNetworkGenericProxyUsername	= PARAMKEY(0x00100,string),	// username for proxy
    kNetworkGenericProxyPassword	= PARAMKEY(0x00101,string),	// password for proxy

    // FTP protocol specific
    kNetworkFtpPassiveTransfers		= PARAMKEY(0x01001,bool),	// use passive mode transfers
    kNetworkFtpTransferMode			= PARAMKEY(0x01002,string),	// transfer mode ("A" or "I")
    kNetworkFtpUniqueStores			= PARAMKEY(0x01003,bool),	// request unique stores (STOU)

    // HTTP/HTTPS protocol specific
    kNetworkHttpCommand				= PARAMKEY(0x02001,string),	// access command (GET et al.)
    kNetworkHttpUserAgent			= PARAMKEY(0x02002,string),	// User-Agent: value
    kNetworkHttpMoreHeaders			= PARAMKEY(0x02003,string),	// arbitrary more headers
    kNetworkHttpAcceptExpiredCerts	= PARAMKEY(0x02004,bool),	// accept expired certs
    kNetworkHttpAcceptUnknownRoots	= PARAMKEY(0x02005,bool),	// accept untrusted root certificates
    kNetworkHttpPostContentType		= PARAMKEY(0x02006,string),	// Content-Type: for posted data
    kNetworkHttpUseVersion			= PARAMKEY(0x02007,integer), // subversion of HTTP/1 to use
    
    // Legacy interface use ONLY. Not valid for modern use
    kNetworkLegacyIsSecure			= PARAMKEY(0x100001,bool),	// secure connection (SSL)
    kNetworkLegacyRespHeader		= PARAMKEY(0x100002,string), // collected response headers
    kNetworkLegacyReqBody			= PARAMKEY(0x100003,data), // request body (in memory, as string)
    
    kNetworkLegacyResourceSize		= PARAMKEY(0x100004,integer),
    kNetworkLegacyFileType			= PARAMKEY(0x100005,integer),
    kNetworkLegacyFileCreator		= PARAMKEY(0x100006,integer),
    kNetworkLegacyLastModifiedTime	= PARAMKEY(0x100007,integer),
    kNetworkLegacyMIMEType			= PARAMKEY(0x100008,string),
    kNetworkLegacyResourceName			= PARAMKEY(0x100009,string),
    
    // @@@ mistakenly added -- to be removed
    kNetworkGenericURL				= PARAMKEY(0x00004,string),
    kNetworkGenericHost				= PARAMKEY(0x00005,string)
};


}	// end namespace Security
}	// end namespace Network

#endif //_H_UAPARAMETERS
