/*
 *  dsnchooser.c
 *
 *  $Id: dsnchooser.c,v 1.3 2004/11/11 01:52:39 luesang Exp $
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1999-2002 by OpenLink Software <iodbc@openlinksw.com>
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <errno.h>

#include "../gui.h"
#include "odbc4.xpm"


#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

/*
 *  Linux glibc has the function, but does not have the prototype
 *  with the default header files.
 */
#if defined(HAVE_ASPRINTF) && defined(__GLIBC__)
extern int asprintf (char **ret, const char *format, ...);
#endif


char *szDSNColumnNames[] = {
  "Name",
  "Description",
  "Driver"
};

char *szTabNames[] = {
  "User DSN",
  "System DSN",
  "File DSN",
  "ODBC Drivers",
  "Connection Pooling",
  "Tracing",
  "About"
};

char *szDSNButtons[] = {
  "_Add",
  "_Remove",
  "Confi_gure",
  "_Test",
};


void
addlistofdir_to_optionmenu (GtkWidget *widget, LPCSTR path,
    TDSNCHOOSER *choose_t)
{
  GtkWidget *menu, *menu_item;
  char *curr_dir, *prov, *dir;
  void **array;

  if (!path || !GTK_IS_OPTION_MENU (widget) || !(prov = strdup (path)))
    return;

  if (prov[STRLEN (prov) - 1] == '/' && STRLEN (prov) > 1)
    prov[STRLEN (prov) - 1] = 0;

  /* Add the root directory */
  menu = gtk_menu_new ();
  menu_item = gtk_menu_item_new_with_label ("/");
  gtk_widget_show (menu_item);
  gtk_menu_prepend (GTK_MENU (menu), menu_item);
  if (!(array = (void **) malloc (sizeof (void *) * 2)))
    return;
  array[0] = g_strdup ("/");
  array[1] = choose_t;
  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
      GTK_SIGNAL_FUNC (filedsn_lookin_clicked), array);

  for (curr_dir = prov, dir = NULL; curr_dir;
      curr_dir = strchr (curr_dir + 1, '/'))
    {
      if (strchr (curr_dir + 1, '/'))
	{
	  dir = strchr (curr_dir + 1, '/');
	  *dir = 0;
	}

      menu_item = gtk_menu_item_new_with_label (prov);
      gtk_widget_show (menu_item);
      gtk_menu_prepend (GTK_MENU (menu), menu_item);
      if (!(array = (void **) malloc (sizeof (void *) * 2)))
	return;
      array[0] = g_strdup (prov);
      array[1] = choose_t;
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
	  GTK_SIGNAL_FUNC (filedsn_lookin_clicked), array);
      if (dir)
	*dir = '/';
    }

  gtk_option_menu_remove_menu (GTK_OPTION_MENU (widget));
  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);

  free (prov);
}


void
adddirectories_to_list (HWND hwnd, GtkWidget *widget, LPCSTR path)
{
  DIR *dir;
#if defined (HAVE_ASPRINTF)
  char *path_buf;
#else
  char path_buf[MAXPATHLEN];
#endif
  struct dirent *dir_entry;
  struct stat fstat;
  char *data[1];

  if (!path || !GTK_IS_CLIST (widget))
    return;

  if ((dir = opendir (path)))
    {
      /* Remove all the entries */
      gtk_clist_clear (GTK_CLIST (widget));

      while ((dir_entry = readdir (dir)))
	{
#if defined (HAVE_ASPRINTF)
	  asprintf (&path_buf, "%s/%s", path, dir_entry->d_name);
#elif defined (HAVE_SNPRINTF)
	  snprintf (path_buf, sizeof (path_buf), "%s/%s",
	      path, dir_entry->d_name);
#else
	  STRCPY (path_buf, path);
	  if (path[STRLEN (path) - 1] != '/')
	    STRCAT (path_buf, "/");
	  STRCAT (path_buf, dir_entry->d_name);
#endif

	  if (stat ((LPCSTR) path_buf, &fstat) >= 0
	      && S_ISDIR (fstat.st_mode))
	    if (strcmp (path, "/") || strcmp (dir_entry->d_name, ".."))
	      {
		data[0] = dir_entry->d_name;
		gtk_clist_append (GTK_CLIST (widget), data);
	      }

#if defined (HAVE_ASPRINTF)
	  free (path_buf);
#endif
	}

      /* Close the directory entry */
      closedir (dir);

      if (GTK_CLIST (widget)->rows > 0)
	gtk_clist_sort (GTK_CLIST (widget));
    }
  else
    create_error (hwnd, NULL, "Error during accessing directory information",
	strerror (errno));
}


void
addfiles_to_list (HWND hwnd, GtkWidget *widget, LPCSTR path)
{
  DIR *dir;
#if defined (HAVE_ASPRINTF)
  char *path_buf;
#else
  char path_buf[MAXPATHLEN];
#endif
  struct dirent *dir_entry;
  struct stat fstat;
  char *data[1];

  if (!path || !GTK_IS_CLIST (widget))
    return;

  if ((dir = opendir (path)))
    {
      /* Remove all the entries */
      gtk_clist_clear (GTK_CLIST (widget));

      while ((dir_entry = readdir (dir)))
	{
#if defined (HAVE_ASPRINTF)
	  asprintf (&path_buf, "%s/%s", path, dir_entry->d_name);
#elif defined (HAVE_SNPRINTF)
	  snprintf (path_buf, sizeof (path_buf), "%s/%s",
	      path, dir_entry->d_name);
#else
	  STRCPY (path_buf, path);
	  if (path[STRLEN (path) - 1] != '/')
	    STRCAT (path_buf, "/");
	  STRCAT (path_buf, dir_entry->d_name);
#endif

	  if (stat ((LPCSTR) path_buf, &fstat) >= 0
	      && !S_ISDIR (fstat.st_mode)
	      && strstr (dir_entry->d_name, ".dsn"))
	    {
	      data[0] = dir_entry->d_name;
	      gtk_clist_append (GTK_CLIST (widget), data);
	    }

#if defined (HAVE_ASPRINTF)
	  free (path_buf);
#endif
	}

      /* Close the directory entry */
      closedir (dir);

      if (GTK_CLIST (widget)->rows > 0)
	gtk_clist_sort (GTK_CLIST (widget));
    }
  else
    create_error (hwnd, NULL, "Error during accessing directory information",
	strerror (errno));
}


void
adddsns_to_list (GtkWidget *widget, BOOL systemDSN)
{
  char *curr, *buffer = (char *) malloc (sizeof (char) * 65536);
  char diz[1024], driver[1024];
  char *data[3];
  int len, _case = 0;

  if (!buffer || !GTK_IS_CLIST (widget))
    return;
  gtk_clist_clear (GTK_CLIST (widget));

  /* Get the list of DSN in the user context */
  SQLSetConfigMode (systemDSN ? ODBC_SYSTEM_DSN : ODBC_USER_DSN);
  len =
      SQLGetPrivateProfileString ("ODBC Data Sources", NULL, "", buffer,
      65536, NULL);
  if (len)
    goto process;

  _case = 1;
  SQLSetConfigMode (systemDSN ? ODBC_SYSTEM_DSN : ODBC_USER_DSN);
  len =
      SQLGetPrivateProfileString ("ODBC 32 bit Data Sources", NULL, "",
      buffer, 65536, NULL);
  if (len)
    goto process;

  goto end;

process:
  for (curr = buffer; *curr; curr += (STRLEN (curr) + 1))
    {
      SQLSetConfigMode (systemDSN ? ODBC_SYSTEM_DSN : ODBC_USER_DSN);
      SQLGetPrivateProfileString (curr, "Description", "", diz, sizeof (diz),
	  NULL);
      SQLSetConfigMode (systemDSN ? ODBC_SYSTEM_DSN : ODBC_USER_DSN);
      switch (_case)
	{
	case 0:
	  SQLGetPrivateProfileString ("ODBC Data Sources", curr, "", driver,
	      sizeof (driver), NULL);
	  break;
	case 1:
	  SQLGetPrivateProfileString ("ODBC 32 bit Data Sources", curr, "",
	      driver, sizeof (driver), NULL);
	  break;
	};

      if (STRLEN (curr) && STRLEN (driver))
	{
	  data[0] = curr;
	  data[1] = diz;
	  data[2] = driver;
	  gtk_clist_append (GTK_CLIST (widget), data);
	}
    }

end:
  SQLSetConfigMode (ODBC_BOTH_DSN);

  if (GTK_CLIST (widget)->rows > 0)
    {
      gtk_clist_columns_autosize (GTK_CLIST (widget));
      gtk_clist_sort (GTK_CLIST (widget));
    }

  /* Make the clean up */
  free (buffer);
}


static void
dsnchooser_switch_page (GtkNotebook *notebook, GtkNotebookPage *page,
    gint page_num, TDSNCHOOSER *choose_t)
{
  switch (page_num)
    {
    case 0:
      if (choose_t)
	    {
	      choose_t->type_dsn = 0;
	      adddsns_to_list (choose_t->udsnlist, FALSE);
		 }
      break;

    case 1:
      if (choose_t)
	    {
	      choose_t->type_dsn = 1;
	      adddsns_to_list (choose_t->sdsnlist, TRUE);
		 }
      break;

    case 2:
      if (choose_t)
	    {
	      choose_t->type_dsn = 2;
		 }
      break;
    };

      if (choose_t)
    {
      if (choose_t->uremove)
	    gtk_widget_set_sensitive (choose_t->uremove, FALSE);
      if (choose_t->uconfigure)
	    gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
      if (choose_t->utest)
	    gtk_widget_set_sensitive (choose_t->utest, FALSE);
      if (choose_t->sremove)
	    gtk_widget_set_sensitive (choose_t->sremove, FALSE);
      if (choose_t->sconfigure)
	    gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
      if (choose_t->stest)
	    gtk_widget_set_sensitive (choose_t->stest, FALSE);
    }
}


void
userdsn_add_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char connstr[4096] = { 0 };
  char drv[1024] = { 0 };
  int sqlstat;
  DWORD error;

  if (choose_t)
    {
      /* Try first to get the driver name */
      SQLSetConfigMode (ODBC_USER_DSN);
      if (_iodbcdm_drvchoose_dialbox (choose_t->mainwnd, drv, sizeof (drv),
	      &sqlstat) == SQL_SUCCESS)
	{
	  SQLSetConfigMode (ODBC_USER_DSN);
	  if (!SQLConfigDataSource (choose_t->mainwnd, ODBC_ADD_DSN,
		  drv + STRLEN ("DRIVER="), connstr))
	    {
	      if (SQLInstallerError (1, &error, connstr, sizeof (connstr),
		      NULL) != SQL_NO_DATA)
		_iodbcdm_errorbox (choose_t->mainwnd, NULL,
		    "An error occured when trying to add the DSN : ");
	      goto done;
	    }

	  adddsns_to_list (choose_t->udsnlist, FALSE);
	}

    done:
      if (GTK_CLIST (choose_t->udsnlist)->selection == NULL)
	{
	  if (choose_t->uremove)
	    gtk_widget_set_sensitive (choose_t->uremove, FALSE);
	  if (choose_t->uconfigure)
	    gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
	  if (choose_t->utest)
	    gtk_widget_set_sensitive (choose_t->utest, FALSE);
	}
    }

  return;
}


void
userdsn_remove_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char dsn[1024] = { 0 };
  char *szDSN = NULL, *szDriver = NULL;

  if (choose_t)
    {
      /* Retrieve the DSN name */
      if (GTK_CLIST (choose_t->udsnlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->
		  data), 0, &szDSN);
	  gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->
		  data), 2, &szDriver);
	}

      /* Call the right function */
      if (szDSN
	  && create_confirm (choose_t->mainwnd, szDSN,
	      "Are you sure you want to remove this DSN ?"))
	{
	  sprintf (dsn, "DSN=%s", szDSN);
	  if (!SQLConfigDataSource (choose_t->mainwnd, ODBC_REMOVE_DSN,
		  szDriver, dsn))
	    _iodbcdm_errorbox (choose_t->mainwnd, szDSN,
		"An error occured when trying to remove the DSN : ");
	  adddsns_to_list (choose_t->udsnlist, FALSE);
	}

      if (GTK_CLIST (choose_t->udsnlist)->selection == NULL)
	{
	  if (choose_t->uremove)
	    gtk_widget_set_sensitive (choose_t->uremove, FALSE);
	  if (choose_t->uconfigure)
	    gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
	  if (choose_t->utest)
	    gtk_widget_set_sensitive (choose_t->utest, FALSE);
	}
    }
}


void
userdsn_configure_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char connstr[4096] = { 0 };
  char tokenstr[4096] = { 0 };
  char *szDSN = NULL, *szDriver = NULL, *curr, *cour;
  int size = sizeof (connstr);
  DWORD error;

  if (choose_t)
    {
      /* Retrieve the DSN name */
      if (GTK_CLIST (choose_t->udsnlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->
		  data), 0, &szDSN);
	  gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->
		  data), 2, &szDriver);
	}

      /* Call the right function */
      if (szDSN)
	{
	  sprintf (connstr, "DSN=%s", szDSN);
	  size -= (STRLEN (connstr) + 1);

	  SQLSetConfigMode (ODBC_USER_DSN);
	  if (!SQLGetPrivateProfileString (szDSN, NULL, "", tokenstr,
		  sizeof (tokenstr), NULL))
	    {
	      _iodbcdm_errorbox (choose_t->mainwnd, szDSN,
		  "An error occured when trying to configure the DSN : ");
	      goto done;
	    }

	  for (curr = tokenstr, cour = connstr + STRLEN (connstr) + 1; *curr;
	      curr += (STRLEN (curr) + 1), cour += (STRLEN (cour) + 1))
	    {
	      STRCPY (cour, curr);
	      cour[STRLEN (curr)] = '=';
	      SQLSetConfigMode (ODBC_USER_DSN);
	      SQLGetPrivateProfileString (szDSN, curr, "",
		  cour + STRLEN (curr) + 1, size - STRLEN (curr) - 1, NULL);
	      size -= (STRLEN (cour) + 1);
	    }

	  *cour = 0;

	  SQLSetConfigMode (ODBC_USER_DSN);
	  if (!SQLConfigDataSource (choose_t->mainwnd, ODBC_CONFIG_DSN,
		  szDriver, connstr))
	    {
	      if (SQLInstallerError (1, &error, connstr, sizeof (connstr),
		      NULL) != SQL_NO_DATA
		  && error != ODBC_ERROR_REQUEST_FAILED)
		_iodbcdm_errorbox (choose_t->mainwnd, szDSN,
		    "An error occured when trying to configure the DSN : ");
	      goto done;
	    }

	  adddsns_to_list (choose_t->udsnlist, FALSE);
	}

    done:
      if (GTK_CLIST (choose_t->udsnlist)->selection == NULL)
	{
	  if (choose_t->uremove)
	    gtk_widget_set_sensitive (choose_t->uremove, FALSE);
	  if (choose_t->uconfigure)
	    gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
	  if (choose_t->utest)
	    gtk_widget_set_sensitive (choose_t->utest, FALSE);
	}
    }

  return;
}


void
userdsn_test_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  HENV henv;
  HDBC hdbc;
  SWORD buflen;
  char connstr[4096] = { 0 };
  char outconnstr[4096] = { 0 };
  char *szDSN;

  if (choose_t)
    {
      /* Retrieve the DSN name */
      if (GTK_CLIST (choose_t->udsnlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->data),
	    0, &szDSN);

      if (szDSN && STRLEN (szDSN))
	{
#if (ODBCVER < 0x300)
	  if (SQLAllocEnv (&henv) != SQL_SUCCESS)
#else
	  if (SQLAllocHandle (SQL_HANDLE_ENV, NULL, &henv) != SQL_SUCCESS)
#endif
	    {
	      _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			  henv, SQL_NULL_HDBC, SQL_NULL_HSTMT);
	      return;
	    }

#if (ODBCVER < 0x300)
	  if (SQLAllocConnect (henv, &hdbc) != SQL_SUCCESS)
#else
	  SQLSetEnvAttr (henv, SQL_ATTR_ODBC_VERSION,
	      (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
	  if (SQLAllocHandle (SQL_HANDLE_DBC, henv, &hdbc) != SQL_SUCCESS)
#endif
	    {
	      _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			  henv, hdbc, SQL_NULL_HSTMT);
	      SQLFreeEnv (henv);
	      return;
	    }

	  sprintf (connstr, "DSN=%s", szDSN);

	  SQLSetConfigMode (ODBC_USER_DSN);
	  if (SQLDriverConnect (hdbc, choose_t->mainwnd, connstr, SQL_NTS,
		  outconnstr, sizeof (outconnstr), &buflen,
		  SQL_DRIVER_PROMPT) != SQL_SUCCESS)
	    _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			 henv, hdbc, SQL_NULL_HSTMT);
	  else
	    {
	      _iodbcdm_messagebox (choose_t->mainwnd, szDSN,
		  "The connection DSN was tested successfully, and can be used at this time.");
	      SQLDisconnect (hdbc);
	    }

#if (ODBCVER < 0x300)
	  SQLFreeConnect (hdbc);
	  SQLFreeEnv (henv);
#else
	  SQLFreeHandle (SQL_HANDLE_DBC, hdbc);
	  SQLFreeHandle (SQL_HANDLE_ENV, henv);
#endif
	}

      if (GTK_CLIST (choose_t->udsnlist)->selection == NULL)
	{
	  if (choose_t->uremove)
	    gtk_widget_set_sensitive (choose_t->uremove, FALSE);
	  if (choose_t->uconfigure)
	    gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
	  if (choose_t->utest)
	    gtk_widget_set_sensitive (choose_t->utest, FALSE);
	}

    }
}


void
systemdsn_add_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char connstr[4096] = { 0 };
  char drv[1024] = { 0 };
  int sqlstat;
  DWORD error;

  if (choose_t)
    {
      /* Try first to get the driver name */
      SQLSetConfigMode (ODBC_SYSTEM_DSN);
      if (_iodbcdm_drvchoose_dialbox (choose_t->mainwnd, drv, sizeof (drv),
	      &sqlstat) == SQL_SUCCESS)
	{
	  if (!SQLConfigDataSource (choose_t->mainwnd, ODBC_ADD_SYS_DSN,
		  drv + STRLEN ("DRIVER="), connstr))
	    {
	      if (SQLInstallerError (1, &error, connstr, sizeof (connstr),
		      NULL) != SQL_NO_DATA)
		_iodbcdm_errorbox (choose_t->mainwnd, NULL,
		    "An error occured when trying to add the DSN : ");
	      goto done;
	    }

	  adddsns_to_list (choose_t->sdsnlist, TRUE);
	}

    done:
      if (GTK_CLIST (choose_t->sdsnlist)->selection == NULL)
	{
	  if (choose_t->sremove)
	    gtk_widget_set_sensitive (choose_t->sremove, FALSE);
	  if (choose_t->sconfigure)
	    gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
	  if (choose_t->stest)
	    gtk_widget_set_sensitive (choose_t->stest, FALSE);
	}
    }

  return;
}


void
systemdsn_remove_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char dsn[1024] = { 0 };
  char *szDSN = NULL, *szDriver = NULL;

  if (choose_t)
    {
      /* Retrieve the DSN name */
      if (GTK_CLIST (choose_t->sdsnlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->
		  data), 0, &szDSN);
	  gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->
		  data), 2, &szDriver);
	}

      /* Call the right function */
      if (szDSN
	  && create_confirm (choose_t->mainwnd, szDSN,
	      "Are you sure you want to remove this DSN ?"))
	{
	  sprintf (dsn, "DSN=%s", szDSN);
	  if (!SQLConfigDataSource (choose_t->mainwnd, ODBC_REMOVE_SYS_DSN,
		  szDriver, dsn))
	    _iodbcdm_errorbox (choose_t->mainwnd, szDSN,
		"An error occured when trying to remove the DSN : ");
	  adddsns_to_list (choose_t->sdsnlist, TRUE);
	}

      if (GTK_CLIST (choose_t->sdsnlist)->selection == NULL)
	{
	  if (choose_t->sremove)
	    gtk_widget_set_sensitive (choose_t->sremove, FALSE);
	  if (choose_t->sconfigure)
	    gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
	  if (choose_t->stest)
	    gtk_widget_set_sensitive (choose_t->stest, FALSE);
	}
    }
}


void
systemdsn_configure_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char connstr[4096] = { 0 };
  char tokenstr[4096] = { 0 };
  char *szDSN = NULL, *szDriver = NULL, *curr, *cour;
  int size = sizeof (connstr);
  DWORD error;

  if (choose_t)
    {
      /* Retrieve the DSN name */
      if (GTK_CLIST (choose_t->sdsnlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->
		  data), 0, &szDSN);
	  gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->
		  data), 2, &szDriver);
	}

      /* Call the right function */
      if (szDSN)
	{
	  sprintf (connstr, "DSN=%s", szDSN);
	  size -= (STRLEN (connstr) + 1);

	  SQLSetConfigMode (ODBC_SYSTEM_DSN);
	  if (!SQLGetPrivateProfileString (szDSN, NULL, "", tokenstr,
		  sizeof (tokenstr), NULL))
	    {
	      _iodbcdm_errorbox (choose_t->mainwnd, szDSN,
		  "An error occured when trying to configure the DSN : ");
	      goto done;
	    }

	  for (curr = tokenstr, cour = connstr + STRLEN (connstr) + 1; *curr;
	      curr += (STRLEN (curr) + 1), cour += (STRLEN (cour) + 1))
	    {
	      STRCPY (cour, curr);
	      cour[STRLEN (curr)] = '=';
	      SQLSetConfigMode (ODBC_SYSTEM_DSN);
	      SQLGetPrivateProfileString (szDSN, curr, "",
		  cour + STRLEN (curr) + 1, size - STRLEN (curr) - 1, NULL);
	      size -= (STRLEN (cour) + 1);
	    }

	  *cour = 0;

	  if (!SQLConfigDataSource (choose_t->mainwnd, ODBC_CONFIG_SYS_DSN,
		  szDriver, connstr))
	    {
	      if (SQLInstallerError (1, &error, connstr, sizeof (connstr),
		      NULL) != SQL_NO_DATA
		  && error != ODBC_ERROR_REQUEST_FAILED)
		_iodbcdm_errorbox (choose_t->mainwnd, szDSN,
		    "An error occured when trying to configure the DSN : ");
	      goto done;
	    }

	  adddsns_to_list (choose_t->sdsnlist, TRUE);
	}

    done:
      if (GTK_CLIST (choose_t->sdsnlist)->selection == NULL)
	{
	  if (choose_t->sremove)
	    gtk_widget_set_sensitive (choose_t->sremove, FALSE);
	  if (choose_t->sconfigure)
	    gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
	  if (choose_t->stest)
	    gtk_widget_set_sensitive (choose_t->stest, FALSE);
	}
    }

  return;
}


void
systemdsn_test_clicked (GtkWidget * widget, TDSNCHOOSER * choose_t)
{
  HENV henv;
  HDBC hdbc;
  SWORD buflen;
  SQLCHAR connstr[4096] = { 0 };
  SQLCHAR outconnstr[4096] = { 0 };
  char *szDSN = NULL;

  if (choose_t)
    {
      /* Retrieve the DSN name */
      if (GTK_CLIST (choose_t->sdsnlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->data),
	    0, &szDSN);

      if (szDSN && STRLEN (szDSN))
	{
#if (ODBCVER < 0x300)
	  if (SQLAllocEnv (&henv) != SQL_SUCCESS)
#else
	  if (SQLAllocHandle (SQL_HANDLE_ENV, NULL, &henv) != SQL_SUCCESS)
#endif
	    {
	      _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			  henv, SQL_NULL_HDBC, SQL_NULL_HSTMT);
	      return;
	    }

#if (ODBCVER < 0x300)
	  if (SQLAllocConnect (henv, &hdbc) != SQL_SUCCESS)
#else
	  SQLSetEnvAttr (henv, SQL_ATTR_ODBC_VERSION,
	      (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
	  if (SQLAllocHandle (SQL_HANDLE_DBC, henv, &hdbc) != SQL_SUCCESS)
#endif
	    {
	      _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			  henv, hdbc, SQL_NULL_HSTMT);
	      SQLFreeEnv (henv);
	      return;
	    }

	  sprintf (connstr, "DSN=%s", szDSN);

	  SQLSetConfigMode (ODBC_SYSTEM_DSN);
	  if (SQLDriverConnect (hdbc, choose_t->mainwnd, connstr, SQL_NTS,
		  outconnstr, sizeof (outconnstr), &buflen,
		  SQL_DRIVER_PROMPT) != SQL_SUCCESS)
	    _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			 henv, hdbc, SQL_NULL_HSTMT);
	  else
	    {
	      _iodbcdm_messagebox (choose_t->mainwnd, szDSN,
		  "The connection DSN was tested successfully, and can be used at this time.");
	      SQLDisconnect (hdbc);
	    }

#if (ODBCVER < 0x300)
	  SQLFreeConnect (hdbc);
	  SQLFreeEnv (henv);
#else
	  SQLFreeHandle (SQL_HANDLE_DBC, hdbc);
	  SQLFreeHandle (SQL_HANDLE_ENV, henv);
#endif
	}

      if (GTK_CLIST (choose_t->sdsnlist)->selection == NULL)
	{
	  if (choose_t->sremove)
	    gtk_widget_set_sensitive (choose_t->sremove, FALSE);
	  if (choose_t->sconfigure)
	    gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
	  if (choose_t->stest)
	    gtk_widget_set_sensitive (choose_t->stest, FALSE);
	}

    }
}


void
filedsn_add_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  HENV henv;
  HDBC hdbc;
  SQLCHAR connstr[4096] = { 0 };
  SQLCHAR drv[1024] = { 0 };
  SQLCHAR outconnstr[4096] = { 0 };
  SWORD buflen;
  int sqlstat;
  /* DWORD error; */
  LPSTR dsn = NULL;

  if (choose_t)
    {
      /* Try first to get the driver name */
      SQLSetConfigMode (ODBC_USER_DSN);
      if (_iodbcdm_drvchoose_dialbox (choose_t->mainwnd, drv, sizeof (drv),
	      &sqlstat) == SQL_SUCCESS)
	{
	  /* First get the DSN name */
	  do
	    {
	      dsn = (LPSTR) create_filedsn ((HWND) (choose_t->mainwnd));
	    }
	  while (dsn && !SQLValidDSN (dsn));

	  if (!dsn)
	    goto quit;

	  /* Build the string connection */
	  sprintf (connstr, "DSN=%s;%s;SAVEFILE=%s/%s.dsn", dsn, drv,
	      choose_t->curr_dir, dsn);
	  free (dsn);

	  if (SQLAllocEnv (&henv) != SQL_SUCCESS)
	    {
	      _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			  henv, SQL_NULL_HDBC, SQL_NULL_HSTMT);
	      return;
	    }

	  if (SQLAllocConnect (henv, &hdbc) != SQL_SUCCESS)
	    {
	      _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			  henv, hdbc, SQL_NULL_HSTMT);
	      SQLFreeEnv (henv);
	      return;
	    }

	  if (SQLDriverConnect (hdbc, choose_t->mainwnd, connstr, SQL_NTS,
		  outconnstr, sizeof (outconnstr), &buflen,
		  SQL_DRIVER_PROMPT) != SQL_SUCCESS)
	    _iodbcdm_nativeerrorbox (choose_t->mainwnd,
			 henv, hdbc, SQL_NULL_HSTMT);
	  else
	    SQLDisconnect (hdbc);

	  SQLFreeConnect (hdbc);
	  SQLFreeEnv (henv);

	  /*SQLSetConfigMode(ODBC_SYSTEM_DSN);
	     if( !SQLConfigDataSource(choose_t->mainwnd, ODBC_ADD_DSN, drv+STRLEN("DRIVER="), "") )
	     {
	     if( SQLInstallerError(1, &error, connstr, sizeof(connstr), NULL) != SQL_NO_DATA && error!=ODBC_ERROR_REQUEST_FAILED)
	     _iodbcdm_errorbox(choose_t->mainwnd, NULL, "An error occured when trying to add the DSN : ");
	     goto done;
	     }

	     adddsns_to_list(choose_t->sdsnlist, TRUE); */

	  if (choose_t->uremove)
	    gtk_widget_set_sensitive (choose_t->uremove, FALSE);
	  if (choose_t->uconfigure)
	    gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
	  if (choose_t->utest)
	    gtk_widget_set_sensitive (choose_t->utest, FALSE);
	  if (choose_t->sremove)
	    gtk_widget_set_sensitive (choose_t->sremove, FALSE);
	  if (choose_t->sconfigure)
	    gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
	  if (choose_t->stest)
	    gtk_widget_set_sensitive (choose_t->stest, FALSE);
	  /*if(choose_t->fremove) gtk_widget_set_sensitive(choose_t->fremove,FALSE);
	     if(choose_t->fconfigure) gtk_widget_set_sensitive(choose_t->fconfigure,FALSE);
	     if(choose_t->ftest) gtk_widget_set_sensitive(choose_t->ftest,FALSE); */
	}
    }

quit:
  return;
}


void
filedsn_remove_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
}


void
filedsn_configure_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
}


void
filedsn_test_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
}


void
filedsn_filelist_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  LPSTR filename = NULL, temp = NULL;

  if (choose_t)
    {
      /* Get the file name */
      gtk_clist_get_text (GTK_CLIST (choose_t->file_list), row, 0, &filename);

      /* Update the directory and file list */
      temp =
	  (LPSTR) malloc (STRLEN (filename) + STRLEN (choose_t->curr_dir) +
	  2);

      if (temp)
	{
	  STRCPY (temp, choose_t->curr_dir);
	  if (temp[STRLEN (temp) - 1] != '/')
	    STRCAT (temp, "/");
	  STRCAT (temp, filename);

	  /* Check if it's a valid file */
	  gtk_entry_set_text (GTK_ENTRY (choose_t->file_entry), temp);

	  /* And activate buttons */
	  if (choose_t->fremove)
	    gtk_widget_set_sensitive (choose_t->fremove, TRUE);
	  if (choose_t->fconfigure)
	    gtk_widget_set_sensitive (choose_t->fconfigure, TRUE);
	  if (choose_t->ftest)
	    gtk_widget_set_sensitive (choose_t->ftest, TRUE);

	  free (temp);
	}
    }
}


void
filedsn_filelist_unselect (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  if (choose_t)
    {
      /* Check if it's a valid file */
      gtk_entry_set_text (GTK_ENTRY (choose_t->file_entry), "");

      /* And des-activate buttons */
      if (choose_t->fremove)
	gtk_widget_set_sensitive (choose_t->fremove, FALSE);
      if (choose_t->fconfigure)
	gtk_widget_set_sensitive (choose_t->fconfigure, FALSE);
      if (choose_t->ftest)
	gtk_widget_set_sensitive (choose_t->ftest, FALSE);
    }
}


void
filedsn_dirlist_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  LPSTR filename = NULL, temp = NULL;
  int i;

  if (choose_t)
    {
      /* Get the directory name */
      gtk_clist_get_text (GTK_CLIST (choose_t->dir_list), row, 0, &filename);

      if (filename && event && event->type == GDK_2BUTTON_PRESS)
	{
	  /* Update the directory and file list */
	  temp =
	      (LPSTR) malloc (STRLEN (filename) +
	      STRLEN (choose_t->curr_dir) + 2);

	  if (temp)
	    {
	      if (!strcmp (filename, "."))
		STRCPY (temp, choose_t->curr_dir);
	      else if (!strcmp (filename, ".."))
		{
		  STRCPY (temp, choose_t->curr_dir);
		  for (i = STRLEN (temp) - 1; i - 1 && temp[i] != '/'; i--);
		  temp[i] = 0;
		}
	      else
		{
		  STRCPY (temp, choose_t->curr_dir);
		  if (temp[STRLEN (temp) - 1] != '/')
		    STRCAT (temp, "/");
		  STRCAT (temp, filename);
		}

	      if (choose_t->curr_dir)
		free (choose_t->curr_dir);
	      choose_t->curr_dir = temp;
	      addlistofdir_to_optionmenu (choose_t->dir_combo,
		  choose_t->curr_dir, choose_t);
	      adddirectories_to_list (choose_t->mainwnd, choose_t->dir_list,
		  choose_t->curr_dir);
	      addfiles_to_list (choose_t->mainwnd, choose_t->file_list,
		  choose_t->curr_dir);
	    }
	}
    }
}


void
filedsn_lookin_clicked (GtkWidget *widget, void **array)
{
  if (array && array[0] && array[1] && ((TDSNCHOOSER *) array[1])->curr_dir
      && strcmp (((TDSNCHOOSER *) array[1])->curr_dir, array[0]))
    {
      /* Update the directory and file list */
      if (((TDSNCHOOSER *) array[1])->curr_dir)
	free (((TDSNCHOOSER *) array[1])->curr_dir);
      ((TDSNCHOOSER *) array[1])->curr_dir = strdup (array[0]);
      addlistofdir_to_optionmenu (((TDSNCHOOSER *) array[1])->dir_combo,
	  (LPCSTR) array[0], (TDSNCHOOSER *) array[1]);
      adddirectories_to_list (((TDSNCHOOSER *) array[1])->mainwnd,
	  ((TDSNCHOOSER *) array[1])->dir_list, (LPCSTR) array[0]);
      addfiles_to_list (((TDSNCHOOSER *) array[1])->mainwnd,
	  ((TDSNCHOOSER *) array[1])->file_list, (LPCSTR) array[0]);
    }
}


void
userdsn_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  char *szDSN = NULL;

  if (choose_t)
    {
      if (GTK_CLIST (choose_t->udsnlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->data),
	    0, &szDSN);

      if (szDSN && event && event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit_by_name (GTK_OBJECT (choose_t->uconfigure), "clicked",
	    choose_t);

      gtk_widget_set_sensitive (choose_t->uremove, TRUE);
      gtk_widget_set_sensitive (choose_t->uconfigure, TRUE);
      gtk_widget_set_sensitive (choose_t->utest, TRUE);
    }
}


void
userdsn_list_unselect (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  if (choose_t)
    {
      gtk_widget_set_sensitive (choose_t->uremove, FALSE);
      gtk_widget_set_sensitive (choose_t->uconfigure, FALSE);
      gtk_widget_set_sensitive (choose_t->utest, FALSE);
    }
}


void
systemdsn_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  char *szDSN = NULL;

  if (choose_t)
    {
      if (GTK_CLIST (choose_t->sdsnlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->data),
	    0, &szDSN);

      if (szDSN && event && event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit_by_name (GTK_OBJECT (choose_t->sconfigure), "clicked",
	    choose_t);

      gtk_widget_set_sensitive (choose_t->sremove, TRUE);
      gtk_widget_set_sensitive (choose_t->sconfigure, TRUE);
      gtk_widget_set_sensitive (choose_t->stest, TRUE);
    }
}


void
systemdsn_list_unselect (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDSNCHOOSER *choose_t)
{
  if (choose_t)
    {
      gtk_widget_set_sensitive (choose_t->sremove, FALSE);
      gtk_widget_set_sensitive (choose_t->sconfigure, FALSE);
      gtk_widget_set_sensitive (choose_t->stest, FALSE);
    }
}


static void
dsnchooser_ok_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  char *szDSN;

  if (choose_t)
    {
      switch (choose_t->type_dsn)
	     {
	       case USER_DSN:
	         if (GTK_CLIST (choose_t->udsnlist)->selection != NULL)
	           {
	             gtk_clist_get_text (GTK_CLIST (choose_t->udsnlist),
		            GPOINTER_TO_INT (GTK_CLIST (choose_t->udsnlist)->selection->
		            data), 0, &szDSN);
	             choose_t->dsn = strdup (szDSN);
	           }
	         else
	           choose_t->dsn = NULL;
	         break;

	       case SYSTEM_DSN:
	         if (GTK_CLIST (choose_t->sdsnlist)->selection != NULL)
	           {
	             gtk_clist_get_text (GTK_CLIST (choose_t->sdsnlist),
		            GPOINTER_TO_INT (GTK_CLIST (choose_t->sdsnlist)->selection->
		            data), 0, &szDSN);
	             choose_t->dsn = strdup (szDSN);
	           }
	         else
	           choose_t->dsn = NULL;
	         break;

	       default:
	         choose_t->dsn = NULL;
	         break;
	    };

    done:
      choose_t->udsnlist = choose_t->sdsnlist = NULL;
      choose_t->uadd = choose_t->uremove = choose_t->utest =
	   choose_t->uconfigure = NULL;
      choose_t->sadd = choose_t->sremove = choose_t->stest =
	   choose_t->sconfigure = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	     GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static void
dsnchooser_cancel_clicked (GtkWidget *widget, TDSNCHOOSER *choose_t)
{
  if (choose_t)
    {
      choose_t->udsnlist = choose_t->sdsnlist = NULL;
      choose_t->uadd = choose_t->uremove = choose_t->utest =
	   choose_t->uconfigure = NULL;
      choose_t->sadd = choose_t->sremove = choose_t->stest =
	   choose_t->sconfigure = NULL;
      choose_t->type_dsn = -1;
      choose_t->dsn = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	     GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget, GdkEvent *event, TDSNCHOOSER *choose_t)
{
  dsnchooser_cancel_clicked (widget, choose_t);

  return FALSE;
}


void
create_dsnchooser (HWND hwnd, TDSNCHOOSER * choose_t)
{
  GtkWidget *dsnchooser, *dialog_vbox1, *notebook1, *vbox1, *fixed1,
      *scrolledwindow1;
  GtkWidget *clist1, *l_name, *l_description, *l_driver, *l_usdsn, *frame1,
      *table1;
  GtkWidget *l_explanation, *pixmap1, *vbuttonbox1, *b_add, *b_remove,
      *b_configure;
  GtkWidget *b_test, *udsn, *fixed2, *scrolledwindow2, *clist2;
  GtkWidget *l_sdsn, *frame2, *table2, *pixmap2, *vbuttonbox2, *sdsn,
      *dialog_action_area1;
  GtkWidget *hbuttonbox1, *b_ok, *b_cancel;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkAccelGroup *accel_group;
  guint b_key;

  if (!GTK_IS_WIDGET (hwnd))
    {
      gtk_init (0, NULL);
      hwnd = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    }

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return;

  accel_group = gtk_accel_group_new ();

  dsnchooser = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (dsnchooser), "dsnchooser", dsnchooser);
  gtk_widget_set_usize (dsnchooser, 565, 415);
  gtk_window_set_title (GTK_WINDOW (dsnchooser), "Select Data Source");
  gtk_window_set_position (GTK_WINDOW (dsnchooser), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dsnchooser), TRUE);
  gtk_window_set_policy (GTK_WINDOW (dsnchooser), FALSE, FALSE, FALSE);

  dialog_vbox1 = GTK_DIALOG (dsnchooser)->vbox;
  gtk_object_set_data (GTK_OBJECT (dsnchooser), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  notebook1 = gtk_notebook_new ();
  gtk_widget_ref (notebook1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "notebook1", notebook1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (notebook1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), notebook1, TRUE, TRUE, 0);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "vbox1", vbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox1);

  /* User DSN panel */
  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_widget_set_usize (scrolledwindow1, 456, 232);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 3, 19);

  clist1 = gtk_clist_new (3);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "clist1", clist1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clist1);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 100);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 162);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 2, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_name = gtk_label_new (szDSNColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_name);

  l_description = gtk_label_new (szDSNColumnNames[1]);
  gtk_widget_ref (l_description);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_description",
      l_description, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_description);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_description);

  l_driver = gtk_label_new (szDSNColumnNames[2]);
  gtk_widget_ref (l_driver);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_driver", l_driver,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_driver);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 2, l_driver);

  l_usdsn = gtk_label_new ("User Data Sources :");
  gtk_widget_ref (l_usdsn);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_usdsn", l_usdsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_usdsn);
  gtk_fixed_put (GTK_FIXED (fixed1), l_usdsn, 8, 8);
  gtk_widget_set_uposition (l_usdsn, 8, 8);
  gtk_widget_set_usize (l_usdsn, 112, 16);
  gtk_label_set_justify (GTK_LABEL (l_usdsn), GTK_JUSTIFY_LEFT);

  frame1 = gtk_frame_new (NULL);
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "frame1", frame1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_fixed_put (GTK_FIXED (fixed1), frame1, 8, 264);
  gtk_widget_set_uposition (frame1, 8, 264);
  gtk_widget_set_usize (frame1, 546, 64);

  table1 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "table1", table1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame1), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 6);

  l_explanation =
      gtk_label_new
      ("An ODBC User data source stores information about how to connect to\nthe indicated data provider. A User data source is only available to you,\nand can only be used on the current machine.");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table1), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  style = gtk_widget_get_style (GTK_WIDGET (hwnd));
  pixmap =
      gdk_pixmap_create_from_xpm_d (GTK_WIDGET (hwnd)->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) odbc4_xpm);
  pixmap1 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "pixmap1", pixmap1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap1);
  gtk_table_attach (GTK_TABLE (table1), pixmap1, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  vbuttonbox1 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "vbuttonbox1",
      vbuttonbox1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox1);
  gtk_fixed_put (GTK_FIXED (fixed1), vbuttonbox1, 472, 16);
  gtk_widget_set_uposition (vbuttonbox1, 472, 16);
  gtk_widget_set_usize (vbuttonbox1, 85, 135);

  b_add = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szDSNButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_add", b_add,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_remove = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_remove)->child),
      szDSNButtons[1]);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_remove);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_remove", b_remove,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_remove);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_remove);
  GTK_WIDGET_SET_FLAGS (b_remove, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
      'R', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_remove, FALSE);

  b_configure = gtk_button_new_with_label ("");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_configure)->child),
      szDSNButtons[2]);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_configure);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_configure",
      b_configure, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_configure);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_configure);
  GTK_WIDGET_SET_FLAGS (b_configure, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
      'G', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_configure, FALSE);

  b_test = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_test)->child),
      szDSNButtons[3]);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_test);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_test", b_test,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_test);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_test);
  GTK_WIDGET_SET_FLAGS (b_test, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
      'T', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_test, FALSE);

  choose_t->uadd = b_add;
  choose_t->uremove = b_remove;
  choose_t->utest = b_test;
  choose_t->uconfigure = b_configure;

  udsn = gtk_label_new (szTabNames[0]);
  gtk_widget_ref (udsn);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "udsn", udsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (udsn);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 0), udsn);

  /* System DSN panel */
  fixed2 = gtk_fixed_new ();
  gtk_widget_ref (fixed2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "fixed2", fixed2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed2);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed2);
  gtk_container_set_border_width (GTK_CONTAINER (fixed2), 6);

  scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "scrolledwindow2",
      scrolledwindow2, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow2);
  gtk_widget_set_usize (scrolledwindow2, 456, 232);
  gtk_fixed_put (GTK_FIXED (fixed2), scrolledwindow2, 3, 19);

  clist2 = gtk_clist_new (3);
  gtk_widget_ref (clist2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "clist2", clist2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist2);
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), clist2);
  gtk_clist_set_column_width (GTK_CLIST (clist2), 0, 100);
  gtk_clist_set_column_width (GTK_CLIST (clist2), 1, 163);
  gtk_clist_set_column_width (GTK_CLIST (clist2), 2, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist2));

  l_name = gtk_label_new (szDSNColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist2), 0, l_name);

  l_description = gtk_label_new (szDSNColumnNames[1]);
  gtk_widget_ref (l_description);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_description",
      l_description, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_description);
  gtk_clist_set_column_widget (GTK_CLIST (clist2), 1, l_description);

  l_driver = gtk_label_new (szDSNColumnNames[2]);
  gtk_widget_ref (l_driver);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_driver", l_driver,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_driver);
  gtk_clist_set_column_widget (GTK_CLIST (clist2), 2, l_driver);

  l_sdsn = gtk_label_new ("System Data Sources :");
  gtk_widget_ref (l_sdsn);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_sdsn", l_sdsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_sdsn);
  gtk_fixed_put (GTK_FIXED (fixed2), l_sdsn, 8, 8);
  gtk_widget_set_uposition (l_sdsn, 8, 8);
  gtk_widget_set_usize (l_sdsn, 130, 16);
  gtk_label_set_justify (GTK_LABEL (l_sdsn), GTK_JUSTIFY_LEFT);

  frame2 = gtk_frame_new (NULL);
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "frame2", frame2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
  gtk_fixed_put (GTK_FIXED (fixed2), frame2, 8, 264);
  gtk_widget_set_uposition (frame2, 8, 264);
  gtk_widget_set_usize (frame2, 546, 64);

  table2 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "table2", table2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (frame2), table2);
  gtk_container_set_border_width (GTK_CONTAINER (table2), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table2), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 6);

  l_explanation =
      gtk_label_new
      ("An ODBC System data source stores information about how to connect\nto the indicated data provider. A system data source is visible to all\nusers on this machine, including daemons.");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table2), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap2 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "pixmap2", pixmap2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap2);
  gtk_table_attach (GTK_TABLE (table2), pixmap2, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  vbuttonbox2 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox2);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "vbuttonbox2",
      vbuttonbox2, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox2);
  gtk_fixed_put (GTK_FIXED (fixed2), vbuttonbox2, 472, 16);
  gtk_widget_set_uposition (vbuttonbox2, 472, 16);
  gtk_widget_set_usize (vbuttonbox2, 85, 135);

  b_add = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szDSNButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_add", b_add,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (vbuttonbox2), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_remove = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_remove)->child),
      szDSNButtons[1]);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_remove);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_remove", b_remove,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_remove);
  gtk_container_add (GTK_CONTAINER (vbuttonbox2), b_remove);
  GTK_WIDGET_SET_FLAGS (b_remove, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
      'R', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_remove, FALSE);

  b_configure = gtk_button_new_with_label ("");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_configure)->child),
      szDSNButtons[2]);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_configure);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_configure",
      b_configure, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_configure);
  gtk_container_add (GTK_CONTAINER (vbuttonbox2), b_configure);
  GTK_WIDGET_SET_FLAGS (b_configure, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
      'G', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_configure, FALSE);

  b_test = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_test)->child),
      szDSNButtons[3]);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_test);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_test", b_test,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_test);
  gtk_container_add (GTK_CONTAINER (vbuttonbox2), b_test);
  GTK_WIDGET_SET_FLAGS (b_test, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
      'T', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_test, FALSE);

  choose_t->sadd = b_add;
  choose_t->sremove = b_remove;
  choose_t->stest = b_test;
  choose_t->sconfigure = b_configure;

  sdsn = gtk_label_new (szTabNames[1]);
  gtk_widget_ref (sdsn);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "sdsn", sdsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sdsn);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 1), sdsn);

  dialog_action_area1 = GTK_DIALOG (dsnchooser)->action_area;
  gtk_object_set_data (GTK_OBJECT (dsnchooser), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "hbuttonbox1",
      hbuttonbox1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE,
      0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 10);

  b_ok = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_ok)->child), "_Ok");
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_ok);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_ok", b_ok,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_ok);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_ok);
  GTK_WIDGET_SET_FLAGS (b_ok, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      'O', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_cancel = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_cancel)->child),
      "_Cancel");
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_cancel);
  gtk_object_set_data_full (GTK_OBJECT (dsnchooser), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Notebook events */
  gtk_signal_connect_after (GTK_OBJECT (notebook1), "switch_page",
      GTK_SIGNAL_FUNC (dsnchooser_switch_page), choose_t);
  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_ok), "clicked",
      GTK_SIGNAL_FUNC (dsnchooser_ok_clicked), choose_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (dsnchooser_cancel_clicked), choose_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (dsnchooser), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), choose_t);
  gtk_signal_connect (GTK_OBJECT (dsnchooser), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* Add user DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->uadd), "clicked",
      GTK_SIGNAL_FUNC (userdsn_add_clicked), choose_t);
  /* Remove user DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->uremove), "clicked",
      GTK_SIGNAL_FUNC (userdsn_remove_clicked), choose_t);
  /* Test user DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->utest), "clicked",
      GTK_SIGNAL_FUNC (userdsn_test_clicked), choose_t);
  /* Configure user DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->uconfigure), "clicked",
      GTK_SIGNAL_FUNC (userdsn_configure_clicked), choose_t);
  /* Add system DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->sadd), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_add_clicked), choose_t);
  /* Remove system DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->sremove), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_remove_clicked), choose_t);
  /* Test system DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->stest), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_test_clicked), choose_t);
  /* Configure system DSN button events */
  gtk_signal_connect (GTK_OBJECT (choose_t->sconfigure), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_configure_clicked), choose_t);
  /* User DSN list events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (userdsn_list_select), choose_t);
  gtk_signal_connect (GTK_OBJECT (clist1), "unselect_row",
      GTK_SIGNAL_FUNC (userdsn_list_unselect), choose_t);
  /* System DSN list events */
  gtk_signal_connect (GTK_OBJECT (clist2), "select_row",
      GTK_SIGNAL_FUNC (systemdsn_list_select), choose_t);
  gtk_signal_connect (GTK_OBJECT (clist2), "unselect_row",
      GTK_SIGNAL_FUNC (systemdsn_list_unselect), choose_t);

  gtk_window_add_accel_group (GTK_WINDOW (dsnchooser), accel_group);

  adddsns_to_list (clist1, FALSE);

  choose_t->udsnlist = clist1;
  choose_t->sdsnlist = clist2;
  choose_t->type_dsn = USER_DSN;
  choose_t->mainwnd = dsnchooser;

  gtk_widget_show_all (dsnchooser);
  gtk_main ();
}
