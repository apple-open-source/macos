/* termios and other support for WIN32.

   Derived from sources with the following header comments:
   Written by Doug Evans and Steve Chamberlain of Cygnus Support

   THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "winpdo-sup.h"

#include <windows.h>
#include <errno.h>
#include <malloc.h>
#include <io.h>
#include <windows.h>

#include <fcntl.h>
#include <sys/stat.h>

#ifndef W_OK
#define W_OK 2
#endif

#define NOT_OUR_FD(fd) (fd < 0 || fd > 2)

int
access (const char *fn, int flags)
{
  struct stat s;

  if (stat (fn, &s))
    return -1;
  if (s.st_mode & S_IFDIR)
    return 0;
  if (flags & W_OK)
    {
      if (s.st_mode & S_IWRITE)
	return 0;
      return -1;
    }
  return 0;
}

int
chdir (const char *dir)
{
  int res = SetCurrentDirectory (dir);
  return res ? 0 : -1;
}


char *
getcwd (char *buf, size_t len)
{
  int res;

  if (buf == (char *) 0)
    if ((buf = malloc(len)) == (char *) 0)
      return (char *) 0;

  if ((res = GetCurrentDirectoryA (len, buf)) == 0)
    {
      errno = ENOENT;
      return (char *) 0;
    }
  if (res >= len)
    {
      /* len was too small */
      errno = ERANGE;
      return (char *) 0;
    }
  return buf;
}

int
ScreenRows ()
{
  CONSOLE_SCREEN_BUFFER_INFO p;
  GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &p);
  return (p.srWindow.Bottom - p.srWindow.Top + 1);
}


int
ScreenCols ()
{
  CONSOLE_SCREEN_BUFFER_INFO p;
  GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &p);
  return (p.srWindow.Right - p.srWindow.Left + 1);
}

void 
ScreenGetCursor (int *row, int *col)
{
  CONSOLE_SCREEN_BUFFER_INFO p;
  GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &p);
  *row = p.dwCursorPosition.Y;
  *col = p.dwCursorPosition.X;
}

void 
ScreenSetCursor (int row, int col)
{
  COORD p;
  p.X = col;
  p.Y = row;
  SetConsoleCursorPosition (GetStdHandle(STD_OUTPUT_HANDLE), p);
}

int
ioctl (int fd, int cmd, void *buf)
{
  if (NOT_OUR_FD(fd))
    {
      errno = EBADF;
      return -1;
    }

  switch (cmd)
    {
    case TIOCGWINSZ:
      {
	CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE fdHandle = get_std_handle(fd);

	GetConsoleScreenBufferInfo (fdHandle, &info);
	((struct winsize *) buf)->ws_row = info.dwSize.Y;
	((struct winsize *) buf)->ws_col = info.dwSize.X;
	return 0;
      }
    case TIOCSWINSZ:
      return 0;
    default:
      errno = EINVAL;
      return -1;
    }

  return 0;
}

int kbhit ()
{
  INPUT_RECORD i;
  unsigned long n;
#if 0
  printf ("peek %d\n", PeekConsoleInput (GetStdHandle (STD_INPUT_HANDLE, &i, 1, &n));
#else
  PeekConsoleInput (GetStdHandle(STD_INPUT_HANDLE), &i, 1, &n);
#endif

  return n;
}

int getkey ()
{

  INPUT_RECORD i;
  unsigned long n;

  do
    {
      ReadConsoleInput (GetStdHandle(STD_INPUT_HANDLE), &i, 1, &n);

      if (i.EventType == KEY_EVENT && i.Event.KeyEvent.bKeyDown)
	{
	  if (i.Event.KeyEvent.uChar.AsciiChar == 0)
	    switch (i.Event.KeyEvent.wVirtualKeyCode) {
	      /* translate arrow keys to emacs keys */
	      case 35:	/* end */
		i.Event.KeyEvent.uChar.AsciiChar = 'E' - 64;	/* ^e */
		break;
	      case 36:	/* home */
		i.Event.KeyEvent.uChar.AsciiChar = 'A' - 64;	/* ^a */
		break;
	      case 37:	/* left arrow */
		i.Event.KeyEvent.uChar.AsciiChar = 'B' - 64;	/* ^b */
		break;
	      case 38:	/* up arrow */
		i.Event.KeyEvent.uChar.AsciiChar = 'P' - 64;	/* ^p */
		break;
	      case 39:	/* right arrow */
		i.Event.KeyEvent.uChar.AsciiChar = 'F' - 64;	/* ^f */
		break;
	      case 40:	/* down arrow */
		i.Event.KeyEvent.uChar.AsciiChar = 'N' - 64;	/* ^n */
		break;
	      case 46:	/* delete */
		i.Event.KeyEvent.uChar.AsciiChar = 'D' - 64;	/* ^d */
		break;
	      default:
		continue;
	    }
	  if (i.Event.KeyEvent.uChar.AsciiChar != 0)
	    break;
	}
    } while (1);

  return i.Event.KeyEvent.uChar.AsciiChar;
}

#include <signal.h>
#include <process.h>

int
kill (int pid, int sig)
{
  if (pid != getpid())
    return -1;
  if (sig == 0)
    return 0;

  return  raise (sig);
}

int 
getuid()
{
  return getpid();	/* really just a stub! */
}

int
getgid()
{
  return getpid();	/* really just a stub! */
}

#include <string.h>

void
path_to_real_path(in, out)
     char *in;
     char *out;
{
  strcpy(out, in);
}

void dos_path_to_unix_path_keep_rel(in, out)
     char *in;
     char *out;
{
  strcpy(out, in);
}

void dos_path_to_unix_path(in, out)
     char *in;
     char *out;
{
  strcpy(out, in);
}

void unix_path_to_dos_path_keep_rel(in, out)
     char *in;
     char *out;
{
  strcpy(out, in);
}

void unix_path_to_dos_path(in, out)
     char *in;
     char *out;
{
  strcpy(out, in);
}

static HANDLE 
get_std_handle (int fd)
{
  DWORD chan[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
  /* requires: 0 <= fd <= 2 */
  return GetStdHandle(chan[fd]);
}

int
tcsendbreak (int fd, int duration)
{
  return 0;
}

int
tcdrain (int fd)
{
  return 0;
}

int
tcflush (int fd, int queue)
{
  int res = 0;

  if (NOT_OUR_FD (fd))
    {
      errno = EBADF;
      res = -1;
    }

  else
    {
      HANDLE fdHandle = get_std_handle(fd);

      if (queue & (TCOFLUSH | TCIOFLUSH))
	{
	  PurgeComm (fdHandle, PURGE_TXABORT | PURGE_TXCLEAR);
	}
      if (queue & (TCIFLUSH | TCIOFLUSH))
	{
	  /* Input flushing by polling until nothing turns up
	     (we stop after 1000 chars anyway) */
	  COMMTIMEOUTS old;
	  COMMTIMEOUTS tmp;
	  char b;
	  DWORD more = 1;
	  int max = 1000;
	  PurgeComm (fdHandle, PURGE_RXABORT | PURGE_RXCLEAR);
	  GetCommTimeouts (fdHandle, &old);
	  memset (&tmp, 0, sizeof (tmp));
	  tmp.ReadTotalTimeoutConstant = 100;
	  SetCommTimeouts (fdHandle, &tmp);
	  while (max > 0 && more)
	    {
	      ReadFile (fdHandle, &b, 1, &more, 0);
	      max--;
	    }
	  SetCommTimeouts (fdHandle, &old);
	}
    }
  return res;
}

int
tcflow (int fd, int action)
{
  return 0;
}
static void
tdump (int fd)
{
}

static void ds (char *when, DCB *s)
{
}

int
tcsetattr (int fd, int actions, const struct termios *t)
{
  int newrate;
  int newsize;

  COMMTIMEOUTS to;
  DCB state;
  HANDLE fdHandle;

  if (NOT_OUR_FD (fd))
    {
      errno = EBADF;
      return -1;
    }

  switch (t->c_ospeed)
    {
    case B110:
      newrate = CBR_110;
      break;
    case B300:
      newrate = CBR_300;
      break;
    case B600:
      newrate = CBR_600;
      break;
    case B1200:
      newrate = CBR_1200;
      break;
    case B2400:
      newrate = CBR_2400;
      break;
    case B4800:
      newrate = CBR_4800;
      break;
    case B9600:
      newrate = CBR_9600;
      break;
    case B19200:
      newrate = CBR_19200;
      break;
    case B38400:
      newrate = CBR_38400;
      break;
    default:
      errno = EINVAL;
      return -1;
    }

  switch (t->c_cflag & CSIZE)
    {
    case CS5:
      newsize = 5;
      break;
    case CS6:
      newsize = 6;
      break;
    case CS7:
      newsize = 7;
      break;
    case CS8:
      newsize = 8;
      break;
    }

  fdHandle = get_std_handle(fd);
  
  GetCommState (fdHandle, &state);
  ds("First in tcsetattr", &state);
  state.BaudRate = newrate;
  state.ByteSize = newsize;
  state.fBinary = 1;
  state.fParity = 0;
  state.fOutxCtsFlow = 0; /*!!*/
  state.fOutxDsrFlow = 0; /*!!*/
  state.fDsrSensitivity = 0; /*!!*/

  if (t->c_cflag & PARENB) {
    state.Parity = (t->c_cflag & PARODD) ? ODDPARITY:EVENPARITY;
  } else {
    state.Parity = NOPARITY;
  }

  ds("Before SetCommState", &state);  
  SetCommState (fdHandle, &state);
#if 0
  h->r_binary = (t->c_iflag & IGNCR) ? 0 : 1;
  h->w_binary = (t->c_oflag & ONLCR) ? 0 : 1;

  h->vtime = t->c_cc[VTIME];
  h->vmin = t->c_cc[VMIN];
#endif
  memset (&to, 0, sizeof (to));

#if 0
  to.ReadTotalTimeoutConstant = h->vtime * 100;
#endif

  SetCommTimeouts (fdHandle, &to);
  tdump (fd);
  return 0;
}

int
tcgetattr (int fd, struct termios *t)
{
  int res = 0;
  
  if (NOT_OUR_FD (fd))
    {
      errno = EBADF;
      res = -1;
    }
  else
    {
      DCB state;
      int thisspeed;
      int thissize;

      HANDLE fdHandle = get_std_handle(fd);

      GetCommState (fdHandle, &state);
      ds("In tcgetattr", &state);
      switch (state.BaudRate)
	{
	case CBR_110:
	  thisspeed = B110;
	  break;
	case CBR_300:
	  thisspeed = B300;
	  break;
	case CBR_600:
	  thisspeed = B600;
	  break;
	case CBR_1200:
	  thisspeed = B1200;
	  break;
	case CBR_2400:
	  thisspeed = B2400;
	  break;
	case CBR_4800:
	  thisspeed = B4800;
	  break;
	case CBR_9600:
	  thisspeed = B9600;
	  break;
	case CBR_19200:
	  thisspeed = B19200;
	  break;
	case CBR_38400:
	  thisspeed = B38400;
	  break;
	default:
	  thisspeed = B9600;
	  errno = EINVAL;
	}

      switch (state.ByteSize)
	{
	case 5:
	  thissize = CS5;
	  break;
	case 6:
	  thissize = CS6;
	  break;
	case 7:
	  thissize = CS7;
	  break;
	default:
	case 8:
	  thissize = CS8;
	  break;
	}

      memset (t, 0, sizeof (*t));

      t->c_ospeed = t->c_ispeed = thisspeed;
      t->c_cflag |= thissize;
#if 0
      if (!h->r_binary)
	t->c_iflag |= IGNCR;
      if (!h->w_binary)
	t->c_oflag |= ONLCR;

      t->c_cc[VTIME] = h->vtime ;
      t->c_cc[VMIN] = h->vmin;
#endif
    }
  tdump (fd);
  return res;
}

void tcgetpgrp ()
{
}

void tcsetpgrp ()
{
}

void setpgid ()
{
}
