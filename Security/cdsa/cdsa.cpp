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


/* Headers. */
#include <cdsa/cssmconfig.h>
#include <cdsa/cssmerr.h>
#include <cdsa/cssmtype.h>
#include <cdsa/cssmapple.h>
#include <cdsa/cssmapi.h>
#include <cdsa/cssmspi.h>
#include <cdsa/cssm.h>
#include <cdsa/cssmaci.h>
#include <cdsa/cssmcli.h>
#include <cdsa/cssmcspi.h>
#include <cdsa/cssmdli.h>
#include <cdsa/cssmkrapi.h>
#include <cdsa/cssmkrspi.h>
#include <cdsa/cssmtpi.h>
#include <cdsa/eisl.h>
#include <cdsa/emmspi.h>
#include <cdsa/emmtype.h>
#include <cdsa/mds.h>
#include <cdsa/mds_schema.h>
#include <cdsa/oidsbase.h>
#include <cdsa/oidscert.h>
#include <cdsa/oidscrl.h>
#include <cdsa/x509defs.h>
#include <cdsa/certextensions.h>
#include <cdsa/oidsattr.h>
#include <cdsa/oidsalg.h>

#include <cssm/attachfactory.h>
#include <cssm/attachment.h>
#include <cssm/cspattachment.h>
#include <cssm/cssmcontext.h>
#include <cssm/cssmint.h>
#include <cssm/cssmmds.h>
#include <cssm/manager.h>
#include <cssm/module.h>

/* Source files. */
#include <cssm/attachfactory.cpp>
#include <cssm/attachment.cpp>
#include <cssm/cspattachment.cpp>
#include <cssm/cssm.cpp>
#include <cssm/cssmcontext.cpp>
#include <cssm/cssmmds.cpp>
#include <cssm/manager.cpp>
#include <cssm/module.cpp>
#include <cssm/oidscert.cpp>
#include <cssm/oidscrl.cpp>
#include <cssm/transition.cpp>
#include <cssm/oidsattr.c>
#include <cssm/oidsalg.c>
