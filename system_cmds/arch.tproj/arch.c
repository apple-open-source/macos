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
 * arch.c - determine the architecture of the machine ran on
 */

#include <stdio.h>
#include <string.h>
#include <mach/mach.h>
#include <mach-o/arch.h>

char *
  get_progname (char *name)
{
  char *tmp;
  if (tmp = strrchr(name,'/'))
    return tmp+1;
  else return name;
}

int 
  main (int argc, char **argv)
{
  char *progname;
  const NXArchInfo *arch;

  if (argc > 1)
    {
      fprintf (stderr,"*error: %s takes no arguments\n",argv[0]);
      exit (-1);
    }
  arch = NXGetLocalArchInfo();
  if (arch == NULL)
    {
      fprintf (stderr,"Unknown architecture.\n");
      exit(-1);
    }
  progname = get_progname(argv[0]);
  if (strcmp (progname,ARCH_PROG) == 0) {
    arch = NXGetArchInfoFromCpuType(arch->cputype, CPU_SUBTYPE_MULTIPLE);
    if (arch == NULL)
    {
	fprintf (stderr,"Unknown architecture.\n");
	exit(-1);
    }
  }
  else if (strcmp (progname,MACHINE_PROG) == 0)
    ;
  else
    {
      fprintf 
	(stderr,"*error: This program must be named either %s or %s\n",ARCH_PROG,MACHINE_PROG);
      exit (-1);
    }
  if (!isatty(fileno(stdin)))
    printf("%s", arch->name);
  else
    printf ("%s\n", arch->name);
    return 0;
}
