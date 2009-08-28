/* Mac OS X support for GDB, the GNU debugger.
   Copyright 2007
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#define _DARWIN_C_SOURCE /* for CMSG_LEN */

#include <sys/socket.h>
#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>

#include "defs.h"
#include "target.h"
#include "remote.h"
#include "serial.h"
#include "ser-base.h"
#include "ser-unix.h"

/* This is the "remote-mobile" target ops.  Remote-mobile only differs from
   the ordinary remote target in how we open the connection to the remote
   server.  So the target ops has only a to_open field and all the other function
   fields are NULL.  Once we've gotten the open connection, we'll just call
   push_remote_target to switch to the standard remote target.  */

static struct target_ops remote_mobile_ops;
static char *remote_executable_path;

static char *remote_mobile_shortname = "remote-mobile";
static char *remote_mobile_longname = "Remote connection to a mobile device using gdb-specific protocol";
static char *remote_mobile_doc =     "Connect to a remote mobile device, using a gdb-specific protocol.\n\
Specify the port which we should connect to to receive the filedescriptor for remote connections.";

/* This is standard socket code to open an unnamed UNIX domain socket,
   and connect it to another UNIX domain socket whose name is given in
   NAME.  The opened socket is returned, or -1 on error.  */

static int
open_unix_socket (char *name)
{
  struct sockaddr_un sockaddr;
  int source_fd;
  int len;
  int retval;

  source_fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (source_fd < 0)
    {
      warning ("Couldn't open a unix domain socket: %d.\n", source_fd);
      return -1;
    }

  memset (&sockaddr, 0, sizeof (sockaddr));
  sockaddr.sun_family = AF_UNIX;
  strcpy (sockaddr.sun_path, name);
  len = offsetof (struct sockaddr_un, sun_path) + strlen (name);
  
  retval = connect (source_fd, (struct sockaddr *) &sockaddr, len);
  if (retval == -1)
    {
      warning ("Couldn't connect to unix service: \"%s\", error: \"%s\".",
	       name, strerror(errno));
      close (source_fd);
      return -1;
    }

  return source_fd;
}

/* receive_fd receives a control message on SOURCE_FD containing
   a file descriptor.  It returns the file descriptor, or
   -1 on error.  Right now we do nothing with the actual
   data in the message, though it would be easy enough to 
   do that.  */

static int
receive_fd (int source_fd)
{
  struct msghdr msg;
  static struct cmsghdr *control = NULL;
  struct iovec iov[1];
  char buf[4096];
  int return_fd = -1;
  int nbytes;
  int numerrors = 0;
  int control_len = CMSG_LEN (sizeof (int));

  if (control == NULL)
    control = malloc (control_len);

  for (;;)
    {
      iov[0].iov_base = buf;
      iov[0].iov_len = sizeof(buf);
      msg.msg_iov = iov;
      msg.msg_iovlen = 1;

      msg.msg_name = NULL;
      msg.msg_namelen = 0;

      msg.msg_control = control;
      msg.msg_controllen = control_len;

      nbytes = recvmsg (source_fd, &msg, 0);
      if (nbytes < 0)
	{
	  warning ("Error from recvmsg.\n");
	  numerrors++;
	  if (numerrors < 10)
	    {
	      warning ("More than 10 errors, giving up.");
	      return -1;
	    }
	  else
	    continue;
	}
      else if (nbytes == 0)
	{
	  warning ("Source fd %d closed connection.\n", source_fd);
	  return -1;
	}
      else
	{
	  if (msg.msg_controllen != control_len)
	    {
	      warning ("Message received with wrong size for fd: %d.",
		       msg.msg_controllen);
	      return -1;
	    }
	  return_fd = *((int *) CMSG_DATA (control));
	  break;
	}
    }
   
  
  return return_fd;
}

/* remote_mobile_open opens a socket to the port passed in in NAME,
   and from there we get the file handle that we will really be
   talking to.  Then we switch to the remote target, passing the
   filedescriptor as a "filedesc" type serial device.  */

static void
remote_mobile_open (char *unix_sock_name, int from_tty)
{
  int source_fd;
  int md_fd;
  char *name;

  /* So first we have to get the file descriptor from our provider.  */
  source_fd = open_unix_socket (unix_sock_name);
  if (source_fd <= 0)
      error ("Could not open socket: %s to get mobile device file descriptor.", unix_sock_name);

  md_fd = receive_fd (source_fd);
  close (source_fd);

  if (md_fd < 0)
    error ("Could not get the mobile device fd - error: %d.\n", md_fd); 
     
  /* Now construct the file descriptor target name, and push the remote 
     target.  */
  
  name = malloc (strlen ("filedesc:") + 12);
  sprintf (name, "filedesc:%d", md_fd);
  push_remote_target (name, from_tty);

  /* Now that we've gotten the remote target, let's fix up a few things. */
  current_target.to_shortname = remote_mobile_shortname;
  current_target.to_longname = remote_mobile_longname;
  current_target.to_doc = remote_mobile_doc;
  
}

static void
init_remote_mobile_ops (void)
{
  remote_mobile_ops.to_shortname = remote_mobile_shortname;
  remote_mobile_ops.to_longname = remote_mobile_longname;
  remote_mobile_ops.to_doc = remote_mobile_doc;
  remote_mobile_ops.to_open = remote_mobile_open;
  /* The rest are left NULL since the only thing this target can do is open, then
     it switches to the standard remote target.  */
}

/* This is the serial_ops "open" function for the filedesc
   type.  It corresponds to an already open file descriptor,
   which is encoded in NAME in the form filedesc:fdnum.  So
   we just peel off the fdnum, convert it to an int, and stick
   it in the fd field of SB.  */

static int
filedesc_open (struct serial *sb, const char *name)
{
  char *num_end;
  int fd;

  if (strstr (name, "filedesc:") != name)
    {
      warning ("filedesc_open passed non-filedesc name: \"%s\".", name);
      return -1;
    }

  name += strlen ("filedesc:");

  if (*name == '\0')
    {
      warning ("No file descriptor for filedesc_open.");
      return -1;
    }

  fd = strtol (name, &num_end, 0);
  if (*num_end != '\0')
    {
      warning ("Junk at end of file descriptor: \"%s\".\n", name);
      return -1;
    }

  sb->fd = fd;
  /* Do I need to do this? */
  signal (SIGPIPE, SIG_IGN);

  return 0;

}

/* This is the serial_ops "close" function.  It just closes the file
   descriptor, so we probably don't need it, but just in case we ever
   need to do anything special in the future I put it in here...  */

static void
filedesc_close (struct serial *sb)
{
  close(sb->fd);
}

void
_initialize_remote_mobile (void)
{
  struct serial_ops *ops = xmalloc (sizeof(struct serial_ops));

  init_remote_mobile_ops ();
  add_target (&remote_mobile_ops);

  /* This is the "file handle" serial ops.  The only difference
     from the standard serial ops is that we already have an open
     file handle which gets passed to us from some source that
     can talk to the mobile device framework.  */

  memset (ops, 0, sizeof (struct serial_ops));
  ops->name = "filedesc";
  ops->next = 0;
  ops->open = filedesc_open;
  ops->close = filedesc_close;
  ops->readchar = ser_base_readchar;
  ops->write = ser_base_write;
  ops->flush_output = ser_base_flush_output;
  ops->flush_input = ser_base_flush_input;
  ops->send_break = ser_base_send_break;
  ops->go_raw = ser_base_raw;
  ops->get_tty_state = ser_base_get_tty_state;
  ops->set_tty_state = ser_base_set_tty_state;
  ops->print_tty_state = ser_base_print_tty_state;
  ops->noflush_set_tty_state = ser_base_noflush_set_tty_state;
  ops->setbaudrate = ser_base_setbaudrate;
  ops->setstopbits = ser_base_setstopbits;
  ops->drain_output = ser_base_drain_output;
  ops->async = ser_base_async;
  ops->read_prim = ser_unix_read_prim;
  ops->write_prim = ser_unix_write_prim;
  serial_add_interface (ops);

}
