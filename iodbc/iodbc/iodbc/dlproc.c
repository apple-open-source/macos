/*
 *  dlproc.c
 *
 *  $Id: dlproc.c,v 1.3 2004/11/11 01:52:37 luesang Exp $
 *
 *  Load driver and resolve driver's function entry point
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com> 
 *  Copyright (C) 1996-2002 by OpenLink Software <iodbc@openlinksw.com>
 *  All Rights Reserved.
 *
 *  This software is released under the terms of either of the following
 *  licenses:
 *
 *      - GNU Library General Public License (see LICENSE.LGPL) 
 *      - The BSD License (see LICENSE.BSD).
 *
 *  While not mandated by the BSD license, any patches you make to the
 *  iODBC source code may be contributed back into the iODBC project
 *  at your discretion. Contributions will benefit the Open Source and
 *  Data Access community as a whole. Submissions may be made at:
 *
 *      http://www.iodbc.org
 *
 *
 *  GNU Library Generic Public License Version 2
 *  ============================================
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  The BSD License
 *  ===============
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. Neither the name of OpenLink Software Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL OPENLINK OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iodbc.h>

#include <sql.h>
#include <sqlext.h>

#include <dlproc.h>

#include <herr.h>
#include <henv.h>
#include <hdbc.h>

#include <itrace.h>

char *odbcapi_symtab[] =
{
    "UNKNOWN FUNCTION"
#define FUNCDEF(A, B, C)	,C
#include "henv.ci"
#undef FUNCDEF
};


HPROC
_iodbcdm_getproc (HDBC hdbc, int idx)
{
  CONN (pdbc, hdbc);
  ENV_t *penv;
  HPROC *phproc;

  if (idx <= 0 || idx >= __LAST_API_FUNCTION__)
    return SQL_NULL_HPROC;

  penv = (ENV_t *) (pdbc->henv);

  if (penv == NULL)
    return SQL_NULL_HPROC;

  phproc = penv->dllproc_tab + idx;

  if (*phproc == SQL_NULL_HPROC)
    *phproc = _iodbcdm_dllproc (penv->hdll, odbcapi_symtab[idx]);

  return *phproc;
}


static dlproc_t *pRoot = NULL;


HDLL
_iodbcdm_dllopen (char *path)
{
  return (HDLL) DLL_OPEN (path);
}


HPROC
_iodbcdm_dllproc (HDLL hdll, char *sym)
{
  return (HPROC) DLL_PROC (hdll, sym);
}


int
_iodbcdm_dllclose (HDLL hdll)
{
  DLL_CLOSE (hdll);

  return 0;
}


char *
_iodbcdm_dllerror ()
{
  return DLL_ERROR ();
}


/* 
 *  If driver manager determines this driver is safe, flag the driver can
 *  be unloaded if not used.
 */
void
_iodbcdm_safe_unload (HDLL hdll)
{
  dlproc_t *pDrv = NULL, *p;

  /*
   *  Find loaded driver
   */
  for (p = pRoot; p; p = p->next)
    {
      if (p->dll == hdll)
	{
	  pDrv = p;
	  break;
	}
    }

  /*
   *  Driver not found
   */
  if (!pDrv)
    return;

  pDrv->safe_unload = 1;
}
