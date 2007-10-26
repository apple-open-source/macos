/*
 * Copyright (C) 1995-2005 The Free Software Foundation, Inc.
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
 */

/* mkdir.c --- mkdir for Windows NT
   Jim Blandy <jimb@cyclic.com> --- July 1995  */

#include <assert.h>

#include "cvs.h"

#undef mkdir

int
wnt_mkdir (const char *path, int mode)
{
  /* This is true for all extant calls to CVS_MKDIR.  If
     someone adds a call that uses something else later,
     we should tweak this function to handle that.  */
  assert (mode == 0777);

  return mkdir (path);
}
