/* $NetBSD: getusershell.c,v 1.4 2008/09/21 16:35:25 lukem Exp $ */
/* from	NetBSD: getusershell.c,v 1.12 1998/11/13 15:49:29 christos Exp */

/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "tnftpd.h"

/*
 * Local shells should NOT be added here.  They should be added in
 * /etc/shells.
 */

static char *okshells[] = { _PATH_BSHELL, _PATH_CSHELL, NULL };
static char **curshell, **shells;
static char *strings;
static char **initshells(void);

/*
 * Get a list of shells from _PATH_SHELLS, if it exists.
 */
char *
getusershell(void)
{
	char *ret;

	if (curshell == NULL)
		curshell = initshells();

	ret = (char *)*curshell;

	if (ret != NULL)
		curshell++;
	return (ret);
}

void
endusershell(void)
{
	
	if (shells != NULL)
		free(shells);
	shells = NULL;
	if (strings != NULL)
		free(strings);
	strings = NULL;
	curshell = NULL;
}

void
setusershell(void)
{

	curshell = initshells();
}

static char **
initshells(void)
{
	char **sp;
	char *cp;
	FILE *fp;
	struct stat statb;

	if (shells != NULL)
		free(shells);
	shells = NULL;
	if (strings != NULL)
		free(strings);
	strings = NULL;
	if ((fp = fopen(_PATH_SHELLS, "r")) == NULL)
		return (okshells);
	if (fstat(fileno(fp), &statb) == -1) {
		(void)fclose(fp);
		return (okshells);
	}
	if ((cp = malloc((unsigned int)statb.st_size)) == NULL) {
		(void)fclose(fp);
		return (okshells);
	}
	sp = calloc((unsigned)statb.st_size / 3, sizeof (char *));
	if (sp == NULL) {
		(void)fclose(fp);
		free(cp);
		strings = NULL;
		return (okshells);
	}
	shells = sp;
	strings = cp;
	while (fgets(cp, MAXPATHLEN + 1, fp) != NULL) {
		while (*cp != '#' && *cp != '/' && *cp != '\0')
			cp++;
		if (*cp == '#' || *cp == '\0')
			continue;
		*sp++ = cp;
		while (!isspace(*cp) && *cp != '#' && *cp != '\0')
			cp++;
		*cp++ = '\0';
	}
	*sp = NULL;
	(void)fclose(fp);
	return (shells);
}
