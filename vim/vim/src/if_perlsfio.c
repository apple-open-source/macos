/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */
/*
 * if_perlsfio.c: Special I/O functions for Perl interface.
 */

#define _memory_h	/* avoid memset redeclaration */
#define IN_PERL_FILE	/* don't include if_perl.pro from prot.h */

#include "vim.h"

/*
 * Avoid clashes between Perl and Vim namespace.
 */
#undef NORMAL
#undef STRLEN
#undef FF
#undef OP_DELETE
#undef OP_JOIN
/* remove MAX and MIN, included by glib.h, redefined by sys/param.h */
#ifdef MAX
# undef MAX
#endif
#ifdef MIN
# undef MIN
#endif
/* We use _() for gettext(), Perl uses it for function prototypes... */
#ifdef _
# undef _
#endif
#ifdef DEBUG
# undef DEBUG
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#if defined(USE_SFIO) || defined(PROTO)

#ifndef USE_SFIO	/* just generating prototypes */
# define Sfio_t int
# define Sfdisc_t int
#endif

#define NIL(type)	((type)0)

    static int
sfvimwrite(f, buf, n, disc)
    Sfio_t	    *f;		/* stream involved */
    char	    *buf;	/* buffer to read from */
    int		    n;		/* number of bytes to write */
    Sfdisc_t	    *disc;	/* discipline */
{
    char_u *str;

    str = vim_strnsave((char_u *)buf, n);
    if (str == NULL)
	return 0;
    msg_split((char *)str);
    vim_free(str);

    return n;
}

/*
 * sfdcnewnvi --
 *  Create Vim discipline
 */
    Sfdisc_t *
sfdcnewvim()
{
    Sfdisc_t	*disc;

    disc = (Sfdisc_t *)alloc((unsigned)sizeof(Sfdisc_t));
    if (disc == NULL)
	return NULL;

    disc->readf = (Sfread_f)NULL;
    disc->writef = sfvimwrite;
    disc->seekf = (Sfseek_f)NULL;
    disc->exceptf = (Sfexcept_f)NULL;

    return disc;
}

#endif /* USE_SFIO */
