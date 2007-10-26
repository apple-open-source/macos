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
#define INFO_FILE 1
#define INFO_NIS 2
#define INFO_DIRECTORYSERVICES 3

#ifndef __SLICK__
#define _PASSWD_FILE "/etc/master.passwd"
#else
#define _PASSWD_FILE "/etc/passwd"
#endif

#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <libc.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include "stringops.h"

#ifdef __SLICK__
#define _PASSWORD_LEN 8
#endif

static int literal = 0;

extern int file_check_passwd(char *, char *);
extern int nis_check_passwd(char *, char *);
extern int ds_check_passwd(char *, char *);

void
checkpasswd(char *name, char *old_pw)
{
	int isNull;
	char *p;

	printf("Checking password for %s.\n", name);

	p = "";
	isNull = 0;
	if (old_pw == NULL) isNull = 1;
	if ((isNull == 0) && (old_pw[0] == '\0')) isNull = 1;
	if (isNull == 0)
	{
		p = getpass("Password:");
		sleep(1); // make sure this doesn't go too quickly
		if (strcmp(literal ? p : crypt(p, old_pw), old_pw))
		{
			errno = EACCES;
			fprintf(stderr, "Sorry\n");
			exit(1);
		}
	}
	return;
}

void
usage()
{
	fprintf(stderr, "usage: chkpasswd [-i infosystem] [-l location] [-c] [name]\n");
	fprintf(stderr, "supported infosystems are:\n");
	fprintf(stderr, "    file\n");
	fprintf(stderr, "    nis\n");
	fprintf(stderr, "    opendirectory\n");
	fprintf(stderr, "for file, location may be a file name (%s is the default)\n",
		_PASSWD_FILE);
	fprintf(stderr, "for nis, location may be a NIS domainname\n");
	fprintf(stderr, "for opendirectory, location may be a directory node name\n");
	fprintf(stderr, "if -c is specified, the password you supply is compared\n");
	fprintf(stderr, "verbatim without first being crypted\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *user, *locn;
	int i, infosystem;
	struct passwd *pw;

	infosystem = INFO_DIRECTORYSERVICES;
	user = NULL;
	locn = NULL;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-i"))
		{
			if (++i >= argc)
			{
				fprintf(stderr, "no argument for -i option\n");
				usage();
			}

			if (!strcmp(argv[i], "File")) infosystem = INFO_FILE;
			else if (!strcmp(argv[i], "file")) infosystem = INFO_FILE;
			else if (!strcmp(argv[i], "NIS")) infosystem = INFO_NIS;
			else if (!strcmp(argv[i], "nis")) infosystem = INFO_NIS;
			else if (!strcmp(argv[i], "YP")) infosystem = INFO_NIS;
			else if (!strcmp(argv[i], "yp")) infosystem = INFO_NIS;
			else if (!strcasecmp(argv[i], "opendirectory")) infosystem = INFO_DIRECTORYSERVICES;
			else
			{
				fprintf(stderr, "unknown info system \"%s\"\n", argv[i]);
				usage();
			}
		}

		else if (!strcmp(argv[i], "-l"))
		{
			if (++i >= argc)
			{
				fprintf(stderr, "no argument for -l option\n");
				usage();
			}
			locn = argv[i];
		}

		else if (!strcmp(argv[i], "-c")) literal++;
		else if (user == NULL) user = argv[i];
		else usage();
	}

	if (user == NULL)
	{
		if ((pw = getpwuid(getuid())) == NULL || (user = pw->pw_name) == NULL)
		{
			fprintf(stderr, "you don't have a login name\n");
			exit(1);
		}
	}
	
	switch (infosystem)
	{
		case INFO_FILE:
			file_check_passwd(user, locn);
			break;
		case INFO_NIS:
			nis_check_passwd(user, locn);
			break;
		case INFO_DIRECTORYSERVICES:
			ds_check_passwd(user, locn);
			break;
	}

	exit(0);
}

