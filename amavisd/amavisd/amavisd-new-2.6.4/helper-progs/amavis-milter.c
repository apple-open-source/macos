/*
 * (V2.6) Based on amavis-milter.c,v 1.1.2.3.2.40 2003/06/06 12:34:58 lhecking Exp
 */

/*
 * sendmail/milter client for amavis
 * amavisd version
 *
 * Author: Geoff Winkless <gwinkless@users.sourceforge.net>
 * Additional work and patches by:
 *   Gregory Ade
 *   Anne Bennett
 *   Thomas Biege
 *   Pierre-Yves Bonnetain
 *   Lars Hecking
 *   Rainer Link
 *   Dale Perkel
 *   Julio Sanchez
 *   Stephane Lentz
 *   Mark Martinec
 */

/*
 * Add some copyright notice here ...
 *
 */

#include "config.h"

#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include <grp.h>
#include <pwd.h>

#ifdef HAVE_SM_GEN_H
# include "sm/gen.h"
#endif
#include "libmilter/mfapi.h"

typedef int mybool;

#define BUFFLEN 255
/* Must be the same as the buffer length for recv() in amavisd */
#define SOCKBUFLEN 8192

#ifndef RUNTIME_DIR
# define RUNTIME_DIR "/var/amavis"
#endif

#ifndef AMAVISD_SOCKET
# define AMAVISD_SOCKET RUNTIME_DIR ## "/amavisd.sock"
#endif

/* Activate the sendmail add-on features */
#define WITH_SENDMAIL_QUEUEID_TEMP_DNAME 1
#define WITH_SYNTHESIZED_RECEIVED_HEADER 1

#define D_TEMPPREFIX "/amavis-milter-"
#define D_TEMPLATE "XXXXXXXX"
#define F_TEMPLATE "/email.txt"

#define DEVNULL "/dev/null"

/* #ifndef AMAVIS_USER
 * # define AMAVIS_USER "amavis"
 * #endif
 * #ifndef MILTER_SOCKET_GROUP
 * # define MILTER_SOCKET_GROUP "amavis"
 * #endif
 */

/* Extracted from the code for better configurability
 * These will be set by configure/make eventually */
#ifndef X_HEADER_TAG
# define X_HEADER_TAG "X-Virus-Scanned"
#endif
#ifndef X_HEADER_LINE
# define X_HEADER_LINE "by amavisd-milter (http://www.amavis.org/)"
#endif

#define DBG_NONE    0
#define DBG_FATAL   1
#define DBG_WARN    2
#define DBG_INFO    3
#define DBG_DEBUG   4

typedef struct llstrct {
    char *str;
    struct llstrct *next;
} ll;

struct mlfiPriv {
    char *mlfi_fname;  /* temporary file name */
    FILE *mlfi_fp;     /* file descriptor of the temporary file */
    char *mlfi_helo;
    char *mlfi_client_addr;
    char *mlfi_client_name;
    char *mlfi_queueid;
    char *mlfi_envfrom;
    ll mlfi_envto;
    ll *mlfi_thisenvto;
    int mlfi_numto;
};

static int verbosity = DBG_WARN;
static int AM_DAEMON = 1;

static struct group *miltergroup;
static gid_t amavis_gid;
static struct utsname amavis_uts;
static int enable_x_header = 1;  /* enabled by default */

static void amavis_syslog(const int, const char *, ...);
static char *amavis_mkdtemp(char *, int);
static int group_member(const char *);
static void freeenvto(ll *);
static sfsistat clearpriv(SMFICTX *, sfsistat, int);
static int allocmem(SMFICTX *);
static sfsistat mlfi_connect(SMFICTX *, char *, _SOCK_ADDR *);
static sfsistat mlfi_helo(SMFICTX *, char *);
static sfsistat mlfi_envfrom(SMFICTX *, char **);
static sfsistat mlfi_envto(SMFICTX *, char **);
static sfsistat mlfi_header(SMFICTX *, char *, char *);
static sfsistat mlfi_eoh(SMFICTX *);
static sfsistat mlfi_body(SMFICTX *, u_char *, size_t);
static sfsistat mlfi_eom(SMFICTX *);
static sfsistat mlfi_abort(SMFICTX *);
static sfsistat mlfi_close(SMFICTX *);
static sfsistat mlfi_cleanup(SMFICTX *, sfsistat, mybool);


static void
amavis_syslog(const int level, const char *fmt, ...)
{
    time_t tmpt;
    char *timestamp;
    char buf[512];
    va_list ap;
    int loglevel;

    if (level > verbosity) return;
    switch (level) {  /* map internal log level to syslog priority */
	case DBG_FATAL: loglevel = LOG_ERR;     break;
	case DBG_WARN:  loglevel = LOG_WARNING; break;
	case DBG_INFO:  loglevel = LOG_INFO;    break;
	case DBG_DEBUG: loglevel = LOG_DEBUG;   break;
	default:        loglevel = LOG_INFO;
    }
    if (verbosity > 1 && loglevel == LOG_DEBUG) loglevel = LOG_INFO;
    buf[0] = 0;
    va_start(ap, fmt);

    if (AM_DAEMON == 0) {
	tmpt = time(NULL);
	timestamp = ctime(&tmpt);
	/* A 26 character string according ctime(3c)
	 * we cut off the trailing \n\0 */
	timestamp[24] = 0;

	snprintf(buf,sizeof(buf),"%s %s amavis-milter[%ld]: ",
		 timestamp,
		 (amavis_uts.nodename ? amavis_uts.nodename : "localhost"),
		 (long) getpid());
    }

    vsnprintf(buf+strlen(buf),sizeof(buf)-strlen(buf),fmt,ap);
    va_end(ap);

    if (AM_DAEMON == 0) {
	fprintf(stderr,"%s\n",buf);
    }
    
    /* HG: does it make sense to open and close the log each time? */
    openlog("amavis-milter", LOG_PID|LOG_CONS, LOG_MAIL);
    
    syslog(loglevel,"%s\n",buf);
    closelog();
}

static char *
amavis_mkdtemp(char *s, int use_fixed_name)
{
    char *stt;
    int count = 0;

    if (use_fixed_name) {
	if (!mkdir(s, S_IRWXU|S_IRGRP|S_IXGRP)) return s;  /*succeeded */
	amavis_syslog(DBG_FATAL, "(amavis_mkdtemp) creating directory %s failed: %s",
		      s, strerror(errno));
    }
    /* fall back to inventing temporary directory names */
    strcat(s, D_TEMPLATE);  /* storage has been preallocated */

#ifdef HAVE_MKDTEMP
    stt = mkdtemp(s);
    if (stt == NULL)
	amavis_syslog(DBG_FATAL, "(amavis_mkdtemp) mkdtemp %s failed: %s",
		      s, strerror(errno));
    return stt;
#else
    /* magic number alert */
    while (count++ < 20) {
# ifdef HAVE_MKTEMP
	stt = mktemp(s);
# else
	/* relies on template format */
	stt = strrchr(s, '-') + 1;
	if (stt) {
	    /* more magic number alert */
	    snprintf(stt, strlen(s) - 1 - (stt - s), "%08d", lrand48() / 215);
	    stt = s;
	} else {
	    /* invalid template */
	    amavis_syslog(DBG_FATAL, "(amavis_mkdtemp) mktemp failed %s",s);
	    return NULL;
	}
# endif
	if (stt) {
	    if (!mkdir(s, S_IRWXU|S_IRGRP|S_IXGRP)) {
		return s;
	    } else {
		continue;
	    }
	}
    }
    amavis_syslog(DBG_FATAL, "(amavis_mkdtemp) creating (3) directory %s failed: %s",
		  s, strerror(errno));
    return NULL;
#endif /* HAVE_MKDTEMP */
}

static
int group_member(const char *group)
{
    int i, r, rc = -1;
    gid_t *grouplist = 0;

    if (!(miltergroup = getgrnam(group))) {
	perror("getgrnam");
	return rc;
    }

    if ((r = getgroups(0, grouplist)) < 0) {
	perror("getgroups");
    } else if ((grouplist = malloc (r*sizeof(gid_t))) == NULL) {
	perror("malloc");
	r = 0;
    } else if ((r = getgroups(r, grouplist)) < 0) {
	perror("getgroups");
	free(grouplist);
    }

    for (i=0;i<r;i++) {
	if (miltergroup->gr_gid == grouplist[i]) {
	    rc = 0;
	    break;
	}
    }

    if (grouplist)
	free(grouplist);

    return rc;
}

#define MLFIPRIV	((struct mlfiPriv *) smfi_getpriv(ctx))

static void
freeenvto(ll * envto)
{
    while (envto) {
	ll *new = envto->next;
	if (envto->str) {
	    free(envto->str);
	    envto->str = NULL;
	}
	free(envto);
	envto = new;
    }
}

static sfsistat
clearpriv(SMFICTX *ctx, sfsistat retme, int clearall)
{
    /* clear or release private memory and return retme */
    struct mlfiPriv *priv = MLFIPRIV;

    if (priv) {
	if (priv->mlfi_fp) {
	    if (fclose(priv->mlfi_fp) != 0)
		amavis_syslog(DBG_FATAL, "(clearpriv) close failed: %s",
			      strerror(errno));
	    priv->mlfi_fp = NULL;
	}
	if (priv->mlfi_fname)
	    { free(priv->mlfi_fname); priv->mlfi_fname = NULL; }
	if (priv->mlfi_queueid)
	    { free(priv->mlfi_queueid); priv->mlfi_queueid = NULL; }
	if (priv->mlfi_envfrom)
	    { free(priv->mlfi_envfrom); priv->mlfi_envfrom = NULL; }
	if (priv->mlfi_envto.next)
	    { freeenvto(priv->mlfi_envto.next); priv->mlfi_envto.next = NULL; }
	if (priv->mlfi_envto.str)
	    { free(priv->mlfi_envto.str); priv->mlfi_envto.str = NULL; }
	priv->mlfi_thisenvto = NULL;
	priv->mlfi_numto = 0;
	if (clearall) {
	    if (priv->mlfi_client_addr)
	       { free(priv->mlfi_client_addr); priv->mlfi_client_addr = NULL; }
	    if (priv->mlfi_client_name)
	       { free(priv->mlfi_client_name); priv->mlfi_client_name = NULL; }
	    if (priv->mlfi_helo)
	       { free(priv->mlfi_helo); priv->mlfi_helo = NULL; }
	    free(priv); priv = NULL;
	    if (smfi_setpriv(ctx, priv) != MI_SUCCESS) {
		/* Not sure what we need to do here */
		amavis_syslog(DBG_WARN, "(clearpriv) smfi_setpriv failed");
	    }
        }
    }
    return retme;
}

/*
 * allocate some private memory if not already allocated
 * returns 0 if ok, 1 if not
 */
static int
allocmem(SMFICTX * ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;

    if (priv != NULL) {
	/* amavis_syslog(DBG_DEBUG, "allocmem not needed"); */
    } else {
	amavis_syslog(DBG_DEBUG, "(allocmem) allocating private variables");
	priv = malloc(sizeof *priv);
	if (priv == NULL) {
	    /* can't accept this message right now */
	    amavis_syslog(DBG_FATAL, "failed to malloc %d bytes for private store: %s",
		    sizeof(*priv), strerror(errno));
	    return 1;
	}
	memset(priv, 0, sizeof *priv);
	amavis_syslog(DBG_DEBUG, "malloced priv successfully");
	if (smfi_setpriv(ctx, priv) != MI_SUCCESS) {
	    /* Not sure what we need to do here */
	    amavis_syslog(DBG_WARN, "(allocmem) smfi_setpriv failed");
	}
    }
    return 0;
}

static sfsistat
mlfi_connect(SMFICTX * ctx, char *hostname, _SOCK_ADDR * gen_hostaddr)
{
    struct mlfiPriv *priv;
    /* discard any possible data from previous session */
    amavis_syslog(DBG_INFO, "(mlfi_connect) client connect: hostname %s; clearing all variables", hostname);
    clearpriv(ctx, SMFIS_CONTINUE, 1);  /* discard data if any, just in case */
    if (allocmem(ctx)) return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;

    if (priv->mlfi_client_addr)
	{ free(priv->mlfi_client_addr); priv->mlfi_client_addr = NULL; }
    if (priv->mlfi_client_name)
	{ free(priv->mlfi_client_name); priv->mlfi_client_name = NULL; }
    if (gen_hostaddr) {
	char *s = inet_ntoa( ((struct sockaddr_in *)gen_hostaddr)->sin_addr );
	if (s && *s) {
	    if ((priv->mlfi_client_addr = strdup(s)) == NULL)
		return (SMFIS_TEMPFAIL);
	}
    }
    if (hostname) {
	if ((priv->mlfi_client_name = strdup(hostname)) == NULL)
	    return (SMFIS_TEMPFAIL);
    }
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_helo(SMFICTX * ctx, char *helohost)
{
    struct mlfiPriv *priv;
    amavis_syslog(DBG_INFO, "(mlfi_helo) HELO argument is %s", helohost);
    if (allocmem(ctx)) return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;
    if (priv->mlfi_helo) { free(priv->mlfi_helo); priv->mlfi_helo = NULL; }
    if ((priv->mlfi_helo = strdup(helohost)) == NULL) return (SMFIS_TEMPFAIL);
    return SMFIS_CONTINUE;
}

/* write synthesized received header to temp file as the first header */
void write_received(SMFICTX *ctx)
{
#ifdef WITH_SYNTHESIZED_RECEIVED_HEADER
    char date_str[64];
    struct mlfiPriv *priv = MLFIPRIV;
    /* sendmail macros present by default */
    const char *quid      = smfi_getsymval(ctx, "i"); /* sendmail queue id */
    const char *hostname  = smfi_getsymval(ctx, "j"); /* sendmail's host */
    /* optional sendmail milter macros */
    const char *date      = smfi_getsymval(ctx, "b"); /* time of transaction */
    if (!date) {              /* fallback if milter macro {b} is not defined */
	time_t t; time(&t);
	date = date_str;
	if (!strftime(date_str, sizeof(date_str), "%a, %e %b %Y %H:%M:%S %z",
		      localtime(&t))) { date = NULL; }
    }
    if (fprintf(priv->mlfi_fp,
	  "Received: from %s (%s [%s])\n\tby %s (amavis-milter) id %s; %s\n",
	  priv->mlfi_helo && *(priv->mlfi_helo) ? priv->mlfi_helo : "unknown",
	  priv->mlfi_client_name ? priv->mlfi_client_name : "",
	  priv->mlfi_client_addr ? priv->mlfi_client_addr : "",
	  hostname ? hostname : "(milter macro {j} not defined)",
	  quid     ? quid     : "(milter macro {i} not defined)",
	  date     ? date     : "(milter macro {b} not defined)"
		) < 0
	) amavis_syslog(DBG_FATAL,"(write_received) write of header failed: %s",
			strerror(errno));
#endif
}

static sfsistat
mlfi_envfrom(SMFICTX * ctx, char **envfrom)
{
    struct mlfiPriv *priv;
    struct stat StatBuf;
    char *messagepath;
    const char *sendmail_queueid = NULL;
    int use_fixed_name;

    /* discard any message data from previous SMTP transaction */
    amavis_syslog(DBG_DEBUG, "(mlfi_envfrom) clearing message variables");
    clearpriv(ctx, SMFIS_CONTINUE, 0);  /* discard previos msg data if any */
    if (allocmem(ctx)) return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;

    sendmail_queueid = smfi_getsymval(ctx, "i");
    if (!sendmail_queueid) sendmail_queueid = "";
    priv->mlfi_queueid = strdup(sendmail_queueid);
    if (!priv->mlfi_queueid) {
	amavis_syslog(DBG_FATAL,"%s: (mlfi_envfrom) failed to alloc mlfi_queueid", sendmail_queueid);
	return SMFIS_TEMPFAIL;
    }
    priv->mlfi_envfrom = strdup(*envfrom);
    if (!priv->mlfi_envfrom) {
	amavis_syslog(DBG_FATAL,"%s: (mlfi_envfrom) failed to alloc mlfi_envfrom", sendmail_queueid);
	return SMFIS_TEMPFAIL;
    }

    /* tmp dir */
    messagepath = malloc(strlen(RUNTIME_DIR) +
        strlen(D_TEMPPREFIX) + strlen(D_TEMPLATE) +  /*reserve for worst case*/
	(!sendmail_queueid ? 0 : strlen(sendmail_queueid)) +
	strlen(F_TEMPLATE) + 1);
    if (messagepath == NULL) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_envfrom) failed to allocate memory for temp file name: %s",
				 sendmail_queueid, strerror(errno));
	return SMFIS_TEMPFAIL;
    }

    strcpy(messagepath, RUNTIME_DIR);
    strcat(messagepath, D_TEMPPREFIX);
    use_fixed_name = 0;
#ifdef WITH_SENDMAIL_QUEUEID_TEMP_DNAME
    if (sendmail_queueid && *sendmail_queueid) {
        strcat(messagepath, sendmail_queueid); use_fixed_name = 1;
    }
#endif
    if (amavis_mkdtemp(messagepath,use_fixed_name) == NULL) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_envfrom) failed to create temp dir %s: %s", messagepath,
				 sendmail_queueid, strerror(errno));
	return SMFIS_TEMPFAIL;
    }
/*  if (chown(messagepath, (uid_t)-1, amavis_gid) < 0) {
 *	amavis_syslog(DBG_FATAL, "Failed to adjust %s group ownership (%d): %s",
 *		      messagepath, amavis_gid, strerror(errno));
 *	return SMFIS_TEMPFAIL;
 *  }
 */
    if (lstat(messagepath, &StatBuf) < 0) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_envfrom) lstat(%s) failed: %s",
			sendmail_queueid, messagepath, strerror(errno));
	return SMFIS_TEMPFAIL;
    }
    /* may be too restrictive for you, but is good to avoid problems */
    if (!S_ISDIR(StatBuf.st_mode) ||
	StatBuf.st_uid != geteuid() || StatBuf.st_gid != getegid() ) {
	amavis_syslog(DBG_FATAL,
		"%s, Security Warning: %s must be a directory, owned by User %d "
		"and Group %d", messagepath, sendmail_queueid, geteuid(), getegid());
    } else if ( ((StatBuf.st_mode & 0777) != (S_IRWXU|S_IRGRP|S_IXGRP)) ) {
	amavis_syslog(DBG_FATAL,
		"%s, Security Warning: %s %o07 must be readable/writeable by the "
		"User %d and readable by Group %d only",
		sendmail_queueid, messagepath, StatBuf.st_mode, geteuid(), getegid());
    }
    /* there is still a race condition here if RUNTIME_DIR is writeable by the attacker :-\ */

    /* tmp file name */
    strcat(messagepath, F_TEMPLATE);
    amavis_syslog(DBG_INFO, "%s: (mlfi_envfrom) MAIL FROM: %s, tempdir: %s",
			    sendmail_queueid, *envfrom, messagepath);
    priv->mlfi_fname = messagepath; messagepath = NULL;

    if ((priv->mlfi_fp = fopen(priv->mlfi_fname, "w+")) == NULL) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_envfrom) creating file %s failed: %s",
		      sendmail_queueid, priv->mlfi_fname, strerror(errno));
	return SMFIS_TEMPFAIL;
    } else if (fchmod(fileno(priv->mlfi_fp), S_IRUSR|S_IWUSR|S_IRGRP) == -1) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_envfrom) fchmod on %s failed: %s",
		      sendmail_queueid, priv->mlfi_fname, strerror(errno));
	return SMFIS_TEMPFAIL;
    }

    /* prepend synthesized header to the temporary file */
    write_received(ctx);

    /* continue processing */
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_envto(SMFICTX * ctx, char **envto)
{
    struct mlfiPriv *priv;
    const char *sendmail_queueid;
    if (allocmem(ctx)) return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;
    sendmail_queueid = !priv->mlfi_queueid ? "" : priv->mlfi_queueid;
    if (!(priv->mlfi_thisenvto)) {
	/* first one... */
	priv->mlfi_thisenvto = &(priv->mlfi_envto);
	priv->mlfi_numto = 1;
    } else {
	if ((priv->mlfi_thisenvto->next = malloc(sizeof(ll))) == NULL)
	    return (SMFIS_TEMPFAIL);
	priv->mlfi_thisenvto = priv->mlfi_thisenvto->next;
	priv->mlfi_numto++;
    }
    priv->mlfi_thisenvto->next = NULL;
    priv->mlfi_thisenvto->str = NULL;
    if ((priv->mlfi_thisenvto->str = strdup(*envto)) == NULL)
	return (SMFIS_TEMPFAIL);
    amavis_syslog(DBG_INFO, "%s: (mlfi_envto) RCPT TO: %s",
		  (!priv->mlfi_queueid ? "" : priv->mlfi_queueid), *envto);
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_header(SMFICTX *ctx, char *headerf, char *headerv)
{
    struct mlfiPriv *priv = MLFIPRIV;

    /* write the header to the temporary file */
    if (fprintf(priv->mlfi_fp, "%s: %s\n", headerf, headerv) < 0)
	amavis_syslog(DBG_FATAL, "%s: (mlfi_header) write of header failed: %s",
	    (!priv->mlfi_queueid ? "" : priv->mlfi_queueid), strerror(errno));

    /* continue processing */
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_eoh(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    const char *sendmail_queueid;

    sendmail_queueid = !priv->mlfi_queueid ? "" : priv->mlfi_queueid;
    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eoh)", sendmail_queueid);
    /* output the blank line between the header and the body */
    if (fprintf(priv->mlfi_fp, "\n") < 0)
	amavis_syslog(DBG_FATAL, "%s: (mlfi_eoh) writing an empty line failed: %s",
				 sendmail_queueid, strerror(errno));
    /* continue processing */
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_body(SMFICTX *ctx, u_char *bodyp, size_t bodylen)
{
    struct mlfiPriv *priv = MLFIPRIV;
    /* output body block to log file */
    u_char *d = bodyp, *s = bodyp;
    u_char *lastc = bodyp + bodylen - 1;

    /* convert crlf to lf */
    while (s <= lastc) {
	if (s != lastc && *s == 13 && *(s+1) == 10)
	    s++;

	*d++ = *s++;
    }
    bodylen = (size_t)(d - bodyp);

    if (bodylen && fwrite(bodyp, bodylen, 1, priv->mlfi_fp) <= 0) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_body) write of %d bytes failed: %s",
		      (!priv->mlfi_queueid ? "" : priv->mlfi_queueid),
		      bodylen, strerror(errno));
	(void) fclose(priv->mlfi_fp); priv->mlfi_fp = NULL;
	return mlfi_cleanup(ctx, SMFIS_TEMPFAIL, 0);  /* write failed */
    }

    /* continue processing */
    return SMFIS_CONTINUE;
}

/* Simple "protocol" */
const char _EOT = '\3';

static sfsistat
mlfi_eom(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    char buff[7];
    int sock, r;
    char *sender;
    char retval;
    struct sockaddr_un saddr;
    sfsistat rstat = SMFIS_CONTINUE;
    const char *sendmail_queueid;

    if (!priv) {	/* no priv object */
	amavis_syslog(DBG_FATAL, "(mlfi_eom) no private object");
	rstat = SMFIS_TEMPFAIL;
	return rstat;
    }
    sendmail_queueid = !priv->mlfi_queueid ? "" : priv->mlfi_queueid;
    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom)", sendmail_queueid);
    /* close the file so we can run checks on it */
    if (priv->mlfi_fp) {
	if (fclose(priv->mlfi_fp) != 0)
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) close failed: %s",
			  sendmail_queueid, strerror(errno));
	priv->mlfi_fp = NULL;
    }
    /* AFAIK, AF_UNIX is obsolete. POSIX defines AF_LOCAL */
    saddr.sun_family = AF_UNIX;
    if (strlen(AMAVISD_SOCKET)+1 > sizeof(saddr.sun_path)) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) socket path too long: %d",
				 sendmail_queueid, strlen(AMAVISD_SOCKET));
	exit(EX_TEMPFAIL);
    }
    strcpy(saddr.sun_path, AMAVISD_SOCKET);
    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) allocate socket()", sendmail_queueid);
    r = (sock = socket(PF_UNIX, SOCK_STREAM, 0));
    if (r < 0) {
	amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to allocate socket: %s",
				 sendmail_queueid, strerror(errno));
    }
    if (r >= 0) {
	amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) connect", sendmail_queueid);
	r = connect(sock, (struct sockaddr *) (&saddr), sizeof(saddr));
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to connect(): %s",
				     sendmail_queueid, strerror(errno));
    }
    if (r >= 0) {
	char *p = strrchr(priv->mlfi_fname, '/');
	amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) sendfile", sendmail_queueid);
	/* amavisd wants the directory, not the filename */
	*p = '\0';
	r = send(sock, priv->mlfi_fname, strlen(priv->mlfi_fname), 0);
	*p = '/';
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to send() file name: %s",
				     sendmail_queueid, strerror(errno));
    }
    if (r >= 0) {
	r = recv(sock, &retval, 1, 0);
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to recv() file name confirmation: %s",
				     sendmail_queueid, strerror(errno));
    }
    if (r >= 0) {
	size_t sender_l;
	sender = (strlen(priv->mlfi_envfrom) > 0) ? priv->mlfi_envfrom : "<>";
	amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) sendfrom() %s", sendmail_queueid, sender);
	sender_l = strlen(sender);
	if (sender_l > SOCKBUFLEN) {
	    amavis_syslog(DBG_WARN, "%s: (mlfi_eom) Sender too long (%d), truncated to %d characters",
				    sendmail_queueid, sender_l, SOCKBUFLEN);
	    sender_l = SOCKBUFLEN;
	}
	r = send(sock, sender, sender_l, 0);
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to send() Sender: %s",
				    sendmail_queueid, strerror(errno));
	else if (r < sender_l)
	    amavis_syslog(DBG_WARN, "%s: (mlfi_eom) failed to send() complete Sender, truncated to %d characters",
				    sendmail_queueid, r);
    }
    if (r >= 0) {
	r = recv(sock, &retval, 1, 0);
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to recv() ok for Sender info: %s",
				     sendmail_queueid, strerror(errno));
    }
    if (r >= 0) {
	int x;
	priv->mlfi_thisenvto = &(priv->mlfi_envto);
	for (x = 0; (r >= 0) && (x < priv->mlfi_numto); x++) {
	    size_t recipient_l;
	    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) sendto() %s",
				  sendmail_queueid, priv->mlfi_thisenvto->str);
	    recipient_l = strlen(priv->mlfi_thisenvto->str);
	    if (recipient_l > SOCKBUFLEN) {
		amavis_syslog(DBG_WARN, "%s: (mlfi_eom) Recipient too long (%d), truncated to %d characters",
				    sendmail_queueid, recipient_l, SOCKBUFLEN);
		recipient_l = SOCKBUFLEN;
	    }
	    r = send(sock, priv->mlfi_thisenvto->str, recipient_l, 0);
	    if (r < 0)
		amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to send() Recipient: %s",
					    sendmail_queueid, strerror(errno));
	    else {
		if (r < recipient_l)
		    amavis_syslog(DBG_WARN, "%s: (mlfi_eom) failed to send() complete Recipient, truncated to %d characters ",
					    sendmail_queueid, r);
		r = recv(sock, &retval, 1, 0);
		if (r < 0)
		    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to recv() ok for recip info: %s",
					    sendmail_queueid, strerror(errno));
		priv->mlfi_thisenvto = priv->mlfi_thisenvto->next;
	    }
	}
    }
    if (r >= 0) {
	amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) send() EOT", sendmail_queueid);
	r = send(sock, &_EOT, 1, 0);
	/* send "end of args" msg */
	if (r < 0) {
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) failed to send() EOT: %s",
				     sendmail_queueid, strerror(errno));
	} else {
	    /* get result from amavisd */
	    memset(buff, 0, sizeof *buff);
	    r = recv(sock, buff, 6, 0);
	    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) received %s from daemon", sendmail_queueid, buff);
	    if (r < 0)
		amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) Failed to recv() final result: %s",
					sendmail_queueid, strerror(errno));
	    else if (!r)
		amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) Failed to recv() final result: empty status string",
					 sendmail_queueid);
	    /* get back final result */
	}
    }
    close(sock);

    if (r < 0) {
	/* some point of the communication failed miserably - so give up */
	amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) communication failure", sendmail_queueid);
	return mlfi_cleanup(ctx, SMFIS_TEMPFAIL, 0);
    }
    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) finished conversation", sendmail_queueid);

    /* Protect against empty return string */
    if (*buff)
	retval = atoi(buff);
    else
	retval = 1;

    if (retval == 99) {
	amavis_syslog(DBG_INFO, "%s: (mlfi_eom) discarding mail, retval is %d",
				sendmail_queueid, retval);
	rstat = SMFIS_DISCARD;
    } else if (retval == EX_UNAVAILABLE) {  /* REJECT handling */
				    /* by Didi Rieder and Mark Martinec */
	amavis_syslog(DBG_INFO, "%s: (mlfi_eom) rejecting mail, retval is %d",
				sendmail_queueid, retval);
	if (smfi_setreply(ctx, "550", "5.7.1", "Message content rejected") != MI_SUCCESS) {
	    /* Not sure what we need to do here */
	    amavis_syslog(DBG_FATAL, "%s: (mlfi_eom) smfi_setreply failed",
				     sendmail_queueid);
	}
	rstat = SMFIS_REJECT;
    } else if (retval == 0) {
	if (enable_x_header) {
	    amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) adding/changing header", sendmail_queueid);
	    if (smfi_chgheader(ctx, X_HEADER_TAG, 1, X_HEADER_LINE) == MI_FAILURE) {
		amavis_syslog(DBG_DEBUG, "%s: (mlfi_eom) adding header", sendmail_queueid);
		if (smfi_addheader(ctx, X_HEADER_TAG, X_HEADER_LINE) != MI_SUCCESS) {
		    amavis_syslog(DBG_FATAL,
			"%s: (mlfi_eom) smfi_addheader failed, perhaps milter session timed out",
			sendmail_queueid);
		}
	    }
	}
	amavis_syslog(DBG_INFO, "%s: (mlfi_eom) CONTINUE delivery", sendmail_queueid);
	rstat = SMFIS_CONTINUE;
    } else {
	/* if we got any unexpected exit status, we didn't check the file...
	 * so don't add the header. We return TEMPFAIL instead */
	amavis_syslog(DBG_WARN, "%s: (mlfi_eom) TEMPFAIL, retval is %d",
				sendmail_queueid, retval);
	rstat = SMFIS_TEMPFAIL;
    }
 /* return mlfi_cleanup(ctx, rstat, 0); */    /* _we_ must delete dir & file */
    return mlfi_cleanup(ctx, rstat, 1); /* server will delete the dir & file */
}

static sfsistat
mlfi_abort(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    amavis_syslog(DBG_WARN, "%s: (mlfi_abort)",
		(!priv || !priv->mlfi_queueid ? "?" : priv->mlfi_queueid) );
    return mlfi_cleanup(ctx, SMFIS_CONTINUE, 0);
}

static sfsistat
mlfi_close(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    amavis_syslog(DBG_DEBUG, "(mlfi_close) %sclearing all variables",
		(!priv || !priv->mlfi_queueid ? "" : priv->mlfi_queueid) );
    return clearpriv(ctx, SMFIS_CONTINUE, 1);  /* discard all data */
}

static sfsistat
mlfi_cleanup(SMFICTX *ctx, sfsistat rstat, mybool keep)
{
    struct mlfiPriv *priv = MLFIPRIV;
    const char *sendmail_queueid;

    if (!priv)
	return rstat;
    sendmail_queueid = !priv->mlfi_queueid ? "" : priv->mlfi_queueid;

    if (keep) {
        /* don't delete the file */
    } else {
	/* message was aborted -- delete the archive file */
	if (priv->mlfi_fp) {
	    if (fclose(priv->mlfi_fp) != 0)
		amavis_syslog(DBG_FATAL, "%s: (mlfi_cleanup) close failed: %s",
			      sendmail_queueid, strerror(errno));
	    priv->mlfi_fp = NULL;
	}
	if (priv->mlfi_fname) {
	    char *p;
	    amavis_syslog(DBG_DEBUG, "%s: (mlfi_cleanup) deleting temp file",
				     sendmail_queueid);
	    if (unlink(priv->mlfi_fname) < 0)
		amavis_syslog(DBG_FATAL, "%s: (mlfi_cleanup) unlinking %s failed: %s",
			sendmail_queueid, priv->mlfi_fname, strerror(errno));
	    p = strrchr(priv->mlfi_fname, '/');
	    if (!p) {
		amavis_syslog(DBG_FATAL, "%s: (mlfi_cleanup) no '/' in %s",
					 sendmail_queueid, priv->mlfi_fname);
	    } else {
		*p = '\0';
		if (rmdir(priv->mlfi_fname) < 0)
		    amavis_syslog(DBG_FATAL, "%s: (mlfi_cleanup) rmdir of %s failed: %s",
			sendmail_queueid, priv->mlfi_fname, strerror(errno));
		*p = '/';
	    }
	}
    }

    /* clear message data, return status */
    amavis_syslog(DBG_DEBUG, "%s: (mlfi_cleanup) clearing message variables",
			     sendmail_queueid);
    return clearpriv(ctx, rstat, 0);  /* discard message data if any */
}


struct smfiDesc smfilter = {
    "amavis-milter",		/* filter name */
    SMFI_VERSION,		/* version code -- do not change */
    SMFIF_ADDHDRS|SMFIF_CHGHDRS,/* flags */
    mlfi_connect,		/* connection info filter */
    mlfi_helo,			/* SMTP HELO command filter */
    mlfi_envfrom,		/* envelope sender filter */
    mlfi_envto,			/* envelope recipient filter */
    mlfi_header,		/* header filter */
    mlfi_eoh,			/* end of header */
    mlfi_body,			/* body block filter */
    mlfi_eom,			/* end of message */
    mlfi_abort,			/* message aborted */
    mlfi_close			/* connection cleanup */
};


void
usage(void)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  amavis-milter -p local:<unix-socket> [-d] [-v]\n");
    fprintf(stderr, "  amavis-milter -p inet:port@0.0.0.0 [-d] [-v]\n");
    fprintf(stderr, "  amavis-milter -h\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "-p specifies a milter socket on which amavis-milter\n");
    fprintf(stderr, "   will listen for connections from sendmail.\n");
    fprintf(stderr, "   The argument is passed directly to libmilter, see sendmail milter\n");
    fprintf(stderr, "   documentation for details. The socket specified must match the\n");
    fprintf(stderr, "   INPUT_MAIL_FILTER macro call in the sendmail configuration file.\n");
    fprintf(stderr, "-d debug: disables daemonisation and turns log level fully up (-vvvv) \n");
    fprintf(stderr, "-v increases logging level by one, may be specified up to 4 times\n");
    fprintf(stderr, "-h help: displays this usage text and exits\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options -g, -x, -D are allowed for compatibility but ignored.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This helper prgram (milter daemon) is normally started as:\n");
    fprintf(stderr, "# su amavis -c '/usr/local/sbin/amavis-milter -p local:/var/amavis/amavis-milter.sock'\n");
};

int
main(int argc, char *argv[])
{
/*  struct passwd *userinfo;	*amavis uid*  */
    int c, i;
    char *p, *milter_socket = NULL, *milter_socket_group = NULL;
/*  const char *args = "dg:p:vx";  */
    const char *args = ":hdg:p:Dvx";  /* some mix of old and new options!!! */

    pid_t pid;
    int devnull;

#if !defined(HAVE_MKDTEMP) && !defined(HAVE_MKTEMP)
    int mypid = getpid();

    srand48(time(NULL) ^ (mypid + (mypid << 15)));
#endif

    umask(0007);

    /* Process command line options */
    while ((c = getopt(argc, argv, args)) != -1) {
	switch (c) {
	case 'd':
	    /* don't daemonise, log to stderr */
	    verbosity = DBG_DEBUG;  /* full debugging log level */
	    AM_DAEMON = 0;
	    break;
	case 'g':
	    /* name of milter socket group owner */
	    if (optarg == NULL || *optarg == '\0') {
		fprintf(stderr, "%s: Illegal group: %s\n", argv[0], optarg);
	    }
	    fprintf(stderr, "%s: group specification ignored (not implemented)\n", argv[0]);
	    milter_socket_group = strdup(optarg);
	    break;
	case 'p':
	    /* socket name - see smfi_setconn man page */
	    if (optarg == NULL || *optarg == '\0') {
		fprintf(stderr, "%s: Illegal conn: %s\n", argv[0], optarg);
		exit(EXIT_FAILURE);
	    }
	    milter_socket = strdup(optarg);
	    break;
	case 'v':
	    verbosity++;
	    break;
	case 'D':
	    AM_DAEMON = 1;   /* which is also a default, unless debugging */
	    break;
	case 'x':
	 /* enable_x_header++; */    /* older versions */
	 /* enable_x_header = 0;*/   /* since 1.1.2.3.2.40 */
	    fprintf(stderr, "%s: option -x ignored to avoid confusion with older versions\n", argv[0]);
	    break;
	case 'h':
	    usage();
	    exit(EXIT_SUCCESS);
	    break;
	default:
	    usage();
	    exit(EXIT_FAILURE);
	}
    }

    if (smfi_register(smfilter) == MI_FAILURE) {
	fprintf(stderr, "%s: smfi_register failed\n", argv[0]);
	exit(EXIT_FAILURE);
    }

    uname(&amavis_uts);

    /* check user and group */
/*  if (!(userinfo = getpwnam(AMAVIS_USER))) {
 *	perror("getpwnam");
 *	exit(EXIT_FAILURE);
 *  }
 *  amavis_gid = userinfo->pw_gid;
 *  if (!milter_socket_group) {
 *	milter_socket_group = strdup(MILTER_SOCKET_GROUP);
 *	if (!milter_socket_group) {
 *	    perror("strdup");
 *	    exit(EXIT_FAILURE);
 *	}
 *  }
 *  if (group_member(milter_socket_group) < 0) {
 *	fprintf(stderr, "%s not member of %s group\n", AMAVIS_USER, milter_socket_group);
 *	exit(EXIT_FAILURE);
 *  }
 */
    if (!milter_socket) {
	fprintf(stderr, "%s: no milter socket specified (missing option -p)\n\n", argv[0]);
	usage();
	exit(EXIT_FAILURE);
    }

    /* check socket */
    if ((p = strchr(milter_socket,'/'))) {
	/* Unlink any existing file that might be in place of
	 * the socket we want to create.  This might not exactly
	 * be safe, or friendly, but I'll deal with that later.
	 * Be nice and issue a warning if we have a problem, but
	 * other than that, ignore it. */
	if (unlink(p) < 0) {
	    amavis_syslog(DBG_INFO, "INFO: Cannot unlink old socket %s: %s", milter_socket, strerror(errno));
	}
    }

    /* Errors are detected in smfi_main */
    if (smfi_setconn(milter_socket) != MI_SUCCESS) {
	amavis_syslog(DBG_FATAL, "(main) smfi_setconn failed");
    }

    /* See if we're supposed to become a daemonized process */
    if (AM_DAEMON == 1) {

	/* 2001/11/09 Anne Bennett: daemonize properly.
	 * OK, let's be a real daemon.  Taken from page 417
	 * of Stevens' "Advanced Programming in the UNIX Environment".
	 */

	/* Step 1: Fork and have parent exit.  This not only
	 * backgrounds us but makes sure we are not a process group
	 * leader.
	 */

	/* Fork ourselves into the background, and see if it worked */
	if ((pid = fork()) > 0) {

	    amavis_syslog(DBG_INFO, "amavis-milter forked into background");
	    /* We are the parent; exit. */
	    exit(EXIT_SUCCESS);

	} else if (pid == -1) {
	    perror("fork");
	    exit(EXIT_FAILURE);
	}

	/* OK, we're backgrounded.
	 * Step 2: Call setsid to create a new session.  This makes
	 * sure among other things that we have no controlling
	 * terminal.
	 */
	if (setsid() < (pid_t)0) {
	    amavis_syslog(DBG_FATAL, "setsid() returned error: %s", strerror(errno));
	    exit(EXIT_FAILURE);
	}

	/* Step 3: Set the working directory appropriately. */
	if (chdir("/") < 0 ) {
	    amavis_syslog(DBG_FATAL, "chdir(/) returned error: %s", strerror(errno));
	    exit(EXIT_FAILURE);
	}

	/* Step 4: Close all file descriptors. */
	for (i = 0; i < _POSIX_OPEN_MAX ; i++) {
	    close(i);
	}

	/* Open /dev/null read-only (fd 0 = STDIN) */
	if ((devnull = open(DEVNULL, O_RDONLY, 0)) < 0) {
	     amavis_syslog(DBG_FATAL, "Could not open %s as STDIN: %s", DEVNULL, strerror(errno));
	     exit(EXIT_FAILURE);
	}
	if (devnull != 0) {
	     amavis_syslog(DBG_FATAL, "Got wrong file descriptor as STDIN: %s != 0", DEVNULL);
	     exit(EXIT_FAILURE);
	}

	/* Open /dev/null write-only (fd 1 = STDOUT) */
	if ((devnull = open(DEVNULL, O_WRONLY, 0)) < 0) {
	     amavis_syslog(DBG_FATAL, "Could not open %s as STDOUT: %s", DEVNULL, strerror(errno));
	     exit(EXIT_FAILURE);
	}
	if (devnull != 1) {
	     amavis_syslog(DBG_FATAL, "Got wrong file descriptor as STDOUT: %s != 1", DEVNULL);
	     exit(EXIT_FAILURE);
	}

	/* Open /dev/null write-only (fd 2 = STDERR) */
	if ((devnull = open(DEVNULL, O_WRONLY, 0)) < 0) {
	     amavis_syslog(DBG_FATAL, "Could not open %s as STDERR: %s", DEVNULL, strerror(errno));
	     exit(EXIT_FAILURE);
	}
	if (devnull != 2) {
	     amavis_syslog(DBG_FATAL, "Got wrong file descriptor as STDERR: %s != 2", DEVNULL);
	     exit(EXIT_FAILURE);
	}
    }

    /* change process group id */
    if (miltergroup && (setgid(miltergroup->gr_gid)) < 0) {
	amavis_syslog(DBG_FATAL, "setgid(%d): %s", miltergroup->gr_gid, strerror(errno));
	exit(EX_UNAVAILABLE);
    }

    /* smfi_settimeout(1800); */     /* defaults to 7210 seconds */

    /* hand control over to libmilter */
    amavis_syslog(DBG_WARN, "Starting, handing off to smfi_main");
    return smfi_main();
}

/* eof */
