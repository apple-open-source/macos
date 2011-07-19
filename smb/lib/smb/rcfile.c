/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * $Id: rcfile.c,v 1.1.1.2 2001/07/06 22:38:43 conrad Exp $
 */
#include <err.h>
#include <netsmb/smb_lib.h>

#include "rcfile.h"
#include "rcfile_priv.h"

#define	SMB_CFG_FILE		"/etc/nsmb.conf"
#define SMB_CFG_LOCAL_FILE	"/Library/Preferences/nsmb.conf"

static void
rc_key_free(struct rckey *p)
{
	if (p->rk_value)
		free(p->rk_value);
	if (p->rk_name)
		free(p->rk_name);
	if (p)
		free(p);
}

static struct rckey *
rc_sect_findkey(struct rcsection *rsp, const char *keyname)
{
	struct rckey *p;
	
	SLIST_FOREACH(p, &rsp->rs_keys, rk_next)
	if (strcmp(p->rk_name, keyname)==0)
		return p;
	return NULL;
}

static struct rckey *
rc_sect_addkey(struct rcsection *rsp, const char *name, const char *value)
{
	struct rckey *p;
	
	p = rc_sect_findkey(rsp, name);
	if (p) {
		free(p->rk_value);
	} else {
		p = malloc(sizeof(*p));
		if (!p) return NULL;
		SLIST_INSERT_HEAD(&rsp->rs_keys, p, rk_next);
		p->rk_name = strdup(name);
	}
	p->rk_value = value ? strdup(value) : strdup("");
	return p;
}

static struct rcsection *
rc_findsect(struct rcfile *rcp, const char *sectname)
{
	struct rcsection *p;
	
	SLIST_FOREACH(p, &rcp->rf_sect, rs_next)
	if (strcmp(p->rs_name, sectname)==0)
		return p;
	return NULL;
}

static struct rcsection * rc_addsect(struct rcfile *rcp, const char *sectname)
{
	struct rcsection *p;
	
	/* Not the best way to do this, but it improves the lookup speed */
	if (strcmp(sectname, "global") == 0)
		sectname = "default";	/* To us they are the same section */
	p = rc_findsect(rcp, sectname);
	if (p) return p;
	p = malloc(sizeof(*p));
	if (!p) return NULL;
	p->rs_name = strdup(sectname);
	SLIST_INIT(&p->rs_keys);
	SLIST_INSERT_HEAD(&rcp->rf_sect, p, rs_next);
	return p;
}

enum { stNewLine, stHeader, stSkipToEOL, stGetKey, stGoToGetValue, stGetValue};

static void
rc_parse(struct rcfile *rcp) 
{
	FILE *f = rcp->rf_f;
	int state = stNewLine, c;
	struct rcsection *rsp = NULL;
	struct rckey *rkp = NULL;
	char buf[2048];
	char *next = buf, *last = &buf[sizeof(buf)-1];
	
	while ((c = getc (f)) != EOF) {
		if (c == '\r')
			continue;
		if (state == stNewLine) {
			next = buf;
			if (isspace(c))
				continue;	/* skip leading junk */
			if (c == '[') {
				state = stHeader;
				rsp = NULL;
				continue;
			}
			if (c == '#' || c == ';') {
				state = stSkipToEOL;
			} else {		/* something meaningfull */
				state = stGetKey;
			}
		}
		if (state == stSkipToEOL || next == last) {/* ignore long lines */
			if (c == '\n'){
				state = stNewLine;
				next = buf;
			}
			continue;
		}
		if (state == stHeader) {
			if (c == ']') {
				*next = 0;
				next = buf;
				rsp = rc_addsect(rcp, buf);
				state = stSkipToEOL;
			} else
				*next++ = c;
			continue;
		}
		if (state == stGetKey) {
			if (c == ' ' || c == '\t')/* side effect: 'key name='*/
				continue;	  /* become 'keyname=' 	     */
			if (c == '\n') {	/* silently ignore ... */
				state = stNewLine;
				continue;
			}
			if (c != '=') {
				*next++ = c;
				continue;
			}
			*next = 0;
			if (rsp == NULL) {
				fprintf(stderr, "Key '%s' defined before section\n", buf);
				state = stSkipToEOL;
				continue;
			}
			rkp = rc_sect_addkey(rsp, buf, NULL);
			next = buf;
			state = stGoToGetValue;
			continue;
		}
		/* only stGetValue or stGoToGetValue left */
		if ((state != stGetValue) && (state != stGoToGetValue)) {
			fprintf(stderr, "Well, I can't parse file '%s'\n",rcp->rf_name);
			state = stSkipToEOL;
		}
		/* Remove any spaces or tabs between equal sign and the value */
		if ((state == stGoToGetValue) && ((c == ' ') || (c == '\t')))
			continue;
		else /* Now only the value left get it */
			state = stGetValue;
		
		if (c != '\n') {
			*next++ = c;
			continue;
		}
		*next = 0;
		if (rkp->rk_value)
			free(rkp->rk_value);
		rkp->rk_value = strdup(buf);
		state = stNewLine;
		rkp = NULL;
	} 	/* while */
	if (c == EOF && state == stGetValue) {
		*next = 0;
		if (rkp->rk_value)
			free(rkp->rk_value);
		rkp->rk_value = strdup(buf);
	}
	return;
}

/*
 * open rcfile and load its content, if already open - return previous handle
 */
static int
rc_open(const char *filename, const char *mode, struct rcfile **rcfile)
{
	struct rcfile *rcp;
	FILE *f;
	
	f = fopen(filename, mode);
	if (f == NULL)
		return errno;
	rcp = malloc(sizeof(struct rcfile));
	if (rcp == NULL) {
		fclose(f);
		return ENOMEM;
	}
	bzero(rcp, sizeof(struct rcfile));
	rcp->rf_name = strdup(filename);
	rcp->rf_f = f;

	rc_parse(rcp);
	*rcfile = rcp;
	return 0;
}

static int
rc_merge(const char *filename, struct rcfile **rcfile)
{
	struct rcfile *rcp = *rcfile;
	FILE *f, *t;
	
	if (rcp == NULL) {
		return rc_open(filename, "r", rcfile);
	}
	f = fopen (filename, "r");
	if (f == NULL)
		return errno;
	t = rcp->rf_f;
	rcp->rf_f = f;
	rc_parse(rcp);
	rcp->rf_f = t;
	fclose(f);
	return 0;
}

static int
rc_freesect(struct rcfile *rcp, struct rcsection *rsp)
{
	struct rckey *p,*n;
	
	SLIST_REMOVE(&rcp->rf_sect, rsp, rcsection, rs_next);
	for(p = SLIST_FIRST(&rsp->rs_keys);p;) {
		n = p;
		p = SLIST_NEXT(p,rk_next);
		rc_key_free(n);
	}
	free(rsp->rs_name);
	free(rsp);
	return 0;
}

int
rc_close(struct rcfile *rcp)
{
	struct rcsection *p, *n;

	fclose(rcp->rf_f);
	for(p = SLIST_FIRST(&rcp->rf_sect); p;) {
		n = p;
		p = SLIST_NEXT(p,rs_next);
		rc_freesect(rcp, n);
	}
	free(rcp->rf_name);
	free(rcp);
	return 0;
}

#if 0
void
rc_sect_delkey(struct rcsection *rsp, struct rckey *p)
{

	SLIST_REMOVE(&rsp->rs_keys, p, rckey, rk_next);
	rc_key_free(p);
	return;
}
#endif

int
rc_getstringptr(struct rcfile *rcp, const char *section, const char *key,
	char **dest)
{
	struct rcsection *rsp;
	struct rckey *rkp;
	
	*dest = NULL;
	rsp = rc_findsect(rcp, section);
	if (!rsp) return ENOENT;
	rkp = rc_sect_findkey(rsp,key);
	if (!rkp) return ENOENT;
	*dest = rkp->rk_value;
	return 0;
}

int
rc_getstring(struct rcfile *rcp, const char *section, const char *key,
	size_t maxlen, char *dest)
{
	char *value;
	int error;

	error = rc_getstringptr(rcp, section, key, &value);
	if (error)
		return error;
	if (strlen(value) >= maxlen) {
		warnx("line too long for key '%s' in section '%s', max = %zu\n", key, section, maxlen);
		return EINVAL;
	}
	strlcpy(dest, value, maxlen);
	return 0;
}

int
rc_getint(struct rcfile *rcp, const char *section, const char *key, int *value)
{
	struct rcsection *rsp;
	struct rckey *rkp;
	
	rsp = rc_findsect(rcp, section);
	if (!rsp)
		return ENOENT;
	rkp = rc_sect_findkey(rsp, key);
	if (!rkp)
		return ENOENT;
	errno = 0;
	*value = (int)strtol(rkp->rk_value, NULL, 0);
	if (errno) {
		warnx("invalid int value '%s' for key '%s' in section '%s'\n", rkp->rk_value, key, section);
		return errno;
	}
	return 0;
}

/*
 * 1,yes,true
 * 0,no,false
 */
int
rc_getbool(struct rcfile *rcp, const char *section, const char *key, int *value)
{
	struct rcsection *rsp;
	struct rckey *rkp;
	char *p;
	
	rsp = rc_findsect(rcp, section);
	if (!rsp) return ENOENT;
	rkp = rc_sect_findkey(rsp,key);
	if (!rkp) return ENOENT;
	p = rkp->rk_value;
	while (*p && isspace(*p)) p++;
	if (*p == '0' || strcasecmp(p,"no") == 0 || strcasecmp(p,"false") == 0) {
		*value = 0;
		return 0;
	}
	if (*p == '1' || strcasecmp(p,"yes") == 0 || strcasecmp(p,"true") == 0) {
		*value = 1;
		return 0;
	}
	fprintf(stderr, "invalid boolean value '%s' for key '%s' in section '%s' \n",p, key, section);
	return EINVAL;
}


/*
 * first read ~/.smbrc, next try to merge SMB_CFG_FILE - 
 */
struct rcfile * smb_open_rcfile(int noUserPrefs)
{
	struct rcfile * smb_rc = NULL;
	char *home = NULL;
	char *fn;
	int error;
	int fnlen;
	
	if (! noUserPrefs)
		home = getenv("HOME");
	
	if (home) {
		fnlen = (int)(strlen(home) + strlen(SMB_CFG_LOCAL_FILE) + 1);
		fn = malloc(fnlen);
		snprintf(fn, fnlen, "%s%s", home, SMB_CFG_LOCAL_FILE);		
		error = rc_open(fn, "r", &smb_rc);
		/* Used for debugging bad configuration files. */
		if (error && (error != ENOENT) )
			smb_log_info("%s: Can't open %s, syserr = %s", 
						 ASL_LEVEL_DEBUG, __FUNCTION__, fn, strerror(errno));
		free(fn);
	}
	fn = (char *)SMB_CFG_FILE;
	error = rc_merge(fn, &smb_rc);
	/* Used for debugging bad configuration files. */
	if (error && (error != ENOENT) )
		smb_log_info("%s: Can't open %s, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, fn, strerror(errno));
	
	
	return smb_rc;
}
