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


#define SYSLOG_NAMES	// compile syslog name tables

/* Headers. */
#include <cdsa_utilities/acl_password.h>
#include <cdsa_utilities/acl_any.h>
#include <cdsa_utilities/acl_threshold.h>
#include <cdsa_utilities/acl_process.h>
#include <cdsa_utilities/AtomicFile.h>
#include <cdsa_utilities/callback.h>
#include <cdsa_utilities/context.h>
#include <cdsa_utilities/cssm_adt_utils.h>
#include <cdsa_utilities/cssmacl.h>
#include <cdsa_utilities/cssmalloc.h>
#include <cdsa_utilities/cssmdata.h>
#include <cdsa_utilities/cssmdates.h>
#include <cdsa_utilities/cssmdb.h>
#include <cdsa_utilities/cssmerrno.h>
#include <cdsa_utilities/cssmlist.h>
#include <cdsa_utilities/Database.h>
#include <cdsa_utilities/DatabaseSession.h>
#include <cdsa_utilities/DbContext.h>
#include <cdsa_utilities/DbName.h>
#include <cdsa_utilities/DbQuery.h>
#include <cdsa_utilities/globalizer.h>
#include <cdsa_utilities/handleobject.h>
#include <cdsa_utilities/memutils.h>
#include <cdsa_utilities/modloader.h>
#include <cdsa_utilities/os9utils.h>
#include <cdsa_utilities/refcount.h>
#include <cdsa_utilities/threading.h>
#include <cdsa_utilities/utilities.h>
#include <cdsa_utilities/utility_config.h>
#include <cdsa_utilities/walkers.h>
#include <cdsa_utilities/cssmwalkers.h>
#include <cdsa_utilities/cssmaclpod.h>
#include <cdsa_utilities/mach++.h>
#include <cdsa_utilities/machserver.h>
#include <cdsa_utilities/cssmcred.h>
#include <cdsa_utilities/mach_notify.h>
#include <cdsa_utilities/machrunloopserver.h>
#include <cdsa_utilities/AppleDatabase.h>
#include <cdsa_utilities/DbIndex.h>
#include <cdsa_utilities/DbValue.h>
#include <cdsa_utilities/MetaAttribute.h>
#include <cdsa_utilities/MetaRecord.h>
#include <cdsa_utilities/ReadWriteSection.h>
#include <cdsa_utilities/SelectionPredicate.h>
#include <cdsa_utilities/debugging.h>
#include <cdsa_utilities/debugsupport.h>
#include <cdsa_utilities/logging.h>
#include <cdsa_utilities/tqueue.h>
#include <cdsa_utilities/codesigning.h>
#include <cdsa_utilities/acl_codesigning.h>
#include <cdsa_utilities/osxsigning.h>
#include <cdsa_utilities/daemon.h>
#include <cdsa_utilities/acl_comment.h>

/* Source files. */
#include <cdsa_utilities/acl_password.cpp>
#include <cdsa_utilities/AtomicFile.cpp>
#include <cdsa_utilities/callback.cpp>
#include <cdsa_utilities/context.cpp>
#include <cdsa_utilities/cssm_adt_utils.cpp>
#include <cdsa_utilities/cssmacl.cpp>
#include <cdsa_utilities/cssmalloc.cpp>
#include <cdsa_utilities/cssmdata.cpp>
#include <cdsa_utilities/cssmdates.cpp>
#include <cdsa_utilities/cssmdb.cpp>
#include <cdsa_utilities/cssmerrno.cpp>
#include <cdsa_utilities/cssmlist.cpp>
#include <cdsa_utilities/Database.cpp>
#include <cdsa_utilities/DatabaseSession.cpp>
#include <cdsa_utilities/DbContext.cpp>
#include <cdsa_utilities/DbName.cpp>
#include <cdsa_utilities/DbQuery.cpp>
#include <cdsa_utilities/globalizer.cpp>
#include <cdsa_utilities/guids.cpp>
#include <cdsa_utilities/handleobject.cpp>
#include <cdsa_utilities/modloader.cpp>
#include <cdsa_utilities/modloader9.cpp>
#include <cdsa_utilities/os9utils.cpp>
#include <cdsa_utilities/threading.cpp>
#include <cdsa_utilities/utilities.cpp>
#include <cdsa_utilities/walkers.cpp>
#include <cdsa_utilities/cssmwalkers.cpp>
#include <cdsa_utilities/acl_any.cpp>
#include <cdsa_utilities/cssmaclpod.cpp>
#include <cdsa_utilities/mach++.cpp>
#include <cdsa_utilities/machserver.cpp>
#include <cdsa_utilities/mach_notify.c>
#include <cdsa_utilities/cssmcred.cpp>
#include <cdsa_utilities/acl_threshold.cpp>
#include <cdsa_utilities/acl_process.cpp>
#include <cdsa_utilities/machrunloopserver.cpp>
#include <cdsa_utilities/AppleDatabase.cpp>
#include <cdsa_utilities/DbIndex.cpp>
#include <cdsa_utilities/DbValue.cpp>
#include <cdsa_utilities/MetaAttribute.cpp>
#include <cdsa_utilities/MetaRecord.cpp>
#include <cdsa_utilities/SelectionPredicate.cpp>
#include <cdsa_utilities/debugging.cpp>
#include <cdsa_utilities/logging.cpp>
#include <cdsa_utilities/tqueue.cpp>
#include <cdsa_utilities/codesigning.cpp>
#include <cdsa_utilities/acl_codesigning.cpp>
#include <cdsa_utilities/osxsigning.cpp>
#include <cdsa_utilities/daemon.cpp>
#include <cdsa_utilities/acl_comment.cpp>
