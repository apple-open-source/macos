/* GetPass.c */

#include "Sys.h"

#include <signal.h>

#include "Util.h"
#include "GetPass.h"

/* This is responsible for turning off character echoing so a user
 * can type a password without it showing up on the screen.  Why
 * not just the getpass() library function?  Some systems' getpass
 * only returns 8 characters.  We require longer "passwords" so
 * users can use their email addresses when prompted for a password.
 *
 * We have one function, Echo, which turns character echoing on/off.
 * GetPass then uses Echo to turn it off, reads the password,
 * and turns it back on again.
 *
 * Unfortunately this whole thing isn't very portable.  Some systems
 * have termio, others, sgtty.  Some have both.  Newer systems support
 * termios, too.
 */
 
#ifdef CANNOT_HIDE_TEXT

/* If you can't get it to compile, you an always do this. */ 
void Echo(FILE *fp, int on)
{
	return;
}

#else

#ifdef HAVE_TERMIOS_H
#		include <termios.h>
#else
#	ifdef HAVE_TERMIO_H
#		include <termio.h>
#	else
#		ifdef HAVE_SYS_IOCTL_H
#			include <sys/ioctl.h>	/* For TIOxxxx constants. */
#		endif
#		include <sgtty.h>
#	endif
#endif /* !HAVE_TERMIOS_H */

#ifdef STRICT_PROTOS
int ioctl(int, int, ...);
#endif


extern int gWinInit;

void Echo(FILE *fp, int on)
{
#ifdef HAVE_TERMIOS_H
	static struct termios orig, noecho, *tp;
#else
#	ifdef HAVE_TERMIO_H
	static struct termio orig, noecho, *tp;
#	else
	static struct sgttyb orig, noecho, *tp;
#	endif
#endif
	static int state = 0;
	int fd = fileno(fp);

	/* Don't do this if we've setup curses, which makes this crap
	 * irrelevant anyway.
	 */
	if (gWinInit)
		return;

	if (!isatty(fd))
		return;

	if (state == 0) {
#ifdef HAVE_TERMIOS_H
		if (tcgetattr(fd, &orig) < 0)
			perror("tcgetattr");
		noecho = orig;
		noecho.c_lflag &= ~ECHO;
#else
#	ifdef HAVE_TERMIO_H
		if (ioctl(fd, TCGETA, &orig) < 0)
			perror("ioctl");
		noecho = orig;
		noecho.c_lflag &= ~ECHO;
#	else
		if (ioctl(fd, TIOCGETP, &orig) < 0)
			perror("ioctl");
		noecho = orig;
		noecho.sg_flags &= ~ECHO;
#	endif
#endif
		state = 1;
	}
	tp = NULL;
	if (on && state == 2) {
		/* Turn echo back on. */
		tp = &orig;
		state = 1;
	} else if (!on && state == 1) {
		/* Turn echo off. */
		tp = &noecho;
		state = 2;
	}
	if (tp != NULL) {
#ifdef HAVE_TERMIOS_H
		if (tcsetattr(fd, TCSANOW, tp) < 0)
			perror("tcsetattr");
#else
#	ifdef HAVE_TERMIO_H
		if (ioctl(fd, TCSETA, tp) < 0)
			perror("ioctl");
#	else
		if (ioctl(fd, TIOCSETP, tp) < 0)
			perror("ioctl");
#	endif
#endif	/* !HAVE_TERMIOS_H */
	}
}	/* Echo */

#endif	/* CANNOT_HIDE_TEXT */



void GetPass(char *promptstr, char *answer, size_t siz)
{
	FILE *fp, *outfp;
	Sig_t oldintr;
#ifdef SIGTSTP
	Sig_t oldtstp;
#endif

	/*
	 * read and write to /dev/tty if possible; else read from
	 * stdin and write to stderr.
	 */
  	if ((outfp = fp = fopen("/dev/tty", "r+")) == NULL) {
  		outfp = stderr;
  		fp = stdin;
  	}

	/* Ignore some signals that could cause problems.  We don't want
	 * to return with echoing off.
	 */
	oldintr = SIGNAL(SIGINT, SIG_IGN);
#ifdef SIGTSTP
	oldtstp = SIGNAL(SIGTSTP, SIG_IGN);
#endif

	Echo(fp, 0);		/* Turn echoing off. */
	(void) fputs(promptstr, outfp);
	(void) fflush(outfp);
	(void) rewind(outfp);
	answer[0] = '\0';
	if (fgets(answer, (int) siz, fp) != NULL)
		answer[strlen(answer) - 1] = '\0';
	(void) fflush(fp);
	Echo(fp, 1);
	(void) rewind(outfp);
	(void) fputc('\n', outfp);

	(void) SIGNAL(SIGINT, oldintr);
#ifdef SIGTSTP
	(void) SIGNAL(SIGTSTP, oldtstp);
#endif

	if (fp != stdin)
		(void)fclose(fp);
}	/* Getpass */

/* eof Getpass.c */
