/* Copyright 1996, The XFree86 Project, Inc */

/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86Beta.c,v 3.9 2002/05/31 18:45:57 dawes Exp $ */

/*
 * This is for publicly released beta server binaries.
 *
 * The current version is written to $HOME/.xf86ServerName
 * If $HOME isn't set (as may be the case when starting xdm at boot time)
 * try '/'.
 *
 * Defining EXPIRE_SERVER enables the server expiry date.  If EXPIRY_TIME
 * is 0, this is disabled.
 *
 * Defining SHOW_BETA_MESSAGE enables displaying the beta message when first
 * running a new beta version (the current version is checked against the
 * contents of $HOME/.xf86ServerName)
 *
 * If EXPIRE_SERVER is defined, the message will be displayed both with
 * WARNING_DAYS days of expiry and after expiry regardless of
 * SHOW_BETA_MESSAGE.
 *
 * MESSAGE_SLEEP sets the sleep time (in seconds) after displaying the
 * message.
 */

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "xf86Version.h"

#ifndef EXPIRY_TIME
#define EXPIRY_TIME 0
#endif
#ifndef SHOW_BETA_MESSAGE
#if (XF86_VERSION_BETA > 0) && (XF86_VERSION_ALPHA == 0)
#define SHOW_BETA_MESSAGE
#endif
#endif
#ifndef EXPIRE_SERVER
#if (XF86_VERSION_BETA > 0) && (XF86_VERSION_ALPHA == 0)
#define EXPIRE_SERVER
#endif
#endif
#ifndef MESSAGE_SLEEP
#define MESSAGE_SLEEP 10
#endif
#ifndef WARNING_DAYS
#define WARNING_DAYS 7
#endif
#define DAY_IN_SECONDS (24 * 60 * 60)

#define XOR_VALUE_1 0x39479da4L
#define XOR_VALUE_2 0x7df6324bL
#define KEY_LENGTH 16

void
xf86CheckBeta(int extraDays, char *key)
{
  FILE *m = NULL;
  Bool showmessage = FALSE;
  Bool expired = FALSE;
  Bool expiresoon = FALSE;
  Bool requireconfirm = FALSE;
  int expiryextended = 0;
#ifdef SHOW_BETA_MESSAGE
  FILE *f = NULL;
  char *home = NULL;
  char *filename = NULL;
  Bool writefile = FALSE;
  char oldvers[16] = {0, };
#endif
  time_t expirytime = EXPIRY_TIME;
 
  /*
   * Check if the beta message should be displayed.  It is displayed when
   * the version doesn't match that in $HOME/.XFree86, and when within
   * one week of the expiry date.
   */

#ifdef SHOW_BETA_MESSAGE
  if (!(home = getenv("HOME")))
    home = "/";
  {
    char homebuf[PATH_MAX];
    /* getenv might return R/O memory, as with OS/2 */
    strncpy(homebuf,home,PATH_MAX-1);
    homebuf[PATH_MAX-1] = '\0';
    home = homebuf;

    if (!(filename =
	  (char *)ALLOCATE_LOCAL(strlen(home) + 
	  			 strlen(xf86ServerName) + 3)))
      showmessage = TRUE;
    else {
      if (home[0] == '/' && home[1] == '\0')
        home[0] = '\0';
      sprintf(filename, "%s/.%s", home, xf86ServerName);
      writefile = TRUE;
      if (!(f = fopen(filename, "r+")))
        showmessage = TRUE;
      else {
	fgets(oldvers, 16, f);
	if (strncmp(oldvers, XF86_VERSION, 15)) {
	  showmessage = TRUE;
	}
	fclose(f);
      }
    }
  }
#endif
#ifdef EXPIRE_SERVER
  if (expirytime != 0) {
    if (extraDays > 0 && key != NULL && strlen(key) == KEY_LENGTH) {
      time_t newExpiry;
      unsigned long key1, key2;
      char tmp[9];
      
      strncpy(tmp, key, 8);
      tmp[8] = '\0';
      key1 = strtoul(tmp, NULL, 16);
      key2 = strtoul(key + 8, NULL, 16);
      newExpiry = expirytime + extraDays * DAY_IN_SECONDS;
      if ((newExpiry ^ key1) == XOR_VALUE_1 &&
	  (newExpiry ^ key2) == XOR_VALUE_2) {
	expirytime = newExpiry;
        expiryextended = extraDays;
      }
    }
    if (time(NULL) > expirytime) {
      showmessage = TRUE;
      expired = TRUE;
    } else if (expirytime - time(NULL) < WARNING_DAYS * (DAY_IN_SECONDS)) {
      showmessage = TRUE;
      expiresoon = TRUE;
    }
  }
#endif

  if (showmessage) {

#if 0
  /*
   * This doesn't work very well.  stdin is usually closed, and even if
   * the server doesn't close it, xinit prevents this from being useful.
   */
    /* Check if stderr is a tty */
    if (isatty(fileno(stderr))) {
      requireconfirm = TRUE;
    }
#endif

#if 0
    /* This doesn't work when the server is started by xinit */
    /* See if /dev/tty can be opened */
    m = fopen("/dev/tty", "r+");
#endif

    if (m)
      requireconfirm = TRUE;
    else
      m = stderr;
    if (m) {
      putc('\007', m);
      fprintf(m, "\n");
      fprintf(m, "             This is a beta version of XFree86.\n\n");
      fprintf(m, " This binary may be redistributed providing it is not"
		 " modified in any way.\n\n");
      fprintf(m, " Please send success and problem reports to"
		 " <report@XFree86.org>.\n\n");
      if (expired) {
	fprintf(m, " This version (%s) has expired.\n", XF86_VERSION);
	fprintf(m, " Please get the release version or a newer beta"
		   " version.\n");
      } else if (expiresoon) {
	fprintf(m, " WARNING! This version (%s) will expire in less than"
		   " %ld day(s)\n at %s\n", XF86_VERSION,
		(long)(expirytime - time(NULL)) / DAY_IN_SECONDS + 1,
		ctime(&expirytime));
	fprintf(m, " Please get the release version or a newer beta"
		   " version.\n");
      } else if (expirytime != 0) {
	fprintf(m, " This version (%s) will expire at %s\n", XF86_VERSION,
		ctime(&expirytime));
      }
      if (!expired && expiryextended) {
	fprintf(m, " Note: the expiry date has been extended by %d days\n",
		expiryextended);
      }

      if (!expired) {
	if (requireconfirm) {
	  char c[2];
	  fflush(m);
	  fprintf(m, "\nHit <Enter> to continue: ");
	  fflush(m);
	  fgets(c, 2, m);
	} else {
	  fflush(m);
	  fprintf(m, "\nWaiting for %d seconds...", MESSAGE_SLEEP);
	  fflush(m);
	  sleep(MESSAGE_SLEEP);
	  fprintf(m, "\n");
	}
      }
      fprintf(m, "\n");
      if (m != stderr)
	fclose(m);
    }
  }
#ifdef SHOW_BETA_MESSAGE

#define WRITE_BETA_FILE  { \
      unlink(filename); \
      if (f = fopen(filename, "w")) { \
        fprintf(f, XF86_VERSION); \
        fclose(f); \
      } \
    }

  if (writefile) {
#if !defined(__UNIXOS2__)
#if defined(SYSV) || defined(linux)
    /* Need to fork to change to ruid without loosing euid */
    if (getuid() != 0) {
      switch (fork()) {
      case -1:
	FatalError("xf86CheckBeta(): fork failed (%s)\n", strerror(errno));
	break;
      case 0:	/* child */
	setuid(getuid());
	WRITE_BETA_FILE
	exit(0);
	break;
      default:	/* parent */
	wait(NULL);
      }
    } else {
      WRITE_BETA_FILE
    }
#else /* ! (SYSV || linux) */
    {
      int realuid = getuid();
#if !defined(SVR4) && !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__FreeBSD__) && !defined(__GNU__)
      setruid(0);
#endif
      seteuid(realuid);
      WRITE_BETA_FILE
      seteuid(0);
#if !defined(SVR4) && !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__FreeBSD__) && !defined(__GNU__)
      setruid(realuid);
#endif
    }
#endif /* SYSV || linux */
#else /* __UNIXOS2__ */
    WRITE_BETA_FILE
#endif /* __UNIXOS2__ */
  }
  if (filename) {
    DEALLOCATE_LOCAL(filename);
    filename = NULL;
  }
#endif
  if (expired)
    exit(1);
}
