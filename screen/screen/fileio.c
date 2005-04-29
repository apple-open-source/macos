/* Copyright (c) 1993-2002
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef SIGINT
# include <signal.h>
#endif

#include "config.h" 
#include "screen.h"
#include "extern.h"

extern struct display *display, *displays;
extern struct win *fore;
extern struct layer *flayer;
extern int real_uid, eff_uid;
extern int real_gid, eff_gid;
extern char *extra_incap, *extra_outcap;
extern char *home, *RcFileName;
extern char SockPath[], *SockName;
#ifdef COPY_PASTE
extern char *BufferFile;
#endif
extern int hardcopy_append;
extern char *hardcopydir;

static char *CatExtra __P((char *, char *));
static char *findrcfile __P((char *));


char *rc_name = "";
int rc_recursion = 0;

static char *
CatExtra(str1, str2)
register char *str1, *str2;
{
  register char *cp;
  register int len1, len2, add_colon;

  len1 = strlen(str1);
  if (len1 == 0)
    return str2;
  add_colon = (str1[len1 - 1] != ':');
  if (str2)
    {
      len2 = strlen(str2);
      if ((cp = realloc(str2, (unsigned) len1 + len2 + add_colon + 1)) == NULL)
	Panic(0, strnomem);
      bcopy(cp, cp + len1 + add_colon, len2 + 1);
    }
  else
    {
      if (len1 == 0)
	return 0;
      if ((cp = malloc((unsigned) len1 + add_colon + 1)) == NULL)
	Panic(0, strnomem);
      cp[len1 + add_colon] = '\0'; 
    }
  bcopy(str1, cp, len1);
  if (add_colon)
    cp[len1] = ':';

  return cp;
}

static char *
findrcfile(rcfile)
char *rcfile;
{
  char buf[256];
  char *p;

  if (rcfile)
    {
      char *rcend = rindex(rc_name, '/');
      if (*rcfile != '/' && rcend && (rcend - rc_name) + strlen(rcfile) + 2 < sizeof(buf))
	{
	  strncpy(buf, rc_name, rcend - rc_name + 1);
	  strcpy(buf + (rcend - rc_name) + 1, rcfile);
	  if (access(buf, R_OK) == 0)
	    return SaveStr(buf);
	}
      debug1("findrcfile: you specified '%s'\n", rcfile);
      return SaveStr(rcfile);
    }
  debug("findrcfile: you specified nothing...\n");
  if ((p = getenv("SCREENRC")) != NULL && *p != '\0')
    {
      debug1("  $SCREENRC has: '%s'\n", p);
      return SaveStr(p);
    }
  else
    {
      debug("  ...nothing in $SCREENRC, defaulting $HOME/.screenrc\n");
      if (strlen(home) > sizeof(buf) - 12)
	Panic(0, "Rc: home too large");
      sprintf(buf, "%s/.screenrc", home);
      return SaveStr(buf);
    }
}

/*
 * this will be called twice:
 * 1) rcfilename = "/etc/screenrc"
 * 2) rcfilename = RcFileName
 */
void
StartRc(rcfilename)
char *rcfilename;
{
  register int argc, len;
  register char *p, *cp;
  char buf[2048];
  char *args[MAXARGS];
  int argl[MAXARGS];
  FILE *fp;
  char *oldrc_name = rc_name;

  /* always fix termcap/info capabilities */
  extra_incap = CatExtra("TF", extra_incap);

  /* Special settings for vt100 and others */
  if (display && (!strncmp(D_termname, "vt", 2) || !strncmp(D_termname, "xterm", 5)))
    extra_incap = CatExtra("xn:f0=\033Op:f1=\033Oq:f2=\033Or:f3=\033Os:f4=\033Ot:f5=\033Ou:f6=\033Ov:f7=\033Ow:f8=\033Ox:f9=\033Oy:f.=\033On:f,=\033Ol:fe=\033OM:f+=\033Ok:f-=\033Om:f*=\033Oj:f/=\033Oo:fq=\033OX", extra_incap);

  rc_name = findrcfile(rcfilename);

  if ((fp = secfopen(rc_name, "r")) == NULL)
    {
      if (!rc_recursion && RcFileName && !strcmp(RcFileName, rc_name))
	{
          /*
           * User explicitly gave us that name,
           * this is the only case, where we get angry, if we can't read
           * the file.
           */
	  debug3("StartRc: '%s','%s', '%s'\n", RcFileName, rc_name, rcfilename);
          Panic(0, "Unable to open \"%s\".", rc_name);
	  /* NOTREACHED */
	}
      debug1("StartRc: '%s' no good. ignored\n", rc_name);
      Free(rc_name);
      rc_name = oldrc_name;
      return;
    }
  while (fgets(buf, sizeof buf, fp) != NULL)
    {
      if ((p = rindex(buf, '\n')) != NULL)
	*p = '\0';
      if ((argc = Parse(buf, sizeof buf, args, argl)) == 0)
	continue;
      if (strcmp(args[0], "echo") == 0)
	{
	  if (!display)
	    continue;
	  if (argc < 2 || (argc == 3 && strcmp(args[1], "-n")) || argc > 3)
	    {
	      Msg(0, "%s: 'echo [-n] \"string\"' expected.", rc_name);
	      continue;
	    }
	  AddStr(args[argc - 1]);
	  if (argc != 3)
	    {
	      AddStr("\r\n");
	      Flush();
	    }
	}
      else if (strcmp(args[0], "sleep") == 0)
	{
	  if (!display)
	    continue;
	  debug("sleeeeeeep\n");
	  if (argc != 2)
	    {
	      Msg(0, "%s: sleep: one numeric argument expected.", rc_name);
	      continue;
	    }
	  DisplaySleep1000(1000 * atoi(args[1]), 1);
	}
#ifdef TERMINFO
      else if (!strcmp(args[0], "termcapinfo") || !strcmp(args[0], "terminfo"))
#else
      else if (!strcmp(args[0], "termcapinfo") || !strcmp(args[0], "termcap"))
#endif
	{
	  if (!display)
	    continue;
	  if (argc < 3 || argc > 4)
	    {
	      Msg(0, "%s: %s: incorrect number of arguments.", rc_name, args[0]);
	      continue;
	    }
	  for (p = args[1]; p && *p; p = cp)
	    {
	      if ((cp = index(p, '|')) != 0)
		*cp++ = '\0';
	      len = strlen(p);
	      if (p[len - 1] == '*')
		{
		  if (!(len - 1) || !strncmp(p, D_termname, len - 1))
		    break;
		}
	      else if (!strcmp(p, D_termname))
		break;
	    }
	  if (!(p && *p))
	    continue;
	  extra_incap = CatExtra(args[2], extra_incap);
	  if (argc == 4)
	    extra_outcap = CatExtra(args[3], extra_outcap);
	}
      else if (!strcmp(args[0], "source"))
	{
	  if (rc_recursion <= 10)
	    {
	      rc_recursion++;
	      StartRc(args[1]);
	      rc_recursion--;
	    }
	}
    }
  fclose(fp);
  Free(rc_name);
  rc_name = oldrc_name;
}

void
FinishRc(rcfilename)
char *rcfilename;
{
  char buf[2048];
  FILE *fp;
  char *oldrc_name = rc_name;

  rc_name = findrcfile(rcfilename);

  if ((fp = secfopen(rc_name, "r")) == NULL)
    {
      if (rc_recursion)
        Msg(errno, "%s: source %s", oldrc_name, rc_name);
      else if (RcFileName && !strcmp(RcFileName, rc_name))
	{
    	  /*
 	   * User explicitly gave us that name, 
	   * this is the only case, where we get angry, if we can't read
	   * the file.
	   */
  	  debug3("FinishRc:'%s','%s','%s'\n", RcFileName, rc_name, rcfilename);
          Panic(0, "Unable to open \"%s\".", rc_name);
	  /* NOTREACHED */
	}
      debug1("FinishRc: '%s' no good. ignored\n", rc_name);
      Free(rc_name);
      rc_name = oldrc_name;
      return;
    }

  debug("finishrc is going...\n");
  while (fgets(buf, sizeof buf, fp) != NULL)
    RcLine(buf, sizeof buf);
  (void) fclose(fp);
  Free(rc_name);
  rc_name = oldrc_name;
}

void
do_source(rcfilename)
char *rcfilename;
{
  if (rc_recursion > 10)
    {
      Msg(0, "%s: source: recursion limit reached", rc_name);
      return;
    }
  rc_recursion++;
  FinishRc(rcfilename);
  rc_recursion--;
}


/*
 * Running a Command Line in the environment determined by the display.
 * The fore window is taken from the display as well as the user.
 * This is bad when we run detached.
 */
void
RcLine(ubuf, ubufl)
char *ubuf;
int ubufl;
{
  char *args[MAXARGS];
  int argl[MAXARGS];
#ifdef MULTIUSER
  extern struct acluser *EffectiveAclUser;	/* acl.c */
  extern struct acluser *users;		/* acl.c */
#endif

  if (display)
    {
      fore = D_fore;
      flayer = D_forecv->c_layer;
    }
  else
    flayer = fore ? fore->w_savelayer : 0;
  if (Parse(ubuf, ubufl, args, argl) <= 0)
    return;
#ifdef MULTIUSER
  if (!display)
    {
      /* the session owner does it, when there is no display here */
      EffectiveAclUser = users;        
      debug("RcLine: WARNING, no display no user! Session owner executes command\n");
    }
#endif
  DoCommand(args, argl);
#ifdef MULTIUSER
  EffectiveAclUser = 0;
#endif
}

/*
 * needs display for copybuffer access and termcap dumping
 */
void
WriteFile(user, fn, dump)
struct acluser *user;
char *fn;
int dump;
{
  /* dump==0:	create .termcap,
   * dump==1:	hardcopy,
   * #ifdef COPY_PASTE
   * dump==2:	BUFFERFILE
   * #endif COPY_PASTE 
   * dump==1:	scrollback,
   */
  register int i, j, k;
  register char *p;
  register FILE *f;
  char fnbuf[1024];
  char *mode = "w";
#ifdef COPY_PASTE
  int public = 0;
# ifdef _MODE_T
  mode_t old_umask;
# else
  int old_umask;
# endif
# ifdef HAVE_LSTAT
  struct stat stb, stb2;
  int fd, exists = 0;
# endif
#endif

  switch (dump)
    {
    case DUMP_TERMCAP:
      if (fn == 0)
	{
	  i = SockName - SockPath;
	  if (i > (int)sizeof(fnbuf) - 9)
	    i = 0;
	  strncpy(fnbuf, SockPath, i);
	  strcpy(fnbuf + i, ".termcap");
	  fn = fnbuf;
	}
      break;
    case DUMP_HARDCOPY:
    case DUMP_SCROLLBACK:
      if (fn == 0)
	{
	  if (fore == 0)
	    return;
	  if (hardcopydir && *hardcopydir && strlen(hardcopydir) < sizeof(fnbuf) - 21)
	    sprintf(fnbuf, "%s/hardcopy.%d", hardcopydir, fore->w_number);
	  else
	    sprintf(fnbuf, "hardcopy.%d", fore->w_number);
	  fn = fnbuf;
	}
      if (hardcopy_append && !access(fn, W_OK))
	mode = "a";
      break;
#ifdef COPY_PASTE
    case DUMP_EXCHANGE:
      if (fn == 0)
	{
	  strncpy(fnbuf, BufferFile, sizeof(fnbuf) - 1);
	  fnbuf[sizeof(fnbuf) - 1] = 0;
	  fn = fnbuf;
	}
      public = !strcmp(fn, DEFAULT_BUFFERFILE);
# ifdef HAVE_LSTAT
      exists = !lstat(fn, &stb);
      if (public && exists && (S_ISLNK(stb.st_mode) || stb.st_nlink > 1))
	{
	  Msg(0, "No write to links, please.");
	  return;
	}
# endif
      break;
#endif
    }

  debug2("WriteFile(%d) %s\n", dump, fn);
  if (UserContext() > 0)
    {
      debug("Writefile: usercontext\n");
#ifdef COPY_PASTE
      if (dump == DUMP_EXCHANGE && public)
	{
          old_umask = umask(0);
# ifdef HAVE_LSTAT
	  if (exists)
	    {
	      if ((fd = open(fn, O_WRONLY, 0666)) >= 0)
		{
		  if (fstat(fd, &stb2) == 0 && stb.st_dev == stb2.st_dev && stb.st_ino == stb2.st_ino)
		    ftruncate(fd, 0);
		  else
		    {
		      close(fd);
		      fd = -1;
		    }
		}
	    }
	  else
	    fd = open(fn, O_WRONLY|O_CREAT|O_EXCL, 0666);
	  f = fd >= 0 ? fdopen(fd, mode) : 0;
# else
          f = fopen(fn, mode);
# endif
          umask(old_umask);
	}
      else
#endif /* COPY_PASTE */
        f = fopen(fn, mode);
      if (f == NULL)
	{
	  debug2("WriteFile: fopen(%s,\"%s\") failed\n", fn, mode);
	  UserReturn(0);
	}
      else
	{
	  switch (dump)
	    {
	    case DUMP_HARDCOPY:
	    case DUMP_SCROLLBACK:
	      if (!fore)
		break;
	      if (*mode == 'a')
		{
		  putc('>', f);
		  for (j = fore->w_width - 2; j > 0; j--)
		    putc('=', f);
		  fputs("<\n", f);
		}
	      if (dump == DUMP_SCROLLBACK)
		{
		  for (i = 0; i < fore->w_histheight; i++)
		    {
		      p = (char *)(WIN(i)->image);
		      for (k = fore->w_width - 1; k >= 0 && p[k] == ' '; k--)
			;
		      for (j = 0; j <= k; j++)
			putc(p[j], f);
		      putc('\n', f);
		    }
		}
	      for (i = 0; i < fore->w_height; i++)
		{
		  p = (char *)fore->w_mlines[i].image;
		  for (k = fore->w_width - 1; k >= 0 && p[k] == ' '; k--)
		    ;
		  for (j = 0; j <= k; j++)
		    putc(p[j], f);
		  putc('\n', f);
		}
	      break;
	    case DUMP_TERMCAP:
	      if ((p = index(MakeTermcap(fore->w_aflag), '=')) != NULL)
		{
		  fputs(++p, f);
		  putc('\n', f);
		}
	      break;
#ifdef COPY_PASTE
	    case DUMP_EXCHANGE:
	      p = user->u_plop.buf;
	      for (i = user->u_plop.len; i-- > 0; p++)
		if (*p == '\r' && (i == 0 || p[1] != '\n'))
		  putc('\n', f);
		else
		  putc(*p, f);
	      break;
#endif
	    }
	  (void) fclose(f);
	  UserReturn(1);
	}
    }
  if (UserStatus() <= 0)
    Msg(0, "Cannot open \"%s\"", fn);
  else if (display && !*rc_name)
    {
      switch (dump)
	{
	case DUMP_TERMCAP:
	  Msg(0, "Termcap entry written to \"%s\".", fn);
	  break;
	case DUMP_HARDCOPY:
	case DUMP_SCROLLBACK:
	  Msg(0, "Screen image %s to \"%s\".",
	      (*mode == 'a') ? "appended" : "written", fn);
	  break;
#ifdef COPY_PASTE
	case DUMP_EXCHANGE:
	  Msg(0, "Copybuffer written to \"%s\".", fn);
#endif
	}
    }
}

#ifdef COPY_PASTE

/*
 * returns an allocated buffer which holds a copy of the file named fn.
 * lenp (if nonzero) points to a location, where the buffer size should be 
 * stored.
 */
char *
ReadFile(fn, lenp)
char *fn;
int *lenp;
{
  int i, l, size;
  char c, *bp, *buf;
  struct stat stb;

  ASSERT(lenp);
  debug1("ReadFile(%s)\n", fn);
  if ((i = secopen(fn, O_RDONLY, 0)) < 0)
    {
      Msg(errno, "no %s -- no slurp", fn);
      return NULL;
    }
  if (fstat(i, &stb))
    {
      Msg(errno, "no good %s -- no slurp", fn);
      close(i);
      return NULL;
    }
  size = stb.st_size;
  if ((buf = malloc(size)) == NULL)
    {
      close(i);
      Msg(0, strnomem);
      return NULL;
    }
  errno = 0;
  if ((l = read(i, buf, size)) != size)
    {
      if (l < 0) 
        l = 0;
      Msg(errno, "Got only %d bytes from %s", l, fn);
      close(i);
    }
  else
    {
      if (read(i, &c, 1) > 0)
	Msg(0, "Slurped only %d characters (of %d) into buffer - try again", 
	    l, size);
      else
	Msg(0, "Slurped %d characters into buffer", l);
    }
  close(i);
  *lenp = l;
  for (bp = buf; l-- > 0; bp++)
    if (*bp == '\n' && (bp == buf || bp[-1] != '\r'))
      *bp = '\r';
  return buf;
}

void
KillBuffers()
{
  if (UserContext() > 0)
    UserReturn(unlink(BufferFile) ? errno : 0);
  errno = UserStatus();
  Msg(errno, "%s %sremoved", BufferFile, errno ? "not " : "");
}
#endif	/* COPY_PASTE */


/*
 * (Almost) secure open and fopen...
 */

FILE *
secfopen(name, mode)
char *name;
char *mode;
{
  FILE *fi;
#ifndef USE_SETEUID
  int flags, fd;
#endif

  debug2("secfopen(%s, %s)\n", name, mode);
#ifdef USE_SETEUID
  xseteuid(real_uid);
  xsetegid(real_gid);
  fi = fopen(name, mode);
  xseteuid(eff_uid);
  xsetegid(eff_gid);
  return fi;
#else
  if (eff_uid == real_uid)
    return fopen(name, mode);
  if (mode[0] && mode[1] == '+')
    flags = O_RDWR;
  else
    flags = (mode[0] == 'r') ? O_RDONLY : O_WRONLY;
  if (mode[0] == 'w')
    flags |= O_CREAT | O_TRUNC;
  else if (mode[0] == 'a')
    flags |= O_CREAT | O_APPEND;
  else if (mode[0] != 'r')
    {
      errno = EINVAL;
      return 0;
    }
  if ((fd = secopen(name, flags, 0666)) < 0)
    return 0;
  if ((fi = fdopen(fd, mode)) == 0)
    {
      close(fd);
      return 0;
    }
  return fi;
#endif
}


int
secopen(name, flags, mode)
char *name;
int flags;
int mode;
{
  int fd;
#ifndef USE_SETEUID
  int q;
  struct stat stb;
#endif

  debug3("secopen(%s, 0x%x, 0%03o)\n", name, flags, mode);
#ifdef USE_SETEUID
  xseteuid(real_uid);
  xsetegid(real_gid);
  fd = open(name, flags, mode);
  xseteuid(eff_uid);
  xsetegid(eff_gid);
  return fd;
#else
  if (eff_uid == real_uid)
    return open(name, flags, mode);
  /* Truncation/creation is done in UserContext */
  if ((flags & O_TRUNC) || ((flags & O_CREAT) && access(name, F_OK)))
    {
      if (UserContext() > 0)
	{
          if ((fd = open(name, flags, mode)) >= 0)
	    {
	      close(fd);
	      UserReturn(0);
            }
	  if (errno == 0)
	    errno = EACCES;
	  UserReturn(errno);
	}
      if ((q = UserStatus()))
	{
	  if (q > 0)
	    errno = q;
          return -1;
	}
    }
  if (access(name, F_OK))
    return -1;
  if ((fd = open(name, flags & ~(O_TRUNC | O_CREAT), 0)) < 0)
    return -1;
  debug("open successful\n");
  if (fstat(fd, &stb))
    {
      close(fd);
      return -1;
    }
  debug("fstat successful\n");
  if (stb.st_uid != real_uid)
    {
      switch (flags & (O_RDONLY | O_WRONLY | O_RDWR))
        {
	case O_RDONLY:
	  q = 0004;
	  break;
	case O_WRONLY:
	  q = 0002;
	  break;
	default:
	  q = 0006;
	  break;
        }
      if ((stb.st_mode & q) != q)
	{
          debug1("secopen: permission denied (%03o)\n", stb.st_mode & 07777);
	  close(fd);
	  errno = EACCES;
	  return -1;
	}
    }
  debug1("secopen ok - returning %d\n", fd);
  return fd;
#endif
}


int
printpipe(p, cmd)
struct win *p;
char *cmd;
{
  int pi[2];
  if (pipe(pi))
    {
      WMsg(p, errno, "printing pipe");
      return -1;
    }
  switch (fork())
    {
    case -1:
      WMsg(p, errno, "printing fork");
      return -1;
    case 0:
      display = p->w_pdisplay;
      displays = 0;
#ifdef DEBUG
      if (dfp && dfp != stderr)
        fclose(dfp);
#endif
      close(0);
      dup(pi[0]);
      closeallfiles(0);
      if (setgid(real_gid) || setuid(real_uid))
        Panic(errno, "printpipe setuid");
#ifdef SIGPIPE
      signal(SIGPIPE, SIG_DFL);
#endif
      execl("/bin/sh", "sh", "-c", cmd, 0);
      Panic(errno, "/bin/sh");
    default:
      break;
    }
  close(pi[0]);
  return pi[1];
}

int
readpipe(cmdv)
char **cmdv;
{
  int pi[2];

  if (pipe(pi))
    {
      Msg(errno, "pipe");
      return -1;
    }
  switch (fork())
    {
    case -1:
      Msg(errno, "fork");
      return -1;
    case 0:
      displays = 0;
#ifdef DEBUG
      if (dfp && dfp != stderr)
        fclose(dfp);
#endif
      close(1);
      if (dup(pi[1]) != 1)
	{
	  close(pi[1]);
          Panic(0, "dup");
	}
      closeallfiles(1);
      if (setgid(real_gid) || setuid(real_uid))
	{
          close(1);
          Panic(errno, "setuid/setgid");
	}
#ifdef SIGPIPE
      signal(SIGPIPE, SIG_DFL);
#endif
      execvp(*cmdv, cmdv);
      close(1);
      Panic(errno, *cmdv);
    default:
      break;
    }
  close(pi[1]);
  return pi[0];
}
