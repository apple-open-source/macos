/* Wrapper code for Taylor UUCP on Amiga Unix (SVR4) for cron invoked UUCP */
/* uucico. */

/* The problem: Cron and rmail (as forked by uuxqt, which may in
   turn invoke uucico) are not "licensed" processes. Any process
   that grabs a controlling terminal needs to be licensed.  Taylor
   UUCP needs controlling terminals.  Taylor UUCP does relinquish
   the controlling terminal before fork(), so the "UUCP" license is
   appropriate.  This simple program does the "right" thing, but
   *MUST* be SETUID ROOT.

   To use this program, you must move 'uucico' to 'uucico.real' (or
   change the *name = below), compile this program, move it to where
   uucico was originally, and make it SETUID ROOT. 

   This program is intended to be used as a wapper for Taylor UUCP's
   uucico so that the annoying 'unlicensed user attempted to fork'
   messages are eliminated.  */

/* Written by: Lawrence E. Rosenman <ler@lerami.lerctr.org>
   Modified by: Donald Phillips <don@blkhole.resun.com> */

#include <sys/sysm68k.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>

int main(int argc,char *argv[],char *envp)
{
  struct passwd *pw;
  char   *name = {"/usr/lib/uucp/uucico.real"};

  if (sysm68k(_m68k_LIMUSER,EUA_GET_LIC) == 0 ) { /* are we unlicensed? */
    if (sysm68k(_m68k_LIMUSER,EUA_UUCP) == -1) { /* yes, get a "uucp"
                                                    license */
      fprintf(stderr,"sysm68k failed, errno=%d\n",errno); /* we didn't grab
                                                             it? */
      exit(errno);
    }

    pw = getpwnam("uucp");        /* get the Password Entry for uucp */
    if (pw == NULL) {
      fprintf(stderr,"User ID \"uucp\" doesn't exist.\n");
      exit(1);
    }
    setgid(pw->pw_gid);           /* set gid to uucp */
    setuid(pw->pw_uid);           /* set uid to uucp */ 
  }

  argv[0]=name;                 /* have PS not lie... */
  execv(name, argv);            /* go to the real program */
  exit(errno);
}
