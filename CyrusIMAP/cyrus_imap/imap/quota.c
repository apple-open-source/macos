/* quota.c -- program to report/reconstruct quotas
 *
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $Id: quota.c,v 1.67 2007/02/05 18:41:48 jeaton Exp $ */


#include <config.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>

#ifdef APPLE_OS_X_SERVER
#include <com_err.h>
#include <pwd.h>
#include <sys/wait.h>
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "assert.h"
#include "cyrusdb.h"
#include "global.h"
#include "exitcodes.h"
#include "imap_err.h"
#include "mailbox.h"
#include "xmalloc.h"
#include "xstrlcpy.h"
#include "xstrlcat.h"
#include "mboxlist.h"
#include "mboxname.h"
#include "quota.h"
#include "convert_code.h"
#include "util.h"

extern int optind;
extern char *optarg;

/* current namespace */
static struct namespace quota_namespace;

/* config.c stuff */
const int config_need_data = CONFIG_NEED_PARTITION_DATA;

struct fix_rock {
    char *domain;
    struct txn **tid;
    unsigned long change_count;
};

struct quotaentry {
#ifdef APPLE_OS_X_SERVER
    struct quota mail_quota;
#else
    struct quota quota;
#endif
    int refcount;
    int deleted;
    uquota_t newused;
};

/* forward declarations */
void usage(void);
void reportquota(void);
int buildquotalist(char *domain, char **roots, int nroots,
		   struct fix_rock *frock);
int fixquota_mailbox(char *name, int matchlen, int maycreate, void *rock);
int fixquota(struct fix_rock *frock);
int fixquota_fixroot(struct mailbox *mailbox, char *root);
int fixquota_finish(int thisquota, struct txn **tid, unsigned long *count);

#ifdef APPLE_OS_X_SERVER
void genquotareport(void);
void doquotacheck( void );
#endif

#define QUOTAGROW 300

#ifdef APPLE_OS_X_SERVER
struct quotaentry *quota_list;
#else
struct quotaentry *quota;
#endif
int quota_num = 0, quota_alloc = 0;

int firstquota = 0;
int redofix = 0;
int partial;

int main(int argc,char **argv)
{
    int opt;
    int fflag = 0;
    int r, code = 0;
    char *alt_config = NULL, *domain = NULL;
    struct fix_rock frock;
    struct txn *tid = NULL;
#ifdef APPLE_OS_X_SERVER
	int qflag = 0;
    int rflag = 0;
#endif

    if (geteuid() == 0) fatal("must run as the Cyrus user", EC_USAGE);

#ifdef APPLE_OS_X_SERVER
    while ((opt = getopt(argc, argv, "C:d:fqr")) != EOF) {
#else
    while ((opt = getopt(argc, argv, "C:d:f")) != EOF) {
#endif
	switch (opt) {
	case 'C': /* alt config file */
	    alt_config = optarg;
	    break;

	case 'd':
	    domain = optarg;
	    break;

	case 'f':
	    fflag = 1;
	    break;

#ifdef APPLE_OS_X_SERVER
	case 'q':
		qflag = 1;
		fflag = 1;
		break;

	case 'r':
	    rflag = 1;
	    break;
#endif

	default:
	    usage();
	}
    }

#ifdef APPLE_OS_X_SERVER
    cyrus_init(alt_config, "cyrus-quota", 0);
#else
    cyrus_init(alt_config, "quota", 0);
#endif

    /* Set namespace -- force standard (internal) */
    if ((r = mboxname_init_namespace(&quota_namespace, 1)) != 0) {
	syslog(LOG_ERR, error_message(r));
	fatal(error_message(r), EC_CONFIG);
    }

    /*
     * Lock mailbox list to prevent mailbox creation/deletion
     * during work
     */
    mboxlist_init(0);
    mboxlist_open(NULL);

    quotadb_init(0);
    quotadb_open(NULL);

    if (!r) {
	frock.domain = domain;
	frock.tid = &tid;
	frock.change_count = 0;

	r = buildquotalist(domain, argv+optind, argc-optind,
			   fflag ? &frock : NULL);
    }

    if (!r && fflag) {
	partial = argc-optind;
	r = fixquota(&frock);
    }

    quotadb_close();
    quotadb_done();

    mboxlist_close();
    mboxlist_done();

#ifdef APPLE_OS_X_SERVER
	if(!r)
	{
		if ( qflag == 1 )
		{
			doquotacheck();
		}
		else if ( rflag == 1 )
		{
			genquotareport();
		}
		else
		{
			reportquota();
		}
	}
	else
	{
		 code = convert_code(r);
	}
#else
    if (r) code = convert_code(r);
    else reportquota();
#endif

    cyrus_done();

    return code;
}

void usage(void)
{
    fprintf(stderr,
#ifdef APPLE_OS_X_SERVER
	    "usage: cyrus-quota [-C <alt_config>] [-d <domain>] [-f] [prefix]...\n");
#else
	    "usage: quota [-C <alt_config>] [-d <domain>] [-f] [prefix]...\n");
#endif
    exit(EC_USAGE);
}    

void errmsg(const char *fmt, const char *arg, int err)
{
    char buf[1024];
    int len;

    len = snprintf(buf, sizeof(buf), fmt, arg);
    if (len < sizeof(buf))
	len += snprintf(buf+len, sizeof(buf)-len, ": %s", error_message(err));
    if ((err == IMAP_IOERROR) && (len < sizeof(buf)))
	len += snprintf(buf+len, sizeof(buf)-len, ": %%m");

    syslog(LOG_ERR, buf);
    fprintf(stderr, "%s\n", buf);
}

/*
 * A matching mailbox was found, process it.
 */
static int found_match(char *name, int matchlen, int maycreate, void *frock)
{
    int r;

    if (quota_num == quota_alloc) {
	/* Create new qr list entry */
	quota_alloc += QUOTAGROW;
#ifdef APPLE_OS_X_SERVER
	quota_list = (struct quotaentry *)
	    xrealloc((char *)quota_list, quota_alloc * sizeof(struct quotaentry));
	memset(&quota_list[quota_num], 0, QUOTAGROW * sizeof(struct quotaentry));
#else
	quota = (struct quotaentry *)
	    xrealloc((char *)quota, quota_alloc * sizeof(struct quotaentry));
	memset(&quota[quota_num], 0, QUOTAGROW * sizeof(struct quotaentry));
#endif
    }

    /* See if the mailbox name corresponds to a quotaroot */
#ifdef APPLE_OS_X_SERVER
    quota_list[quota_num].mail_quota.root = name;
    do {
	r = quota_read(&quota_list[quota_num].mail_quota, NULL, 0);
#else
    quota[quota_num].quota.root = name;
    do {
	r = quota_read(&quota[quota_num].quota, NULL, 0);
#endif
    } while (r == IMAP_AGAIN);

    switch (r) {
    case 0:
	/* Its a quotaroot! */
#ifdef APPLE_OS_X_SERVER
	quota_list[quota_num++].mail_quota.root = xstrdup(name);
#else
	quota[quota_num++].quota.root = xstrdup(name);
#endif
	break;
    case IMAP_QUOTAROOT_NONEXISTENT:
	if (!frock || !quota_num ||
#ifdef APPLE_OS_X_SERVER
	    strncmp(name, quota_list[quota_num-1].mail_quota.root,
		    strlen(quota_list[quota_num-1].mail_quota.root))) {
#else
	    strncmp(name, quota[quota_num-1].quota.root,
		    strlen(quota[quota_num-1].quota.root))) {
#endif
	    /* Its not a quotaroot, and either we're not fixing quotas,
	       or its not part of the most recent quotaroot */
	    return 0;
	}
	break;
    default:
	return r;
	break;
    }

    if (frock) {
	/* Recalculate the quota (we need the subfolders too!) */
	r = fixquota_mailbox(name, matchlen, maycreate, frock);
    }

    return r;
}

/*
 * Build the list of quota roots in 'quota'
 */
int buildquotalist(char *domain, char **roots, int nroots,
		   struct fix_rock *frock)
{
    int i, r;
    char buf[MAX_MAILBOX_NAME+1], *tail;
    size_t domainlen = 0;

    buf[0] = '\0';
    tail = buf;
    if (domain) {
	domainlen = snprintf(buf, sizeof(buf), "%s!", domain);
	tail += domainlen;
    }

    /*
     * Walk through all given pattern(s) and resolve them to all
     * matching mailbox names. Call found_match() for every mailbox
     * name found. If no pattern is given, assume "*".
     */
    i = 0;
    do {
	if (nroots > 0) {
	    /* Translate separator in quotaroot.
	     *
	     * We do this directly instead of using the mboxname_tointernal()
	     * function pointer because we know that we are using the internal
	     * namespace and so we don't have to allocate a buffer for the
	     * translated name.
	     */
	    mboxname_hiersep_tointernal(&quota_namespace, roots[i], 0);

	    strlcpy(tail, roots[i], sizeof(buf) - domainlen);
	}
	else {
	    strlcpy(tail, "*", sizeof(buf) - domainlen);
	}
	i++;

	r = (*quota_namespace.mboxlist_findall)(&quota_namespace, buf, 1, 0, 0,
						&found_match, frock);
	if (r < 0) {
	    errmsg("failed building quota list for '%s'", buf, IMAP_IOERROR);
	    return IMAP_IOERROR;
	}

    } while (i < nroots);

    return 0;
}

/*
 * Account for mailbox 'name' when fixing the quota roots
 */
int fixquota_mailbox(char *name,
		     int matchlen __attribute__((unused)),
		     int maycreate __attribute__((unused)),
		     void *rock)
{
    int r;
    struct mailbox mailbox;
    int i, len, thisquota, thisquotalen;
    struct fix_rock *frock = (struct fix_rock *) rock;
    char *p, *domain = frock->domain;

    /* make sure the domains match */
    if (domain &&
	(!(p = strchr(name, '!')) || (p - name) != strlen(domain) ||
	 strncmp(name, domain, p - name))) {
	return 0;
    }

    while (firstquota < quota_num &&
#ifdef APPLE_OS_X_SERVER
	   strncmp(name, quota_list[firstquota].mail_quota.root,
		       strlen(quota_list[firstquota].mail_quota.root)) > 0) {
#else
	   strncmp(name, quota[firstquota].quota.root,
		       strlen(quota[firstquota].quota.root)) > 0) {
#endif
	r = fixquota_finish(firstquota++, frock->tid, &frock->change_count);
	if (r) return r;
    }

    thisquota = -1;
    thisquotalen = 0;
    for (i = firstquota;
#ifdef APPLE_OS_X_SERVER
	 i < quota_num && strcmp(name, quota_list[i].mail_quota.root) >= 0; i++) {
	len = strlen(quota_list[i].mail_quota.root);
	if (!strncmp(name, quota_list[i].mail_quota.root, len) &&
#else
	 i < quota_num && strcmp(name, quota[i].quota.root) >= 0; i++) {
	len = strlen(quota[i].quota.root);
	if (!strncmp(name, quota[i].quota.root, len) &&
#endif
	    (!name[len] || name[len] == '.' ||
	     (domain && name[len-1] == '!'))) {
#ifdef APPLE_OS_X_SERVER
	    quota_list[i].refcount++;
#else
	    quota[i].refcount++;
#endif
	    if (len > thisquotalen) {
		thisquota = i;
		thisquotalen = len;
	    }
	}
    }

    if (partial && thisquota == -1) return 0;

    r = mailbox_open_header(name, 0, &mailbox);
    if (r) errmsg("failed opening header for mailbox '%s'", name, r);
    else {
	if (thisquota == -1) {
	    if (mailbox.quota.root) {
		r = fixquota_fixroot(&mailbox, (char *)0);
	    }
	}
	else {
	    if (!mailbox.quota.root ||
#ifdef APPLE_OS_X_SERVER
		strcmp(mailbox.quota.root, quota_list[thisquota].mail_quota.root) != 0) {
		r = fixquota_fixroot(&mailbox, quota_list[thisquota].mail_quota.root);
#else
		strcmp(mailbox.quota.root, quota[thisquota].quota.root) != 0) {
		r = fixquota_fixroot(&mailbox, quota[thisquota].quota.root);
#endif
	    }
	    if (!r) {
		r = mailbox_open_index(&mailbox);
		if (r) errmsg("failed opening index for mailbox '%s'", name, r);
	    }

#ifdef APPLE_OS_X_SERVER
	    if (!r) quota_list[thisquota].newused += mailbox.quota_mailbox_used;
#else
	    if (!r) quota[thisquota].newused += mailbox.quota_mailbox_used;
#endif
	}

	mailbox_close(&mailbox);
    }

    if (r) {
	/* mailbox error of some type, commit what we have */
	quota_commit(frock->tid);
	*(frock->tid) = NULL;
    }

    return r;
}
	
int fixquota_fixroot(struct mailbox *mailbox,
		     char *root)
{
    int r;

    redofix = 1;

    r = mailbox_lock_header(mailbox);
    if (r) {
	errmsg("failed locking header for mailbox '%s'", mailbox->name, r);
	return r;
    }

    printf("%s: quota root %s --> %s\n", mailbox->name,
	   mailbox->quota.root ? mailbox->quota.root : "(none)",
	   root ? root : "(none)");

    if (mailbox->quota.root) free(mailbox->quota.root);
    if (root) {
	mailbox->quota.root = xstrdup(root);
    }
    else {
	mailbox->quota.root = 0;
    }

    r = mailbox_write_header(mailbox);
    (void) mailbox_unlock_header(mailbox);
    if (r) errmsg("failed writing header for mailbox '%s'", mailbox->name, r);

    return r;
}

/*
 * Finish fixing up a quota root
 */
int fixquota_finish(int thisquota, struct txn **tid, unsigned long *count)
{
    int r = 0;

#ifdef APPLE_OS_X_SERVER
    if (!quota_list[thisquota].refcount) {
	if (!quota_list[thisquota].deleted++) {
	    printf("%s: removed\n", quota_list[thisquota].mail_quota.root);
	    r = quota_delete(&quota_list[thisquota].mail_quota, tid);
#else
    if (!quota[thisquota].refcount) {
	if (!quota[thisquota].deleted++) {
	    printf("%s: removed\n", quota[thisquota].quota.root);
	    r = quota_delete(&quota[thisquota].quota, tid);
#endif
	    if (r) {
		errmsg("failed deleting quotaroot '%s'",
#ifdef APPLE_OS_X_SERVER
		       quota_list[thisquota].mail_quota.root, r);
#else
		       quota[thisquota].quota.root, r);
#endif
		return r;
	    }
	    (*count)++;
#ifdef APPLE_OS_X_SERVER
	    free(quota_list[thisquota].mail_quota.root);
	    quota_list[thisquota].mail_quota.root = NULL;
	}
	return 0;
    }

    if (quota_list[thisquota].mail_quota.used != quota_list[thisquota].newused) {
#else
	    free(quota[thisquota].quota.root);
	    quota[thisquota].quota.root = NULL;
	}
	return 0;
    }

    if (quota[thisquota].quota.used != quota[thisquota].newused) {
#endif
	/* re-read the quota with the record locked */
#ifdef APPLE_OS_X_SERVER
	r = quota_read(&quota_list[thisquota].mail_quota, tid, 1);
#else
	r = quota_read(&quota[thisquota].quota, tid, 1);
#endif
	if (r) {
	    errmsg("failed reading quotaroot '%s'",
#ifdef APPLE_OS_X_SERVER
		   quota_list[thisquota].mail_quota.root, r);
	    return r;
	}
	(*count)++;
    }
    if (quota_list[thisquota].mail_quota.used != quota_list[thisquota].newused) {
#else
		   quota[thisquota].quota.root, r);
	    return r;
	}
	(*count)++;
    }
    if (quota[thisquota].quota.used != quota[thisquota].newused) {
#endif
	printf("%s: usage was " UQUOTA_T_FMT ", now " UQUOTA_T_FMT "\n",
#ifdef APPLE_OS_X_SERVER
	       quota_list[thisquota].mail_quota.root,
	       quota_list[thisquota].mail_quota.used, quota_list[thisquota].newused);
	quota_list[thisquota].mail_quota.used = quota_list[thisquota].newused;
	r = quota_write(&quota_list[thisquota].mail_quota, tid);
#else
	       quota[thisquota].quota.root,
	       quota[thisquota].quota.used, quota[thisquota].newused);
	quota[thisquota].quota.used = quota[thisquota].newused;
	r = quota_write(&quota[thisquota].quota, tid);
#endif
	if (r) {
	    errmsg("failed writing quotaroot '%s'",
#ifdef APPLE_OS_X_SERVER
		   quota_list[thisquota].mail_quota.root, r);
#else
		   quota[thisquota].quota.root, r);
#endif
	    return r;
	}
	(*count)++;
    }

    /* commit the transaction every 100 changes */
    if (*count && !(*count % 100)) {
	quota_commit(tid);
	*tid = NULL;
    }

    return r;
}


/*
 * Fix all the quota roots
 */
int fixquota(struct fix_rock *frock)
{
    int i, r = 0;

    while (!r && redofix) {
	while (!r && firstquota < quota_num) {
	    r = fixquota_finish(firstquota++, frock->tid, &frock->change_count);
	}

	redofix = 0;
	firstquota = 0;

	/*
	 * Loop over all qr entries and recalculate the quota.
	 * We need the subfolders too!
	 */
	for (i = 0; !r && i < quota_num; i++) {
	    r = (*quota_namespace.mboxlist_findall)(&quota_namespace,
#ifdef APPLE_OS_X_SERVER
						    quota_list[i].mail_quota.root,
#else
						    quota[i].quota.root,
#endif
						    1, 0, 0, fixquota_mailbox,
						    frock);
	}
    }

    while (!r && firstquota < quota_num) {
	r = fixquota_finish(firstquota++, frock->tid, &frock->change_count);
    }

    if (!r && *(frock->tid)) quota_commit(frock->tid);
    
    return 0;
}
    
/*
 * Print out the quota report
 */
void reportquota(void)
{
    int i;
    char buf[MAX_MAILBOX_PATH+1];

    printf("   Quota   %% Used     Used Root\n");

    for (i = 0; i < quota_num; i++) {
#ifdef APPLE_OS_X_SERVER
	if (quota_list[i].deleted) continue;
	if (quota_list[i].mail_quota.limit > 0) {
	    printf(" %7d " QUOTA_REPORT_FMT , quota_list[i].mail_quota.limit,
		   ((quota_list[i].mail_quota.used / QUOTA_UNITS) * 100) / quota_list[i].mail_quota.limit);
	}
	else if (quota_list[i].mail_quota.limit == 0) {
#else
	if (quota[i].deleted) continue;
	if (quota[i].quota.limit > 0) {
	    printf(" %7d " QUOTA_REPORT_FMT , quota[i].quota.limit,
		   ((quota[i].quota.used / QUOTA_UNITS) * 100) / quota[i].quota.limit);
	}
	else if (quota[i].quota.limit == 0) {
#endif
	    printf("       0        ");
	}
	else {
	    printf("                ");
	}
	/* Convert internal name to external */
	(*quota_namespace.mboxname_toexternal)(&quota_namespace,
#ifdef APPLE_OS_X_SERVER
					       quota_list[i].mail_quota.root,
					       "_cyrus", buf);
#else
					       quota[i].quota.root,
					       "cyrus", buf);
#endif
	printf(" " QUOTA_REPORT_FMT " %s\n",
#ifdef APPLE_OS_X_SERVER
	       quota_list[i].mail_quota.used / QUOTA_UNITS, buf);
#else
	       quota[i].quota.used / QUOTA_UNITS, buf);
#endif
    }
}

#ifdef APPLE_OS_X_SERVER
/*
 * Print out the quota report
 */
const char *_dict = "		<dict>\n"
"			<key>acctLocation</key>\n"
"			<string>%s</string>\n"
"			<key>diskQuota</key>\n"
"			<integer>%lu</integer>\n"
"			<key>diskUsed</key>\n"
"			<integer>%lu</integer>\n"
"			<key>percentFree</key>\n"
"			<integer>%lu</integer>\n"
"			<key>name</key>\n"
"			<string>%s</string>\n"
"		</dict>\n";

const char *header		= "<dict>\n	<key>accountsArray</key>\n	<array>\n";
const char *end_array	= "	</array>\n";
const char *ms_key		= "	<key>mail_store_size</key>\n	<string>%lld</string>\n";
const char *end_dict	= "</dict>";

#include "libcyr_cfg.h"

void genquotareport ( void )
{
    int				i = 0;
	char		   *partition = NULL;
	int				type = 0;
	const char	   *dbDir = libcyrus_config_getstring( CYRUSOPT_CONFIG_DIR );
	FILE		   *f = NULL;
	char			userbuf[ MAX_MAILBOX_PATH + 1 ];
    char			dataBuf[ MAX_MAILBOX_PATH + 1 ];
    char			filePath[ MAX_MAILBOX_PATH + 1 ];
	long long unsigned int				total = 0;

	if ( !dbDir )
	{
		return;
	}

	strcpy( filePath, dbDir );
	strcat( filePath, "/quota/quotadata.txt" );

    f = fopen( filePath, "w+" );
	if ( f )
	{
		mboxlist_init( 0 );
		mboxlist_open( NULL );

		lseek( fileno( f ), 0, SEEK_SET );
		fwrite( header, sizeof( char ), strlen( header ), f );

		for ( i = 0; i < quota_num; i++ )
		{
			if ( quota_list[ i ].deleted )
			{
				continue;
			}

			/* get the partition */
			mboxlist_detail( quota_list[ i ].mail_quota.root, &type, &partition, NULL, NULL, NULL, NULL );
			if ( partition == NULL )
			{
				partition = "default";
			}

			/* convert internal name to external */
			(*quota_namespace.mboxname_toexternal)
				(&quota_namespace, quota_list[i].mail_quota.root, "_cyrus", userbuf);

			char *p = strchr( userbuf, '/' );
			if ( p != NULL )
			{
				p++;

				unsigned int	quota_limit = 0;
				unsigned int	quota_used = 0;
				unsigned int	quota_free = 100;

				if ( (quota_list[ i ].mail_quota.limit > 0) && (quota_list[ i ].mail_quota.limit != 2147483647) )
				{
					quota_limit = quota_list[i].mail_quota.limit * QUOTA_UNITS;
					quota_free = 100 - (((quota_list[i].mail_quota.used * QUOTA_UNITS) * 100) / quota_list[i].mail_quota.limit);
				}

				quota_used = quota_list[i].mail_quota.used;

				memset( dataBuf, 0, sizeof( dataBuf ) );
				snprintf( dataBuf, sizeof( dataBuf ), _dict, partition, quota_limit, quota_used, quota_free, p );

				fwrite( dataBuf, sizeof( char ), strlen( dataBuf ), f );

				total+= quota_list[ i ].mail_quota.used;
			}
		}

		// Close out the array of dictionaries
		fwrite( end_array, sizeof( char ), strlen( end_array ), f );

		// Set total
		if ( total != 0 )
		{
			snprintf( dataBuf, sizeof( dataBuf ), ms_key, (total / QUOTA_UNITS) );
		}
		else
		{
			snprintf( dataBuf, sizeof( dataBuf ), ms_key, 0 );
		}
		fwrite( dataBuf, sizeof( char ), strlen( dataBuf ), f );
		
		// Close out the dictionary
		fwrite( end_dict, sizeof( char ), strlen( end_dict ), f );

		fflush( f );
		fclose( f );

		mboxlist_close();
		mboxlist_done();
	}
}

char * get_message_text ( enum imapopt opt )
{
	char	   *fileData		= NULL;
	const char *filePath		= NULL;
	FILE	   *filePtr			= NULL;
    struct stat sfilestat;

	/* get custom warning message path */
	filePath = config_getstring( opt );
	if ( filePath != NULL )
	{
		/* does the file exist */
		if ( !stat( filePath, &sfilestat ) )
		{
			/* open file for reading */
			filePtr = fopen( filePath, "r" );
			if ( filePtr && (sfilestat.st_size > 0) )
			{
				/* allocate memory for file data */
				fileData = xmalloc( sfilestat.st_size + 1 );
				if ( fileData != NULL )
				{
					if ( read( fileno( filePtr ), fileData, sfilestat.st_size ) != sfilestat.st_size )
					{
						syslog( LOG_ERR, "Cannot read quota message file: %s", filePath );
						free( fileData );
						fileData = NULL;
					}
					else
					{
						fileData[ sfilestat.st_size ] = '\0';
					}
				}
				else
				{
					syslog( LOG_ERR, "Quota file memory allocation error" );
				}
				fclose( filePtr );
			}
			else
			{
				syslog( LOG_ERR, "Cannot open quota message \"%s\" for reading.", filePath );
			}
		}
		else
		{
			syslog( LOG_ERR, "Cannot find quota message file: %s", filePath );
		}
	}
	else
	{
		syslog( LOG_ERR, "Cannot get quota message path." );
	}

	return( fileData );

} /* get_message_text */


void doquotacheck ( void )
{
    int			i				= 0;
	int			warnLevel		= 0;
	int			status			= 0;
	pid_t		pid				= 0;
	char	   *error_txt		= NULL;
	char	   *warning_txt		= NULL;
    int			used			= 0;
    int			usage			= 0;
    int			limit			= 0;
	char	   *pUser			= NULL;
    char		user[ MAX_MAILBOX_PATH + 1 ];
	int			fd[ 2 ];

	/* don't do quota check of option not set */
	if ( config_getswitch( IMAPOPT_ENABLE_QUOTA_WARNINGS ) )
	{
		syslog( LOG_INFO, "Starting mail quota warning checks." );

		mboxlist_init( 0 );
		mboxlist_open( NULL );

		warning_txt  = get_message_text( IMAPOPT_QUOTA_CUSTOM_WARNING_PATH );
		error_txt = get_message_text( IMAPOPT_QUOTA_CUSTOM_ERROR_PATH );
		warnLevel = config_getint( IMAPOPT_QUOTAWARN );

		for ( i = 0; i < quota_num; i++ )
		{
			if ( quota_list[ i ].deleted )
			{
				continue;
			}

			/* Convert internal name to external */
			(*quota_namespace.mboxname_toexternal)( &quota_namespace, quota_list[ i ].mail_quota.root, "_cyrus", user );
			pUser = strchr( user, '/' );
			if ( pUser == NULL )
			{
				continue;
			}
			pUser++;

			limit = quota_list[ i ].mail_quota.limit;
			used  = quota_list[i].mail_quota.used / QUOTA_UNITS;
			usage = (used * 100) / limit;
			syslog( LOG_INFO, "Account: %s; Quota limit: %d MB; Usage: %d MB", pUser, (limit/QUOTA_UNITS), (used/QUOTA_UNITS) );

			if ( limit > 0 )
			{
				if ( usage >= warnLevel )
				{
					/* send quota warning */
					if ( pipe( fd ) >= 0 )
					{
						pid = fork();
						if ( pid == 0 )
						{
							/* -- child -- */
							/* duplicate pipe's reading end into stdin */
							status = dup2( fd[ 0 ], STDIN_FILENO );
							if ( status == -1 )
							{
								fatal( "deliver: dup2 error", EC_OSERR );
							}

							/* close file descriptors */
							close( fd[ 1 ] );
							close( fd[ 0 ] );

							/* exec deliver */
							execlp( "/usr/bin/cyrus/bin/deliver", "deliver", "-q", pUser, NULL );
							_exit( 0 );
						}
						else
						{
							/* -- parent -- */
							/* close file descriptors */
							close( fd[ 0 ] );

							if ( usage >= 100 )
							{
								/* dump message to the pipe */
								if ( error_txt != NULL )
								{
									syslog( LOG_INFO, "Sending quota exceeded error to: %s", pUser );
									write( fd[ 1 ], error_txt, strlen( error_txt ) );
								}
							}
							else
							{
								/* dump message to the pipe */
								if ( warning_txt != NULL )
								{
									write( fd[ 1 ], warning_txt, strlen( warning_txt ) );
									syslog( LOG_INFO, "Sending quota usage warning to: %s", pUser );
								}
							}

							/* close the pipe when finished */
							close( fd[ 1 ] );

							/* wait for it */
							waitpid( pid, &status, 0 );
						}
					}
					else
					{
						syslog( LOG_DEBUG, "ERROR: pipe failed." );
					}
				}
			}
		}

		mboxlist_close();
		mboxlist_done();
	}
	else
	{
		syslog( LOG_WARNING, "Mail quota warnings not enabled" );
	}
} /* doquotacheck */

#endif
