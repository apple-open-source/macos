/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  db_commands.c
 *  security
 *
 *  Created by Michael Brouwer on Fri May 23 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "db_commands.h"

#include "readline.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <Security/cssmapple.h>
#include <Security/dlclient.h>

using namespace CssmClient;

static int
do_db_create(const CSSM_GUID guid, const char *dbname, Boolean do_openparams, Boolean do_autocommit, Boolean do_mode, mode_t mode, Boolean do_version_0_params)
{
	int result = 0;

	try
	{
		CSSM_APPLEDL_OPEN_PARAMETERS openParameters = { sizeof(CSSM_APPLEDL_OPEN_PARAMETERS),
			(do_version_0_params ? 0 : CSSM_APPLEDL_OPEN_PARAMETERS_VERSION) };
		Cssm cssm;
		Module module(guid, cssm);
		DL dl(module);
		Db db(dl, dbname);

		if (do_openparams)
		{
			openParameters.autoCommit = do_autocommit;
			if (!do_version_0_params && do_mode)
			{
				openParameters.mask |= kCSSM_APPLEDL_MASK_MODE;
				openParameters.mode = mode;
			}

			db->openParameters(&openParameters);
		}

		db->create();
	}
	catch (const CssmCommonError &e)
	{
		OSStatus status = e.osStatus();
		fprintf(stderr, "CSSM_DbCreate %s returned %ld(0x%lx)\n", dbname, status, status);
	}
	catch (...)
	{
		result = 1;
	}

	return result;
}

static int
parse_guid(const char *name, CSSM_GUID *guid)
{
	size_t len = strlen(name);

	if (!strncmp("dl", name, len))
		*guid = gGuidAppleFileDL;
	else if (!strncmp("cspdl", name, len))
		*guid = gGuidAppleCSPDL;
	else
	{
		fprintf(stderr, "Invalid guid: %s\n", name);
		return 2;
	}

	return 0;
}


static int
parse_mode(const char *name, mode_t *pmode)
{
	int result = 0;
	mode_t mode = 0;
	const char *p;

	if (!name || !pmode || *name != '0')
	{
		result = 2;
		goto loser;
	}

	for (p = name + 1; *p; ++p)
	{
		if (*p < '0' || *p > '7')
		{
			result = 2;
			goto loser;
		}

		mode = (mode << 3) + *p - '0';
	}

	*pmode = mode;
	return 0;

loser:
	fprintf(stderr, "Invalid mode: %s\n", name);
	return result;
}

int
db_create(int argc, char * const *argv)
{
	int free_dbname = 0;
	char *dbname = NULL;
	int ch, result = 0;
	bool do_autocommit = true, do_mode = false;
	bool do_openparams = false, do_version_0_params = false;
	mode_t mode;
	CSSM_GUID guid = gGuidAppleFileDL;

	while ((ch = getopt(argc, argv, "0ahg:m:o")) != -1)
	{
		switch  (ch)
		{
		case '0':
			do_version_0_params = true;
			do_openparams = true;
			break;
		case 'a':
			do_autocommit = false;
			do_openparams = true;
			break;
		case 'g':
			result = parse_guid(optarg, &guid);
			if (result)
				goto loser;
			break;
		case 'm':
			result = parse_mode(optarg, &mode);
			if (result)
				goto loser;
			do_mode = true;
			do_openparams = true;
			break;
		case 'o':
			do_openparams = true;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		dbname = *argv;
	else
	{
		fprintf(stderr, "db to create: ");
		dbname = readline(NULL, 0);
		if (!dbname)
		{
			result = -1;
			goto loser;
		}

		free_dbname = 1;
		if (*dbname == '\0')
			goto loser;
	}

	do
	{
		result = do_db_create(guid, dbname, do_openparams, do_autocommit, do_mode, mode, do_version_0_params);
		if (result)
			goto loser;

		argc--;
		argv++;
		dbname = *argv;
	} while (argc > 0);

loser:
	if (free_dbname)
		free(dbname);

	return result;
}
