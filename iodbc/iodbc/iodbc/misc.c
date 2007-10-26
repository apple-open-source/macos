/*
 *  misc.c
 *
 *  $Id: misc.c,v 1.23 2006/07/10 13:49:29 source Exp $
 *
 *  Miscellaneous functions
 *
 *  The iODBC driver manager.
 *
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com>
 *  Copyright (C) 1996-2006 by OpenLink Software <iodbc@openlinksw.com>
 *  All Rights Reserved.
 *
 *  This software is released under the terms of either of the following
 *  licenses:
 *
 *      - GNU Library General Public License (see LICENSE.LGPL)
 *      - The BSD License (see LICENSE.BSD).
 *
 *  Note that the only valid version of the LGPL license as far as this
 *  project is concerned is the original GNU Library General Public License
 *  Version 2, dated June 1991.
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
 *  License as published by the Free Software Foundation; only
 *  Version 2 of the License dated June 1991.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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
#include <odbcinst.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <unicode.h>

#include <fcntl.h>
#include <sys/stat.h>

#include "herr.h"
#include "misc.h"
#include "iodbc_misc.h"

#ifdef _MAC
#include <getfpn.h>
#endif /* _MAC */


/*
 *  Parse a configuration from string (internal)
 */
int
_iodbcdm_cfg_parse_str_Internal (PCONFIG pconfig, char *str)
{
  char *s;
  int count;

  /* init image */
  _iodbcdm_cfg_freeimage (pconfig);
  if (str == NULL)
    {
      /* NULL string is ok */
      return 0;
    }
  s = pconfig->image = strdup (str);

  /* Add [ODBC] section */
  if (_iodbcdm_cfg_storeentry (pconfig, "ODBC", NULL, NULL, NULL, 0) == -1)
    return -1;

  for (count = 0; *s; count++)
    {
      char *keywd = NULL, *value;
      char *cp, *n;

      /* 
       *  Extract KEY=VALUE upto first ';'
       */
      for (cp = s; *cp && *cp != ';'; cp++)
	{
	  if (*cp == '{')
	    {
	      for (cp++; *cp && *cp != '}'; cp++)
		;
	    }
	}

      /*
       *  Store start of next token if available in n and terminate string
       */
      if (*cp)
	{
	  *cp = 0;
	  n = cp + 1;
	}
      else
	n = cp;

      /*
       *  Find '=' in string
       */
      for (cp = s; *cp && *cp != '='; cp++)
	;

      if (*cp)
	{
	  *cp++ = 0;
          keywd = s;
          value = cp;
	}
      else if (count == 0)
	{
	  /*
	   *  Handle missing DSN=... from the beginning of the string, e.g.:
	   *  'dsn_ora7;UID=scott;PWD=tiger'
	   */
          keywd = "DSN";
	  value = s;
	}

      if (keywd != NULL)
        {
          /* store entry */
          if (_iodbcdm_cfg_storeentry (pconfig, NULL,
		  keywd, value, NULL, 0) == -1)
            return -1;
	}

      /*
       *  Continue with next token
       */
      s = n;
    }

  /* we're done */
  pconfig->flags |= CFG_VALID;
  pconfig->dirty = 1;
  return 0;
}


/*
 *  Initialize a configuration from string
 */
int
_iodbcdm_cfg_init_str (PCONFIG *ppconf, void *str, int size, int wide)
{
  PCONFIG pconfig;

  *ppconf = NULL;

  /* init config */
  if ((pconfig = (PCONFIG) calloc (1, sizeof (TCONFIG))) == NULL)
    return -1;

  /* parse */
  if (_iodbcdm_cfg_parse_str (pconfig, str, size, wide) == -1)
    {
      _iodbcdm_cfg_done (pconfig);
      return -1;
    }

  /* we're done */
  *ppconf = pconfig;
  return 0;
}


/*
 *  Parse a configuration from string
 */
int
_iodbcdm_cfg_parse_str (PCONFIG pconfig, void *str, int size, int wide)
{
  int ret;
  char *_str;

  _str = wide ? (char *) dm_SQL_WtoU8 (str, size) : str;

  ret = _iodbcdm_cfg_parse_str_Internal (pconfig, _str);

  if (wide)
    MEM_FREE (_str);

  return ret;
}


#define CATBUF(buf, str, buf_sz)					\
  do {									\
    if (_iodbcdm_strlcat (buf, str, buf_sz) >= buf_sz)			\
      return -1;							\
  } while (0)

int
_iodbcdm_cfg_to_string (PCONFIG pconfig, char *section,
			char *buf, size_t buf_sz)
{
  BOOL atsection;

  if (_iodbcdm_cfg_rewind (pconfig) == -1)
    return -1;

  atsection = FALSE;
  buf[0] = '\0';
  while (_iodbcdm_cfg_nextentry (pconfig) == 0)
    {
      if (atsection)
        {
          if (_iodbcdm_cfg_section (pconfig))
            break;
          else if (_iodbcdm_cfg_define (pconfig))
            {
              if (buf[0] != '\0')
                CATBUF (buf, ";", buf_sz);
              CATBUF (buf, pconfig->id, buf_sz);
              CATBUF (buf, "=", buf_sz);
              CATBUF (buf, pconfig->value, buf_sz);
            }
        }
      else if (_iodbcdm_cfg_section (pconfig) &&
	       !strcasecmp (pconfig->section, section))
        atsection = TRUE;
    }
  return 0;
}
