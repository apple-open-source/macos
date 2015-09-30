/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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


//
//  SDCSPDLDatabase.h - Description t.b.d.
//
#ifndef _H_SD_CSPDLDATABASE
#define _H_SD_CSPDLDATABASE

#include <security_cdsa_plugin/Database.h>
#include <security_cdsa_plugin/DbContext.h>
#include <security_cdsa_utilities/handleobject.h>
#include <security_utilities/refcount.h>
#include <vector>

//
// SDCSPDLDatabaseManager
//
class SDCSPDLDatabaseManager : public DatabaseManager
{
public:
    Database *make(const DbName &inDbName);
};

#endif //_H_SD_CSPDLDATABASE
