/*
 * For license terms, see the file COPYING in this directory.
 */

/***********************************************************************
  module:       getpass.c
  project:      fetchmail
  programmer:   Carl Harris, ceharris@mal.com
  description: 	getpass() replacement which allows for long passwords.
                This version hacked by Wilfred Teiken, allowing the
                password to be piped to fetchmail.
 
 ***********************************************************************/

#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include "fetchmail.h"
#include "i18n.h"

#define INPUT_BUF_SIZE	PASSWORDLEN

#if defined(HAVE_TERMIOS_H) && defined(HAVE_TCSETATTR)
#  include <termios.h>
#else
#if defined(HAVE_TERMIO_H)
#  include <sys/ioctl.h>
#  include <termio.h>
#else
#if defined(HAVE_SGTTY_H)
#  include <sgtty.h>
#endif
#endif
#endif

static int ttyfd;

#if defined(HAVE_TCSETATTR)
  static struct termios termb;
  static tcflag_t flags;
#else
#if defined(HAVE_TERMIO_H)
  static struct termio termb;
  static unsigned short flags;
#else
#if defined(HAVE_STTY)
  static struct sgttyb ttyb;
  static int flags;
#endif
#endif
#endif

static void save_tty_state(void);
static void disable_tty_echo(void);
static void restore_tty_state(void);
static RETSIGTYPE sigint_handler(int);

char *fm_getpassword(char *prompt)
{
#if !(defined(HAVE_TCSETATTR) || defined(HAVE_TERMIO_H) || defined(HAVE_STTY))
#if defined(HAVE_GETPASS) 
    char *getpass();
    return getpass(prompt);
#else
    fputs(GT_("ERROR: no support for getpassword() routine\n"),stderr);
    exit(1);
#endif
#else
    register char *p;
    register int c;
    FILE *fi;
    static char pbuf[INPUT_BUF_SIZE];
    SIGHANDLERTYPE sig = 0;	/* initialization pacifies -Wall */

    int istty = isatty(0);

    /* get the file descriptor for the actual input device if it's a tty */
    if (istty)
    {
	if ((fi = fdopen(open("/dev/tty", 2), "r")) == NULL)
	    fi = stdin;
	else
	    setbuf(fi, (char *)NULL);
    }
    else
	fi = stdin;

    /* store descriptor for the tty */
    ttyfd = fileno(fi);

    if (istty)
    {
	/* preserve tty state before turning off echo */
	save_tty_state();

	/* now that we have the current tty state, we can catch SIGINT and  
	   exit gracefully */
	sig = set_signal_handler(SIGINT, sigint_handler);

	/* turn off echo on the tty */
	disable_tty_echo();

	/* display the prompt and get the input string */
	fprintf(stderr, "%s", prompt);
    }

    for (p = pbuf; (c = getc(fi))!='\n' && c!=EOF;)
    {
	if (p < &pbuf[INPUT_BUF_SIZE - 1])
	    *p++ = c;
    }
    *p = '\0';

    /* write a newline so cursor won't appear to hang */
    if (fi != stdin)
	fprintf(stderr, "\n");

    if (istty)
    {
	/* restore previous state of the tty */
	restore_tty_state();

	/* restore previous state of SIGINT */
	set_signal_handler(SIGINT, sig);
    }
    if (fi != stdin)
	fclose(fi);	/* not checking should be safe, file mode was "r" */

    return(pbuf);
#endif /* !(defined(HAVE_TCSETATTR) || ... */
}

static void save_tty_state (void)
{
#if defined(HAVE_TCSETATTR)
    tcgetattr(ttyfd, &termb);
    flags = termb.c_lflag;
#else
#if defined(HAVE_TERMIO_H)
    ioctl(ttyfd, TCGETA, (char *) &termb);
    flags = termb.c_lflag;
#else  /* we HAVE_STTY */
    gtty(ttyfd, &ttyb);
    flags = ttyb.sg_flags;
#endif
#endif
}

static void disable_tty_echo(void) 
{
    /* turn off echo on the tty */
#if defined(HAVE_TCSETATTR)
    termb.c_lflag &= ~ECHO;
    tcsetattr(ttyfd, TCSAFLUSH, &termb);
#else
#if defined(HAVE_TERMIO_H)
    termb.c_lflag &= ~ECHO;
    ioctl(ttyfd, TCSETA, (char *) &termb);
#else  /* we HAVE_STTY */
    ttyb.sg_flags &= ~ECHO;
    stty(ttyfd, &ttyb);
#endif
#endif
}

static void restore_tty_state(void)
{
    /* restore previous tty echo state */
#if defined(HAVE_TCSETATTR)
    termb.c_lflag = flags;
    tcsetattr(ttyfd, TCSAFLUSH, &termb);
#else
#if defined(HAVE_TERMIO_H)
    termb.c_lflag = flags;
    ioctl(ttyfd, TCSETA, (char *) &termb);
#else  /* we HAVE_STTY */
    ttyb.sg_flags = flags;
    stty(ttyfd, &ttyb);
#endif
#endif
}

static RETSIGTYPE sigint_handler(int signum)
{
    (void)signum;
    restore_tty_state();
    report(stderr, GT_("\nCaught SIGINT... bailing out.\n"));
    exit(1);
}

/* getpass.c ends here */
