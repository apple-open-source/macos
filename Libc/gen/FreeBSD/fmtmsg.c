/*-
 * Copyright (c) 2002 Mike Barcroft <mike@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/fmtmsg.c,v 1.6 2009/11/08 14:02:54 brueffer Exp $");

#include <sys/stat.h>

#include <fmtmsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default value for MSGVERB. */
#define	DFLT_MSGVERB	"label:severity:text:action:tag"

/* Maximum valid size for a MSGVERB. */
#define	MAX_MSGVERB	sizeof(DFLT_MSGVERB)

static char	*printfmt(char *, long, const char *, int, const char *,
		    const char *, const char *);
static char	*nextcomp(const char *);
static const char
		*sevinfo(int);
static int	 validmsgverb(const char *);

static const char * const validlist[] = {
	"label", "severity", "text", "action", "tag", NULL
};

int
fmtmsg(long class, const char *label, int sev, const char *text,
    const char *action, const char *tag)
{
	FILE *fp;
	char *env, *msgverb, *output;
	int ret = MM_OK;

	if (action == NULL)
		action = "";

	if (class & MM_PRINT) {
		if ((env = getenv("MSGVERB")) != NULL && *env != '\0' &&
		    strlen(env) <= strlen(DFLT_MSGVERB)) {
			if ((msgverb = strdup(env)) == NULL)
				return (MM_NOTOK);
			else if (validmsgverb(msgverb) == 0) {
				free(msgverb);
				goto def;
			}
		} else {
def:
			if ((msgverb = strdup(DFLT_MSGVERB)) == NULL)
				return (MM_NOTOK);
		}
		output = printfmt(msgverb, class, label, sev, text, action,
		    tag);
		if (output == NULL) {
			free(msgverb);
			return (MM_NOTOK);
		}
		if (*output != '\0') {
			if (fprintf(stderr, "%s", output) < 0)
				ret = MM_NOMSG;
		}
		free(msgverb);
		free(output);
	}
	if (class & MM_CONSOLE) {
		output = printfmt(DFLT_MSGVERB, class, label, sev, text,
		    action, tag);
		if (output == NULL)
			return (MM_NOCON);
		if (*output != '\0') {
			/*
			 * The Unix conformance test suite expects the
			 * console to be a socket or a device node on a
			 * normal writeable file system.  It tests console
			 * functionality such as fmtmsg(MM_CONSOLE) by
			 * temporarily replacing that socket or device
			 * with a regular file which it can then inspect
			 * after the test.  This worked fine in the 1980s,
			 * but we are no longer in the 1980s, so in order
			 * for the test suite to work at all we have to
			 * lie and tell it that the console is
			 * `/var/log/console`.
			 *
			 * Furthermore, part of the test suite for
			 * fmtmsg() attempts to verify that it returns the
			 * correct error codes when it fails to write to
			 * the console (either MM_NOCON if it successfully
			 * wrote to stderr or MM_NOTOK if it didn't).  It
			 * does this by replacing what it thinks is the
			 * console with a directory, rather than a regular
			 * file, in order to trigger a failure from
			 * fopen().
			 *
			 * In order to pass this misbegotten test, we
			 * check to see if `/var/log/console` exists and
			 * is a directory.  If that is the case, we try to
			 * open that instead of the real console.  We will
			 * of course fail, but that's what we're expected
			 * to do at this point.
			 */
			struct stat sb;
			const char *trap_path = "/var/log/console";
			const char *console_path = "/dev/console";
			if (stat(trap_path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
				/* the trap has been laid, walk into it */
				console_path = trap_path;
			}
			if ((fp = fopen(console_path, "a")) == NULL) {
				if (ret == MM_OK) {
					ret = MM_NOCON;
				} else {
					ret = MM_NOTOK;
				}
			} else {
				fprintf(fp, "%s", output);
				fclose(fp);
			}
		}
		free(output);
	}
	return (ret);
}

#define INSERT_COLON							\
	if (*output != '\0')						\
		strlcat(output, ": ", size)
#define INSERT_NEWLINE							\
	if (*output != '\0')						\
		strlcat(output, "\n", size)
#define INSERT_SPACE							\
	if (*output != '\0')						\
		strlcat(output, " ", size)

/*
 * Returns NULL on memory allocation failure, otherwise returns a pointer to
 * a newly malloc()'d output buffer.
 */
static char *
printfmt(char *msgverb, long class, const char *label, int sev,
    const char *text, const char *act, const char *tag)
{
	size_t size;
	char *comp, *output;
	const char *sevname;

	size = 32;
	if (label != MM_NULLLBL)
		size += strlen(label);
	if ((sevname = sevinfo(sev)) != NULL)
		size += strlen(sevname);
	if (text != MM_NULLTXT)
		size += strlen(text);
	if (act != MM_NULLACT)
		size += strlen(act);
	if (tag != MM_NULLTAG)
		size += strlen(tag);

	if ((output = malloc(size)) == NULL)
		return (NULL);
	*output = '\0';
	while ((comp = nextcomp(msgverb)) != NULL) {
		if (strcmp(comp, "label") == 0 && label != MM_NULLLBL) {
			INSERT_COLON;
			strlcat(output, label, size);
		} else if (strcmp(comp, "severity") == 0 && sevname != NULL) {
			INSERT_COLON;
			strlcat(output, sevinfo(sev), size);
		} else if (strcmp(comp, "text") == 0 && text != MM_NULLTXT) {
			INSERT_COLON;
			strlcat(output, text, size);
		} else if (strcmp(comp, "action") == 0 && act != MM_NULLACT) {
			INSERT_NEWLINE;
			strlcat(output, "TO FIX: ", size);
			strlcat(output, act, size);
		} else if (strcmp(comp, "tag") == 0 && tag != MM_NULLTAG) {
			INSERT_SPACE;
			strlcat(output, tag, size);
		}
	}
	INSERT_NEWLINE;
	return (output);
}

/*
 * Returns a component of a colon delimited string.  NULL is returned to
 * indicate that there are no remaining components.  This function must be
 * called until it returns NULL in order for the local state to be cleared.
 */
static char *
nextcomp(const char *msgverb)
{
	static char lmsgverb[MAX_MSGVERB], *state;
	char *retval;
	
	if (*lmsgverb == '\0') {
		strlcpy(lmsgverb, msgverb, sizeof(lmsgverb));
		retval = strtok_r(lmsgverb, ":", &state);
	} else {
		retval = strtok_r(NULL, ":", &state);
	}
	if (retval == NULL)
		*lmsgverb = '\0';
	return (retval);
}

static const char *
sevinfo(int sev)
{

	switch (sev) {
	case MM_HALT:
		return ("HALT");
	case MM_ERROR:
		return ("ERROR");
	case MM_WARNING:
		return ("WARNING");
	case MM_INFO:
		return ("INFO");
	default:
		return (NULL);
	}
}

/*
 * Returns 1 if the msgverb list is valid, otherwise 0.
 */
static int
validmsgverb(const char *msgverb)
{
	char *msgcomp;
	int i, equality;

	equality = 0;
	while ((msgcomp = nextcomp(msgverb)) != NULL) {
		equality--;
		for (i = 0; validlist[i] != NULL; i++) {
			if (strcmp(msgcomp, validlist[i]) == 0)
				equality++;
		}
	}
	return (!equality);
}
