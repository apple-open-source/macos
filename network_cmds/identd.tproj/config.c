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
**	$Id: config.c,v 1.1.1.1 1999/05/02 03:57:41 wsanchez Exp $
**
** config.c                         This file handles the config file
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 6 Dec 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <stdio.h>
#include <errno.h>

#include "error.h"
#include "identd.h"
#include "xpaths.h"


int parse_config(path, silent_flag)
  char *path;
  int silent_flag;
{
  FILE *fp;

  if (!path)
    path = PATH_CONFIG;
  
  fp = fopen(path, "r");
  if (!fp)
  {
    if (silent_flag)
      return 0;

    ERROR1("error opening %s", path);
  }

  /*
  ** Code should go here to parse the config file data.
  ** For now we just ignore the contents...
  */
  
  
  fclose(fp);
  return 0;
}
