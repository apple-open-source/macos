/*
 *  ConfigDriver.c
 *
 *  $Id: ConfigDriver.c,v 1.10 2006/01/24 11:25:07 source Exp $
 *
 *  The iODBC driver manager.
 *
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
#include <odbcinst.h>
#include <iodbc_error.h>
#include <iodbcadm.h>

#include "gui.h"

BOOL INSTAPI
ConfigDriver (
    HWND	  hwndParent,
    WORD	  fRequest,
    LPCSTR	  lpszDriver,
    LPCSTR	  lpszArgs,
    LPSTR	  lpszMsg,
    WORD	  cbMsgMax,
    WORD	* pcbMsgOut)
{
  char *curr, *cour;
  char driverread[4096] = { 0 };
  BOOL retcode = FALSE;
  UWORD confMode = ODBC_USER_DSN;

  /* Map the request User/System */
  if (fRequest < ODBC_INSTALL_DRIVER || fRequest > ODBC_CONFIG_DRIVER_MAX)
    {
      SQLPostInstallerError (ODBC_ERROR_INVALID_REQUEST_TYPE, NULL);
      goto done;
    }

  if (!lpszDriver || !STRLEN (lpszDriver))
    {
      SQLPostInstallerError (ODBC_ERROR_INVALID_NAME, NULL);
      goto done;
    }

  /* Retrieve the config mode */
  SQLGetConfigMode (&confMode);

  /* Treat corresponding to the request */
  switch (fRequest)
    {
    case ODBC_INSTALL_DRIVER:
      /* Check if the DRIVER with this name already exists */
      SQLSetConfigMode (confMode);
#ifdef WIN32
      if (hwndParent
	  && SQLGetPrivateProfileString ("ODBC 32 bit Drivers", lpszDriver,
	      "", driverread, sizeof (driverread), "odbcinst.ini")
	  && !create_confirm (hwndParent, NULL,
	      "Are you sure you want to overwrite this driver ?"))
#else
#  ifdef _MACX
      if (hwndParent
	  && SQLGetPrivateProfileString ("ODBC Drivers", lpszDriver, "",
	      driverread, sizeof (driverread), "odbcinst.ini")
	  && !create_confirm (hwndParent, NULL,
	      "Are you sure you want to overwrite this driver ?"))
#  else
      if (hwndParent
	  && SQLGetPrivateProfileString ("ODBC Drivers", lpszDriver, "",
	      driverread, sizeof (driverread), "odbcinst.ini")
	  && !create_confirm (hwndParent, NULL,
	      "Are you sure you want to overwrite this driver ?"))
#  endif
#endif
	{
	  SQLPostInstallerError (ODBC_ERROR_DRIVER_SPECIFIC,
	      "Driver already installed previously.");
	  goto done;
	}

      /* Add the Driver to the ODBC Drivers */
      SQLSetConfigMode (confMode);
      if (!SQLInstallDriverEx (lpszArgs, NULL, driverread,
	      sizeof (driverread), NULL, ODBC_INSTALL_COMPLETE, NULL))
	{
	  SQLPostInstallerError (ODBC_ERROR_DRIVER_SPECIFIC,
	      "Could not add the driver informations.");
	  goto done;
	}

      break;

    case ODBC_CONFIG_DRIVER:
      if (!lpszArgs || !STRLEN (lpszArgs))
	{
	  SQLPostInstallerError (ODBC_ERROR_DRIVER_SPECIFIC,
	      "No enough parameters for configururation.");
	  goto done;
	}

      /* Add each keyword and values */
      for (curr = (LPSTR) lpszArgs; *curr; curr += (STRLEN (curr) + 1))
	{
	  STRCPY (driverread, curr);
	  cour = strchr (driverread, '=');
	  if (cour)
	    *cour = 0;
	  SQLSetConfigMode (confMode);
	  if (!SQLWritePrivateProfileString (lpszDriver, driverread, (cour
		      && STRLEN (cour + 1)) ? cour + 1 : NULL,
		  "odbcinst.ini"))
	    goto done;
	}
      break;

    case ODBC_REMOVE_DRIVER:
      /* Remove the Driver to the ODBC Drivers */
      SQLSetConfigMode (confMode);
      if (!SQLRemoveDriver (lpszDriver, TRUE, NULL))
	{
	  SQLPostInstallerError (ODBC_ERROR_DRIVER_SPECIFIC,
	      "Could not remove driver informations.");
	  goto done;
	}
      break;

    default:
      SQLPostInstallerError (ODBC_ERROR_REQUEST_FAILED, NULL);
      goto done;
    };

quit:
  retcode = TRUE;

done:
  if (pcbMsgOut)
    *pcbMsgOut = 0;
  return retcode;
}
