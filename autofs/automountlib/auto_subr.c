/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Portions Copyright 2007-2012 Apple Inc.
 */

#pragma ident	"@(#)auto_subr.c	1.49	05/06/08 SMI"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <pthread.h>
#include <asl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include "autofs.h"
#include "automount.h"
#include <dispatch/dispatch.h>

static char *check_hier(char *);
static int natisa(char *, size_t);

struct mntlist *current_mounts;

static bool_t nodirect_map = FALSE;

void
dirinit(char *mntpnt, char *map, char *opts, int direct, char **stack,
	char ***stkptr)
{
	struct autodir *dir;
	size_t mntpntlen;
	char *p;

	if (strcmp(map, "-null") == 0) {
		if (strcmp(mntpnt, "/-") == 0)
			nodirect_map = TRUE;
		goto enter;
	}

	mntpntlen = strlen(mntpnt);
	if (mntpntlen == 0) {
		pr_msg("dir is empty string");
		return;
	}
	p = mntpnt + (mntpntlen - 1);
	if (*p == '/')
		*p = '\0';	/* trim trailing / */
	if (*mntpnt != '/') {
		pr_msg("dir %s must start with '/'", mntpnt);
		return;
	}
	if ((p = check_hier(mntpnt)) != NULL) {
		pr_msg("hierarchical mountpoint: %s and %s",
			p, mntpnt);
		return;
	}

	/*
	 * If it's a direct map then call dirinit
	 * for every map entry.
	 */
	if ((strcmp(mntpnt, "/-") == 0) && !(nodirect_map)) {
		(void) loaddirect_map(map, map, opts, stack, stkptr);
		return;
	}

enter:
	dir = (struct autodir *)malloc(sizeof (*dir));
	if (dir == NULL)
		goto alloc_failed;
	dir->dir_name = strdup(mntpnt);
	if (dir->dir_name == NULL)
		goto alloc_failed;
	dir->dir_map = strdup(map);
	if (dir->dir_map == NULL)
		goto alloc_failed;
	dir->dir_opts = strdup(opts);
	if (dir->dir_opts == NULL)
		goto alloc_failed;
	dir->dir_direct = direct;
	dir->dir_realpath = NULL;
	dir->dir_next = NULL;

	/*
	 * Append to dir chain
	 */
	if (dir_head == NULL)
		dir_head = dir;
	else
		dir_tail->dir_next = dir;

	dir->dir_prev = dir_tail;
	dir_tail = dir;

	return;

alloc_failed:
	if (dir != NULL) {
		if (dir->dir_opts)
			free(dir->dir_opts);
		if (dir->dir_map)
			free(dir->dir_map);
		if (dir->dir_name)
			free(dir->dir_name);
		free(dir);
	}
	pr_msg("dirinit: memory allocation failed");
}

/*
 *  Check whether the mount point is a
 *  subdirectory or a parent directory
 *  of any previously mounted automount
 *  mount point.
 */
static char *
check_hier(mntpnt)
	char *mntpnt;
{
	register struct autodir *dir;
	register char *p, *q;

	for (dir = dir_head; dir; dir = dir->dir_next) {
		p = dir->dir_name;
		q = mntpnt;
		for (; *p == *q; p++, q++)
			if (*p == '\0')
				break;
		if (*p == '/' && *q == '\0')
			return (dir->dir_name);
		if (*p == '\0' && *q == '/')
			return (dir->dir_name);
		if (*p == '\0' && *q == '\0')
			return (NULL);
	}
	return (NULL);	/* it's not a subdir or parent */
}

/*
 * Gets the next token from the string "p" and copies
 * it into "w".  Both "wq" and "w" are quote vectors
 * for "w" and "p".  Delim is the character to be used
 * as a delimiter for the scan.  A space means "whitespace".
 * The call to getword must provide buffers w and wq of size at
 * least wordsz. getword() will pass strings of maximum length
 * (wordsz-1), since it needs to null terminate the string.
 * Returns 0 on ok and -1 on error.
 */
int
getword(char *w, char *wq, char **p, char **pq, char delim, int wordsz)
{
	char *tmp = w;
	char *tmpq = wq;
	int count = wordsz;

	if (wordsz <= 0) {
		if (verbose)
			syslog(LOG_ERR,
			"getword: input word size %d must be > 0", wordsz);
		return (-1);
	}

	while ((delim == ' ' ? isspace(**p) : **p == delim) && **pq == ' ')
		(*p)++, (*pq)++;

	while (**p &&
		!((delim == ' ' ? isspace(**p) : **p == delim) &&
			**pq == ' ')) {
		if (--count <= 0) {
			*tmp = '\0';
			*tmpq = '\0';
			syslog(LOG_ERR,
			"maximum word length (%d) exceeded", wordsz);
			return (-1);
		}
		*w++  = *(*p)++;
		*wq++ = *(*pq)++;
	}
	*w  = '\0';
	*wq = '\0';

	return (0);
}

/*
 * get_line attempts to get a line from the map, upto LINESZ. A line in
 * the map is a concatenation of lines if the continuation symbol '\'
 * is used at the end of the line. Returns line on success, a NULL on
 * EOF or error, and an empty string on lines > linesz.
 */
char *
get_line(FILE *fp, char *map, char *line, int linesz)
{
	register char *p = line;
	register size_t len;
	int excess = 0;

	*p = '\0';

	for (;;) {
		if (fgets(p, linesz - (int)(p-line), fp) == NULL) {
			return (*line ? line : NULL);	/* EOF or error */
		}

		len = strlen(line);
		if (len <= 0) {
			p = line;
			continue;
		}
		p = &line[len - 1];

		/*
		 * Is input line too long?
		 */
		if (*p != '\n') {
			excess = 1;
			/*
			 * Perhaps last char read was '\'. Reinsert it
			 * into the stream to ease the parsing when we
			 * read the rest of the line to discard.
			 */
			(void) ungetc(*p, fp);
			break;
		}
trim:
		/* trim trailing white space */
		while (p >= line && isspace(*(uchar_t *)p))
			*p-- = '\0';
		if (p < line) {			/* empty line */
			p = line;
			continue;
		}

		if (*p == '\\') {		/* continuation */
			*p = '\0';
			continue;
		}

		/*
		 * Ignore comments. Comments start with '#'
		 * which must be preceded by a whitespace, unless
		 * if '#' is the first character in the line.
		 */
		p = line;
		while ((p = strchr(p, '#')) != NULL) {
			if (p == line || isspace(*(p-1))) {
				*p-- = '\0';
				goto trim;
			}
			p++;
		}
		break;
	}
	if (excess) {
		int c;

		/*
		 * discard rest of line and return an empty string.
		 * done to set the stream to the correct place when
		 * we are done with this line.
		 */
		while ((c = getc(fp)) != EOF) {
			*p = c;
			if (*p == '\n')		/* end of the long line */
				break;
			else if (*p == '\\') {		/* continuation */
				if (getc(fp) == EOF)	/* ignore next char */
					break;
			}
		}
		syslog(LOG_ERR,
			"map %s: line too long (max %d chars)",
			map, linesz-1);
		*line = '\0';
	}

	return (line);
}

/*
 * Gets the retry=n entry from opts.
 * Returns 0 if retry=n is not present in option string,
 * retry=n is invalid, or when option string is NULL.
 */
int
get_retry(const char *opts)
{
	int retry = 0;
	char buf[MAXOPTSLEN];
	char *p, *pb, *lasts;

	if (opts == NULL)
		return (retry);

	CHECK_STRCPY(buf, opts, sizeof (buf));
	pb = buf;
	while ((p = (char *)strtok_r(pb, ",", &lasts)) != NULL) {
		pb = NULL;
		if (strncmp(p, "retry=", 6) == 0)
			retry = atoi(p+6);
	}
	return (retry > 0 ? retry : 0);
}

#if 0
/*
 * Returns zero if "opt" is found in mnt->mnt_opts, setting
 * *sval to whatever follows the equal sign after "opt".
 * str_opt allocates a string long enough to store the value of
 * "opt" plus a terminating null character and returns it as *sval.
 * It is the responsability of the caller to deallocate *sval.
 * *sval will be equal to NULL upon return if either "opt=" is not found,
 * or "opt=" has no value associated with it.
 *
 * stropt will return -1 on error.
 */
int
str_opt(struct mnttab *mnt, char *opt, char **sval)
{
	char *str, *comma;

	/*
	 * is "opt" in the options field?
	 */
	if (str = hasmntopt(mnt, opt)) {
		str += strlen(opt);
		if (*str++ != '=' ||
		    (*str == ',' || *str == '\0')) {
			syslog(LOG_ERR, "Bad option field");
			return (-1);
		}
		comma = strchr(str, ',');
		if (comma != NULL)
			*comma = '\0';
		*sval = strdup(str);
		if (comma != NULL)
			*comma = ',';
		if (*sval == NULL)
			return (-1);
	} else
		*sval = NULL;

	return (0);
}
#endif

/*
 * Performs text expansions in the string "pline".
 * "plineq" is the quote vector for "pline".
 * An identifier prefixed by "$" is replaced by the
 * corresponding environment variable string.  A "&"
 * is replaced by the key string for the map entry.
 *
 * This routine will return an error status, indicating that the
 * macro_expand failed, if *size* would be exceeded after expansion
 * or if a variable name is bigger than MAXVARNAMELEN.
 * This is to prevent writing past the end of pline and plineq or
 * the end of the variable name buffer.
 * Both pline and plineq are left untouched in such error case.
 */
#define MAXVARNAMELEN	64		/* maximum variable name length */
macro_expand_status
macro_expand(key, pline, plineq, size)
	const char *key;
	char *pline, *plineq;
	int size;
{
	register char *p,  *q;
	register char *bp, *bq;
	register const char *s;
	char buffp[LINESZ], buffq[LINESZ];
	char namebuf[MAXVARNAMELEN+1], *pn;
	int expand = 0;
	struct utsname name;
	char isaname[64];

	p = pline;  q = plineq;
	bp = buffp; bq = buffq;

	while (*p) {
		if (*p == '&' && *q == ' ') {	/* insert key */
			/*
			 * make sure we don't overflow buffer
			 */
			if ((int)((bp - buffp) + strlen(key)) < size) {
				for (s = key; *s; s++) {
					*bp++ = *s;
					*bq++ = ' ';
				}
				expand++;
				p++; q++;
				continue;
			} else {
				/*
				 * line too long...
				 */
				return (MEXPAND_LINE_TOO_LONG);
			}
		}

		if (*p == '$' && *q == ' ') {	/* insert env var */
			p++; q++;
			pn = namebuf;
			if (*p == '{') {
				p++; q++;
				while (*p && *p != '}') {
					if (pn >= &namebuf[MAXVARNAMELEN])
						return (MEXPAND_VARNAME_TOO_LONG);
					*pn++ = *p++;
					q++;
				}
				if (*p) {
					p++; q++;
				}
			} else {
				while (*p && (*p == '_' || isalnum(*p))) {
					if (pn >= &namebuf[MAXVARNAMELEN])
						return (MEXPAND_VARNAME_TOO_LONG);
					*pn++ = *p++;
					q++;
				}
			}
			*pn = '\0';

			s = getenv(namebuf);
			if (!s) {
				/* not found in env */
				if (strcmp(namebuf, "HOST") == 0) {
					(void) uname(&name);
					s = name.nodename;
				} else if (strcmp(namebuf, "OSREL") == 0) {
					(void) uname(&name);
					s = name.release;
				} else if (strcmp(namebuf, "OSNAME") == 0) {
					(void) uname(&name);
					s = name.sysname;
				} else if (strcmp(namebuf, "OSVERS") == 0) {
					/*
					 * OS X is BSD-flavored, so the OS
					 * "version" from uname is a string
					 * with all sorts of crud in it.
					 *
					 * In Solaris, this seems to be
					 * something that indicates the
					 * patch level of the OS.  Nothing
					 * like that exists in OS X, so
					 * just say "unknown".
					 */
					s = "unknown";
				} else if (strcmp(namebuf, "NATISA") == 0) {
					if (natisa(isaname, sizeof (isaname)))
						s = isaname;
				}
			}

			if (s) {
				if ((int)((bp - buffp) + strlen(s)) < size) {
					while (*s) {
						*bp++ = *s++;
						*bq++ = ' ';
					}
				} else {
					/*
					 * line too long...
					 */
					return (MEXPAND_LINE_TOO_LONG);
				}
			}
			expand++;
			continue;
		}
		/*
		 * Since buffp needs to be null terminated, we need to
		 * check that there's still room in the buffer to
		 * place at least two more characters, *p and the
		 * terminating null.
		 */
		if (bp - buffp == size - 1) {
			/*
			 * There was not enough room for at least two more
			 * characters, return with an error.
			 */
			return (MEXPAND_LINE_TOO_LONG);
		}
		/*
		 * The total number of characters so far better be less
		 * than the size of buffer passed in.
		 */
		*bp++ = *p++;
		*bq++ = *q++;

	}
	if (!expand)
		return (MEXPAND_OK);
	*bp = '\0';
	*bq = '\0';
	/*
	 * We know buffp/buffq will fit in pline/plineq since we
	 * processed at most size characters.
	 */
	(void) strcpy(pline, buffp);
	(void) strcpy(plineq, buffq);

	return (MEXPAND_OK);
}

/*
 * Removes quotes from the string "str" and returns
 * the quoting information in "qbuf". e.g.
 * original str: 'the "quick brown" f\ox'
 * unquoted str: 'the quick brown fox'
 * and the qbuf: '    ^^^^^^^^^^^  ^ '
 */
void
unquote(str, qbuf)
	char *str, *qbuf;
{
	register int escaped, inquote, quoted;
	register char *ip, *bp, *qp;
	char buf[LINESZ];

	escaped = inquote = quoted = 0;

	for (ip = str, bp = buf, qp = qbuf; *ip; ip++) {
		if (!escaped) {
			if (*ip == '\\') {
				escaped = 1;
				quoted++;
				continue;
			} else
			if (*ip == '"') {
				inquote = !inquote;
				quoted++;
				continue;
			}
		}

		*bp++ = *ip;
		*qp++ = (inquote || escaped) ? '^' : ' ';
		escaped = 0;
	}
	*bp = '\0';
	*qp = '\0';
	if (quoted)
		(void) strcpy(str, buf);
}

/*
 * Removes trailing spaces from string "s".
 */
void
trim(s)
	char *s;
{
	size_t slen;
	char *p;
	
	slen = strlen(s);
	if (slen == 0)
		return;	/* nothing to trim */
	p = &s[slen - 1];

	while (p >= s && isspace(*(uchar_t *)p))
		*p-- = '\0';
}

/*
 * try to allocate memory using malloc, if malloc fails, then flush the
 * rddir caches, and retry. If the second allocation after the readdir
 * caches have been flushed fails too, then return NULL to indicate
 * memory could not be allocated.
 */
char *
auto_rddir_malloc(unsigned nbytes)
{
	char *p;
	int again = 0;

	if ((p = malloc(nbytes)) == NULL) {
		/*
		 * No memory, free rddir caches and try again
		 */
		pthread_mutex_lock(&cleanup_lock);
		pthread_cond_signal(&cleanup_start_cv);
		if (pthread_cond_wait(&cleanup_done_cv, &cleanup_lock)) {
			pthread_mutex_unlock(&cleanup_lock);
			syslog(LOG_ERR, "auto_rddir_malloc interrupted\n");
		} else {
			pthread_mutex_unlock(&cleanup_lock);
			again = 1;
		}
	}

	if (again)
		p = malloc(nbytes);

	return (p);
}

/*
 * try to strdup a string, if it fails, then flush the rddir caches,
 * and retry. If the second strdup fails, return NULL to indicate failure.
 */
char *
auto_rddir_strdup(const char *s1)
{
	char *s2;
	int again = 0;

	if ((s2 = strdup(s1)) == NULL) {
		/*
		 * No memory, free rddir caches and try again
		 */
		pthread_mutex_lock(&cleanup_lock);
		pthread_cond_signal(&cleanup_start_cv);
		if (pthread_cond_wait(&cleanup_done_cv, &cleanup_lock)) {
			pthread_mutex_unlock(&cleanup_lock);
			syslog(LOG_ERR, "auto_rddir_strdup interrupted\n");
		} else {
			pthread_mutex_unlock(&cleanup_lock);
			again = 1;
		}
	}

	if (again)
		s2 = strdup(s1);

	return (s2);
}

/*
 * Returns a pointer to the entry corresponding to 'name' if found,
 * otherwise it returns NULL.
 */
struct dir_entry *
btree_lookup(struct dir_entry *head, const char *name)
{
	register struct dir_entry *p;
	register int direction;

	for (p = head; p != NULL; ) {
		direction = strcmp(name, p->name);
		if (direction == 0)
			return (p);
		if (direction > 0)
			p = p->right;
		else p = p->left;
	}
	return (NULL);
}

/*
 * Add entry to binary tree
 * Duplicate entries are not added
 */
void
btree_enter(struct dir_entry **head, struct dir_entry *ent)
{
	register struct dir_entry *p, *prev = NULL;
	register int direction;

	ent->right = ent->left = NULL;
	if (*head == NULL) {
		*head = ent;
		return;
	}

	for (p = *head; p != NULL; ) {
		prev = p;
		direction = strcmp(ent->name, p->name);
		if (direction == 0) {
			/*
			 * entry already in btree
			 */
			return;
		}
		if (direction > 0)
			p = p->right;
		else p = p->left;
	}
	assert(prev != NULL);
	if (direction > 0)
		prev->right = ent;
	else prev->left = ent;
}

/*
 * If entry doesn't exist already, add it to the linear list
 * after '*last' and to the binary tree list.
 * If '*last == NULL' then the list is walked till the end.
 * *last is always set to the new element after successful completion.
 * if entry already exists '*last' is only updated if not previously
 * provided.
 *
 * Returns 0 on success, -1 if the name isn't valid (".", "..", or
 * contains "/"), an errno value on error.
 */
int
add_dir_entry(const char *name, const char *linebuf, const char *lineqbuf,
    struct dir_entry **list, struct dir_entry **last)
{
	struct dir_entry *e, *l;
	const char *p;

	if (name[0] == '.') {
		if (name[1] == '\0')
			return (-1);	/* "." */
		if (name[1] == '.' && name[2] == '\0')
			return (-1);	/* ".." */
	}
	for (p = name; *p != '\0'; p++) {
		if (*p == '/')
			return (-1);
	}

	if ((*list != NULL) && (*last == NULL)) {
		/*
		 * walk the list to find last element
		 */
		for (l = *list; l != NULL; l = l->next)
			*last = l;
	}

	if (btree_lookup(*list, name) == NULL) {
		/*
		 * not a duplicate, add it to list
		 */
		/* LINTED pointer alignment */
		e = (struct dir_entry *)
			auto_rddir_malloc(sizeof (struct dir_entry));
		if (e == NULL)
			return (ENOMEM);
		(void) memset((char *)e, 0, sizeof (*e));
		e->name = auto_rddir_strdup(name);
		if (e->name == NULL) {
			free(e);
			return (ENOMEM);
		}
		if (linebuf != NULL) {
			/*
			 * If linebuf != NULL, lineqbuf must != NULL
			 * as well.
			 */
			e->line = auto_rddir_strdup(linebuf);
			if (e->line == NULL) {
				free(e->name);
				free(e);
				return (ENOMEM);
			}
			e->lineq = auto_rddir_strdup(lineqbuf);
			if (e->lineq == NULL) {
				free(e->line);
				free(e->name);
				free(e);
				return (ENOMEM);
			}
		} else {
			e->line = NULL;
			e->lineq = NULL;
		}
		e->next = NULL;
		if (*list == NULL) {
			/*
			 * list is empty
			 */
			*list = *last = e;
		} else {
			/*
			 * append to end of list
			 */
			assert(*last != NULL);
			(*last)->next = e;
			*last = e;
		}
		/*
		 * add to binary tree
		 */
		btree_enter(list, e);
	}
	return (0);
}

/*
 * Log trace output.
 */
void
trace_prt(__unused int newmsg, char *fmt, ...)
{
	va_list args;
        static dispatch_once_t pred;

        dispatch_once(&pred, ^{
		/*
		 * Send a message to the syslog that turns
		 * off the messages-per-second limit.
		 */
		aslmsg m = asl_new(ASL_TYPE_MSG);
	
		asl_set(m, "ASLOption", "control");
		asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
		asl_set(m, ASL_KEY_MSG, "= mps_limit 0");
	 	asl_send(NULL, m);
		asl_free(m);
	});

	va_start(args, fmt);
	(void) vsyslog(LOG_ERR, fmt, args);
	va_end(args);
}

/*
 * Return the name of the highest-bitness ISA for this machine.
 * We assume here that, as this is part of the OS, it'll be built
 * fat enough that the ISA for which we're compiled is the ISA in
 * question.  We also assume (correctly, as of the current version
 * of our compiler) that, when building for x86-64, __x86_64__ is
 * defined and __i386__ isn't, and we assume (correctly) that we
 * aren't supporting 64-bit PowerPC any more.
 */
static int
natisa(char *buf, size_t bufsize)
{
#if defined(__ppc__)
	(void) strlcpy(buf, "powerpc", bufsize);
#elif defined(__i386__)
	(void) strlcpy(buf, "i386", bufsize);
#elif defined(__x86_64__)
	(void) strlcpy(buf, "x86_64", bufsize);
#else
#error "can't determine native ISA"
#endif
	return (1);
}
