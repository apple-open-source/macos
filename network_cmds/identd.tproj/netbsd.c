/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
**	$Id: netbsd.c,v 1.1.1.2 2000/01/11 01:48:48 wsanchez Exp $
**
** netbsd.c		Low level kernel access functions for NetBSD
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 17 March 1993
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <nlist.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>

#include "kvm.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <sys/socketvar.h>

#define KERNEL

#include <sys/file.h>

#undef KERNEL
#include <sys/sysctl.h>

#include <fcntl.h>

#include <sys/user.h>

#include <sys/wait.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netinet/in_pcb.h>

#include <netinet/tcp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <arpa/inet.h>

#include "identd.h"
#include "error.h"


extern void *calloc();
extern void *malloc();


struct nlist nl[] =
{
#define N_FILE 0
#define N_NFILE 1
#define N_TCB 2
      
  { "_filehead" },
  { "_nfiles" },
  { "_tcb" },
  { "" }
};

static kvm_t *kd;

static struct file *xfile;
static int nfile;

static struct inpcbhead tcb;
  

int k_open()
{
  /*
  ** Open the kernel memory device
  */
  if ((kd = kvm_openfiles(path_unix, path_kmem, NULL, O_RDONLY, "identd")) ==
      NULL)
    ERROR("main: kvm_open");
  
  /*
  ** Extract offsets to the needed variables in the kernel
  */
  if (kvm_nlist(kd, nl) < 0)
    ERROR("main: kvm_nlist");

  return 0;
}


/*
** Get a piece of kernel memory with error handling.
** Returns 1 if call succeeded, else 0 (zero).
*/
static int getbuf(addr, buf, len, what)
  long addr;
  char *buf;
  int len;
  char *what;
{
  if (kvm_read(kd, addr, buf, len) < 0)
  {
    if (syslog_flag)
      syslog(LOG_ERR, "getbuf: kvm_read(%08x, %d) - %s : %m",
	     addr, len, what);

    return 0;
  }
  
  return 1;
}



/*
** Traverse the inpcb list until a match is found.
** Returns NULL if no match.
*/
static struct socket *
    getlist(pcbp_head, faddr, fport, laddr, lport)
  struct inpcbhead *pcbp_head;
  struct in_addr *faddr;
  int fport;
  struct in_addr *laddr;
  int lport;
{
  struct inpcb *pcbp;
  struct inpcb pcb;

  pcbp = pcbp_head->lh_first;
  while (pcbp &&
	   getbuf((long) pcbp,
		  &pcb,
		  sizeof(struct inpcb),
		  "tcblist"))
   {
      if ( pcb.inp_faddr.s_addr == faddr->s_addr &&
	   pcb.inp_laddr.s_addr == laddr->s_addr &&
	   pcb.inp_fport        == fport &&
	   pcb.inp_lport        == lport )
	  return pcb.inp_socket;

      pcbp = pcb.inp_list.le_next;
  } 

  return NULL;
}



/*
** Return the user number for the connection owner
*/
int k_getuid(faddr, fport, laddr, lport, uid)
  struct in_addr *faddr;
  int fport;
  struct in_addr *laddr;
  int lport;
  int *uid;
{
  long addr;
  struct socket *sockp;
  int i, mib[2];
  struct ucred ucb;
  
  /* -------------------- FILE DESCRIPTOR TABLE -------------------- */
  if (!getbuf(nl[N_NFILE].n_value, &nfile, sizeof(nfile), "nfile"))
    return -1;
  
  if (!getbuf(nl[N_FILE].n_value, &addr, sizeof(addr), "&file"))
    return -1;

  {
    int siz, rv;

    mib[0] = CTL_KERN;
    mib[1] = KERN_FILE;
    if ((rv = sysctl(mib, 2, NULL, &siz, NULL, 0)) == -1)
    {
      ERROR1("k_getuid: sysctl 1 (%d)", rv);
      return -1;
    }
    xfile = malloc(siz);
    if (!xfile)
      ERROR1("k_getuid: malloc(%d)", siz);
    if ((rv = sysctl(mib, 2, xfile, &siz, NULL, 0)) == -1)
    {
      ERROR1("k_getuid: sysctl 2 (%d)", rv);
      return -1;
    }
    xfile = (struct file *)((char *)xfile + sizeof(filehead));
  }
  
  /* -------------------- TCP PCB LIST -------------------- */
  if (!getbuf(nl[N_TCB].n_value, &tcb, sizeof(tcb), "tcb"))
    return -1;
  
  sockp = getlist(&tcb, faddr, fport, laddr, lport);
  
  if (!sockp)
    return -1;

  /*
  ** Locate the file descriptor that has the socket in question
  ** open so that we can get the 'ucred' information
  */
  for (i = 0; i < nfile; i++)
  {
    if (xfile[i].f_count == 0)
      continue;
    
    if (xfile[i].f_type == DTYPE_SOCKET &&
	(struct socket *) xfile[i].f_data == sockp)
    {
      if (!getbuf(xfile[i].f_cred, &ucb, sizeof(ucb), "ucb"))
	return -1;

      *uid = ucb.cr_uid;
      return 0;
    }
  }

  return -1;
}

