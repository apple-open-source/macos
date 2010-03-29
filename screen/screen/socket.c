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

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if !defined(NAMEDPIPE)
#include <sys/socket.h>
#include <sys/un.h>
#endif

#ifndef SIGINT
# include <signal.h>
#endif

#include "screen.h"

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# include <sys/dir.h>
# define dirent direct
#endif

#include "extern.h"

static int   CheckPid __P((int));
static void  ExecCreate __P((struct msg *));
static void  DoCommandMsg __P((struct msg *));
#if defined(_SEQUENT_) && !defined(NAMEDPIPE)
# define connect sconnect	/* _SEQUENT_ has braindamaged connect */
static int   sconnect __P((int, struct sockaddr *, int));
#endif
static void  FinishAttach __P((struct msg *));
static void  AskPassword __P((struct msg *));


extern char *RcFileName, *extra_incap, *extra_outcap;
extern int ServerSocket, real_uid, real_gid, eff_uid, eff_gid;
extern int dflag, iflag, rflag, lsflag, quietflag, wipeflag, xflag;
extern char *attach_tty, *LoginName, HostName[];
extern struct display *display, *displays;
extern struct win *fore, *wtab[], *console_window, *windows;
extern struct layer *flayer;
extern struct NewWindow nwin_undef;
#ifdef MULTIUSER
extern char *multi;
#endif

extern char *getenv();

extern char SockPath[];
extern struct event serv_read;
extern char *rc_name;
extern struct comm comms[];

#ifdef MULTIUSER
# define SOCKMODE (S_IWRITE | S_IREAD | (displays ? S_IEXEC : 0) | (multi ? 1 : 0))
#else
# define SOCKMODE (S_IWRITE | S_IREAD | (displays ? S_IEXEC : 0))
#endif


/*
 *  Socket directory manager
 *
 *  fdp: pointer to store the first good socket.
 *  nfoundp: pointer to store the number of sockets found matching.
 *  notherp: pointer to store the number of sockets not matching.
 *  match: string to match socket name.
 *
 *  The socket directory must be in SockPath!
 *  The global variables LoginName, multi, rflag, xflag, dflag,
 *  quietflag, SockPath are used.
 *
 *  The first good socket is stored in fdp and its name is
 *  appended to SockPath.
 *  If none exists or fdp is NULL SockPath is not changed.
 *
 *  Returns: number of good sockets.
 *    
 */

int
FindSocket(fdp, nfoundp, notherp, match)
int *fdp;
int *nfoundp, *notherp;
char *match;
{
  DIR *dirp;
  struct dirent *dp;
  struct stat st;
  int mode;
  int sdirlen;
  int  matchlen = 0;
  char *name, *n;
  int firsts = -1, sockfd;
  char *firstn = NULL;
  int nfound = 0, ngood = 0, ndead = 0, nwipe = 0, npriv = 0;
  struct sent
    {
      struct sent *next;
      int mode;
      char *name;
    } *slist, **slisttail, *sent, *nsent;
	  
  if (match)
    {
      matchlen = strlen(match);
#ifdef NAME_MAX
      if (matchlen > NAME_MAX)
	matchlen = NAME_MAX;
#endif
    }

  /*
   * SockPath contains the socket directory.
   * At the end of FindSocket the socket name will be appended to it.
   * Thus FindSocket() can only be called once!
   */
  sdirlen = strlen(SockPath);

#ifdef USE_SETEUID
  xseteuid(real_uid);
  xsetegid(real_gid);
#endif

  if ((dirp = opendir(SockPath)) == 0)
    Panic(errno, "Cannot opendir %s", SockPath);

  slist = 0;
  slisttail = &slist;
  while ((dp = readdir(dirp)))
    {
      name = dp->d_name;
      debug1("- %s\n",  name);
      if (*name == 0 || *name == '.' || strlen(name) > 2*MAXSTR)
	continue;
      if (matchlen)
	{
	  n = name;
	  /* if we don't want to match digits. Skip them */
	  if ((*match <= '0' || *match > '9') && (*n > '0' && *n <= '9'))
	    {
	      while (*n >= '0' && *n <= '9')
		n++;
	      if (*n == '.')
		n++;
	    }
	  /* the tty prefix is optional */
	  if (strncmp(match, "tty", 3) && strncmp(n, "tty", 3) == 0)
	    n += 3;
	  if (strncmp(match, n, matchlen))
	    continue;
	  debug1("  -> matched %s\n", match);
	}
      sprintf(SockPath + sdirlen, "/%s", name);

      debug1("stat %s\n", SockPath);
      errno = 0;
      debug2("uid = %d, gid = %d\n", getuid(), getgid());
      debug2("euid = %d, egid = %d\n", geteuid(), getegid());
      if (stat(SockPath, &st))
	{
	  debug1("errno = %d\n", errno);
	  continue;
	}

#ifndef SOCK_NOT_IN_FS
# ifdef NAMEDPIPE
#  ifdef S_ISFIFO
      debug("S_ISFIFO?\n");
      if (!S_ISFIFO(st.st_mode))
	continue;
#  endif
# else
#  ifdef S_ISSOCK
      debug("S_ISSOCK?\n");
      if (!S_ISSOCK(st.st_mode))
	continue;
#  endif
# endif
#endif

      debug2("st.st_uid = %d, real_uid = %d\n", st.st_uid, real_uid);
      if ((int)st.st_uid != real_uid)
	continue;
      mode = (int)st.st_mode & 0777;
      debug1("  has mode 0%03o\n", mode);
#ifdef MULTIUSER 
      if (multi && ((mode & 0677) != 0601))
        {
	  debug("  is not a MULTI-USER session");
	  if (strcmp(multi, LoginName))
	    {
	      debug(" and we are in a foreign directory.\n");
	      mode = -4;
	    }
	  else
	    {
	      debug(", but it is our own session.\n");
	    }
	}
#endif
      debug("  store it.\n");
      if ((sent = (struct sent *)malloc(sizeof(struct sent))) == 0)
	continue;
      sent->next = 0;
      sent->name = SaveStr(name);
      sent->mode = mode;
      *slisttail = sent;
      slisttail = &sent->next;
      nfound++;
      sockfd = MakeClientSocket(0);
#ifdef USE_SETEUID
      /* MakeClientSocket sets ids back to eff */
      xseteuid(real_uid);
      xsetegid(real_gid);
#endif
      if (sockfd == -1)
	{
	  debug2("  MakeClientSocket failed, unreachable? %d %d\n",
	         matchlen, wipeflag);
	  sent->mode = -3;
#ifndef SOCKDIR_IS_LOCAL_TO_HOST
	  /* Unreachable - it is dead if we detect that it's local
           * or we specified a match
           */
	  n = name + strlen(name) - 1;
	  while (n != name && *n != '.')
	    n--;
	  if (matchlen == 0  && !(*n == '.' && n[1] && strncmp(HostName, n + 1, strlen(n + 1)) == 0))
	    {
	      npriv++;		/* a good socket that was not for us */
	      continue;
	    }
#endif
	  ndead++;
	  sent->mode = -1;
	  if (wipeflag)
	    {
	      if (unlink(SockPath) == 0)
		{
		  sent->mode = -2;
		  nwipe++;
		}
	    }
	  continue;
	}

      mode &= 0776;
      /* Shall we connect ? */
      debug2("  connecting: mode=%03o, rflag=%d, ", mode, rflag);
      debug2("xflag=%d, dflag=%d ?\n", xflag, dflag);

      /*
       * mode 600: socket is detached.
       * mode 700: socket is attached.
       * xflag implies rflag here.
       *
       * fail, when socket mode mode is not 600 or 700
       * fail, when we want to detach w/o reattach, but it already is detached.
       * fail, when we only want to attach, but mode 700 and not xflag.
       * fail, if none of dflag, rflag, xflag is set.
       */
      if ((mode != 0700 && mode != 0600) ||
          (dflag && !rflag && !xflag && mode == 0600) ||
	  (!dflag && rflag && mode == 0700 && !xflag) ||
	  (!dflag && !rflag && !xflag))
	{
	  close(sockfd);
	  debug("  no!\n");
	  npriv++;		/* a good socket that was not for us */
	  continue;
	}
      ngood++;
      if (fdp && firsts == -1)
	{
	  firsts = sockfd;
	  firstn = sent->name;
	  debug("  taken.\n");
	}
      else
        {
	  debug("  discarded.\n");
	  close(sockfd);
	} 
    }
  (void)closedir(dirp);
  if (nfound && (lsflag || ngood != 1) && !quietflag)
    {
      switch(ngood)
	{
	case 0:
	  Msg(0, nfound > 1 ? "There are screens on:" : "There is a screen on:");
	  break;
	case 1:
	  Msg(0, nfound > 1 ? "There are several screens on:" : "There is a suitable screen on:");
	  break;
	default:
	  Msg(0, "There are several suitable screens on:");
	  break;
	}
      for (sent = slist; sent; sent = sent->next)
	{
	  switch (sent->mode)
	    {
	    case 0700:
	      printf("\t%s\t(Attached)\n", sent->name);
	      break;
	    case 0600:
	      printf("\t%s\t(Detached)\n", sent->name);
	      break;
#ifdef MULTIUSER
	    case 0701:
	      printf("\t%s\t(Multi, attached)\n", sent->name);
	      break;
	    case 0601:
	      printf("\t%s\t(Multi, detached)\n", sent->name);
	      break;
#endif
	    case -1:
	      /* No trigraphs here! */
	      printf("\t%s\t(Dead ?%c?)\n", sent->name, '?');
	      break;
	    case -2:
	      printf("\t%s\t(Removed)\n", sent->name);
	      break;
	    case -3:
	      printf("\t%s\t(Remote or dead)\n", sent->name);
	      break;
	    case -4:
	      printf("\t%s\t(Private)\n", sent->name);
	      break;
	    }
	}
    }
  if (ndead && !quietflag)
    {
      char *m = "Remove dead screens with 'screen -wipe'.";
      if (wipeflag)
        Msg(0, "%d socket%s wiped out.", nwipe, nwipe > 1 ? "s" : "");
      else
        Msg(0, m, ndead > 1 ? "s" : "", ndead > 1 ? "" : "es");	/* other args for nethack */
    }
  if (firsts != -1)
    {
      sprintf(SockPath + sdirlen, "/%s", firstn);
      *fdp = firsts;
    }
  else
    SockPath[sdirlen] = 0;
  for (sent = slist; sent; sent = nsent)
    {
      nsent = sent->next;
      free(sent->name);
      free((char *)sent);
    }
#ifdef USE_SETEUID
  xseteuid(eff_uid);
  xsetegid(eff_gid);
#endif
  if (notherp)
    *notherp = npriv;
  if (nfoundp)
    *nfoundp = nfound - nwipe;
  return ngood;
}
  

/*
**
**        Socket/pipe create routines
**
*/

#ifdef NAMEDPIPE

int
MakeServerSocket()
{
  register int s;
  struct stat st;

# ifdef USE_SETEUID
  xseteuid(real_uid);
  xsetegid(real_gid);
# endif
  if ((s = open(SockPath, O_WRONLY | O_NONBLOCK)) >= 0)
    {
      debug("huii, my fifo already exists??\n");
      if (quietflag)
	{
	  Kill(D_userpid, SIG_BYE);
	  eexit(11);
	}
      Msg(0, "There is already a screen running on %s.", Filename(SockPath));
      if (stat(SockPath, &st) == -1)
	Panic(errno, "stat");
      if ((int)st.st_uid != real_uid)
	Panic(0, "Unfortunatelly you are not its owner.");
      if ((st.st_mode & 0700) == 0600)
	Panic(0, "To resume it, use \"screen -r\"");
      else
	Panic(0, "It is not detached.");
      /* NOTREACHED */
    }
# ifdef USE_SETEUID
  (void) unlink(SockPath);
  if (mkfifo(SockPath, SOCKMODE))
    Panic(0, "mkfifo %s failed", SockPath);
#  ifdef BROKEN_PIPE
  if ((s = open(SockPath, O_RDWR | O_NONBLOCK, 0)) < 0)
#  else
  if ((s = open(SockPath, O_RDONLY | O_NONBLOCK, 0)) < 0)
#  endif
    Panic(errno, "open fifo %s", SockPath);
  xseteuid(eff_uid);
  xsetegid(eff_gid);
  return s;
# else /* !USE_SETEUID */
  if (UserContext() > 0)
    {
      (void) unlink(SockPath);
      UserReturn(mkfifo(SockPath, SOCKMODE));
    }
  if (UserStatus())
    Panic(0, "mkfifo %s failed", SockPath);
#  ifdef BROKEN_PIPE
  if ((s = secopen(SockPath, O_RDWR | O_NONBLOCK, 0)) < 0)
#  else
  if ((s = secopen(SockPath, O_RDONLY | O_NONBLOCK, 0)) < 0)
#  endif
    Panic(errno, "open fifo %s", SockPath);
  return s;
# endif /* !USE_SETEUID */
}


int
MakeClientSocket(err)
int err;
{
  register int s = 0;

  if ((s = secopen(SockPath, O_WRONLY | O_NONBLOCK, 0)) >= 0)
    {
      (void) fcntl(s, F_SETFL, 0);
      return s;
    }
  if (err)
    Msg(errno, "%s", SockPath);
  debug2("MakeClientSocket() open %s failed (%d)\n", SockPath, errno);
  return -1;
}


#else	/* NAMEDPIPE */


int
MakeServerSocket()
{
  register int s;
  struct sockaddr_un a;
  struct stat st;

  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    Panic(errno, "socket");
  a.sun_family = AF_UNIX;
  strncpy(a.sun_path, SockPath, sizeof(a.sun_path));
  a.sun_path[sizeof(a.sun_path) - 1] = 0;
# ifdef USE_SETEUID
  xseteuid(real_uid);
  xsetegid(real_gid);
# endif
  if (connect(s, (struct sockaddr *) &a, strlen(SockPath) + 2) != -1)
    {
      debug("oooooh! socket already is alive!\n");
      if (quietflag)
	{ 
	  Kill(D_userpid, SIG_BYE);
	  /* 
	   * oh, well. nobody receives that return code. papa 
	   * dies by signal.
	   */
	  eexit(11);
	}
      Msg(0, "There is already a screen running on %s.", Filename(SockPath));
      if (stat(SockPath, &st) == -1)
	Panic(errno, "stat");
      if (st.st_uid != real_uid)
	Panic(0, "Unfortunatelly you are not its owner.");
      if ((st.st_mode & 0700) == 0600)
	Panic(0, "To resume it, use \"screen -r\"");
      else
	Panic(0, "It is not detached.");
      /* NOTREACHED */
    }
#if defined(m88k) || defined(sysV68)
  close(s);	/* we get bind: Invalid argument if this is not done */
  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    Panic(errno, "reopen socket");
#endif
  (void) unlink(SockPath);
  if (bind(s, (struct sockaddr *) & a, strlen(SockPath) + 2) == -1)
    Panic(errno, "bind (%s)", SockPath);
#ifdef SOCK_NOT_IN_FS
    {
      int f;
      if ((f = secopen(SockPath, O_RDWR | O_CREAT, SOCKMODE)) < 0)
        Panic(errno, "shadow socket open");
      close(f);
    }
#else
  chmod(SockPath, SOCKMODE);
# ifndef USE_SETEUID
  chown(SockPath, real_uid, real_gid);
# endif
#endif /* SOCK_NOT_IN_FS */
  if (listen(s, 5) == -1)
    Panic(errno, "listen");
# ifdef F_SETOWN
  fcntl(s, F_SETOWN, getpid());
  debug1("Serversocket owned by %d\n", fcntl(s, F_GETOWN, 0));
# endif /* F_SETOWN */
# ifdef USE_SETEUID
  xseteuid(eff_uid);
  xsetegid(eff_gid);
# endif
  return s;
}

int
MakeClientSocket(err)
int err;
{
  register int s;
  struct sockaddr_un a;

  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    Panic(errno, "socket");
  a.sun_family = AF_UNIX;
  strncpy(a.sun_path, SockPath, sizeof(a.sun_path));
  a.sun_path[sizeof(a.sun_path) - 1] = 0;
# ifdef USE_SETEUID
  xseteuid(real_uid);
  xsetegid(real_gid);
# else
  if (access(SockPath, W_OK))
    {
      if (err)
	Msg(errno, "%s", SockPath);
      debug2("MakeClientSocket: access(%s): %d.\n", SockPath, errno);
      close(s);
      return -1;
    }
# endif
  if (connect(s, (struct sockaddr *) &a, strlen(SockPath) + 2) == -1)
    {
      if (err)
	Msg(errno, "%s: connect", SockPath);
      debug("MakeClientSocket: connect failed.\n");
      close(s);
      s = -1;
    }
# ifdef USE_SETEUID
  xseteuid(eff_uid);
  xsetegid(eff_gid);
# endif
  return s;
}
#endif /* NAMEDPIPE */


/*
**
**       Message send and receive routines
**
*/

void
SendCreateMsg(sty, nwin)
char *sty;
struct NewWindow *nwin;
{
  int s;
  struct msg m;
  register char *p;
  register int len, n;
  char **av = nwin->args;

#ifdef NAME_MAX
  if (strlen(sty) > NAME_MAX)
    sty[NAME_MAX] = 0;
#endif
  if (strlen(sty) > 2 * MAXSTR - 1)
    sty[2 * MAXSTR - 1] = 0;
  sprintf(SockPath + strlen(SockPath), "/%s", sty);
  if ((s = MakeClientSocket(1)) == -1)
    exit(1);
  debug1("SendCreateMsg() to '%s'\n", SockPath);
  bzero((char *)&m, sizeof(m));
  m.type = MSG_CREATE;
  strncpy(m.m_tty, attach_tty, sizeof(m.m_tty) - 1);
  m.m_tty[sizeof(m.m_tty) - 1] = 0;
  p = m.m.create.line;
  n = 0;
  if (nwin->args != nwin_undef.args)
    for (av = nwin->args; *av && n < MAXARGS - 1; ++av, ++n)
      {
        len = strlen(*av) + 1;
        if (p + len >= m.m.create.line + sizeof(m.m.create.line) - 1)
	  break;
        strcpy(p, *av);
        p += len;
      }
  if (nwin->aka != nwin_undef.aka && p + strlen(nwin->aka) + 1 < m.m.create.line + sizeof(m.m.create.line))
    strcpy(p, nwin->aka);
  else
    *p = '\0';
  m.m.create.nargs = n;
  m.m.create.aflag = nwin->aflag;
  m.m.create.flowflag = nwin->flowflag;
  m.m.create.lflag = nwin->lflag;
  m.m.create.hheight = nwin->histheight;
  if (getcwd(m.m.create.dir, sizeof(m.m.create.dir)) == 0)
    {
      Msg(errno, "getcwd");
      return;
    }
  if (nwin->term != nwin_undef.term)
    strncpy(m.m.create.screenterm, nwin->term, 19);
  m.m.create.screenterm[19] = '\0';
  m.protocol_revision = MSG_REVISION;
  debug1("SendCreateMsg writing '%s'\n", m.m.create.line);
  if (write(s, (char *) &m, sizeof m) != sizeof m)
    Msg(errno, "write");
  close(s);
}

int
SendErrorMsg(tty, buf)
char *tty, *buf;
{
  int s;
  struct msg m;

  strncpy(m.m.message, buf, sizeof(m.m.message) - 1);
  m.m.message[sizeof(m.m.message) - 1] = 0;
  s = MakeClientSocket(0);
  if (s < 0)
    return -1;
  m.type = MSG_ERROR;
  strncpy(m.m_tty, tty, sizeof(m.m_tty) - 1);
  m.m_tty[sizeof(m.m_tty) - 1] = 0;
  m.protocol_revision = MSG_REVISION;
  debug1("SendErrorMsg(): writing to '%s'\n", SockPath);
  (void) write(s, (char *) &m, sizeof m);
  close(s);
  return 0;
}

static void
ExecCreate(mp)
struct msg *mp;
{
  struct NewWindow nwin;
  char *args[MAXARGS];
  register int n;
  register char **pp = args, *p = mp->m.create.line;

  nwin = nwin_undef;
  n = mp->m.create.nargs;
  if (n > MAXARGS - 1)
    n = MAXARGS - 1;
  /* ugly hack alert... should be done by the frontend! */
  if (n)
    {
      int l, num;
      char buf[20];

      l = strlen(p);
      if (IsNumColon(p, 10, buf, sizeof(buf)))
	{
	  if (*buf)
	    nwin.aka = buf;
	  num = atoi(p);
	  if (num < 0 || num > MAXWIN - 1)
	    num = 0;
	  nwin.StartAt = num;
	  p += l + 1;
	  n--;
	}
    }
  for (; n > 0; n--)
    {
      *pp++ = p;
      p += strlen(p) + 1;
    }
  *pp = 0;
  if (*p)
    nwin.aka = p;
  if (*args)
    nwin.args = args;
  nwin.aflag = mp->m.create.aflag;
  nwin.flowflag = mp->m.create.flowflag;
  if (*mp->m.create.dir)
    nwin.dir = mp->m.create.dir;
  nwin.lflag = mp->m.create.lflag;
  nwin.histheight = mp->m.create.hheight;
  if (*mp->m.create.screenterm)
    nwin.term =  mp->m.create.screenterm;
  MakeWindow(&nwin);
}

static int
CheckPid(pid)
int pid;
{
  debug1("Checking pid %d\n", pid);
  if (pid < 2)
    return -1;
  if (eff_uid == real_uid)
    return kill(pid, 0);
  if (UserContext() > 0)
    UserReturn(kill(pid, 0));
  return UserStatus();
}

#ifdef hpux
/*
 * From: "F. K. Bruner" <napalm@ugcs.caltech.edu>
 * From: "Dan Egnor" <egnor@oracorp.com> Tue Aug 10 06:56:45 1993
 * The problem is that under HPUX (and possibly other systems too) there are
 * two equivalent device files for each pty/tty device:
 * /dev/ttyxx == /dev/pty/ttyxx
 * /dev/ptyxx == /dev/ptym/ptyxx
 * I didn't look into the exact specifics, but I've run across this problem
 * before: Even if you open /dev/ttyxx as fds 0 1 & 2 for a process, if that
 * process calls the system to determine its tty, it'll get /dev/pty/ttyxx.
 *
 * Earlier versions seemed to work -- wonder what they did.
 */
static int
ttycmp(s1, s2)
char *s1, *s2;
{
  if (strlen(s1) > 5) s1 += strlen(s1) - 5;
  if (strlen(s2) > 5) s2 += strlen(s2) - 5;
  return strcmp(s1, s2);
}
# define TTYCMP(a, b) ttycmp(a, b)
#else
# define TTYCMP(a, b) strcmp(a, b)
#endif

void
ReceiveMsg()
{
  int left, len, i;
  static struct msg m;
  char *p;
  int ns = ServerSocket;
  struct mode Mode;
  struct win *wi;
#ifdef REMOTE_DETACH
  struct display *next;
#endif
  struct display *olddisplays = displays;

#ifdef NAMEDPIPE
  debug("Ha, there was someone knocking on my fifo??\n");
  if (fcntl(ServerSocket, F_SETFL, 0) == -1)
    Panic(errno, "BLOCK fcntl");
#else
  struct sockaddr_un a;

  len = sizeof(a);
  debug("Ha, there was someone knocking on my socket??\n");
  if ((ns = accept(ns, (struct sockaddr *) &a, &len)) < 0)
    {
      Msg(errno, "accept");
      return;
    }
#endif				/* NAMEDPIPE */

  p = (char *) &m;
  left = sizeof(m);
  while (left > 0)
    {
      len = read(ns, p, left);
      if (len < 0 && errno == EINTR)
	continue;
      if (len <= 0)
	break;
      p += len;
      left -= len;
    }

#ifdef NAMEDPIPE
# ifndef BROKEN_PIPE
  /* Reopen pipe to prevent EOFs at the select() call */
  close(ServerSocket);
  if ((ServerSocket = secopen(SockPath, O_RDONLY | O_NONBLOCK, 0)) < 0)
    Panic(errno, "reopen fifo %s", SockPath);
  evdeq(&serv_read);
  serv_read.fd = ServerSocket;
  evenq(&serv_read);
# endif
#else
  close(ns);
#endif

  if (len < 0)
    {
      Msg(errno, "read");
      return;
    }
  if (left > 0)
    {
      if (left != sizeof(m))
        Msg(0, "Message %d of %d bytes too small", left, (int)sizeof(m));
      else
	debug("No data on socket.\n");
      return;
    }
  if (m.protocol_revision != MSG_REVISION)
    {
      Msg(0, "Invalid message (magic 0x%08x).", m.protocol_revision);
      return;
    }

  debug2("*** RecMsg: type %d tty %s\n", m.type, m.m_tty);
  for (display = displays; display; display = display->d_next)
    if (TTYCMP(D_usertty, m.m_tty) == 0)
      break;
  debug2("display: %s display %sfound\n", m.m_tty, display ? "" : "not ");
  wi = 0;
  if (!display)
    {
      for (wi = windows; wi; wi = wi->w_next)
        if (!TTYCMP(m.m_tty, wi->w_tty))
	  {
	    /* XXX: hmmm, rework this? */
            display = wi->w_layer.l_cvlist ? wi->w_layer.l_cvlist->c_display : 0;
	    debug2("but window %s %sfound.\n", m.m_tty, display ? "" : 
	    	   "(backfacing)");
	    break;
          }
    }

  /* Remove the status to prevent garbage on the screen */
  if (display && D_status)
    RemoveStatus();

  if (display && !D_tcinited && m.type != MSG_HANGUP)
    return;		/* ignore messages for bad displays */

  switch (m.type)
    {
    case MSG_WINCH:
      if (display)
        CheckScreenSize(1); /* Change fore */
      break;
    case MSG_CREATE:
      /*
       * the window that issued the create message need not be an active
       * window. Then we create the window without having a display.
       * Resulting in another inactive window.
       * 
       * Currently we enforce that at least one display exists. But why?
       * jw.
       */
      if (displays)
	ExecCreate(&m);
      break;
    case MSG_CONT:
	if (display && D_userpid != 0 && kill(D_userpid, 0) == 0)
	  break;		/* Intruder Alert */
      debug2("RecMsg: apid=%d,was %d\n", m.m.attach.apid, display ? D_userpid : 0);
      /* FALLTHROUGH */

    case MSG_ATTACH:
      if (CheckPid(m.m.attach.apid))
	{
	  Msg(0, "Attach attempt with bad pid(%d)!", m.m.attach.apid);
          break;
	}
      if ((i = secopen(m.m_tty, O_RDWR | O_NONBLOCK, 0)) < 0)
	{
	  Msg(errno, "Attach: Could not open %s!", m.m_tty);
	  Kill(m.m.attach.apid, SIG_BYE);
	  break;
	}
# ifdef MULTIUSER
      Kill(m.m.attach.apid, SIGCONT);
# endif

#if defined(ultrix) || defined(pyr) || defined(NeXT)
      brktty(i);	/* for some strange reason this must be done */
#endif

      if (display || wi)
	{
	  write(i, "Attaching from inside of screen?\n", 33);
	  close(i);
	  Kill(m.m.attach.apid, SIG_BYE);
	  Msg(0, "Attach msg ignored: coming from inside.");
	  break;
	}

#ifdef MULTIUSER
      if (strcmp(m.m.attach.auser, LoginName))
        if (*FindUserPtr(m.m.attach.auser) == 0)
	  {
              write(i, "Access to session denied.\n", 26);
	      close(i);
	      Kill(m.m.attach.apid, SIG_BYE);
	      Msg(0, "Attach: access denied for user %s.", m.m.attach.auser);
	      break;
	  }
#endif

      debug2("RecMsg: apid %d is o.k. and we just opened '%s'\n", m.m.attach.apid, m.m_tty);
#ifndef MULTI
      if (displays)
	{
	  write(i, "Screen session in use.\n", 23);
	  close(i);
	  Kill(m.m.attach.apid, SIG_BYE);
	  break;
	}
#endif

      /* create new display */
      GetTTY(i, &Mode);
      if (MakeDisplay(m.m.attach.auser, m.m_tty, m.m.attach.envterm, i, m.m.attach.apid, &Mode) == 0)
        {
	  write(i, "Could not make display.\n", 24);
	  close(i);
	  Msg(0, "Attach: could not make display for user %s", m.m.attach.auser);
	  Kill(m.m.attach.apid, SIG_BYE);
	  break;
        }
#ifdef ENCODINGS
# ifdef UTF8
      D_encoding = m.m.attach.encoding == 1 ? UTF8 : m.m.attach.encoding ? m.m.attach.encoding - 1 : 0;
# else
      D_encoding = m.m.attach.encoding ? m.m.attach.encoding - 1 : 0;
# endif
      if (D_encoding < 0 || !EncodingName(D_encoding))
	D_encoding = 0;
#endif
      /* turn off iflag on a multi-attach... */
      if (iflag && olddisplays)
	{
	  iflag = 0;
#if defined(TERMIO) || defined(POSIX)
	  olddisplays->d_NewMode.tio.c_cc[VINTR] = VDISABLE;
	  olddisplays->d_NewMode.tio.c_lflag &= ~ISIG;
#else /* TERMIO || POSIX */
	  olddisplays->d_NewMode.m_tchars.t_intrc = -1;
#endif /* TERMIO || POSIX */
	  SetTTY(olddisplays->d_userfd, &olddisplays->d_NewMode);
	}
      SetMode(&D_OldMode, &D_NewMode, D_flow, iflag);
      SetTTY(D_userfd, &D_NewMode);
      if (fcntl(D_userfd, F_SETFL, FNBLOCK))
        Msg(errno, "Warning: NBLOCK fcntl failed");

#ifdef PASSWORD
      if (D_user->u_password && *D_user->u_password)
	AskPassword(&m);
      else
#endif
        FinishAttach(&m);
      break;
    case MSG_ERROR:
      Msg(0, "%s", m.m.message);
      break;
    case MSG_HANGUP:
      if (!wi)		/* ignore hangups from inside */
        Hangup();
      break;
#ifdef REMOTE_DETACH
    case MSG_DETACH:
# ifdef POW_DETACH
    case MSG_POW_DETACH:
# endif				/* POW_DETACH */
      for (display = displays; display; display = next)
	{
	  next = display->d_next;
# ifdef POW_DETACH
	  if (m.type == MSG_POW_DETACH)
	    Detach(D_REMOTE_POWER);
	  else
# endif				/* POW_DETACH */
	  if (m.type == MSG_DETACH)
	    Detach(D_REMOTE);
	}
      break;
#endif
    case MSG_COMMAND:
      DoCommandMsg(&m);
      break;
    default:
      Msg(0, "Invalid message (type %d).", m.type);
    }
}

#if defined(_SEQUENT_) && !defined(NAMEDPIPE)
#undef connect
/*
 *  sequent_ptx socket emulation must have mode 000 on the socket!
 */
static int
sconnect(s, sapp, len)
int s, len;
struct sockaddr *sapp;
{
  register struct sockaddr_un *sap;
  struct stat st;
  int x;

  sap = (struct sockaddr_un *)sapp;
  if (stat(sap->sun_path, &st))
    return -1;
  chmod(sap->sun_path, 0);
  x = connect(s, (struct sockaddr *) sap, len);
  chmod(sap->sun_path, st.st_mode);
  return x;
}
#endif


/*
 * Set the mode bits of the socket to the current status
 */
int
chsock()
{
  int r, euid = geteuid();
  if (euid != real_uid)
    {
      if (UserContext() <= 0)
        return UserStatus();
    }
  r = chmod(SockPath, SOCKMODE);
  /* 
   * Sockets usually reside in the /tmp/ area, where sysadmin scripts
   * may be happy to remove old files. We manually prevent the socket
   * from becoming old. (chmod does not touch mtime).
   */
  (void)utimes(SockPath, NULL);

  if (euid != real_uid)
    UserReturn(r);
  return r;
}

/*
 * Try to recreate the socket/pipe
 */
int
RecoverSocket()
{
  close(ServerSocket);
  if ((int)geteuid() != real_uid)
    {
      if (UserContext() > 0)
	UserReturn(unlink(SockPath));
      (void)UserStatus();
    }
  else
    (void) unlink(SockPath);

  if ((ServerSocket = MakeServerSocket()) < 0)
    return 0;
  evdeq(&serv_read);
  serv_read.fd = ServerSocket;
  evenq(&serv_read);
  return 1;
}


static void
FinishAttach(m)
struct msg *m;
{
  char *p;
  int pid;
  int noshowwin;

#ifndef __APPLE__
  struct win *wi;
#endif

  ASSERT(display);
  pid = D_userpid;

#if defined(pyr) || defined(xelos) || defined(sequent)
  /*
   * Kludge for systems with braindamaged termcap routines,
   * which evaluate $TERMCAP, regardless weather it describes
   * the correct terminal type or not.
   */
  debug("unsetenv(TERMCAP) in case of a different terminal");
  unsetenv("TERMCAP");
#endif

  /*
   * We reboot our Terminal Emulator. Forget all we knew about
   * the old terminal, reread the termcap entries in .screenrc
   * (and nothing more from .screenrc is read. Mainly because
   * I did not check, weather a full reinit is safe. jw) 
   * and /etc/screenrc, and initialise anew.
   */
  if (extra_outcap)
    free(extra_outcap);
  if (extra_incap)
    free(extra_incap);
  extra_incap = extra_outcap = 0;
  debug2("Message says size (%dx%d)\n", m->m.attach.columns, m->m.attach.lines);
#ifdef ETCSCREENRC
# ifdef ALLOW_SYSSCREENRC
  if ((p = getenv("SYSSCREENRC")))
    StartRc(p);
  else
# endif
    StartRc(ETCSCREENRC);
#endif
  StartRc(RcFileName);
  if (InitTermcap(m->m.attach.columns, m->m.attach.lines))
    {
      FreeDisplay();
      Kill(pid, SIG_BYE);
      return;
    }
  MakeDefaultCanvas();
  InitTerm(m->m.attach.adaptflag);	/* write init string on fd */
  if (displays->d_next == 0)
    (void) chsock();
  signal(SIGHUP, SigHup);
  if (m->m.attach.esc != -1 && m->m.attach.meta_esc != -1)
    {
      D_user->u_Esc = m->m.attach.esc;
      D_user->u_MetaEsc = m->m.attach.meta_esc;
    }

#ifdef UTMPOK
  /*
   * we set the Utmp slots again, if we were detached normally
   * and if we were detached by ^Z.
   * don't log zomies back in!
   */
  RemoveLoginSlot();
  if (displays->d_next == 0)
    for (wi = windows; wi; wi = wi->w_next)
      if (wi->w_ptyfd >= 0 && wi->w_slot != (slot_t) -1)
	SetUtmp(wi);
#endif

  D_fore = NULL;
  /*
   * there may be a window that we remember from last detach:
   */
  debug1("D_user->u_detachwin = %d\n", D_user->u_detachwin);
  if (D_user->u_detachwin >= 0) 
    fore = wtab[D_user->u_detachwin];
  else
    fore = 0;

  /* Wayne wants us to restore the other window too. */
  if (D_user->u_detachotherwin >= 0)
    D_other = wtab[D_user->u_detachotherwin];

  noshowwin = 0;
  if (*m->m.attach.preselect)
    {
      if (!strcmp(m->m.attach.preselect, "="))
        fore = 0;
      else if (!strcmp(m->m.attach.preselect, "-"))
	{
          fore = 0;
	  noshowwin = 1;
	}
      else
        fore = FindNiceWindow(fore, m->m.attach.preselect);
    }
  else
    fore = FindNiceWindow(fore, 0);
  if (fore)
    SetForeWindow(fore);
  else if (!noshowwin)
    {
#ifdef MULTIUSER
      if (!AclCheckPermCmd(D_user, ACL_EXEC, &comms[RC_WINDOWLIST]))
#endif
	{
	  flayer = D_forecv->c_layer;
	  display_wlist(1, WLIST_NUM);
	  noshowwin = 1;
	}
    }
  Activate(0);
  ResetIdle();
  if (!D_fore && !noshowwin)
    ShowWindows(-1);
  if (displays->d_next == 0 && console_window)
    {
      if (TtyGrabConsole(console_window->w_ptyfd, 1, "reattach") == 0)
	Msg(0, "console %s is on window %d", HostName, console_window->w_number);
    }
  debug("activated...\n");

# if defined(DEBUG) && defined(SIG_NODEBUG)
  if (!dfp)
    {
      sleep(1);
      debug1("Attacher %d must not debug, as we have debug off.\n", pid);
      kill(pid, SIG_NODEBUG);
    }
# endif /* SIG_NODEBUG */
}


#ifdef PASSWORD
static void PasswordProcessInput __P((char *, int));

struct pwdata {
  int l;
  char buf[20 + 1];
  struct msg m;
};

static void
AskPassword(m)
struct msg *m;
{
  struct pwdata *pwdata;
  ASSERT(display);
  pwdata = (struct pwdata *)malloc(sizeof(struct pwdata));
  if (!pwdata)
    Panic(0, strnomem);
  pwdata->l = 0;
  pwdata->m = *m;
  D_processinputdata = (char *)pwdata;
  D_processinput = PasswordProcessInput;
  AddStr("Screen password: ");
}

static void
PasswordProcessInput(ibuf, ilen)
char *ibuf;
int ilen;
{
  struct pwdata *pwdata;
  int c, l;
  char *up;
  int pid = D_userpid;

  pwdata = (struct pwdata *)D_processinputdata;
  l = pwdata->l;
  while (ilen-- > 0)
    {
      c = *(unsigned char *)ibuf++;
      if (c == '\r' || c == '\n')
	{
	  up = D_user->u_password;
	  pwdata->buf[l] = 0;
	  if (strncmp(crypt(pwdata->buf, up), up, strlen(up)))
	    {
	      /* uh oh, user failed */
	      bzero(pwdata->buf, sizeof(pwdata->buf));
	      AddStr("\r\nPassword incorrect.\r\n");
	      D_processinputdata = 0;	/* otherwise freed by FreeDis */
	      FreeDisplay();
	      Msg(0, "Illegal reattach attempt from terminal %s.", pwdata->m.m_tty);
	      free(pwdata);
	      Kill(pid, SIG_BYE);
	      return;
	    }
	  /* great, pw matched, all is fine */
	  bzero(pwdata->buf, sizeof(pwdata->buf));
	  AddStr("\r\n");
	  D_processinputdata = 0;
	  D_processinput = ProcessInput;
	  FinishAttach(&pwdata->m);
	  free(pwdata);
	  return;
	}
      if (c == Ctrl('c'))
	{
	  AddStr("\r\n");
	  FreeDisplay();
	  Kill(pid, SIG_BYE);
	  return;
	}
      if (c == '\b' || c == 0177)
	{
	  if (l > 0)
	    l--;
	  continue;
	}
      if (c == Ctrl('u'))
	{
	  l = 0;
	  continue;
	}
      if (l < (int)sizeof(pwdata->buf) - 1)
	pwdata->buf[l++] = c;
    }
  pwdata->l = l;
}
#endif

static void
DoCommandMsg(mp)
struct msg *mp;
{
  char *args[MAXARGS];
  int argl[MAXARGS];
  int n, *lp;
  register char **pp = args, *p = mp->m.command.cmd;
  struct acluser *user;
#ifdef MULTIUSER
  extern struct acluser *EffectiveAclUser;	/* acls.c */
#else
  extern struct acluser *users;			/* acls.c */
#endif

  lp = argl;
  n = mp->m.command.nargs;
  if (n > MAXARGS - 1)
    n = MAXARGS - 1;
  for (; n > 0; n--)
    {
      *pp++ = p;
      *lp = strlen(p);
      p += *lp++ + 1;
    }
  *pp = 0;
#ifdef MULTIUSER
  user = *FindUserPtr(mp->m.attach.auser);
  if (user == 0)
    {
      Msg(0, "Unknown user %s tried to send a command!", mp->m.attach.auser);
      return;
    }
#else
  user = users;
#endif
#ifdef PASSWORD
  if (user->u_password && *user->u_password)
    {
      Msg(0, "User %s has a password, cannot use -X option.", mp->m.attach.auser);
      return;
    }
#endif
  if (!display)
    for (display = displays; display; display = display->d_next)
      if (D_user == user)
	break;
  for (fore = windows; fore; fore = fore->w_next)
    if (!TTYCMP(mp->m_tty, fore->w_tty))
      {
	if (!display)
	  display = fore->w_layer.l_cvlist ? fore->w_layer.l_cvlist->c_display : 0;
	break;
      }
  if (!display)
    display = displays;		/* sigh */
  if (*mp->m.command.preselect)
    {
      int i = -1;
      if (strcmp(mp->m.command.preselect, "-"))
        i = WindowByNoN(mp->m.command.preselect);
      fore = i >= 0 ? wtab[i] : 0;
    }
  else if (!fore)
    {
      if (display && D_user == user)
	fore = Layer2Window(display->d_forecv->c_layer);
      if (!fore)
	{
	  fore = user->u_detachwin >= 0 ? wtab[user->u_detachwin] : 0;
	  fore = FindNiceWindow(fore, 0);
	}
    }
#ifdef MULTIUSER
  EffectiveAclUser = user;
#endif
  if (*args)
    {
      char *oldrcname = rc_name;
      rc_name = "-X";
      debug3("Running command on display %x window %x (%d)\n", display, fore, fore ? fore->w_number : -1);
      flayer = fore ? &fore->w_layer : 0;
      if (fore && fore->w_savelayer && (fore->w_blocked || fore->w_savelayer->l_cvlist == 0))
	flayer = fore->w_savelayer;
      DoCommand(args, argl);
      rc_name = oldrcname;
    }
#ifdef MULTIUSER
  EffectiveAclUser = 0;
#endif
}
