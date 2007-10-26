/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

/*!
    @header CFOpenDirectoryConsts
    @abstract   Constants that are shared between CoreFoundation based and Objective-C based OpenDirectory APIs
    @discussion Constants that are shared between CoreFoundation based and Objective-C based OpenDirectory APIs
*/

#ifndef __CFOPENDIRECTORYCONSTS_H
#define __CFOPENDIRECTORYCONSTS_H

#include <stdint.h>

/*!
    @enum       ODNodeType
    @abstract   Types of nodes that can be opened
    @discussion Various types of nodes that can be opened.
    @constant   kODTypeAuthenticationSearchNode is a node type commonly used for all authentications or record lookups
    @constant   kODTypeContactSearchNode is a node type commonly used for applications that deal with contact data
    @constant   kODTypeNetworkSearchNode is a node type used for looking for network resource type data
    @constant   kODTypeLocalNode is a node type that specifically looks at the local directory
    @constant   kODTypeConfigNode is a node type that refers to the configuration node within DS
*/
enum
{
    kODTypeAuthenticationSearchNode     = 0x2201,   // eDSAuthenticationSearchNodeName
    kODTypeContactSearchNode            = 0x2204,   // eDSContactsSearchNodeName
    kODTypeNetworkSearchNode            = 0x2205,   // eDSNetworkSearchNodeName
    
    kODTypeLocalNode                    = 0x2200,   // eDSLocalNodeNames
    kODTypeConfigNode                   = 0x2202    // eDSConfigNodeName
};

typedef uint32_t ODNodeType;

/*!
    @enum       ODMatchType
    @abstract   Are types of matching types used for doing searches.  Each type is self explanatory based on the name.
*/
enum
{
    kODMatchEqualTo                = 0x2001,    // eDSExact
    kODMatchBeginsWith             = 0x2002,    // eDSStartsWith
    kODMatchContains               = 0x2004,    // eDSContains
    kODMatchEndsWith               = 0x2003,    // eDSEndsWith
    
    kODMatchInsensitiveEqualTo     = 0x2101,    // eDSiExact
    kODMatchInsensitiveBeginsWith  = 0x2102,    // eDSiStartsWith
    kODMatchInsensitiveContains    = 0x2104,    // eDSiContains
    kODMatchInsensitiveEndsWith    = 0x2103,    // eDSiEndsWith
    
    kODMatchGreaterThan            = 0x2006,    // eDSGreaterThan
    kODMatchLessThan               = 0x2007,    // eDSLessThan
    
    kODMatchCompoundExpression     = 0x200B     // eDSCompoundExpression
};

typedef uint32_t ODMatchType;

/*!
    @enum       ODFrameworkErrors
    @abstract   Errors specific to the framework and no underlying calls
    @discussion Errors specific to the framework and no underlying calls
    @constant   kODErrorQuerySynchronize is an error code that is returned when a synchronize has been initiated
*/
enum ODFrameworkErrors
{
    kODErrorQuerySynchronize            = 101
};

#endif
