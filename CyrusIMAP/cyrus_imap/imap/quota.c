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

/* $Id: quota.c,v 1.10 2005/08/10 21:38:44 dasenbro Exp $ */


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
#include <com_err.h>
#include <pwd.h>
#include <sys/wait.h>

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

/* forward declarations */
void usage(void);
void reportquota(void);
void genquotareport(void);
int doquotacheck( void );
int buildquotalist(char *domain, char **roots, int nroots);
int fixquota_mailbox(char *name,
		     int matchlen,
		     int maycreate,
		     void *rock);
int fixquota(char *domain, int ispartial);
int fixquota_fixroot(struct mailbox *mailbox,
		     char *root);
int fixquota_finish(int thisquota, struct txn **tid, unsigned long *count);

struct fix_rock {
    char *domain;
    struct txn **tid;
    unsigned long change_count;
};

struct quotaentry {
    struct quota mail_quota;
    int refcount;
    int deleted;
    unsigned long newused;
};

#define QUOTAGROW 300

struct quotaentry *quota_list;
int quota_num = 0, quota_alloc = 0;

int firstquota;
int redofix;
int partial;

int main(int argc,char **argv)
{
    int opt;
    int fflag = 0;
	int qflag = 0;
    int rflag = 0;
    int r, code = 0;
    char *alt_config = NULL, *domain = NULL;

    if (geteuid() == 0) fatal("must run as the Cyrus user", EC_USAGE);

    while ((opt = getopt(argc, argv, "C:d:fqr")) != EOF) {
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

	case 'q':
		qflag = 1;
		fflag = 1;
		break;

	case 'r':
	    rflag = 1;
	    break;

	default:
	    usage();
	}
    }

    cyrus_init(alt_config, "cyrus-quota", 0);

    /* Set namespace -- force standard (internal) */
    if ((r = mboxname_init_namespace(&quota_namespace, 1)) != 0) {
	syslog(LOG_ERR, error_message(r));
	fatal(error_message(r), EC_CONFIG);
    }

    quotadb_init(0);
    quotadb_open(NULL);

    if (!r) r = buildquotalist(domain, argv+optind, argc-optind);

    if (!r && fflag) {
	mboxlist_init(0);
	r = fixquota(domain, argc-optind);
	mboxlist_done();
    }

    quotadb_close();
    quotadb_done();

	if ( !r )
	{
		if ( qflag == 1 )
		{
			r = doquotacheck();
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

    if (r) {
	com_err("cyrus-quota", r, (r == IMAP_IOERROR) ? error_message(errno) : NULL);
	code = convert_code(r);
    }

    cyrus_done();

    return code;
}

void usage(void)
{
    fprintf(stderr, "usage: cyrus-quota [-C <alt_config>] [-d <domain>] [-f] [prefix]...\n");
    exit(EC_USAGE);
}    

struct find_rock {
    char **roots;
    int nroots;
};

static int find_p(void *rockp,
		  const char *key, int keylen,
		  const char *data __attribute__((unused)),
		  int datalen __attribute__((unused)))
{
    struct find_rock *frock = (struct find_rock *) rockp;
    int i;

    /* If restricting our list, see if this quota root matches */
    if (frock->nroots) {
	const char *p;

	/* skip over domain */
	if (config_virtdomains && (p = strchr(key, '!'))) {
	    keylen -= (++p - key);
	    key = p;
	}

 	for (i = 0; i < frock->nroots; i++) {
	    if (keylen >= strlen(frock->roots[i]) &&
		!strncmp(key, frock->roots[i], strlen(frock->roots[i]))) {
		break;
	    }
	}
	if (i == frock->nroots) return 0;
    }

    return 1;
}

/*
 * Add a quota root to the list in 'quota'
 */
static int find_cb(void *rockp __attribute__((unused)),
		   const char *key, int keylen,
		   const char *data, int datalen __attribute__((unused)))
{
    if (!data) return 0;

    if (quota_num == quota_alloc) {
	quota_alloc += QUOTAGROW;
	quota_list = (struct quotaentry *)
	  xrealloc((char *)quota_list, quota_alloc * sizeof(struct quotaentry));
    }
    memset(&quota_list[quota_num], 0, sizeof(struct quotaentry));
    quota_list[quota_num].mail_quota.root = xstrndup(key, keylen);
    sscanf(data, "%lu %d",
	   &quota_list[quota_num].mail_quota.used, &quota_list[quota_num].mail_quota.limit);
  
    quota_num++;

    return 0;
}

/*
 * Build the list of quota roots in 'quota'
 */
int buildquotalist(char *domain, char **roots, int nroots)
{
    int i;
    char buf[MAX_MAILBOX_NAME+1];
    struct find_rock frock;

    frock.roots = roots;
    frock.nroots = nroots;

    /* Translate separator in mailboxnames.
     *
     * We do this directly instead of using the mboxname_tointernal()
     * function pointer because we know that we are using the internal
     * namespace and so we don't have to allocate a buffer for the
     * translated name.
     */
    for (i = 0; i < nroots; i++) {
	mboxname_hiersep_tointernal(&quota_namespace, roots[i], 0);
    }

    buf[0] = '\0';
    if (domain) snprintf(buf, sizeof(buf), "%s!", domain);

    /* if we have exactly one root specified, narrow the search */
    if (nroots == 1) strlcat(buf, roots[0], sizeof(buf));
	    
    config_quota_db->foreach(qdb, buf, strlen(buf),
			     &find_p, &find_cb, &frock, NULL);

    return 0;
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


int doquotacheck ( void )
{
	int			r				= 0;
    int			i				= 0;
	int			fd[ 2 ];
	int			warnLevel		= 0;
	int			status			= 0;
	pid_t		pid				= 0;
	char	   *warnData		= NULL;
	char	   *errorData		= NULL;
    int			usage			= 0;
    char		user[ MAX_MAILBOX_PATH + 1 ];
    char		buf[ MAX_MAILBOX_PATH + 1 ];

	/* don't do quota check of option not set */
	if ( config_getswitch( IMAPOPT_ENABLE_QUOTA_WARNINGS ) )
	{
		warnData  = get_message_text( IMAPOPT_QUOTA_CUSTOM_WARNING_PATH );
		errorData = get_message_text( IMAPOPT_QUOTA_CUSTOM_ERROR_PATH );

		warnLevel = config_getint( IMAPOPT_QUOTAWARN );

		for ( i = 0; i < quota_num; i++ )
		{
			if ( quota_list[ i ].deleted )
			{
				continue;
			}

			if ( quota_list[ i ].mail_quota.limit > 0 )
			{
				usage = ((quota_list[i].mail_quota.used / QUOTA_UNITS) * 100) / quota_list[i].mail_quota.limit;
				if ( usage >= warnLevel )
				{
					/* send quota warning */
					if ( pipe( fd ) >= 0 )
					{
						if ( (pid = fork()) >= 0 )
						{
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

								/* get deliver path */
								r = snprintf( buf, sizeof( buf ), "%s/deliver", SERVICE_PATH );
								if ( (r < 0) || (r >= sizeof( buf )) )
								{
									fatal( "deliver command buffer not sufficiently big", EC_CONFIG );
								}
								else
								{
									/* Convert internal name to external */
									(*quota_namespace.mboxname_toexternal)( &quota_namespace, quota_list[ i ].mail_quota.root, "cyrusimap", user );
									char *p = strchr( user, '/' );
									if ( p )
									{
										/* execute deliver command with user id */
										p++;
										execl( buf, buf, "-q", p, NULL );
									}
								}
								exit( 1);
							}
							else
							{
								/* -- parent -- */

								/* close file descriptors */
								close( STDIN_FILENO );
								close( STDOUT_FILENO );
								close( fd[ 0 ] );

								if ( usage >= 100 )
								{
									/* dump message to the pipe */
									if ( errorData != NULL )
									{
										write( fd[ 1 ], errorData, strlen( errorData ) );
									}
								}
								else
								{
									/* dump message to the pipe */
									if ( warnData != NULL )
									{
										write( fd[ 1 ], warnData, strlen( warnData ) );
									}
								}

								/* close the pipe when finished */
								close( fd[ 1 ] );

								/* wait for it */
								(*quota_namespace.mboxname_toexternal)( &quota_namespace, quota_list[ i ].mail_quota.root, "cyrusimap", user );

								if ( usage >= 100 )
								{
									syslog( LOG_INFO, "User account \"%s\" has exceeded quota.", user );
								}
								else
								{
									syslog( LOG_INFO, "User account \"%s\" is approaching quota.", user );
								}
								waitpid( pid, &status, 0 );
							}
						}
					}
				}
			}
		}
	}

	return( 0 );

} /* doquotacheck */


/*
 * Account for mailbox 'name' when fixing the quota roots
 */
int fixquota_mailbox(char *name,
		     int matchlen __attribute__((unused)),
		     int maycreate __attribute__((unused)),
		     void* rock)
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
	   strncmp(name, quota_list[firstquota].mail_quota.root,
		       strlen(quota_list[firstquota].mail_quota.root)) > 0) {
	r = fixquota_finish(firstquota++, frock->tid, &frock->change_count);
	if (r) return r;
    }

    thisquota = -1;
    thisquotalen = 0;
    for (i = firstquota;
	 i < quota_num && strcmp(name, quota_list[i].mail_quota.root) >= 0; i++) {
	len = strlen(quota_list[i].mail_quota.root);
	if (!strncmp(name, quota_list[i].mail_quota.root, len) &&
	    (!name[len] || name[len] == '.' ||
	     (domain && name[len-1] == '!'))) {
	    quota_list[i].refcount++;
	    if (len > thisquotalen) {
		thisquota = i;
		thisquotalen = len;
	    }
	}
    }

    if (partial && thisquota == -1) return 0;

    r = mailbox_open_header(name, 0, &mailbox);
    if (!r) {
	if (thisquota == -1) {
	    if (mailbox.quota.root) {
		r = fixquota_fixroot(&mailbox, (char *)0);
	    }
	}
	else {
	    if (!mailbox.quota.root ||
		strcmp(mailbox.quota.root, quota_list[thisquota].mail_quota.root) != 0) {
		r = fixquota_fixroot(&mailbox, quota_list[thisquota].mail_quota.root);
	    }
	    if (!r) r = mailbox_open_index(&mailbox);
   
	    if (!r) quota_list[thisquota].newused += mailbox.quota_mailbox_used;
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
    if (r) return r;

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
    return r;
}

/*
 * Finish fixing up a quota root
 */
int fixquota_finish(int thisquota, struct txn **tid, unsigned long *count)
{
    int r;

    if (!quota_list[thisquota].refcount) {
	if (!quota_list[thisquota].deleted++) {
	    printf("%s: removed\n", quota_list[thisquota].mail_quota.root);
	    r = quota_delete(&quota_list[thisquota].mail_quota, tid);
	    if (r) return r;
	    (*count)++;
	    free(quota_list[thisquota].mail_quota.root);
	    quota_list[thisquota].mail_quota.root = NULL;
	}
	return 0;
    }

    if (quota_list[thisquota].mail_quota.used != quota_list[thisquota].newused) {
	/* re-read the quota with the record locked */
	r = quota_read(&quota_list[thisquota].mail_quota, tid, 1);
	if (r) return r;
	(*count)++;
    }
    if (quota_list[thisquota].mail_quota.used != quota_list[thisquota].newused) {
	printf("%s: usage was %lu, now %lu\n", quota_list[thisquota].mail_quota.root,
	       quota_list[thisquota].mail_quota.used, quota_list[thisquota].newused);
	quota_list[thisquota].mail_quota.used = quota_list[thisquota].newused;
	r = quota_write(&quota_list[thisquota].mail_quota, tid);
	if (r) return r;
	(*count)++;
    }

    /* commit the transaction every 100 changes */
    if (*count && !(*count % 100)) {
	quota_commit(tid);
	*tid = NULL;
    }

    return 0;
}


/*
 * Fix all the quota roots
 */
int fixquota(char *domain, int ispartial)
{
    int r = 0;
    static char pattern[2] = "*";
    struct fix_rock frock;
    struct txn *tid = NULL;

    /*
     * Lock mailbox list to prevent mailbox creation/deletion
     * during the fix
     */
    mboxlist_open(NULL);

    frock.domain = domain;
    frock.tid = &tid;
    frock.change_count = 0;

    redofix = 1;
    while (!r && redofix) {
	redofix = 0;
	firstquota = 0;
	partial = ispartial;

	r = (*quota_namespace.mboxlist_findall)(&quota_namespace, pattern, 1,
						0, 0, fixquota_mailbox, &frock);
	while (!r && firstquota < quota_num) {
	    r = fixquota_finish(firstquota++, &tid, &frock.change_count);
	}
    }

    if (!r && tid) quota_commit(&tid);
    
    mboxlist_close();

    return 0;
}
    
/*
 * Print out the quota report
 */
void
reportquota(void)
{
    int i;
    char buf[MAX_MAILBOX_PATH+1];

    printf("   Quota  %% Used    Used Root\n");

    for (i = 0; i < quota_num; i++) {
	if (quota_list[i].deleted) continue;
	if (quota_list[i].mail_quota.limit > 0) {
	    printf(" %7d %7ld", quota_list[i].mail_quota.limit,
		   ((quota_list[i].mail_quota.used / QUOTA_UNITS) * 100) / quota_list[i].mail_quota.limit);
	}
	else if (quota_list[i].mail_quota.limit == 0) {
	    printf("       0        ");
	}
	else {
	    printf("                ");
	}
	/* Convert internal name to external */
	(*quota_namespace.mboxname_toexternal)(&quota_namespace,
					       quota_list[i].mail_quota.root,
					       "cyrusimap", buf);
	printf(" %7ld %s\n", quota_list[i].mail_quota.used / QUOTA_UNITS, buf);
    }
}

/*
 * Print out the quota report
 */
const char *dict = "		<dict>\n"
"			<key>acctLocation</key>\n"
"			<string>%s</string>\n"
"			<key>diskQuota</key>\n"
"			<integer>%d</integer>\n"
"			<key>diskUsed</key>\n"
"			<integer>%d</integer>\n"
"			<key>name</key>\n"
"			<string>%s</string>\n"
"		</dict>\n";

const char *header = "<dict>\n	<key>accountsArray</key>\n	<array>\n";
const char *tailer = "	</array>\n</dict>";
#include "libcyr_cfg.h"

void
genquotareport(void)
{
    int				i = 0;
	char		   *partition = NULL;
	int				type = 0;
	const char	   *dbDir = libcyrus_config_getstring( CYRUSOPT_CONFIG_DIR );
	FILE		   *f = NULL;
	char			userbuf[ MAX_MAILBOX_PATH + 1 ];
    char			tmpbuf[ MAX_MAILBOX_PATH + 1 ];
    char			filePath[ MAX_MAILBOX_PATH + 1 ];

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
			mboxlist_detail( quota_list[ i ].mail_quota.root, &type, NULL, &partition, NULL, NULL );

			/* convert internal name to external */
			(*quota_namespace.mboxname_toexternal)
				(&quota_namespace, quota_list[i].mail_quota.root, "cyrusimap", userbuf);

			char *p = strchr( userbuf, '/' );
			if ( p )
			{
				p++;

				memset( tmpbuf, 0, sizeof( tmpbuf ) );
				if ( (quota_list[ i ].mail_quota.limit == 0) || (quota_list[ i ].mail_quota.limit == 2147483647) )
				{
					snprintf( tmpbuf, sizeof( tmpbuf ), dict, partition, 0, quota_list[ i ].mail_quota.used, p );
				}
				else
				{
					snprintf( tmpbuf, sizeof( tmpbuf ), dict, partition, quota_list[ i ].mail_quota.limit * QUOTA_UNITS, quota_list[ i ].mail_quota.used, p );
				}

				fwrite( tmpbuf, sizeof( char ), strlen( tmpbuf ), f );
			}
		}

		fwrite( tailer, sizeof( char ), strlen( tailer ), f );

		fflush( f );
		fclose( f );

		mboxlist_close();
		mboxlist_done();
	}
}
