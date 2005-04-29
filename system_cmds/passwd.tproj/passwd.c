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
#define INFO_NETINFO 0
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
#include <netinfo/ni.h>
#include "stringops.h"

#ifdef __SLICK__
#define _PASSWORD_LEN 8
#endif

static char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

extern int file_passwd(char *, char *);
extern int netinfo_passwd(char *, char *);
extern int nis_passwd(char *, char *);
extern int ds_passwd(char *, char *);

void
getpasswd(char *name, int isroot, int minlen, int mixcase, int nonalpha,
	char *old_pw, char **new_pw, char **old_clear, char **new_clear)
{
	int i, tries, len, pw_ok, upper, lower, alpha, notalpha;
	int isNull;
	char *p;
	static char obuf[_PASSWORD_LEN+1];
	static char nbuf[_PASSWORD_LEN+1];
	char salt[9];

	printf("Changing password for %s.\n", name);

	p = "";
	isNull = 0;
	if (old_pw == NULL) isNull = 1;
	if ((isNull == 0) && (old_pw[0] == '\0')) isNull = 1;
	if ((isroot == 0) && (isNull == 0))
	{
		p = getpass("Old password:");
		if (strcmp(crypt(p, old_pw), old_pw))
		{
			errno = EACCES;
			fprintf(stderr, "Sorry\n");
			exit(1);
		}
	}
	//strcpy(obuf, p);
	snprintf( obuf, sizeof(obuf), "%s", p );
	
	tries = 0;
	nbuf[0] = '\0';
	for (;;)
	{
		p = getpass("New password:");
		if (!*p)
		{
			printf("Password unchanged.\n");
			exit(0);
		}

		tries++;
		len = strlen(p);
		upper = 0;
		lower = 0;
		alpha = 0;
		notalpha = 0;
		for (i = 0; i < len; i++)
		{
			if (isupper(p[i])) upper++;
			if (islower(p[i])) lower++;
			if (isalpha(p[i])) alpha++;
			else notalpha++;
		}


		pw_ok = 1;
		if (len < minlen) pw_ok = 0;
		if ((mixcase == 1) && ((upper == 0) || (lower == 0))) pw_ok = 0;
		if ((nonalpha == 1) && (notalpha == 0)) pw_ok = 0;

		/*
		 * An insistent root may override security options.
		 */
		if ((isroot == 1) && (tries > 2)) pw_ok = 1;
	
		/*
		 * A very insistent user may override security options.
		 */
		if (tries > 4) pw_ok = 1;
	
		if (pw_ok == 0)
		{
			if (len < minlen)
				printf("Password must be at least %d characters long.\n", minlen);
			if ((mixcase == 1) && ((upper == 0) || (lower == 0)))
				printf("Password must contain both upper and lower case characters.\n");
			if ((nonalpha == 1) && (notalpha == 0))
				printf("Password must contain non-alphabetic characters.\n");
			continue;
		}

		//strcpy(nbuf, p);
		snprintf( nbuf, sizeof(nbuf), "%s", p );
		
		if (!strcmp(nbuf, getpass("Retype new password:"))) break;

		printf("Mismatch; try again, EOF to quit.\n");
	}

	/*
	 * Create a random salt
	 */
	srandom((int)time((time_t *)NULL));
	salt[0] = saltchars[random() % strlen(saltchars)];
	salt[1] = saltchars[random() % strlen(saltchars)];
	salt[2] = '\0';
	*new_pw = crypt(nbuf, salt);

	*old_clear = obuf;
	*new_clear = nbuf;
	return;
}

void
usage()
{
	fprintf(stderr, "usage: passwd [-i infosystem] [-l location] [name]\n");
	fprintf(stderr, "supported infosystems are:\n");
	fprintf(stderr, "    netinfo\n");
	fprintf(stderr, "    file\n");
	fprintf(stderr, "    nis\n");
	fprintf(stderr, "    opendirectory\n");
	fprintf(stderr, "for netinfo, location may be a domain name or server/tag\n");
	fprintf(stderr, "for file, location may be a file name (%s is the default)\n",
		_PASSWD_FILE);
	fprintf(stderr, "for nis, location may be a NIS domainname\n");
	fprintf(stderr, "for opendirectory, location may be a directory node name\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *user, *locn;
	int i, infosystem;
	int free_user = 0;
	
	/* since DS works for most infosystems, make it the default */
	//infosystem = INFO_NETINFO;
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

			if (!strcmp(argv[i], "NetInfo")) infosystem = INFO_NETINFO;
			else if (!strcmp(argv[i], "netinfo")) infosystem = INFO_NETINFO;
			else if (!strcmp(argv[i], "File")) infosystem = INFO_FILE;
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
		else if (user == NULL) user = argv[i];
		else usage();
	}

	if (user == NULL)
	{
		/*
		 * Verify that the login name exists.
		 * lukeh 24 Dec 1997
		 */
		 
		/* getlogin() is the wrong thing to use here because it returns the wrong user after su */
		/* sns 5 Jan 2005 */
		
		struct passwd * userRec = getpwuid(getuid());
		if (userRec != NULL && userRec->pw_name != NULL) {
			/* global static mem is volatile; must strdup */
			user = strdup(userRec->pw_name);
			free_user = 1;
		}
		
		if (user == NULL)
		{
			fprintf(stderr, "you don't have a login name\n");
			exit(1);
		}
	}
	
	switch (infosystem)
	{
		case INFO_NETINFO:
			netinfo_passwd(user, locn);
			break;
		case INFO_FILE:
			file_passwd(user, locn);
			break;
		case INFO_NIS:
			nis_passwd(user, locn);
			break;
		case INFO_DIRECTORYSERVICES:
			ds_passwd(user, locn);
			break;
	}
	
	if (free_user == 1)
		free(user);
	
	exit(0);
}

