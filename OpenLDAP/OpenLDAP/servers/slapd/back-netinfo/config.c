/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "back-netinfo.h"

static struct { char *key; int mask; int value; } flagtab[] = {
	{ "DSSTORE_FLAGS_ACCESS_READONLY", DSSTORE_FLAGS_ACCESS_MASK, DSSTORE_FLAGS_ACCESS_READONLY },
	{ "DSSTORE_FLAGS_ACCESS_READWRITE", DSSTORE_FLAGS_ACCESS_MASK, DSSTORE_FLAGS_ACCESS_READWRITE },
	{ "DSSTORE_FLAGS_SERVER_CLONE", DSSTORE_FLAGS_SERVER_MASK, DSSTORE_FLAGS_SERVER_CLONE },
	{ "DSSTORE_FLAGS_SERVER_MASTER", DSSTORE_FLAGS_SERVER_MASK, DSSTORE_FLAGS_SERVER_MASTER },
	{ "DSSTORE_FLAGS_CACHE_ENABLED", DSSTORE_FLAGS_CACHE_MASK, DSSTORE_FLAGS_CACHE_ENABLED },
	{ "DSSTORE_FLAGS_CACHE_DISABLED", DSSTORE_FLAGS_CACHE_MASK, DSSTORE_FLAGS_CACHE_DISABLED },
	{ "DSSTORE_FLAGS_REMOTE_NETINFO", 0, DSSTORE_FLAGS_REMOTE_NETINFO },
	{ "DSSTORE_FLAGS_OPEN_BY_TAG", 0, DSSTORE_FLAGS_OPEN_BY_TAG },
	{ "DSENGINE_FLAGS_NETINFO_NAMING", DSENGINE_FLAGS_NAMING_MASK, DSENGINE_FLAGS_NETINFO_NAMING },
	{ "DSENGINE_FLAGS_X500_NAMING", DSENGINE_FLAGS_NAMING_MASK, DSENGINE_FLAGS_X500_NAMING },
	{ "DSENGINE_FLAGS_POSIX_NAMING", DSENGINE_FLAGS_NAMING_MASK, DSENGINE_FLAGS_POSIX_NAMING },
	{ "DSENGINE_FLAGS_DEREFERENCE_IDS", 0, DSENGINE_FLAGS_DEREFERENCE_IDS },
	{ "DSENGINE_FLAGS_NATIVE_AUTHORIZATION", 0, DSENGINE_FLAGS_NATIVE_AUTHORIZATION },
	{ NULL, 0, 0 }
};

static void printFlags(int flags)
{
	int i;

	for (i = 0; flagtab[i].key != NULL; i++)
	{
		if (flagtab[i].mask)
		{
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "(%s: %s)\n",
				flagtab[i].key,
				((flags & flagtab[i].mask) == flagtab[i].value) ? "ON" : "OFF", 0));
#else
			Debug(LDAP_DEBUG_TRACE, "(%s: %s)\n", flagtab[i].key,
				((flags & flagtab[i].mask) == flagtab[i].value) ? "ON" : "OFF", 0);
#endif
		}
		else
		{
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "(%s: %s)\n",
				flagtab[i].key,
				(flags & flagtab[i].value) ? "ON" : "OFF", 0));
#else
			Debug(LDAP_DEBUG_TRACE, "(%s: %s)\n", flagtab[i].key,
				(flags & flagtab[i].value) ? "ON" : "OFF", 0);
#endif
		}
	}
}

int netinfo_back_db_config(BackendDB *be, const char *fname, int lineno, int argc, char **argv)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	int rc;

	if (di == NULL)
	{
		fprintf(stderr, "%s: line %d: NetInfo database info is invalid.\n", fname, lineno);
		return 1;
	}

	if (!strcasecmp(argv[0], "flags"))
	{
		int i;

		if (argc < 2)
		{
			fprintf(stderr, "%s: line %d: missing flags in \"flags <flag1> .. <flagN>\" line\n",
				fname, lineno);
			return 1;
		}
		
		for (i = 1; i < argc; ++i)
		{
			int j, found = 0;

			for (j = 0; flagtab[j].key != NULL; j++)
			{
				if (!strcmp(argv[i], flagtab[j].key))
				{
					di->flags &= ~flagtab[j].mask;
					di->flags |= flagtab[j].value;
					++found;
					break;
				}
			}

			if (!found)
			{
				fprintf(stderr, "%s: line %d: unknown flag %s\n", fname, lineno, argv[i]);
				return 1;
			}
		}

		printFlags(di->flags);
	}
	else if (!strcasecmp(argv[0], "datasource"))
	{
		if (argc < 2)
		{
			fprintf(stderr, "%s: line %d: missing source in \"datasource <source>\" line\n",
				fname, lineno);
			return 1;
		}

		if (di->datasource) 
			free(di->datasource);

		if (di->engine)
		{
			dsengine_close(di->engine);
			di->engine = NULL;
		}

		di->datasource = ch_strdup(argv[1]);
		if (dsengine_open(&di->engine, di->datasource, di->flags) != DSStatusOK)
		{
			fprintf(stderr, "%s: line %d: could not open datasource \"%s\" flags [%08x]\n",
				fname, lineno, di->datasource, di->flags);
			return 1;
		}
	}
	else if (!strcasecmp(argv[0], "auth_user"))
	{
		if (argc < 2)
		{
			fprintf(stderr, "%s: line %d: missing user in \"auth_user <user>\" line\n",
				fname, lineno);
			return 1;
		}

		if (di->auth_user) 
			dsdata_release(di->auth_user);

		di->auth_user = cstring_to_dsdata(argv[1]);
		assert(di->auth_user != NULL);
	}
	else if (!strcasecmp(argv[0], "auth_password"))
	{
		if (argc < 2)
		{
			fprintf(stderr, "%s: line %d: missing password in \"auth_password <user>\" line\n",
				fname, lineno);
			return 1;
		}

		if (di->auth_password)
			dsdata_release(di->auth_password);

		di->auth_password = cstring_to_dsdata(argv[1]);
		assert(di->auth_password != NULL);
	}
	else if (!strcasecmp(argv[0], "attributemap"))
	{
		char *where = NULL;
		char *niToX500Arg = NULL;
		char *x500ToNiArg = NULL;

		argc--;
		argv++;

		if (argc < 2)
		{
			fprintf(stderr, "%s: line %d: usage: \"attributemap <netinfo_path> <netinfo_attribute> <attributeType> <netinfo_transform:arg> <x500_transform:arg>\"\n",
				fname, lineno);
			return 1;
		}

		if (argv[0][0] == '/')
		{
			where = argv[0];
			argv++;
			argc--;
		}

		switch (argc)
		{
			case 4:
				x500ToNiArg = strchr(argv[3], ':');
				if (x500ToNiArg != NULL)
				{
					*x500ToNiArg = '\0';
					x500ToNiArg++;
				}
				/* fall through */
			case 3:
				niToX500Arg = strchr(argv[2], ':');
				if (niToX500Arg != NULL)
				{
					*niToX500Arg = '\0';
					niToX500Arg++;
				}
				/* fall through */
			default:
				rc = schemamap_add_at(be, where, argv[0], argv[1],
					(argc > 2) ? argv[2] : NULL, x500ToNiArg,
					(argc > 3) ? argv[3] : NULL, niToX500Arg);	
		}
#ifdef FAIL_ON_SCHEMAMAP_ERRORS
		if (rc < 0)
			return 1;
#endif
	}
	else if (!strcasecmp(argv[0], "objectclassmap"))
	{
		if (argc < 3)
		{
			fprintf(stderr, "%s: line %d: usage \"objectclassmap <netinfo_path> <structuralObjectClass> [<objectClass_1> .. <objectclass_N>]\"\n",
				fname, lineno);
			return 1;
		}

		rc = schemamap_add_oc(be, argv[1], argc - 2, (const char **)&argv[2]);
#ifdef FAIL_ON_SCHEMAMAP_ERRORS
		if (rc < 0)
			return 1;
#endif
	}
	else
	{
		fprintf(stderr, "%s: line %d: unknown directive \"%s\" in NetInfo database definition (ignored)\n",
		    fname, lineno, argv[0]);
	}

	return 0;
}
