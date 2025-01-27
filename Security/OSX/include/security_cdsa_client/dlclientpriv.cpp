/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// dlclientpriv - private client interface to CSSM DLs
//
// This file implements those (non-virtual) methods of Db/DbImpl that
// require additional libraries to function. The OS X linker is too inept
// to eliminate unused functions peacefully (as of OS X 10.3/XCode 1.5 anyway).
//
#include <security_cdsa_client/dlclient.h>
#include <security_cdsa_client/aclclient.h>
#include <securityd_client/ssclient.h>

using namespace CssmClient;


//
// Currently empty.
//
