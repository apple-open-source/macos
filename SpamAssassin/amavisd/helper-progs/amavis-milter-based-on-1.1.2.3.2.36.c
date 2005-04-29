/*
Based on RCSid("$Id: amavis-milter-based-on-1.1.2.3.2.36.c,v 1.1 2004/11/29 22:03:51 dasenbro Exp $")
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
 */

#include "config.h"

#define BUFFLEN 255
/* Must be the same as the buffer length for recv() in amavisd */
#define SOCKBUFLEN 8192

#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <limits.h>

#ifdef HAVE_SM_GEN_H
# include "sm/gen.h"
#endif
#include "libmilter/mfapi.h"

#ifndef HAVE_SM_GEN_BOOL_TYPE
typedef int bool;
#endif

#define TEMPLATE "/amavis-milter-XXXXXXXX"
#define DEVNULL "/dev/null"

/* Extracted from the code for better configurability
 * These will be set by configure/make eventually */
#ifndef X_HEADER_TAG
# define X_HEADER_TAG "X-Virus-Scanned"
#endif
#ifndef X_HEADER_LINE
# define X_HEADER_LINE "by amavisd-milter (http://www.amavis.org/)"
#endif

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

#define DBG_NONE    0
#define DBG_INFO    1
#define DBG_WARN    2
#define DBG_FATAL   4

#define DBG_ALL     (DBG_FATAL | DBG_WARN | DBG_INFO)

/* Don't debug by default - use -d N option to switch it on */
static int debuglevel = DBG_NONE;
static const char *mydebugfile = RUNTIME_DIR "/amavis.client";

static int enable_x_header = 1;

static size_t mystrlcpy(char *, const char *, size_t);
static void mydebug(const int, const char *, ...);
static char *mymktempdir(char *);
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

static struct utsname myuts;

static size_t
mystrlcpy(char *dst, const char *src, size_t size)
{
    size_t src_l = strlen(src);
    if(src_l < size) {
	memcpy(dst, src, src_l + 1);
    } else if(size) {
	memcpy(dst, src, size - 1);
	dst[size - 1] = '\0';
    }
    return src_l;
}

static void
mydebug(const int level, const char *fmt, ...)
{
    FILE *f = NULL;
    time_t tmpt;
    char *timestamp;
    int rv;
    va_list ap;

    /* Only bother to do something if we're told to with -d <n> */
    if (!(level & debuglevel))
	return;

    /* Set up debug file */
    if (mydebugfile && (f = fopen(mydebugfile, "a")) == NULL) {
	fprintf(stderr, "error opening '%s': %s\n", mydebugfile, strerror(errno));
	return;
    }

    tmpt = time(NULL);
    timestamp = ctime(&tmpt);
    /* A 26 character string according ctime(3c)
     * we cut off the trailing \n\0 */
    timestamp[24] = 0;
    rv = fprintf(f, "%s %s amavis(client)[%ld]: ", timestamp,
		 (myuts.nodename ? myuts.nodename : "localhost"), (long) getpid());
    if (rv < 0)
	perror("error writing (fprintf) to debug file");

    va_start(ap, fmt);
    rv = vfprintf(f, fmt, ap);
    va_end(ap);
    if (rv < 0)
	perror("error writing (vfprintf) to debug file");

    rv = fputc('\n', f);
    if (rv < 0)
	perror("error writing (fputc) to debug file");
    rv = fclose(f);
    if (rv < 0)
	perror("error closing debug file f");
}

static char *
mymktempdir(char *s)
{
    char dirname[BUFFLEN];
    char *stt;
    int count = 0;

    mystrlcpy(dirname, s, sizeof(dirname));

#ifdef HAVE_MKDTEMP
    return mkdtemp(s);
#else
    /* magic number alert */
    while (count++ < 20) {
# ifdef HAVE_MKTEMP
	stt = mktemp(s);
# else
	stt = strrchr(dirname, '-') + 1;
	if (stt) {
	    /* more magic number alert */
	    snprintf(stt, sizeof(dirname) - 1 - (stt - dirname), "%08d", lrand48() / 215);
	    /* This assumes that s in the calling function is the same size
	     * as the local dirname! Beware of malicious coders ;-) */
	    mystrlcpy(s, dirname, sizeof(dirname));
	    stt = dirname;
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

    mydebug(DBG_INFO, "Clearing priv (clearall=%d)", clearall);

    if (priv) {
	if (priv->mlfi_fname) {
	    mydebug(DBG_INFO, "clearing fname");
	    free(priv->mlfi_fname);
	    priv->mlfi_fname = NULL;
	}
	if (priv->mlfi_envfrom) {
	    mydebug(DBG_INFO, "clearing envfrom");
	    free(priv->mlfi_envfrom);
	    priv->mlfi_envfrom = NULL;
	}
	if (priv->mlfi_envto.next) {
	    mydebug(DBG_INFO, "clearing multi-envto");
	    freeenvto(priv->mlfi_envto.next);
	    priv->mlfi_envto.next = NULL;
	}
	if (priv->mlfi_envto.str) {
	    mydebug(DBG_INFO, "clearing envto");
	    free(priv->mlfi_envto.str);
	    priv->mlfi_envto.str = NULL;
	}

	mydebug(DBG_INFO, "clearing priv");
	free(priv);
	priv = NULL;
	smfi_setpriv(ctx, priv);
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
	mydebug(DBG_INFO, "priv was null");
	priv = malloc(sizeof *priv);
	if (priv == NULL) {
	    /* can't accept this message right now */
	    mydebug(DBG_FATAL, "failed to malloc %d bytes for private store: %s",
		    sizeof(*priv), strerror(errno));
	    return 1;
	}
	mydebug(DBG_INFO, "malloced priv - now using memset()");
	memset(priv, 0, sizeof *priv);
	mydebug(DBG_INFO, "malloced priv successfully");
	smfi_setpriv(ctx, priv);
    } else {
	mydebug(DBG_WARN, "allocmem tried but priv was already set");
	mydebug(DBG_WARN, "priv->client_addr.s_addr is %d",
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
	mydebug(DBG_INFO, "hostname is %s, addr is %d.%d.%d.%d",
		hostname, (hostaddr->sin_addr.s_addr) & 0xff,
		(hostaddr->sin_addr.s_addr >> 8) & 0xff,
		(hostaddr->sin_addr.s_addr >> 16) & 0xff,
		(hostaddr->sin_addr.s_addr >> 24) & 0xff);
    }
    mydebug(DBG_INFO, "checking allocmem");
    if (allocmem(ctx))
	return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;
    if (hostaddr) {
	priv->client_addr.s_addr = hostaddr->sin_addr.s_addr;
    } else {
	priv->client_addr.s_addr = 0;
    }
    smfi_setpriv(ctx, priv);	/* not really needed but nice */
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
    mydebug(DBG_INFO, "added %s as recip", *envto);
    return SMFIS_CONTINUE;
}

static sfsistat
mlfi_envfrom(SMFICTX * ctx, char **envfrom)
{
    struct mlfiPriv *priv;
    char dirname[BUFFLEN];
    char messagecopy[BUFFLEN];
    struct stat buf, StatBuf;

    if (allocmem(ctx))
	return SMFIS_TEMPFAIL;
    priv = MLFIPRIV;

    /* open a file to store this message */
    mystrlcpy(dirname, RUNTIME_DIR, sizeof(dirname));
    strncat(dirname, TEMPLATE, sizeof(dirname) - 1 - strlen(dirname));
    if (mymktempdir(dirname) == NULL) {
	mydebug(DBG_FATAL, "Failed to create temp dir %s: %s", dirname,
		strerror(errno));
	return SMFIS_TEMPFAIL;
    }
    snprintf(messagecopy, sizeof(messagecopy), "%s/email.txt", dirname);
    mydebug(DBG_INFO, "got %s file, %s sender", messagecopy, *envfrom);

    priv->mlfi_fname = strdup(messagecopy);
    if (priv->mlfi_fname == NULL) {
	return SMFIS_TEMPFAIL;
    }
    priv->mlfi_envfrom = strdup(*envfrom);
    if (!priv->mlfi_envfrom) {
	free(priv->mlfi_fname);
	priv->mlfi_fname = NULL;
	return SMFIS_TEMPFAIL;
    }
    /* umask(0077); */
    if (lstat(dirname, &StatBuf) < 0) {
	mydebug(DBG_FATAL, "Error while trying lstat(%s): %s", dirname,
		strerror(errno));
	exit(EX_UNAVAILABLE);
    }

    /* may be too restrictive for you, but's good to avoid problems */
    if (!S_ISDIR(StatBuf.st_mode) || StatBuf.st_uid != geteuid() || StatBuf.st_gid != getegid() || !(StatBuf.st_mode & S_IRWXU)) {
	mydebug(DBG_FATAL,
		"Security Warning: %s must be a Directory and owned by "
		"User %d and Group %d\n"
		"and just read-/write-able by the User and noone else. "
		"Exit.", dirname, geteuid(), getegid());
	exit(EX_UNAVAILABLE);
    }
    /* there is still a race condition here if RUNTIME_DIR is writeable by the attacker :-\ */

    if ((priv->mlfi_fp = fopen(priv->mlfi_fname, "w+")) == NULL) {
	free(priv->mlfi_fname);
	priv->mlfi_fname = NULL;
	free(priv->mlfi_envfrom);
	priv->mlfi_envfrom = NULL;
	return SMFIS_TEMPFAIL;
    }

    stat(dirname, &buf);
#if 0
    if (buf.st_uid != geteuid() || (buf.st_mode & 0777) != 0700) {
	mydebug(DBG_FATAL,
		"Security alert -- someone changed %s. Uid is %d, mode is %o",
		dirname, buf.st_uid, 0777 & buf.st_mode);
	fclose(priv->mlfi_fp);
	unlink(priv->mlfi_fname);
	unlink(dirname);
	free(priv->mlfi_fname);
	free(priv->mlfi_envfrom);
	free(priv);
	return SMFIS_TEMPFAIL;
    }
#endif

    /* save the private data */

    smfi_setpriv(ctx, priv);

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

    if (priv) {
	int x;

	mydebug(DBG_INFO, "EOM");
	/* close the file so we can run checks on it!!! */
	if (priv->mlfi_fp)
	    fclose(priv->mlfi_fp);

	/* AFAIK, AF_UNIX is obsolete. POSIX defines AF_LOCAL */
	saddr.sun_family = AF_UNIX;
	mystrlcpy(saddr.sun_path, AMAVISD_SOCKET, sizeof(saddr.sun_path));
	mydebug(DBG_INFO, "allocate socket()");
	r = (sock = socket(PF_UNIX, SOCK_STREAM, 0));
	if (r < 0) {
	    mydebug(DBG_FATAL, "failed to allocate socket: %s",
		    strerror(errno));
	}
	if (r >= 0) {
	    mydebug(DBG_INFO, "connect()");
	    r = connect(sock, (struct sockaddr *) (&saddr), sizeof(saddr));
	    if (r < 0)
		mydebug(DBG_FATAL, "failed to connect(): %s", strerror(errno));
	}
	if (r >= 0) {
	    strrchr(priv->mlfi_fname, '/')[0] = 0;
	    /* amavis-perl wants the directory, not the filename */
	    mydebug(DBG_INFO, "senddir()");
	    r = send(sock, priv->mlfi_fname, strlen(priv->mlfi_fname), 0);
	    if (r < 0)
		mydebug(DBG_FATAL, "failed to send() directory: %s",
			strerror(errno));
	}
	if (r >= 0) {
	    r = recv(sock, &retval, 1, 0);
	    if (r < 0)
		mydebug(DBG_FATAL,
			"failed to recv() directory confirmation: %s",
			strerror(errno));
	}
	if (r >= 0) {
	    int sender_l;
	    sender = (strlen(priv->mlfi_envfrom) > 0) ? priv->mlfi_envfrom : "<>";
	    mydebug(DBG_INFO, "sendfrom() %s", sender);
	    sender_l = strlen(sender);
	    if (sender_l > SOCKBUFLEN) {
		mydebug(DBG_WARN, "Sender too long (%d), truncated to %d characters", sender_l, SOCKBUFLEN);
		sender_l = SOCKBUFLEN;
	    }
	    r = send(sock, sender, sender_l, 0);
	    if (r < 0)
		mydebug(DBG_FATAL, "failed to send() Sender: %s", strerror(errno));
	    else if (r < sender_l)
		mydebug(DBG_WARN, "failed to send() complete Sender, truncated to %d characters ", r);
	}
	if (r >= 0) {
	    r = recv(sock, &retval, 1, 0);
	    if (r < 0)
		mydebug(DBG_FATAL, "failed to recv() ok for Sender info: %s",
			strerror(errno));
	}
	if (r >= 0) {
	    priv->mlfi_thisenvto = &(priv->mlfi_envto);
	    for (x = 0; (r >= 0) && (x < priv->mlfi_numto); x++) {
		int recipient_l;
		mydebug(DBG_INFO, "sendto() %s", priv->mlfi_thisenvto->str);
		recipient_l = strlen(priv->mlfi_thisenvto->str);
		if (recipient_l > SOCKBUFLEN) {
		    mydebug(DBG_WARN, "Recipient too long (%d), truncated to %d characters", recipient_l,SOCKBUFLEN);
		    recipient_l = SOCKBUFLEN;
		}
		r = send(sock, priv->mlfi_thisenvto->str, recipient_l, 0);
		if (r < 0)
		    mydebug(DBG_FATAL, "failed to send() Recipient: %s",
			    strerror(errno));
		else {
		    if (r < recipient_l)
			mydebug(DBG_WARN, "failed to send() complete Recipient, truncated to %d characters ", r);
		    r = recv(sock, &retval, 1, 0);
		    if (r < 0)
			mydebug(DBG_FATAL,
				"failed to recv() ok for recip info: %s",
				strerror(errno));
		    priv->mlfi_thisenvto = priv->mlfi_thisenvto->next;
		}
	    }
	}
	if (r >= 0) {
	    mydebug(DBG_INFO, "sendEOT()");
	    r = send(sock, &_EOT, 1, 0);
	    /* send "end of args" msg */
	    if (r < 0) {
		mydebug(DBG_FATAL, "failed to send() EOT: %s", strerror(errno));
	    } else {
		r = recv(sock, buff, 6, 0);
		if (r < 0)
		    mydebug(DBG_FATAL, "Failed to recv() final result: %s",
			    strerror(errno));
		else if (r == 0)
		    mydebug(DBG_FATAL, "Failed to recv() final result: empty status string");
		/* get back final result */
	    }
	}
	close(sock);
	mydebug(DBG_INFO, "finished conversation\n");
	if (r < 0) {
	    return clearpriv(ctx, SMFIS_TEMPFAIL, 0);
	    /* some point of the communication failed miserably - so give up */
	}

	/* Protect against empty return string */
	if (*buff)
	    retval = atoi(buff);
	else
	    retval = 1;

	mydebug(DBG_INFO, "retval is %d", retval);
	if (retval == 99) {
	    /* there was a virus
	     * discard it so it doesn't go bouncing around!! */
	    mydebug(DBG_WARN, "discarding mail");
	    return clearpriv(ctx, SMFIS_DISCARD, 0);
	}
	if (retval == EX_UNAVAILABLE) { /* REJECT handling */
                                       /* by Didi Rieder and Mark Martinec */
	    mydebug(DBG_WARN, "rejecting mail");
	    smfi_setreply(ctx, "550", "5.7.1", "Message content rejected");
	    return clearpriv(ctx, SMFIS_REJECT, 0);
	}
	if (retval == 0) {
	    if (enable_x_header) {
		if (smfi_chgheader(ctx, X_HEADER_TAG, 1, X_HEADER_LINE) == MI_FAILURE) {
		    mydebug(DBG_INFO, "adding header");
		    smfi_addheader(ctx, X_HEADER_TAG, X_HEADER_LINE);
		} else
		    mydebug(DBG_INFO, "header already present");
	    }
	    mydebug(DBG_WARN, "returning ACCEPT");
	    return clearpriv(ctx, SMFIS_ACCEPT, 0);
	}
	/* if we got any exit status but 0, we didn't check the file...
	 * so don't add the header. We return TEMPFAIL instead */

	mydebug(DBG_WARN, "returning TEMPFAIL");
	return clearpriv(ctx, SMFIS_TEMPFAIL, 0);
    }
    mydebug(DBG_WARN, "couldn't scan - no priv object");
    return clearpriv(ctx, SMFIS_TEMPFAIL, 0);	/* no priv object */
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
    char host[512];

    if (!priv)
	return rstat;

    /* close the archive file */
    if (priv->mlfi_fp != NULL && fclose(priv->mlfi_fp) == EOF) {
	/* failed; we have to wait until later */
	rstat = SMFIS_TEMPFAIL;
	(void) unlink(priv->mlfi_fname);
    } else if (ok) {
	/* add a header to the message announcing our presence */
	if (gethostname(host, sizeof(host) - 1) < 0)
	    mystrlcpy(host, "localhost", sizeof host);
	p = strrchr(priv->mlfi_fname, '/');
	if (p == NULL)
	    p = priv->mlfi_fname;
	else
	    p++;
    } else {
	/* message was aborted -- delete the archive file */
	(void) unlink(priv->mlfi_fname);
	*(strrchr(priv->mlfi_fname, '/')) = 0;
	rmdir(priv->mlfi_fname);
    }

    /* return status */
    mydebug(DBG_INFO, "cleanup called");
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


int
main(int argc, char *argv[])
{
    int c, i;
    const char *args = "p:d:Dx";
    pid_t pid;
    int AM_DAEMON = 0;
    int devnull;

#if !defined(HAVE_MKDTEMP) && !defined(HAVE_MKTEMP)
    int mypid = getpid();

    srand48(time(NULL) ^ (mypid + (mypid << 15)));
#endif

/*  umask(0077); */
    umask(0007);

    /* Process command line options */
    while ((c = getopt(argc, argv, args)) != -1) {
	switch (c) {
	case 'p':
	    if (optarg == NULL || *optarg == '\0') {
		mydebug(DBG_FATAL, "Illegal conn: %s", optarg);
		exit(EX_USAGE);
	    }
	    /* Unlink any existing file that might be in place of
	     * the socket we want to create.  This might not exactly
	     * be safe, or friendly, but I'll deal with that later.
	     * Be nice and issue a warning if we have a problem, but
	     * other than that, ignore it. */
	    if ((unlink(optarg)) < 0) {
		mydebug(DBG_WARN, "unlink(): %s: %s", optarg, strerror(errno));
	    }
	    (void) smfi_setconn(optarg);
	    break;
	case 'd':
	    switch (atoi(optarg)) {
	    case 3:
		debuglevel = DBG_INFO;
	    case 2:
		debuglevel = debuglevel | DBG_WARN;
	    case 1:
		debuglevel = debuglevel | DBG_FATAL;
		break;
	    case 0:
		debuglevel = DBG_NONE;
		break;
	    default:
		debuglevel = DBG_ALL;
		break;
	    }
	    break;
	case 'D':
	    AM_DAEMON = 1;
	    break;
	case 'x':
	    enable_x_header++;
	    break;
	default:
	    break;
	}
    }
    if (smfi_register(smfilter) == MI_FAILURE) {
	mydebug(DBG_FATAL, "smfi_register failed");
	exit(EX_UNAVAILABLE);
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

	    mydebug(DBG_INFO, "amavis-milter forked into background");
	    /* We are the parent; exit. */
	    exit(0);

	} else if (pid == -1) {

	    mydebug(DBG_FATAL, "fork() returned error: %s", strerror(errno));
	    exit(EX_UNAVAILABLE);

	}

	/* OK, we're backgrounded.
	 * Step 2: Call setsid to create a new session.  This makes
	 * sure among other things that we have no controlling
	 * terminal.
	 */
	if ( setsid() < (pid_t)0 ) {
	    mydebug(DBG_FATAL, "setsid() returned error: %s", strerror(errno));
	    exit(EX_UNAVAILABLE);
	}

	/* Step 3: Set the working directory appropriately. */
	if (chdir("/") < 0 ) {
	    mydebug(DBG_FATAL, "chdir(/) returned error: %s", strerror(errno));
	    exit(EX_UNAVAILABLE);
	}

	/* Step 4: Close all file descriptors. */
	for (i = 0; i < _POSIX_OPEN_MAX ; i++) {
	    close(i);
	}

	/* Open /dev/null read-only (fd 0 = STDIN) */
	if ((devnull = open(DEVNULL, O_RDONLY, 0)) < 0) {
	     mydebug(DBG_FATAL, "Could not open %s as STDIN: %s", DEVNULL,
		     strerror(errno));
	     exit(EX_UNAVAILABLE);
	}
	if (devnull != 0) {
	     mydebug(DBG_FATAL, "Got wrong file descriptor as STDIN: %s != 0",
	             DEVNULL);
	     exit(EX_UNAVAILABLE);
	}

	/* Open /dev/null write-only (fd 1 = STDOUT) */
	if ((devnull = open(DEVNULL, O_WRONLY, 0)) < 0) {
	     mydebug(DBG_FATAL, "Could not open %s as STDOUT: %s", DEVNULL,
		     strerror(errno));
	     exit(EX_UNAVAILABLE);
	}
	if (devnull != 1) {
	     mydebug(DBG_FATAL, "Got wrong file descriptor as STDOUT: %s != 1",
	             DEVNULL);
	     exit(EX_UNAVAILABLE);
	}

	/* Open /dev/null write-only (fd 2 = STDERR) */
	if ((devnull = open(DEVNULL, O_WRONLY, 0)) < 0) {
	     mydebug(DBG_FATAL, "Could not open %s as STDERR: %s", DEVNULL,
		     strerror(errno));
	     exit(EX_UNAVAILABLE);
	}
	if (devnull != 2) {
	     mydebug(DBG_FATAL, "Got wrong file descriptor as STDERR: %s != 2",
	             DEVNULL);
	     exit(EX_UNAVAILABLE);
	}

    }

    /* hand control over to libmilter */
    mydebug(DBG_INFO, "Starting, handing off to smfi_main");
    return smfi_main();
}

/* eof */
