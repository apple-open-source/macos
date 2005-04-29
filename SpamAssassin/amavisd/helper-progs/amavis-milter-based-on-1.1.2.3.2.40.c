/*
 * Based on amavis-milter.c,v 1.1.2.3.2.40 2003/06/06 12:34:58 lhecking Exp
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
 *
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
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include <grp.h>
#include <pwd.h>

#ifdef HAVE_SM_GEN_H
# include "sm/gen.h"
#endif
#include "libmilter/mfapi.h"

#ifndef HAVE_SM_GEN_BOOL_TYPE
typedef int bool;
#endif

#define BUFFLEN 255
/* Must be the same as the buffer length for recv() in amavisd */
#define SOCKBUFLEN 8192

#ifndef RUNTIME_DIR
# define RUNTIME_DIR "/var/amavis"
#endif

#ifndef AMAVISD_SOCKET
# define AMAVISD_SOCKET RUNTIME_DIR ## "/amavisd.sock"
#endif

#define D_TEMPLATE "/amavis-milter-XXXXXXXX"
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
    struct in_addr client_addr;
    char *mlfi_fname;
    FILE *mlfi_fp;
    char *mlfi_envfrom;
    ll mlfi_envto;
    ll *mlfi_thisenvto;
    int mlfi_numto;
};

static int verbosity;
static int AM_DAEMON = 1;

static struct group *miltergroup;
static gid_t amavis_gid;
static struct utsname amavis_uts;
static int enable_x_header = 1;  /* enabled by default */

static void amavis_syslog(const int, const char *, ...);
static char *amavis_mkdtemp(char *);
static int group_member(const char *);
static void freeenvto(ll *);
static sfsistat clearpriv(SMFICTX *, sfsistat, int);
static int allocmem(SMFICTX *);
static sfsistat mlfi_connect(SMFICTX *, char *, _SOCK_ADDR *);
static sfsistat mlfi_envto(SMFICTX *, char **);
static sfsistat mlfi_envfrom(SMFICTX *, char **);
static sfsistat mlfi_header(SMFICTX *, char *, char *);
static sfsistat mlfi_eoh(SMFICTX *);
static sfsistat mlfi_body(SMFICTX *, u_char *, size_t);
static sfsistat mlfi_eom(SMFICTX *);
static sfsistat mlfi_close(SMFICTX *);
static sfsistat mlfi_abort(SMFICTX *);
static sfsistat mlfi_cleanup(SMFICTX *, bool);


static void
amavis_syslog(const int level, const char *fmt, ...)
{
    time_t tmpt;
    char *timestamp;
    char buf[512];
    va_list ap;

    if (level > verbosity)
	return;

    buf[0] = 0;
    va_start(ap, fmt);

    if (AM_DAEMON == 0) {
	tmpt = time(NULL);
	timestamp = ctime(&tmpt);
	/* A 26 character string according ctime(3c)
	 * we cut off the trailing \n\0 */
	timestamp[24] = 0;

	snprintf(buf,sizeof(buf)-1,"%s %s amavis-milter[%ld]: ",
		 timestamp,
		 (amavis_uts.nodename ? amavis_uts.nodename : "localhost"),
		 (long) getpid());
    }

    vsnprintf(buf+strlen(buf),sizeof(buf)-strlen(buf)-1,fmt,ap);
    va_end(ap);

    if (AM_DAEMON == 0) {
	fprintf(stderr,"%s\n",buf);
    }
    openlog("amavis-milter", LOG_PID|LOG_CONS, LOG_MAIL);
    syslog(LOG_NOTICE,"%s\n",buf);
    closelog();
}

static char *
amavis_mkdtemp(char *s)
{
    char *stt;
    int count = 0;

#ifdef HAVE_MKDTEMP
    return mkdtemp(s);
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
	    return NULL;
	}
# endif
	if (stt) {
	    if (!mkdir(s, S_IRWXU)) {
		return s;
	    } else {
		continue;
	    }
	}
    }
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
    ll *new;

    while (envto) {
	new = envto->next;
	if (envto->str) {
	    free(envto->str);
	    envto->str = NULL;
	}
	if (envto)
	    free(envto);
	envto = new;
    }
}

static sfsistat
clearpriv(SMFICTX *ctx, sfsistat retme, int clearall)
{
    /* release private memory and return retme */
    struct mlfiPriv *priv = MLFIPRIV;

    amavis_syslog(DBG_INFO, "Clearing priv (clearall=%d)", clearall);

    if (priv) {
	if (priv->mlfi_fname) {
	    amavis_syslog(DBG_INFO, "clearing fname");
	    free(priv->mlfi_fname);
	    priv->mlfi_fname = NULL;
	}
	if (priv->mlfi_envfrom) {
	    amavis_syslog(DBG_INFO, "clearing envfrom");
	    free(priv->mlfi_envfrom);
	    priv->mlfi_envfrom = NULL;
	}
	if (priv->mlfi_envto.next) {
	    amavis_syslog(DBG_INFO, "clearing multi-envto");
	    freeenvto(priv->mlfi_envto.next);
	    priv->mlfi_envto.next = NULL;
	}
	if (priv->mlfi_envto.str) {
	    amavis_syslog(DBG_INFO, "clearing envto");
	    free(priv->mlfi_envto.str);
	    priv->mlfi_envto.str = NULL;
	}

	amavis_syslog(DBG_INFO, "clearing priv");
	free(priv);
	priv = NULL;
	if (smfi_setpriv(ctx, priv) != MI_SUCCESS) {
	    /* Not sure what we need to do here */
	    amavis_syslog(DBG_WARN, "(clearpriv)smfi_setpriv failed");
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

    if (priv == NULL) {
	amavis_syslog(DBG_INFO, "(allocmem)priv was null");
	priv = malloc(sizeof *priv);
	if (priv == NULL) {
	    /* can't accept this message right now */
	    amavis_syslog(DBG_FATAL, "failed to malloc %d bytes for private store: %s",
		    sizeof(*priv), strerror(errno));
	    return 1;
	}
	amavis_syslog(DBG_INFO, "malloced priv - now using memset()");
	memset(priv, 0, sizeof *priv);
	amavis_syslog(DBG_INFO, "malloced priv successfully");
	if (smfi_setpriv(ctx, priv) != MI_SUCCESS) {
	    /* Not sure what we need to do here */
	    amavis_syslog(DBG_WARN, "(allocmem)smfi_setpriv failed");
	}
    } else {
	amavis_syslog(DBG_WARN, "allocmem tried but priv was already set");
	amavis_syslog(DBG_WARN, "priv->client_addr.s_addr is %d",
		priv->client_addr.s_addr);
    }
    return 0;
}

static sfsistat
mlfi_connect(SMFICTX * ctx, char *hostname, _SOCK_ADDR * gen_hostaddr)
{
    struct mlfiPriv *priv;
    struct sockaddr_in *hostaddr;

    hostaddr = (struct sockaddr_in *) gen_hostaddr;

    if (hostaddr) {
	amavis_syslog(DBG_INFO, "hostname is %s, addr is %d.%d.%d.%d",
		hostname, (hostaddr->sin_addr.s_addr) & 0xff,
		(hostaddr->sin_addr.s_addr >> 8) & 0xff,
		(hostaddr->sin_addr.s_addr >> 16) & 0xff,
		(hostaddr->sin_addr.s_addr >> 24) & 0xff);
    }
    amavis_syslog(DBG_INFO, "checking allocmem");
    if (allocmem(ctx))
	return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;
    if (hostaddr) {
	priv->client_addr.s_addr = hostaddr->sin_addr.s_addr;
    } else {
	priv->client_addr.s_addr = 0;
    }
    /* not really needed but nice */
    if (smfi_setpriv(ctx, priv) != MI_SUCCESS) {
	/* Not sure what we need to do here */
	amavis_syslog(DBG_WARN, "(mlfi_connect)smfi_setpriv failed");
    }
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_envto(SMFICTX * ctx, char **envto)
{
    struct mlfiPriv *priv;

    if (allocmem(ctx))
	return SMFIS_TEMPFAIL;

    priv = MLFIPRIV;
    if (!(priv->mlfi_thisenvto)) {
	/* first one... */
	priv->mlfi_thisenvto = &(priv->mlfi_envto);
	priv->mlfi_numto = 1;
    } else {
	priv->mlfi_numto++;
	if ((priv->mlfi_thisenvto->next = malloc(sizeof(ll))) == NULL)
	    return (SMFIS_TEMPFAIL);
	priv->mlfi_thisenvto = priv->mlfi_thisenvto->next;
	priv->mlfi_thisenvto->next = NULL;
    }
    if ((priv->mlfi_thisenvto->str = strdup(*envto)) == NULL)
	return (SMFIS_TEMPFAIL);
    amavis_syslog(DBG_INFO, "added %s as recip", *envto);
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_envfrom(SMFICTX * ctx, char **envfrom)
{
    struct mlfiPriv *priv;
    struct stat StatBuf;
    char *messagepath;

    if (allocmem(ctx))
	return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;

    /* tmp dir */
    messagepath = malloc(strlen(RUNTIME_DIR) + strlen(D_TEMPLATE) + strlen(F_TEMPLATE) + 1);
    if (messagepath == NULL) {
	amavis_syslog(DBG_FATAL, "Failed to allocate memory for temp file name: %s", strerror(errno));
	return SMFIS_TEMPFAIL;
    }

    strcpy(messagepath, RUNTIME_DIR);
    strcat(messagepath, D_TEMPLATE);
/*  umask(0077); */
    umask(0007);
    if (amavis_mkdtemp(messagepath) == NULL) {
	amavis_syslog(DBG_FATAL, "Failed to create temp dir %s: %s", messagepath, strerror(errno));
	return SMFIS_TEMPFAIL;
    }
/*  if (chown(messagepath, (uid_t)-1, amavis_gid) < 0) {
 *	amavis_syslog(DBG_FATAL, "Failed to adjust %s group ownership (%d): %s",
 *		      messagepath, amavis_gid, strerror(errno));
 *	return SMFIS_TEMPFAIL;
 *  }
 */
    umask (0007);

    if (lstat(messagepath, &StatBuf) < 0) {
	amavis_syslog(DBG_FATAL, "Error while trying lstat(%s): %s", messagepath, strerror(errno));
	return SMFIS_TEMPFAIL;
    }

    /* may be too restrictive for you, but's good to avoid problems */
    if (!S_ISDIR(StatBuf.st_mode) || StatBuf.st_uid != geteuid() || StatBuf.st_gid != getegid() || !(StatBuf.st_mode & S_IRWXU)) {
	amavis_syslog(DBG_FATAL,
		"Security Warning: %s must be a directory owned by "
		"User %d and Group %d, and read-/write-able by the User "
		"only. Exit.", messagepath, geteuid(), getegid());
	return SMFIS_TEMPFAIL;
    }
    /* there is still a race condition here if RUNTIME_DIR is writeable by the attacker :-\ */

    /* tmp file name */
    strcat(messagepath, F_TEMPLATE);
    amavis_syslog(DBG_INFO, "received: %s; from=%s", messagepath, *envfrom);

    priv->mlfi_fname = strdup(messagepath);
    free(messagepath);
    if (priv->mlfi_fname == NULL) {
	return SMFIS_TEMPFAIL;
    }
    priv->mlfi_envfrom = strdup(*envfrom);
    if (!priv->mlfi_envfrom) {
	free(priv->mlfi_fname);
	priv->mlfi_fname = NULL;
	return SMFIS_TEMPFAIL;
    }

    if ((priv->mlfi_fp = fopen(priv->mlfi_fname, "w+")) == NULL) {
	free(priv->mlfi_fname);
	priv->mlfi_fname = NULL;
	free(priv->mlfi_envfrom);
	priv->mlfi_envfrom = NULL;
	return SMFIS_TEMPFAIL;
    }

    /* save the private data */

    if (smfi_setpriv(ctx, priv) != MI_SUCCESS) {
	/* Not sure what we need to do here */
	amavis_syslog(DBG_WARN, "(mlfi_envfrom)smfi_setpriv failed");
    }

    /* continue processing */
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_header(SMFICTX *ctx, char *headerf, char *headerv)
{
    /* write the header to the log file */
    fprintf(MLFIPRIV->mlfi_fp, "%s: %s\n", headerf, headerv);

    /* continue processing */
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_eoh(SMFICTX *ctx)
{
    /* output the blank line between the header and the body */
    fprintf(MLFIPRIV->mlfi_fp, "\n");

    /* continue processing */
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_body(SMFICTX *ctx, u_char *bodyp, size_t bodylen)
{
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

    if (bodylen && fwrite(bodyp, bodylen, 1, MLFIPRIV->mlfi_fp) <= 0) {
	/* write failed */
	(void) mlfi_cleanup(ctx, 0);
	return SMFIS_TEMPFAIL;
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

    if (!priv) {	/* no priv object */
	amavis_syslog(DBG_WARN, "couldn't scan - no priv object");
	return clearpriv(ctx, SMFIS_TEMPFAIL, 0);
    }

    amavis_syslog(DBG_INFO, "mlfi_eom()");
    /* close the file so we can run checks on it!!! */
    if (priv->mlfi_fp)
	fclose(priv->mlfi_fp);

    /* AFAIK, AF_UNIX is obsolete. POSIX defines AF_LOCAL */
    saddr.sun_family = AF_UNIX;
    if (strlen(AMAVISD_SOCKET)+1 > sizeof(saddr.sun_path)) {
	amavis_syslog(DBG_FATAL, "socket path too long: %d", strlen(AMAVISD_SOCKET));
	exit(EX_TEMPFAIL);
    }
    strcpy(saddr.sun_path, AMAVISD_SOCKET);
    amavis_syslog(DBG_INFO, "allocate socket()");
    r = (sock = socket(PF_UNIX, SOCK_STREAM, 0));
    if (r < 0) {
	amavis_syslog(DBG_FATAL, "failed to allocate socket: %s", strerror(errno));
    }
    if (r >= 0) {
	amavis_syslog(DBG_INFO, "mlfi_eom:connect");
	r = connect(sock, (struct sockaddr *) (&saddr), sizeof(saddr));
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "failed to connect(): %s", strerror(errno));
    }
    if (r >= 0) {
	char *p = strrchr(priv->mlfi_fname, '/');
	amavis_syslog(DBG_INFO, "mlfi_eom:sendfile");
	/* amavisd wants the directory, not the filename */
	*p = 0;
	r = send(sock, priv->mlfi_fname, strlen(priv->mlfi_fname), 0);
	*p = '/';
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "failed to send() file name: %s", strerror(errno));
    }
    if (r >= 0) {
	r = recv(sock, &retval, 1, 0);
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "failed to recv() file name confirmation: %s", strerror(errno));
    }
    if (r >= 0) {
	size_t sender_l;
	sender = (strlen(priv->mlfi_envfrom) > 0) ? priv->mlfi_envfrom : "<>";
	amavis_syslog(DBG_INFO, "sendfrom() %s", sender);
	sender_l = strlen(sender);
	if (sender_l > SOCKBUFLEN) {
	    amavis_syslog(DBG_WARN, "Sender too long (%d), truncated to %d characters", sender_l, SOCKBUFLEN);
	    sender_l = SOCKBUFLEN;
	}
	r = send(sock, sender, sender_l, 0);
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "failed to send() Sender: %s", strerror(errno));
	else if (r < sender_l)
	    amavis_syslog(DBG_WARN, "failed to send() complete Sender, truncated to %d characters ", r);
    }
    if (r >= 0) {
	r = recv(sock, &retval, 1, 0);
	if (r < 0)
	    amavis_syslog(DBG_FATAL, "failed to recv() ok for Sender info: %s", strerror(errno));
    }
    if (r >= 0) {
	int x;
	priv->mlfi_thisenvto = &(priv->mlfi_envto);
	for (x = 0; (r >= 0) && (x < priv->mlfi_numto); x++) {
	    size_t recipient_l;
	    amavis_syslog(DBG_INFO, "sendto() %s", priv->mlfi_thisenvto->str);
	    recipient_l = strlen(priv->mlfi_thisenvto->str);
	    if (recipient_l > SOCKBUFLEN) {
		amavis_syslog(DBG_WARN, "Recipient too long (%d), truncated to %d characters", recipient_l,SOCKBUFLEN);
		recipient_l = SOCKBUFLEN;
	    }
	    r = send(sock, priv->mlfi_thisenvto->str, recipient_l, 0);
	    if (r < 0)
		amavis_syslog(DBG_FATAL, "failed to send() Recipient: %s", strerror(errno));
	    else {
		if (r < recipient_l)
		    amavis_syslog(DBG_WARN, "failed to send() complete Recipient, truncated to %d characters ", r);
		r = recv(sock, &retval, 1, 0);
		if (r < 0)
		    amavis_syslog(DBG_FATAL, "failed to recv() ok for recip info: %s", strerror(errno));
		priv->mlfi_thisenvto = priv->mlfi_thisenvto->next;
	    }
	}
    }
    if (r >= 0) {
	amavis_syslog(DBG_INFO, "send() EOT");
	r = send(sock, &_EOT, 1, 0);
	/* send "end of args" msg */
	if (r < 0) {
	    amavis_syslog(DBG_FATAL, "failed to send() EOT: %s", strerror(errno));
	} else {
	    /* get result from amavisd */
	    r = recv(sock, buff, 6, 0);
	    amavis_syslog(DBG_INFO, "received %s from daemon", buff);
	    if (r < 0)
		amavis_syslog(DBG_FATAL, "Failed to recv() final result: %s", strerror(errno));
	    else if (!r)
		amavis_syslog(DBG_FATAL, "Failed to recv() final result: empty status string");
	    /* get back final result */
	}
    }
    close(sock);

    if (r < 0) {
	amavis_syslog(DBG_FATAL, "communication failure");
	return clearpriv(ctx, SMFIS_TEMPFAIL, 0);
	/* some point of the communication failed miserably - so give up */
    }
    amavis_syslog(DBG_INFO, "finished conversation");

    /* Protect against empty return string */
    if (*buff)
	retval = atoi(buff);
    else
	retval = 1;

    amavis_syslog(DBG_INFO, "retval is %d", retval);

    if (retval == 99) {
	/* there was a virus
	 * discard it so it doesn't go bouncing around!! */
	 amavis_syslog(DBG_WARN, "discarding mail");
	 return clearpriv(ctx, SMFIS_DISCARD, 0);
    }
    if (retval == EX_UNAVAILABLE) { /* REJECT handling */
				    /* by Didi Rieder and Mark Martinec */
	amavis_syslog(DBG_WARN, "rejecting mail");
	if (smfi_setreply(ctx, "550", "5.7.1", "Message content rejected") != MI_SUCCESS) {
	    /* Not sure what we need to do here */
	    amavis_syslog(DBG_WARN, "(mlfi_eom)smfi_setreply failed");
	}
	return clearpriv(ctx, SMFIS_REJECT, 0);
    }
    if (retval == 0) {
	if (enable_x_header) {
	    if (smfi_chgheader(ctx, X_HEADER_TAG, 1, X_HEADER_LINE) == MI_FAILURE) {
		amavis_syslog(DBG_INFO, "adding header");
		if (smfi_addheader(ctx, X_HEADER_TAG, X_HEADER_LINE) != MI_SUCCESS) {
		    /* Not sure what we need to do here */
		    amavis_syslog(DBG_WARN, "(mlfi_eom)smfi_addheader failed");
		}
	    } else
		amavis_syslog(DBG_INFO, "header already present");
	}
	amavis_syslog(DBG_WARN, "returning ACCEPT");
	return clearpriv(ctx, SMFIS_ACCEPT, 0);
    }
    /* if we got any exit status but 0, we didn't check the file...
     * so don't add the header. We return TEMPFAIL instead */

    amavis_syslog(DBG_WARN, "returning TEMPFAIL");
    return clearpriv(ctx, SMFIS_TEMPFAIL, 0);

}

static sfsistat
mlfi_close(SMFICTX *ctx)
{
    return clearpriv(ctx, SMFIS_ACCEPT, 1);
}

static sfsistat
mlfi_abort(SMFICTX *ctx)
{
    return mlfi_cleanup(ctx, 0);
}

static sfsistat
mlfi_cleanup(SMFICTX *ctx, bool ok)
{
    sfsistat rstat = SMFIS_CONTINUE;
    struct mlfiPriv *priv = MLFIPRIV;
    char *p;

    if (!priv)
	return rstat;

    /* close the archive file */
    if (priv->mlfi_fp != NULL && fclose(priv->mlfi_fp) == EOF) {
	/* failed; we have to wait until later */
	rstat = SMFIS_TEMPFAIL;
	(void) unlink(priv->mlfi_fname);
	*(strrchr(priv->mlfi_fname, '/')) = 0;
	rmdir(priv->mlfi_fname);
    } else if (ok) {
	p = strrchr(priv->mlfi_fname, '/');
	if (p == NULL)
	    p = priv->mlfi_fname;
	else
	    p++;
    } else {
	/* message was aborted -- delete the archive file */
	(void) unlink(priv->mlfi_fname);
	*strrchr(priv->mlfi_fname, '/') = 0;
	rmdir(priv->mlfi_fname);
    }

    /* return status */
    amavis_syslog(DBG_INFO, "cleanup called");
    return clearpriv(ctx, rstat, 0);
}


struct smfiDesc smfilter = {
    "amavis-milter",		/* filter name */
    SMFI_VERSION,		/* version code -- do not change */
    SMFIF_ADDHDRS|SMFIF_CHGHDRS,/* flags */
    mlfi_connect,		/* connection info filter */
    NULL,			/* SMTP HELO command filter */
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
    struct passwd *userinfo;	/* amavis uid */
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
	 /* perror("Cannot unlink socket"); */
	    amavis_syslog(DBG_INFO, "INFO: Cannot unlink old socket %s: %s", milter_socket, strerror(errno));
	}
    }

    /* Errors are detected in smfi_main */
    if (smfi_setconn(milter_socket) != MI_SUCCESS) {
	amavis_syslog(DBG_FATAL, "(main)smfi_setconn failed");
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

    /* hand control over to libmilter */
    amavis_syslog(DBG_INFO, "Starting, handing off to smfi_main");
    return smfi_main();
}

/* eof */
