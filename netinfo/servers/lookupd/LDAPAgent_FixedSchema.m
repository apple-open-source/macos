/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * LDAPAgent_FixedSchema.m
 * LDAP agent compiletime schema support
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, October 1997.
 */

#ifndef OIDTABLE

#import <string.h>
#import <stdlib.h>
#import <sys/param.h>
#import <assert.h>

#import "LDAPAgent.h"
#import "LDAPAttributes.h"

static char *user_attributes[] =
	{ NameForKey(OID_UID), NameForKey(OID_USERPASSWORD),
	  NameForKey(OID_UIDNUMBER), NameForKey(OID_GIDNUMBER),
	  NameForKey(OID_CN), NameForKey(OID_HOMEDIRECTORY),
	  NameForKey(OID_LOGINSHELL), NameForKey(OID_GECOS),
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *group_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_USERPASSWORD),
	  NameForKey(OID_MEMBERUID), NameForKey(OID_GIDNUMBER), 
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *alias_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_MAIL), 
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *service_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_IPSERVICEPORT),
	  NameForKey(OID_IPSERVICEPROTOCOL), 
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *protocol_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_IPPROTOCOLNUMBER), 
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *rpc_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_ONCRPCNUMBER), 
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *mount_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_MOUNTDIRECTORY),
	  NameForKey(OID_MOUNTTYPE), NameForKey(OID_MOUNTOPTION),
	  NameForKey(OID_MOUNTDUMPFREQUENCY), NameForKey(OID_MOUNTPASSNUMBER),
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *netgroup_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_NISNETGROUPTRIPLE),
	  NameForKey(OID_MEMBERNISNETGROUP),
          NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *printer_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_LPRHOST),
	  NameForKey(OID_LPRQUEUE), NameForKey(OID_LPRTYPE),
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *host_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_IPHOSTNUMBER),
	  NameForKey(OID_MACADDRESS),
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *network_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_IPNETWORKNUMBER),
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *bootparam_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_BOOTPARAMETER), 
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };

static char *bootp_attributes[] =
	{ NameForKey(OID_CN), NameForKey(OID_BOOTFILE),
	  NameForKey(OID_IPHOSTNUMBER), NameForKey(OID_MACADDRESS),
	  NameForKey(OID_MODIFYTIMESTAMP), NameForKey(OID_TTL), NULL };


@implementation LDAPAgent (FixedSchema)

- (void)initSchema
{
	memset(&nisAttributes, 0, sizeof(nisAttributes));
	memset(&nisClasses, 0, sizeof(nisClasses));

	nisAttributes[LUCategoryUser] = user_attributes;
	nisClasses[LUCategoryUser] = NameForKey(OID_POSIXACCOUNT);

	nisAttributes[LUCategoryGroup] = group_attributes;
	nisClasses[LUCategoryGroup] = NameForKey(OID_POSIXGROUP);

	nisAttributes[LUCategoryHost] = host_attributes;
	nisClasses[LUCategoryHost] = NameForKey(OID_IPHOST);

	nisAttributes[LUCategoryNetwork] = network_attributes;
	nisClasses[LUCategoryNetwork] = NameForKey(OID_IPNETWORK);

	nisAttributes[LUCategoryService] = service_attributes;
	nisClasses[LUCategoryService] = NameForKey(OID_IPSERVICE);

	nisAttributes[LUCategoryProtocol] = protocol_attributes;
	nisClasses[LUCategoryProtocol] = NameForKey(OID_IPPROTOCOL);

	nisAttributes[LUCategoryRpc] = rpc_attributes;
	nisClasses[LUCategoryRpc] = NameForKey(OID_ONCRPC);

	nisAttributes[LUCategoryMount] = mount_attributes;
	nisClasses[LUCategoryMount] = NameForKey(OID_MOUNT);

	nisAttributes[LUCategoryPrinter] = printer_attributes; /* all attributes */
	nisClasses[LUCategoryPrinter] = NameForKey(OID_LPRPRINTER);

	nisAttributes[LUCategoryBootparam] = bootparam_attributes;
	nisClasses[LUCategoryBootparam] = NameForKey(OID_BOOTABLEDEVICE);

	nisAttributes[LUCategoryBootp] = bootp_attributes;
	nisClasses[LUCategoryBootp] = NameForKey(OID_BOOTABLEDEVICE);

	nisAttributes[LUCategoryAlias] = alias_attributes;
	nisClasses[LUCategoryAlias] = NameForKey(OID_RFC822MAILGROUP);

	nisAttributes[LUCategoryEthernet] = bootp_attributes;
	nisClasses[LUCategoryEthernet] = NameForKey(OID_IEEE802DEVICE);

	nisAttributes[LUCategoryNetgroup] = netgroup_attributes;
	nisClasses[LUCategoryNetgroup] = NameForKey(OID_NISNETGROUP);

	nisAttributes[LUCategoryInitgroups] = group_attributes;
	nisClasses[LUCategoryInitgroups] = NameForKey(OID_POSIXGROUP);
	
	return;
}
@end

#endif

