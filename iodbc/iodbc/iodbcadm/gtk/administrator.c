/*
 *  administrator.c
 *
 *  $Id: administrator.c,v 1.13 2007/02/02 11:58:14 source Exp $
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
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "../gui.h"
#include "odbc4.xpm"


#if !defined(HAVE_DL_INFO)
typedef struct
{
  const char *dli_fname;	/* File name of defining object.  */
  void *dli_fbase;		/* Load address of that object.  */
  const char *dli_sname;	/* Name of nearest symbol.  */
  void *dli_saddr;		/* Exact value of nearest symbol.  */
} Dl_info;
#endif /* HAVE_DL_INFO */

static char *szDriverButtons[] = {
  "_Add a driver",
  "_Remove the driver",
  "Confi_gure the driver"
};

static char *szCpLabels[] = {
  "Name",
  "Pool timeout (seconds)",
  "Probe query"
};

static struct
{
  char *lib_name;
  char *lib_desc;
  char *lib_ver_sym;
} iODBC_Components[] =
{
  {"libiodbc.so", "iODBC Driver Manager", "iodbc_version"},
  {"libiodbcadm.so", "iODBC Administrator", "iodbcadm_version"},
  {"libiodbcinst.so", "iODBC Installer", "iodbcinst_version"},
  {"libdrvproxy.so", "iODBC Driver Setup Proxy", "iodbcproxy_version"},
  {"libtranslator.so", "iODBC Translation Manager", "iodbctrans_version"}
};


static void
addcomponents_to_list (GtkWidget *widget)
{
  char _date[1024], _size[1024];
  char *data[5];
  struct stat _stat;
#if defined(HAVE_DLADDR)
  Dl_info info;
#endif
  void *handle, *proc;
  int i;

  if (!GTK_IS_CLIST (widget))
    return;

  gtk_clist_clear (GTK_CLIST (widget));

  for (i = 0; i < sizeof (iODBC_Components) / sizeof (iODBC_Components[0]);
      i++)
    {
      /*
       *  Collect basic info on the components
       */
      data[0] = iODBC_Components[i].lib_desc;
      data[1] = VERSION;
      data[2] = iODBC_Components[i].lib_name;
      data[3] = "";		/* Modification Date */
      data[4] = "";		/* Size */

      if ((handle = dlopen (iODBC_Components[i].lib_name, RTLD_LAZY)))
	{
	  /* Find out the version of the library */
	  if ((proc = dlsym (handle, iODBC_Components[i].lib_ver_sym)))
	    data[1] = *(char **) proc;

	  /* Check the size and modification date of the library */
#ifdef HAVE_DLADDR
	  dladdr (proc, &info);
	  if (!stat (info.dli_fname, &_stat))
	    {
	      sprintf (_size, "%lu Kb",
		  (unsigned long) _stat.st_size / 1024L);
	      sprintf (_date, "%s", ctime (&_stat.st_mtime));
	      _date[strlen (_date) - 1] = '\0';	/* remove last \n */
	      data[3] = _date;
	      data[4] = _size;
	    }
#endif
	  gtk_clist_append (GTK_CLIST (widget), data);

	  dlclose (handle);
	}
    }

  if (GTK_CLIST (widget)->rows > 0)
    {
      gtk_clist_columns_autosize (GTK_CLIST (widget));
      gtk_clist_sort (GTK_CLIST (widget));
    }
}


static void
addconnectionpool_to_list (GtkWidget *widget)
{
  char *curr, *buffer = (char *) malloc (sizeof (char) * 65536), *szDriver;
  char cptime[128], cpprobe[1024];
  char *data[3];
  int len, i;
  BOOL careabout;
  UWORD confMode = ODBC_USER_DSN;

  if (!buffer || !GTK_IS_CLIST (widget))
    return;
  gtk_clist_clear (GTK_CLIST (widget));

  /* Get the current config mode */
  while (confMode != ODBC_SYSTEM_DSN + 1)
    {
      /* Get the list of drivers in the user context */
      SQLSetConfigMode (confMode);
#ifdef WIN32
      len =
	  SQLGetPrivateProfileString ("ODBC 32 bit Drivers", NULL, "", buffer,
	  65535, "odbcinst.ini");
#else
      len =
	  SQLGetPrivateProfileString ("ODBC Drivers", NULL, "", buffer, 65535,
	  "odbcinst.ini");
#endif
      if (len)
	goto process;

      goto end;

    process:
      for (curr = buffer; *curr; curr += (STRLEN (curr) + 1))
	{
	  /* Shadowing system odbcinst.ini */
	  for (i = 0, careabout = TRUE; i < GTK_CLIST (widget)->rows; i++)
	    {
	      gtk_clist_get_text (GTK_CLIST (widget), i, 0, &szDriver);
	      if (!strcmp (szDriver, curr))
		{
		  careabout = FALSE;
		  break;
		}
	    }

	  if (!careabout)
	    continue;

	  SQLSetConfigMode (confMode);
#ifdef WIN32
	  SQLGetPrivateProfileString ("ODBC 32 bit Drivers", curr, "", cpprobe,
	      sizeof (cpprobe), "odbcinst.ini");
#else
	  SQLGetPrivateProfileString ("ODBC Drivers", curr, "", cpprobe,
	      sizeof (cpprobe), "odbcinst.ini");
#endif

	  /* Check if the driver is installed */
	  if (strcasecmp (cpprobe, "Installed"))
	    goto end;

	  /* Get the driver library name */
	  SQLSetConfigMode (confMode);
	  if (!SQLGetPrivateProfileString (curr, "CPTimeout", "<Not pooled>",
		  cptime, sizeof (cptime), "odbcinst.ini"))
	    {
	      SQLSetConfigMode (confMode);
	      SQLGetPrivateProfileString ("Default", "CPTimeout",
		  "<Not pooled>", cptime, sizeof (cptime), "odbcinst.ini");
	    }
	  if (!SQLGetPrivateProfileString (curr, "CPProbe", "",
		  cpprobe, sizeof (cpprobe), "odbcinst.ini"))
	    {
	      SQLSetConfigMode (confMode);
	      SQLGetPrivateProfileString ("Default", "CPProbe",
		  "", cpprobe, sizeof (cpprobe), "odbcinst.ini");
	    }

	  if (STRLEN (curr) && STRLEN (cptime))
	    {
	      data[0] = curr;
	      data[1] = cptime;
	      data[2] = cpprobe;
	      gtk_clist_append (GTK_CLIST (widget), data);
	    }
	}

    end:
      if (confMode == ODBC_USER_DSN)
	confMode = ODBC_SYSTEM_DSN;
      else
	confMode = ODBC_SYSTEM_DSN + 1;
    }

  if (GTK_CLIST (widget)->rows > 0)
    {
      gtk_clist_columns_autosize (GTK_CLIST (widget));
      gtk_clist_sort (GTK_CLIST (widget));
    }

  /* Make the clean up */
  free (buffer);
}


static void
admin_apply_tracing (TTRACING *tracing_t)
{
  /* Write keywords for tracing in the ini file */
  SQLWritePrivateProfileString ("ODBC", "Trace",
      (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tracing_t->
		  allthetime_rb)) == TRUE
	  || gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tracing_t->
		  onetime_rb)) == TRUE) ? "1" : "0", NULL);
  SQLWritePrivateProfileString ("ODBC", "TraceAutoStop",
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tracing_t->
	      onetime_rb)) ? "1" : "0", NULL);
  SQLWritePrivateProfileString ("ODBC", "TraceFile",
      gtk_entry_get_text (GTK_ENTRY (tracing_t->logfile_entry)), NULL);
}


static void
admin_switch_page (GtkNotebook *notebook, GtkNotebookPage *page,
    gint page_num, void **inparams)
{
  TDSNCHOOSER *dsnchoose_t = (inparams) ? inparams[0] : NULL;
  TDRIVERCHOOSER *driverchoose_t = (inparams) ? inparams[1] : NULL;
  TTRACING *tracing_t = (inparams) ? inparams[2] : NULL;
  TCOMPONENT *component_t = (inparams) ? inparams[3] : NULL;
  TCONNECTIONPOOLING *connectionpool_t = (inparams) ? inparams[4] : NULL;
  BOOL trace = FALSE, traceauto = FALSE, perfmon = FALSE;
  char tokenstr[4096] = { 0 };

  switch (page_num)
    {
      /* The User DSN panel */
    case 0:
      if (dsnchoose_t)
	{
	  dsnchoose_t->type_dsn = USER_DSN;
	  adddsns_to_list (dsnchoose_t->udsnlist, FALSE);
	}
      break;

      /* The System DSN panel */
    case 1:
      if (dsnchoose_t)
	{
	  dsnchoose_t->type_dsn = SYSTEM_DSN;
	  adddsns_to_list (dsnchoose_t->sdsnlist, TRUE);
	}
      break;

      /* The File DSN panel */
    case 2:
      if (dsnchoose_t)
        {
          dsnchoose_t->type_dsn = FILE_DSN;

          addlistofdir_to_optionmenu(dsnchoose_t->dir_combo,
	      dsnchoose_t->curr_dir, dsnchoose_t);
          adddirectories_to_list(dsnchoose_t->mainwnd,
	      dsnchoose_t->dir_list, dsnchoose_t->curr_dir);
          addfiles_to_list(dsnchoose_t->mainwnd,
	      dsnchoose_t->file_list, dsnchoose_t->curr_dir);
        }
       break;

      /* The ODBC Driver panel */
    case 3:
      if (driverchoose_t)
	{
	  adddrivers_to_list (driverchoose_t->driverlist,
	      driverchoose_t->mainwnd);
	  gtk_widget_set_sensitive (driverchoose_t->b_remove, FALSE);
	  gtk_widget_set_sensitive (driverchoose_t->b_configure, FALSE);
	}
      break;

      /* The Connection Pooling */
    case 4:
      if (!connectionpool_t->changed)
	{
	  /* Get the connection pooling options */
	  SQLGetPrivateProfileString ("ODBC Connection Pooling", "Perfmon",
	      "", tokenstr, sizeof (tokenstr), "odbcinst.ini");
	  if (!strcasecmp (tokenstr, "1") || !strcasecmp (tokenstr, "On"))
	    perfmon = TRUE;
	  SQLGetPrivateProfileString ("ODBC Connection Pooling", "Retry Wait",
	      "", tokenstr, sizeof (tokenstr), "odbcinst.ini");

	  if (perfmon)
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
		(connectionpool_t->enperfmon_rb), 1);
	  else
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
		(connectionpool_t->disperfmon_rb), 1);

	  gtk_entry_set_text (GTK_ENTRY (connectionpool_t->retwait_entry),
	      tokenstr);

	  connectionpool_t->changed = TRUE;
	}

      addconnectionpool_to_list (connectionpool_t->driverlist);

      break;

      /* The tracing panel */
    case 5:
      if (!tracing_t->changed)
	{
	  /* Get the traces options */
	  SQLGetPrivateProfileString ("ODBC", "Trace", "", tokenstr,
	      sizeof (tokenstr), NULL);
	  if (!strcasecmp (tokenstr, "1") || !strcasecmp (tokenstr, "On"))
	    trace = TRUE;
	  SQLGetPrivateProfileString ("ODBC", "TraceAutoStop", "", tokenstr,
	      sizeof (tokenstr), NULL);
	  if (!strcasecmp (tokenstr, "1") || !strcasecmp (tokenstr, "On"))
	    traceauto = TRUE;
	  SQLGetPrivateProfileString ("ODBC", "TraceFile", "", tokenstr,
	      sizeof (tokenstr), NULL);

	  /* Set the widgets */
	  if (trace)
	    {
	      if (!traceauto)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tracing_t->
			allthetime_rb), 1);
	      else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tracing_t->
			onetime_rb), 1);
	    }
	  else
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tracing_t->
		    donttrace_rb), 1);

	  gtk_entry_set_text (GTK_ENTRY (tracing_t->logfile_entry),
	      (STRLEN (tokenstr)) ? tokenstr : "sql.log");

	  tracing_t->changed = TRUE;
	}
      break;

      /* The About panel */
    case 6:
      if (component_t)
	addcomponents_to_list (component_t->componentlist);
      break;
    };

  if (dsnchoose_t)
    {
      if (dsnchoose_t->uremove)
	gtk_widget_set_sensitive (dsnchoose_t->uremove, FALSE);
      if (dsnchoose_t->uconfigure)
	gtk_widget_set_sensitive (dsnchoose_t->uconfigure, FALSE);
      if (dsnchoose_t->utest)
	gtk_widget_set_sensitive (dsnchoose_t->utest, FALSE);
      if (dsnchoose_t->sremove)
	gtk_widget_set_sensitive (dsnchoose_t->sremove, FALSE);
      if (dsnchoose_t->sconfigure)
	gtk_widget_set_sensitive (dsnchoose_t->sconfigure, FALSE);
      if (dsnchoose_t->stest)
	gtk_widget_set_sensitive (dsnchoose_t->stest, FALSE);
      if (dsnchoose_t->fremove)
	gtk_widget_set_sensitive(dsnchoose_t->fremove, FALSE);
      if (dsnchoose_t->fconfigure)
	gtk_widget_set_sensitive(dsnchoose_t->fconfigure, FALSE);
      if (dsnchoose_t->ftest)
	gtk_widget_set_sensitive(dsnchoose_t->ftest, FALSE);
    }
}


static void
tracing_start_clicked (GtkWidget *widget, TTRACING *tracing_t)
{
  if (tracing_t)
    admin_apply_tracing (tracing_t);
}


static void
tracing_logfile_choosen (GtkWidget *widget, TTRACING *tracing_t)
{
  if (tracing_t)
    {
      gtk_entry_set_text (GTK_ENTRY (tracing_t->logfile_entry),
	  gtk_file_selection_get_filename (GTK_FILE_SELECTION (tracing_t->
		  filesel)));
      tracing_t->filesel = NULL;
    }
}


static void
tracing_browse_clicked (GtkWidget *widget, TTRACING *tracing_t)
{
  GtkWidget *filesel;

  if (tracing_t)
    {
      filesel = gtk_file_selection_new ("Choose your log file ...");
      gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
	  gtk_entry_get_text (GTK_ENTRY (tracing_t->logfile_entry)));
      /* Ok button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      ok_button), "clicked",
	  GTK_SIGNAL_FUNC (tracing_logfile_choosen), tracing_t);
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      ok_button), "clicked", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      /* Cancel button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      cancel_button), "clicked", GTK_SIGNAL_FUNC (gtk_main_quit),
	  NULL);
      /* Close window button events */
      gtk_signal_connect (GTK_OBJECT (filesel), "delete_event",
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

      tracing_t->filesel = filesel;

      gtk_widget_show_all (filesel);
      gtk_main ();
      gtk_widget_destroy (filesel);

      tracing_t->filesel = NULL;
    }
}


static void
cpdriver_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TCONNECTIONPOOLING *connectionpool_t)
{
  char *szDriver = NULL, *cptimeout, *cpprobe;
  char sztime[1024] = { 0 };
  char szprobe[1024] = { 0 };
  TCONNECTIONPOOLING choose_t;

  if (connectionpool_t)
    {
      choose_t = *connectionpool_t;
      if (GTK_CLIST (connectionpool_t->driverlist)->selection != NULL)
	{
          memset(choose_t.timeout, 0, sizeof(choose_t.timeout)); 
          memset(choose_t.probe, 0, sizeof(choose_t.probe)); 

	  gtk_clist_get_text (GTK_CLIST (choose_t.driverlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t.driverlist)->
		  selection->data), 0, &szDriver);
	  gtk_clist_get_text (GTK_CLIST (choose_t.driverlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t.driverlist)->
		  selection->data), 1, &cptimeout);
	  gtk_clist_get_text (GTK_CLIST (choose_t.driverlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t.driverlist)->
		  selection->data), 2, &cpprobe);

          strncpy (choose_t.timeout, cptimeout, sizeof(choose_t.timeout)-1);
          strncpy (choose_t.probe, cpprobe, sizeof(choose_t.probe)-1);
	}

      if (szDriver && event && event->type == GDK_2BUTTON_PRESS
	  && (create_connectionpool (connectionpool_t->mainwnd, &choose_t) == TRUE))
	{
	  sprintf (sztime, "CPTimeout=%s", choose_t.timeout);
	  if (!SQLConfigDriver (connectionpool_t->mainwnd, ODBC_CONFIG_DRIVER,
		  szDriver, sztime, NULL, 0, NULL))
	    _iodbcdm_errorbox (connectionpool_t->mainwnd, szDriver,
		"An error occured when trying to set the connection pooling time-out : ");

	  sprintf (szprobe, "CPProbe=%s", choose_t.probe);
	  if (!SQLConfigDriver (connectionpool_t->mainwnd, ODBC_CONFIG_DRIVER,
		  szDriver, szprobe, NULL, 0, NULL))
	    _iodbcdm_errorbox (connectionpool_t->mainwnd, szDriver,
		"An error occured when trying to set the connection probe query : ");

	  addconnectionpool_to_list (connectionpool_t->driverlist);
	}
    }
}


static void
driver_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDRIVERCHOOSER *choose_t)
{
  char *szDriver = NULL;

  if (choose_t)
    {
      if (GTK_CLIST (choose_t->driverlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->driverlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->driverlist)->selection->
		data), 0, &szDriver);

      if (szDriver && event && event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit_by_name (GTK_OBJECT (choose_t->b_configure),
	    "clicked", choose_t);
      else
	{
	  gtk_widget_set_sensitive (choose_t->b_remove, TRUE);
	  gtk_widget_set_sensitive (choose_t->b_configure, TRUE);
	}
    }
}


static void
driver_list_unselect (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDRIVERCHOOSER *choose_t)
{
  if (choose_t)
    {
      gtk_widget_set_sensitive (choose_t->b_remove, FALSE);
      gtk_widget_set_sensitive (choose_t->b_configure, FALSE);
    }
}


static void
driver_add_clicked (GtkWidget *widget, TDRIVERCHOOSER *choose_t)
{
  char connstr[4096] = { 0 };
  char tokenstr[4096] = { 0 };
  char *cstr;

  if (choose_t)
    {
      cstr = create_driversetup (choose_t->mainwnd, NULL, connstr, FALSE, TRUE);

      if (cstr && cstr != connstr && cstr != (LPSTR) - 1L)
	{
	  SQLSetConfigMode (ODBC_USER_DSN);
	  if (!SQLInstallDriverEx (cstr, NULL, tokenstr, sizeof (tokenstr),
		  NULL, ODBC_INSTALL_COMPLETE, NULL))
	    {
	      _iodbcdm_errorbox (choose_t->mainwnd, NULL,
		  "An error occured when trying to add the driver : ");
	      goto done;
	    }

	  free (cstr);
	}

      adddrivers_to_list (choose_t->driverlist, choose_t->mainwnd);

    done:
      if (GTK_CLIST (choose_t->driverlist)->selection == NULL)
	{
	  if (choose_t->b_remove)
	    gtk_widget_set_sensitive (choose_t->b_remove, FALSE);
	  if (choose_t->b_configure)
	    gtk_widget_set_sensitive (choose_t->b_configure, FALSE);
	}
    }

  return;
}


static void
driver_remove_clicked (GtkWidget *widget, TDRIVERCHOOSER *choose_t)
{
  char *szDriver = NULL;

  if (choose_t)
    {
      /* Retrieve the driver name */
      if (GTK_CLIST (choose_t->driverlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->driverlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->driverlist)->selection->
		data), 0, &szDriver);

      /* Call the right function */
      if (szDriver
	  && create_confirm (choose_t->mainwnd, szDriver,
	      "Are you sure you want to remove this driver ?"))
	{
	  if (!SQLRemoveDriver (szDriver, create_confirm (choose_t->mainwnd,
		      szDriver,
		      "Do you want to remove all the DSN associated to this driver ?"),
		  NULL))
	    {
	      _iodbcdm_errorbox (choose_t->mainwnd, szDriver,
		  "An error occured when trying to remove the driver : ");
	      goto done;
	    }

	  adddrivers_to_list (choose_t->driverlist, choose_t->mainwnd);
	}

    done:
      if (GTK_CLIST (choose_t->driverlist)->selection == NULL)
	{
	  if (choose_t->b_remove)
	    gtk_widget_set_sensitive (choose_t->b_remove, FALSE);
	  if (choose_t->b_configure)
	    gtk_widget_set_sensitive (choose_t->b_configure, FALSE);
	}
    }

  return;
}


static void
driver_configure_clicked (GtkWidget *widget, TDRIVERCHOOSER *choose_t)
{
  char connstr[4096] = { 0 };
  char tokenstr[4096] = { 0 };
  char *szDriver = NULL, *curr, *cour, *cstr;
  int size = sizeof (connstr);

  if (choose_t)
    {
      /* Retrieve the driver name */
      if (GTK_CLIST (choose_t->driverlist)->selection != NULL)
	gtk_clist_get_text (GTK_CLIST (choose_t->driverlist),
	    GPOINTER_TO_INT (GTK_CLIST (choose_t->driverlist)->selection->
		data), 0, &szDriver);

      /* Call the right function */
      if (szDriver)
	{
	  SQLSetConfigMode (ODBC_USER_DSN);
	  if (!SQLGetPrivateProfileString (szDriver, NULL, "", tokenstr,
		  sizeof (tokenstr), "odbcinst.ini"))
	    {
	      _iodbcdm_errorbox (choose_t->mainwnd, szDriver,
		  "An error occured when trying to configure the driver : ");
	      goto done;
	    }

	  for (curr = tokenstr, cour = connstr; *curr;
	      curr += (STRLEN (curr) + 1), cour += (STRLEN (cour) + 1))
	    {
	      STRCPY (cour, curr);
	      cour[STRLEN (curr)] = '=';
	      SQLSetConfigMode (ODBC_USER_DSN);
	      SQLGetPrivateProfileString (szDriver, curr, "",
		  cour + STRLEN (curr) + 1, size - STRLEN (curr) - 1,
		  "odbcinst.ini");
	      size -= (STRLEN (cour) + 1);
	    }

	  *cour = 0;

	  cstr =
	      create_driversetup (choose_t->mainwnd, szDriver, connstr,
	      FALSE, TRUE);

	  if (cstr && cstr != connstr && cstr != (LPSTR) - 1L)
	    {
	      SQLSetConfigMode (ODBC_USER_DSN);
	      if (!SQLInstallDriverEx (cstr, NULL, tokenstr,
		      sizeof (tokenstr), NULL, ODBC_INSTALL_COMPLETE, NULL))
		{
		  _iodbcdm_errorbox (choose_t->mainwnd, NULL,
		      "An error occured when trying to configure the driver : ");
		  goto done;
		}

	      free (cstr);
	    }

	  adddrivers_to_list (choose_t->driverlist, choose_t->mainwnd);
	}

    done:
      if (GTK_CLIST (choose_t->driverlist)->selection == NULL)
	{
	  if (choose_t->b_remove)
	    gtk_widget_set_sensitive (choose_t->b_remove, FALSE);
	  if (choose_t->b_configure)
	    gtk_widget_set_sensitive (choose_t->b_configure, FALSE);
	}
    }

  return;
}


static void
admin_ok_clicked (GtkWidget *widget, void **inparams)
{
  TDSNCHOOSER *dsnchoose_t = (inparams) ? inparams[0] : NULL;
  TDRIVERCHOOSER *driverchoose_t = (inparams) ? inparams[1] : NULL;
  TTRACING *tracing_t = (inparams) ? inparams[2] : NULL;
  TCOMPONENT *component_t = (inparams) ? inparams[3] : NULL;
  TCONNECTIONPOOLING *connectionpool_t = (inparams) ? inparams[4] : NULL;
  GtkWidget *mainwnd = (inparams) ? inparams[5] : NULL;

  if (dsnchoose_t)
    {
      dsnchoose_t->udsnlist = dsnchoose_t->sdsnlist = dsnchoose_t->dir_list =
	  NULL;
      dsnchoose_t->fadd = dsnchoose_t->fremove = dsnchoose_t->ftest =
	  dsnchoose_t->fconfigure = dsnchoose_t->fsetdir = NULL;
      dsnchoose_t->uadd = dsnchoose_t->uremove = dsnchoose_t->utest =
	  dsnchoose_t->uconfigure = NULL;
      dsnchoose_t->sadd = dsnchoose_t->sremove = dsnchoose_t->stest =
	  dsnchoose_t->sconfigure = NULL;
      dsnchoose_t->file_list = dsnchoose_t->file_entry =
	  dsnchoose_t->dir_combo = NULL;
      dsnchoose_t->type_dsn = -1;
      dsnchoose_t->dsn = NULL;
    }

  if (driverchoose_t)
    driverchoose_t->driverlist = NULL;

  if (component_t)
    component_t->componentlist = NULL;

  if (tracing_t)
    {
      if (tracing_t->changed)
	admin_apply_tracing (tracing_t);

      tracing_t->logfile_entry = tracing_t->tracelib_entry =
	  tracing_t->b_start_stop = NULL;
      tracing_t->donttrace_rb = tracing_t->allthetime_rb =
	  tracing_t->onetime_rb = NULL;
    }

  if (connectionpool_t)
    {
      if (connectionpool_t->changed)
	{
	  /* Write keywords for tracing in the ini file */
	  SQLWritePrivateProfileString ("ODBC Connection Pooling", "PerfMon",
	      (GTK_TOGGLE_BUTTON (connectionpool_t->enperfmon_rb)->
		  active) ? "1" : "0", "odbcinst.ini");
	  SQLWritePrivateProfileString ("ODBC Connection Pooling",
	      "Retry Wait",
	      gtk_entry_get_text (GTK_ENTRY (connectionpool_t->
		      retwait_entry)), "odbcinst.ini");
	}

      connectionpool_t->driverlist = connectionpool_t->mainwnd = NULL;
      connectionpool_t->enperfmon_rb = connectionpool_t->disperfmon_rb = NULL;
      connectionpool_t->retwait_entry = NULL;
    }

  if (mainwnd)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (mainwnd);
    }
}


static void
admin_cancel_clicked (GtkWidget *widget, void **inparams)
{
  TDSNCHOOSER *dsnchoose_t = (inparams) ? inparams[0] : NULL;
  TDRIVERCHOOSER *driverchoose_t = (inparams) ? inparams[1] : NULL;
  TTRACING *tracing_t = (inparams) ? inparams[2] : NULL;
  TCOMPONENT *component_t = (inparams) ? inparams[3] : NULL;
  TCONNECTIONPOOLING *connectionpool_t = (inparams) ? inparams[4] : NULL;
  GtkWidget *mainwnd = (inparams) ? inparams[5] : NULL;

  if (dsnchoose_t)
    {
      dsnchoose_t->udsnlist = dsnchoose_t->sdsnlist = dsnchoose_t->dir_list =
	  NULL;
      dsnchoose_t->fadd = dsnchoose_t->fremove = dsnchoose_t->ftest =
	  dsnchoose_t->fconfigure = dsnchoose_t->fsetdir = NULL;
      dsnchoose_t->uadd = dsnchoose_t->uremove = dsnchoose_t->utest =
	  dsnchoose_t->uconfigure = NULL;
      dsnchoose_t->sadd = dsnchoose_t->sremove = dsnchoose_t->stest =
	  dsnchoose_t->sconfigure = NULL;
      dsnchoose_t->file_list = dsnchoose_t->file_entry =
	  dsnchoose_t->dir_combo = NULL;
      dsnchoose_t->type_dsn = -1;
      dsnchoose_t->dsn = NULL;
    }

  if (driverchoose_t)
    driverchoose_t->driverlist = NULL;

  if (component_t)
    component_t->componentlist = NULL;

  if (tracing_t)
    {
      tracing_t->logfile_entry = tracing_t->tracelib_entry =
	  tracing_t->b_start_stop = NULL;
      tracing_t->donttrace_rb = tracing_t->allthetime_rb =
	  tracing_t->onetime_rb = NULL;
    }

  if (connectionpool_t)
    {
      connectionpool_t->driverlist = connectionpool_t->mainwnd = NULL;
      connectionpool_t->enperfmon_rb = connectionpool_t->disperfmon_rb = NULL;
      connectionpool_t->retwait_entry = NULL;
    }

  if (mainwnd)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget, GdkEvent *event, void **inparams)
{
  admin_cancel_clicked (widget, inparams);
  return FALSE;
}


void
create_administrator (HWND hwnd)
{
  GtkWidget *admin, *dialog_vbox1, *notebook1, *vbox1, *fixed1,
      *scrolledwindow1;
  GtkWidget *clist1, *l_name, *l_description, *l_driver, *l_usdsn, *frame1,
      *table1;
  GtkWidget *l_explanation, *pixmap1, *vbuttonbox1, *b_add, *b_remove,
      *b_configure;
  GtkWidget *b_test, *udsn, *fixed2, *scrolledwindow2, *clist2, *hbuttonbox2;
  GtkWidget *l_sdsn, *frame2, *table2, *pixmap2, *vbuttonbox2, *sdsn,
      *dialog_action_area1;
  GtkWidget *hbuttonbox1, *b_ok, *b_cancel, *fixed4, *scrolledwindow5,
      *clist5, *l_file;
  GtkWidget *l_date, *l_size, *frame4, *table4, *pixmap4, *l_drivers,
      *fdrivers;
  GtkWidget *fixed6, *frame6, *hbox1, *vbox2, *b_donottrace, *b_allthetime;
  GtkWidget *b_onetime, *vbox3, *b_start, *frame7, *table6, *t_logfile,
      *b_browse, *frame8;
  GtkWidget *table7, *t_tracelib, *b_select, *frame5, *table5, *pixmap5,
      *ftracing;
  GtkWidget *fixed7, *frame9, *table8, *pixmap6, *l_about, *scrolledwindow6,
      *clist6;
  GtkWidget *l_version, *fabout, *pixmap3, *clist3, *clist4, *l_files,
      *l_selected, *scrolledwindow4;
  GtkWidget *t_fileselected, *l_lookin, *optionmenu1, *frame3, *table3, *fdsn,
      *l_directory, *fixed3;
  GtkWidget *optionmenu1_menu, *scrolledwindow3, *fpooling, *l_poolto,
      *l_poolprobe, *clist7, *l_odbcdrivers;
  GtkWidget *fixed8, *frame11, *alignment1, *vbox4, *frame13, *fixed5,
      *frame10, *table9, *vbuttonbox3;
  GtkWidget *pixmap7, *frame12, *vbox5, *br_enable, *br_disable, *t_retrywait,
      *scrolledwindow7;
  GtkWidget *b_setdir;
  GSList *vbox2_group = NULL, *vbox5_group = NULL;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkAccelGroup *accel_group;
  TDSNCHOOSER dsnchoose_t;
  TDRIVERCHOOSER driverchoose_t;
  TCOMPONENT component_t;
  TTRACING tracing_t;
  TCONNECTIONPOOLING connectionpool_t;
  void *inparams[6];
  guint b_key;

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return;

  accel_group = gtk_accel_group_new ();

  admin = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (admin), "admin", admin);
  gtk_widget_set_usize (admin, 565, 415);
  gtk_window_set_title (GTK_WINDOW (admin),
      "iODBC Data Source Administrator");
  gtk_window_set_position (GTK_WINDOW (admin), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (admin), TRUE);
  gtk_window_set_policy (GTK_WINDOW (admin), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (admin);
#endif

  dialog_vbox1 = GTK_DIALOG (admin)->vbox;
  gtk_object_set_data (GTK_OBJECT (admin), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  notebook1 = gtk_notebook_new ();
  gtk_widget_ref (notebook1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "notebook1", notebook1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (notebook1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), notebook1, TRUE, TRUE, 0);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbox1", vbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox1);

  /* User DSN panel */
  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_widget_set_usize (scrolledwindow1, 456, 232);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 3, 19);

  clist1 = gtk_clist_new (3);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist1", clist1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clist1);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 100);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 162);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 2, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_name = gtk_label_new (szDSNColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_name);

  l_description = gtk_label_new (szDSNColumnNames[1]);
  gtk_widget_ref (l_description);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_description",
      l_description, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_description);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_description);

  l_driver = gtk_label_new (szDSNColumnNames[2]);
  gtk_widget_ref (l_driver);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_driver", l_driver,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_driver);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 2, l_driver);

  l_usdsn = gtk_label_new ("User Data Sources :");
  gtk_widget_ref (l_usdsn);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_usdsn", l_usdsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_usdsn);
  gtk_fixed_put (GTK_FIXED (fixed1), l_usdsn, 8, 8);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_set_uposition (l_usdsn, 12, 35);
  gtk_widget_set_usize (l_usdsn, 120, 16);
#else
  gtk_widget_set_uposition (l_usdsn, 8, 8);
  gtk_widget_set_usize (l_usdsn, 112, 16);
#endif
  gtk_label_set_justify (GTK_LABEL (l_usdsn), GTK_JUSTIFY_LEFT);

  frame1 = gtk_frame_new (NULL);
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame1", frame1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), frame1, 8, 295);
  gtk_widget_set_uposition (frame1, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), frame1, 8, 264);
  gtk_widget_set_uposition (frame1, 8, 264);
#endif
  gtk_widget_set_usize (frame1, 546, 64);

  table1 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table1", table1,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table1), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

#if GTK_CHECK_VERSION(2,0,0)
  style = gtk_widget_get_style (admin);
  pixmap =
      gdk_pixmap_create_from_xpm_d (admin->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) odbc4_xpm);
#else
  style = gtk_widget_get_style (GTK_WIDGET (hwnd));
  pixmap =
      gdk_pixmap_create_from_xpm_d (GTK_WIDGET (hwnd)->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) odbc4_xpm);
#endif
  pixmap1 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap1", pixmap1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap1);
  gtk_table_attach (GTK_TABLE (table1), pixmap1, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  vbuttonbox1 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbuttonbox1", vbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox1);
  gtk_fixed_put (GTK_FIXED (fixed1), vbuttonbox1, 472, 16);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_set_uposition (vbuttonbox1, 472, 52);
#else
  gtk_widget_set_uposition (vbuttonbox1, 472, 16);
#endif
  gtk_widget_set_usize (vbuttonbox1, 85, 135);

  b_add = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szDSNButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_add", b_add,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_remove", b_remove,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_configure", b_configure,
      (GtkDestroyNotify) gtk_widget_unref);
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_test", b_test,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_test);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_test);
  GTK_WIDGET_SET_FLAGS (b_test, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
      'T', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_test, FALSE);

  dsnchoose_t.uadd = b_add;
  dsnchoose_t.uremove = b_remove;
  dsnchoose_t.utest = b_test;
  dsnchoose_t.uconfigure = b_configure;

  udsn = gtk_label_new (szTabNames[0]);
  gtk_widget_ref (udsn);
  gtk_object_set_data_full (GTK_OBJECT (admin), "udsn", udsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (udsn);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 0), udsn);

  /* System DSN panel */
  fixed2 = gtk_fixed_new ();
  gtk_widget_ref (fixed2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed2", fixed2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed2);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed2);
  gtk_container_set_border_width (GTK_CONTAINER (fixed2), 6);

  scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow2",
      scrolledwindow2, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow2);
  gtk_widget_set_usize (scrolledwindow2, 456, 232);
  gtk_fixed_put (GTK_FIXED (fixed2), scrolledwindow2, 3, 19);

  clist2 = gtk_clist_new (3);
  gtk_widget_ref (clist2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist2", clist2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist2);
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), clist2);
  gtk_clist_set_column_width (GTK_CLIST (clist2), 0, 100);
  gtk_clist_set_column_width (GTK_CLIST (clist2), 1, 163);
  gtk_clist_set_column_width (GTK_CLIST (clist2), 2, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist2));

  l_name = gtk_label_new (szDSNColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist2), 0, l_name);

  l_description = gtk_label_new (szDSNColumnNames[1]);
  gtk_widget_ref (l_description);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_description",
      l_description, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_description);
  gtk_clist_set_column_widget (GTK_CLIST (clist2), 1, l_description);

  l_driver = gtk_label_new (szDSNColumnNames[2]);
  gtk_widget_ref (l_driver);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_driver", l_driver,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_driver);
  gtk_clist_set_column_widget (GTK_CLIST (clist2), 2, l_driver);

  l_sdsn = gtk_label_new ("System Data Sources :");
  gtk_widget_ref (l_sdsn);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_sdsn", l_sdsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_sdsn);
  gtk_fixed_put (GTK_FIXED (fixed2), l_sdsn, 8, 8);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_set_uposition (l_sdsn, 12, 35);
  gtk_widget_set_usize (l_sdsn, 138, 16);
#else
  gtk_widget_set_uposition (l_sdsn, 8, 8);
  gtk_widget_set_usize (l_sdsn, 130, 16);
#endif
  gtk_label_set_justify (GTK_LABEL (l_sdsn), GTK_JUSTIFY_LEFT);

  frame2 = gtk_frame_new (NULL);
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame2", frame2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed2), frame2, 8, 295);
  gtk_widget_set_uposition (frame2, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed2), frame2, 8, 264);
  gtk_widget_set_uposition (frame2, 8, 264);
#endif
  gtk_widget_set_usize (frame2, 546, 64);

  table2 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table2", table2,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table2), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap2 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap2", pixmap2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap2);
  gtk_table_attach (GTK_TABLE (table2), pixmap2, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  vbuttonbox2 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbuttonbox2", vbuttonbox2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox2);
  gtk_fixed_put (GTK_FIXED (fixed2), vbuttonbox2, 472, 16);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_set_uposition (vbuttonbox2, 472, 52);
#else
  gtk_widget_set_uposition (vbuttonbox2, 472, 16);
#endif
  gtk_widget_set_usize (vbuttonbox2, 85, 135);

  b_add = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szDSNButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_add", b_add,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_remove", b_remove,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_configure", b_configure,
      (GtkDestroyNotify) gtk_widget_unref);
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_test", b_test,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_test);
  gtk_container_add (GTK_CONTAINER (vbuttonbox2), b_test);
  GTK_WIDGET_SET_FLAGS (b_test, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
      'T', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_test, FALSE);

  dsnchoose_t.sadd = b_add;
  dsnchoose_t.sremove = b_remove;
  dsnchoose_t.stest = b_test;
  dsnchoose_t.sconfigure = b_configure;

  sdsn = gtk_label_new (szTabNames[1]);
  gtk_widget_ref (sdsn);
  gtk_object_set_data_full (GTK_OBJECT (admin), "sdsn", sdsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sdsn);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 1), sdsn);

  /* File DSN panel */
  fixed3 = gtk_fixed_new ();
  gtk_widget_ref (fixed3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed3", fixed3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed3);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed3);
  gtk_container_set_border_width (GTK_CONTAINER (fixed3), 6);

  l_lookin = gtk_label_new ("Look in : ");
  gtk_widget_ref (l_lookin);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_lookin", l_lookin,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_lookin);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), l_lookin, 16, 44);
  gtk_widget_set_uposition (l_lookin, 16, 44);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), l_lookin, 16, 16);
  gtk_widget_set_uposition (l_lookin, 16, 16);
#endif
  gtk_widget_set_usize (l_lookin, 57, 16);
  gtk_label_set_justify (GTK_LABEL (l_lookin), GTK_JUSTIFY_LEFT);

  optionmenu1 = gtk_option_menu_new ();
  gtk_widget_ref (optionmenu1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "optionmenu1", optionmenu1,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (optionmenu1);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), optionmenu1, 72, 40);
  gtk_widget_set_uposition (optionmenu1, 72, 40);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), optionmenu1, 72, 16);
  gtk_widget_set_uposition (optionmenu1, 72, 16);
#endif
  gtk_widget_set_usize (optionmenu1, 392, 24);
  optionmenu1_menu = gtk_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu1), optionmenu1_menu);

  scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow3", scrolledwindow3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow3);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), scrolledwindow3, 8, 74);
  gtk_widget_set_uposition (scrolledwindow3, 8, 74);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), scrolledwindow3, 8, 48);
  gtk_widget_set_uposition (scrolledwindow3, 8, 48);
#endif
  gtk_widget_set_usize (scrolledwindow3, 224, 176);

  clist3 = gtk_clist_new (1);
  gtk_widget_ref (clist3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist3", clist3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist3);
  gtk_container_add (GTK_CONTAINER (scrolledwindow3), clist3);
  gtk_widget_set_usize (clist3, 144, 168);
  gtk_clist_set_column_width (GTK_CLIST (clist3), 0, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist3));

  l_directory = gtk_label_new ("Directories");
  gtk_widget_ref (l_directory);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_directory", l_directory,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_directory);
  gtk_clist_set_column_widget (GTK_CLIST (clist3), 0, l_directory);
  gtk_label_set_justify (GTK_LABEL (l_directory), GTK_JUSTIFY_LEFT);

  scrolledwindow4 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow4", scrolledwindow4,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow4);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), scrolledwindow4, 240, 74);
  gtk_widget_set_uposition (scrolledwindow4, 240, 74);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), scrolledwindow4, 240, 48);
  gtk_widget_set_uposition (scrolledwindow4, 240, 48);
#endif
  gtk_widget_set_usize (scrolledwindow4, 224, 176);

  clist4 = gtk_clist_new (1);
  gtk_widget_ref (clist4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist4", clist4,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist4);
  gtk_container_add (GTK_CONTAINER (scrolledwindow4), clist4);
  gtk_clist_set_column_width (GTK_CLIST (clist4), 0, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist4));

  l_files = gtk_label_new ("Files");
  gtk_widget_ref (l_files);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_files", l_files,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_files);
  gtk_clist_set_column_widget (GTK_CLIST (clist4), 0, l_files);
  gtk_label_set_justify (GTK_LABEL (l_files), GTK_JUSTIFY_LEFT);

  t_fileselected = gtk_entry_new ();
  gtk_widget_ref (t_fileselected);
  gtk_object_set_data_full (GTK_OBJECT (admin), "t_fileselected", t_fileselected,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_fileselected);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), t_fileselected, 95, 260);
  gtk_widget_set_uposition (t_fileselected, 95, 260);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), t_fileselected, 95, 234);
  gtk_widget_set_uposition (t_fileselected, 95, 234);
#endif
  gtk_widget_set_usize (t_fileselected, 370, 22);

  frame3 = gtk_frame_new (NULL);
  gtk_widget_ref (frame3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame3", frame3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame3);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), frame3, 8, 295);
  gtk_widget_set_uposition (frame3, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), frame3, 8, 264);
  gtk_widget_set_uposition (frame3, 8, 264);
#endif
  gtk_widget_set_usize (frame3, 546, 64);

  table3 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table3", table3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table3);
  gtk_container_add (GTK_CONTAINER (frame3), table3);
  gtk_container_set_border_width (GTK_CONTAINER (table3), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table3), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table3), 6);

  l_explanation = gtk_label_new ("Select the file data source that describes the driver that you wish to\nconnect to. You can use any file data source that refers to an ODBC\ndriver which is installed on your machine.");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation", l_explanation,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table3), l_explanation, 1, 2, 0, 1,
     (GtkAttachOptions) (0),
     (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap3 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap3", pixmap3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap3);
  gtk_table_attach (GTK_TABLE (table3), pixmap3, 0, 1, 0, 1,
     (GtkAttachOptions) (GTK_FILL),
     (GtkAttachOptions) (GTK_FILL), 0, 0);

  l_selected = gtk_label_new ("File selected : ");
  gtk_widget_ref (l_selected);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_selected", l_selected,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_selected);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed3), l_selected, 8, 263);
  gtk_widget_set_uposition (l_selected, 8, 263);
#else
  gtk_fixed_put (GTK_FIXED (fixed3), l_selected, 8, 237);
  gtk_widget_set_uposition (l_selected, 8, 237);
#endif
  gtk_widget_set_usize (l_selected, 85, 16);

  vbuttonbox3 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbuttonbox3", vbuttonbox3,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox3);
  gtk_fixed_put (GTK_FIXED (fixed3), vbuttonbox3, 472, 16);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_set_uposition (vbuttonbox3, 472, 52);
#else
  gtk_widget_set_uposition (vbuttonbox3, 472, 16);
#endif
  gtk_widget_set_usize (vbuttonbox3, 85, 165);

  b_add = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
     szDSNButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
     b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_add", b_add,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (vbuttonbox3), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
     'A', GDK_MOD1_MASK,
     GTK_ACCEL_VISIBLE);

  b_remove = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_remove)->child),
     szDSNButtons[1]);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
     b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_remove);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_remove", b_remove,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_remove);
  gtk_container_add (GTK_CONTAINER (vbuttonbox3), b_remove);
  GTK_WIDGET_SET_FLAGS (b_remove, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
     'R', GDK_MOD1_MASK,
     GTK_ACCEL_VISIBLE);

  b_configure = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_configure)->child),
     szDSNButtons[2]);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
  b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_configure);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_configure", b_configure,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_configure);
  gtk_container_add (GTK_CONTAINER (vbuttonbox3), b_configure);
  GTK_WIDGET_SET_FLAGS (b_configure, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
     'C', GDK_MOD1_MASK,
     GTK_ACCEL_VISIBLE);

  b_test = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_test)->child),
     szDSNButtons[3]);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
     b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_test);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_test", b_test,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_test);
  gtk_container_add (GTK_CONTAINER (vbuttonbox3), b_test);
  GTK_WIDGET_SET_FLAGS (b_test, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_test, "clicked", accel_group,
     'T', GDK_MOD1_MASK,
     GTK_ACCEL_VISIBLE);

  b_setdir = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_setdir)->child),
     szDSNButtons[4]);
  gtk_widget_add_accelerator (b_setdir, "clicked", accel_group,
     b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_setdir);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_setdir", b_setdir,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_setdir);
  gtk_container_add (GTK_CONTAINER (vbuttonbox3), b_setdir);
  GTK_WIDGET_SET_FLAGS (b_setdir, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_setdir, "clicked", accel_group,
     'S', GDK_MOD1_MASK,
     GTK_ACCEL_VISIBLE);

  dsnchoose_t.fadd = b_add; dsnchoose_t.fremove = b_remove; dsnchoose_t.fconfigure = b_configure;
  dsnchoose_t.ftest = b_test; dsnchoose_t.dir_list = clist3; dsnchoose_t.dir_combo = optionmenu1;
  dsnchoose_t.file_list = clist4; dsnchoose_t.file_entry = t_fileselected;
  dsnchoose_t.fsetdir = b_setdir;

  fdsn = gtk_label_new (szTabNames[2]);
  gtk_widget_ref (fdsn);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fdsn", fdsn,
     (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fdsn);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 2), fdsn);

  dsnchoose_t.udsnlist = clist1;
  dsnchoose_t.sdsnlist = clist2;
  dsnchoose_t.type_dsn = 0;
  dsnchoose_t.mainwnd = admin;

  /* ODBC Drivers panel */
  fixed4 = gtk_fixed_new ();
  gtk_widget_ref (fixed4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed4", fixed4,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed4);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed4);
  gtk_container_set_border_width (GTK_CONTAINER (fixed4), 6);

  scrolledwindow5 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow5",
      scrolledwindow5, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow5);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed4), scrolledwindow5, 8, 50);
  gtk_widget_set_uposition (scrolledwindow5, 8, 50);
#else
  gtk_fixed_put (GTK_FIXED (fixed4), scrolledwindow5, 8, 24);
  gtk_widget_set_uposition (scrolledwindow5, 8, 24);
#endif
  gtk_widget_set_usize (scrolledwindow5, 536, 200);

  clist5 = gtk_clist_new (4);
  gtk_widget_ref (clist5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist5", clist5,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist5);
  gtk_container_add (GTK_CONTAINER (scrolledwindow5), clist5);
  gtk_clist_set_column_width (GTK_CLIST (clist5), 0, 188);
  gtk_clist_set_column_width (GTK_CLIST (clist5), 1, 170);
  gtk_clist_set_column_width (GTK_CLIST (clist5), 2, 134);
  gtk_clist_set_column_width (GTK_CLIST (clist5), 3, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist5));

  l_name = gtk_label_new (szDriverColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist5), 0, l_name);

  l_file = gtk_label_new (szDriverColumnNames[1]);
  gtk_widget_ref (l_file);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_file", l_file,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_file);
  gtk_clist_set_column_widget (GTK_CLIST (clist5), 1, l_file);

  l_date = gtk_label_new (szDriverColumnNames[2]);
  gtk_widget_ref (l_date);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_date", l_date,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_date);
  gtk_clist_set_column_widget (GTK_CLIST (clist5), 2, l_date);

  l_size = gtk_label_new (szDriverColumnNames[3]);
  gtk_widget_ref (l_size);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_size", l_size,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_size);
  gtk_clist_set_column_widget (GTK_CLIST (clist5), 3, l_size);

  frame4 = gtk_frame_new (NULL);
  gtk_widget_ref (frame4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame4", frame4,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame4);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed4), frame4, 8, 295);
  gtk_widget_set_uposition (frame4, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed4), frame4, 8, 264);
  gtk_widget_set_uposition (frame4, 8, 264);
#endif
  gtk_widget_set_usize (frame4, 546, 64);

  table4 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table4", table4,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table4);
  gtk_container_add (GTK_CONTAINER (frame4), table4);
  gtk_container_set_border_width (GTK_CONTAINER (table4), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table4), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table4), 6);

  l_explanation =
      gtk_label_new
      ("An ODBC driver allows ODBC-enabled programs to get information from\nODBC data sources. To install new drivers, use the driver's setup\nprogram if available, or add it with the 'Add' button.");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table4), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap4 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap4", pixmap4,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap4);
  gtk_table_attach (GTK_TABLE (table4), pixmap4, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  l_drivers =
      gtk_label_new ("ODBC Drivers that are installed on your system : ");
  gtk_widget_ref (l_drivers);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_drivers", l_drivers,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_drivers);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed4), l_drivers, 8, 32);
  gtk_widget_set_uposition (l_drivers, 8, 32);
#else
  gtk_fixed_put (GTK_FIXED (fixed4), l_drivers, 8, 8);
  gtk_widget_set_uposition (l_drivers, 8, 8);
#endif
  gtk_widget_set_usize (l_drivers, 290, 16);
  gtk_label_set_justify (GTK_LABEL (l_drivers), GTK_JUSTIFY_LEFT);

  hbuttonbox2 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "hbuttonbox2", hbuttonbox2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox2);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed4), hbuttonbox2, 16, 256);
  gtk_widget_set_uposition (hbuttonbox2, 16, 256);
#else
  gtk_fixed_put (GTK_FIXED (fixed4), hbuttonbox2, 16, 227);
  gtk_widget_set_uposition (hbuttonbox2, 16, 227);
#endif
  gtk_widget_set_usize (hbuttonbox2, 530, 33);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox2), 6);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox2), 64, -1);

  b_add = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szDriverButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_add", b_add,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (hbuttonbox2), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_remove = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_remove)->child),
      szDriverButtons[1]);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_remove);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_remove", b_remove,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_remove);
  gtk_container_add (GTK_CONTAINER (hbuttonbox2), b_remove);
  GTK_WIDGET_SET_FLAGS (b_remove, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_remove, "clicked", accel_group,
      'R', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_configure = gtk_button_new_with_label ("");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_configure)->child),
      szDriverButtons[2]);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_configure);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_configure", b_configure,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_configure);
  gtk_container_add (GTK_CONTAINER (hbuttonbox2), b_configure);
  GTK_WIDGET_SET_FLAGS (b_configure, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_configure, "clicked", accel_group,
      'G', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  fdrivers = gtk_label_new (szTabNames[3]);
  gtk_widget_ref (fdrivers);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fdrivers", fdrivers,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fdrivers);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 3), fdrivers);

  driverchoose_t.driverlist = clist5;
  driverchoose_t.mainwnd = admin;
  driverchoose_t.b_add = b_add;
  driverchoose_t.b_remove = b_remove;
  driverchoose_t.b_configure = b_configure;

  /* Connection pooling panel */
  fixed5 = gtk_fixed_new ();
  gtk_widget_ref (fixed5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed5", fixed5,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed5);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed5);
  gtk_container_set_border_width (GTK_CONTAINER (fixed5), 6);

  frame10 = gtk_frame_new (NULL);
  gtk_widget_ref (frame10);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame10", frame10,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame10);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed5), frame10, 8, 295);
  gtk_widget_set_uposition (frame10, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed5), frame10, 8, 264);
  gtk_widget_set_uposition (frame10, 8, 264);
#endif
  gtk_widget_set_usize (frame10, 546, 64);

  table9 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table9);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table9", table9,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table9);
  gtk_container_add (GTK_CONTAINER (frame10), table9);
  gtk_container_set_border_width (GTK_CONTAINER (table9), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table9), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table9), 6);

  l_explanation =
      gtk_label_new
      ("Connection pooling allows an application to reuse open connection\nhandles, which saves round-trips to the server.\n");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table9), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap7 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap7);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap7", pixmap7,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap7);
  gtk_table_attach (GTK_TABLE (table9), pixmap7, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  frame12 = gtk_frame_new (" PerfMon ");
  gtk_widget_ref (frame12);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame12", frame12,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame12);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed5), frame12, 424, 32);
  gtk_widget_set_uposition (frame12, 424, 32);
#else
  gtk_fixed_put (GTK_FIXED (fixed5), frame12, 424, 8);
  gtk_widget_set_uposition (frame12, 424, 8);
#endif
  gtk_widget_set_usize (frame12, 128, 72);

  vbox5 = gtk_vbox_new (TRUE, 6);
  gtk_widget_ref (vbox5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbox5", vbox5,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox5);
  gtk_container_add (GTK_CONTAINER (frame12), vbox5);
  gtk_container_set_border_width (GTK_CONTAINER (vbox5), 6);

  br_enable = gtk_radio_button_new_with_label (vbox5_group, "");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (br_enable)->child),
      "_Enable");
  gtk_widget_add_accelerator (br_enable, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  vbox5_group = gtk_radio_button_group (GTK_RADIO_BUTTON (br_enable));
  gtk_widget_ref (br_enable);
  gtk_object_set_data_full (GTK_OBJECT (admin), "br_enable", br_enable,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (br_enable);
  gtk_box_pack_start (GTK_BOX (vbox5), br_enable, FALSE, FALSE, 0);

  br_disable = gtk_radio_button_new_with_label (vbox5_group, "");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (br_disable)->child),
      "_Disable");
  gtk_widget_add_accelerator (br_disable, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  vbox5_group = gtk_radio_button_group (GTK_RADIO_BUTTON (br_disable));
  gtk_widget_ref (br_disable);
  gtk_object_set_data_full (GTK_OBJECT (admin), "br_disable", br_disable,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (br_disable);
  gtk_box_pack_start (GTK_BOX (vbox5), br_disable, FALSE, FALSE, 0);
  gtk_widget_add_accelerator (br_disable, "clicked", accel_group,
      'D', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (br_disable), TRUE);

  frame13 = gtk_frame_new (" Retry Wait time ");
  gtk_widget_ref (frame13);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame13", frame13,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame13);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed5), frame13, 424, 112);
  gtk_widget_set_uposition (frame13, 424, 112);
#else
  gtk_fixed_put (GTK_FIXED (fixed5), frame13, 424, 88);
  gtk_widget_set_uposition (frame13, 424, 88);
#endif
  gtk_widget_set_usize (frame13, 128, 80);

  vbox4 = gtk_vbox_new (TRUE, 0);
  gtk_widget_ref (vbox4);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbox4", vbox4,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox4);
  gtk_container_add (GTK_CONTAINER (frame13), vbox4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox4), 6);

  t_retrywait = gtk_entry_new ();
  gtk_widget_ref (t_retrywait);
  gtk_object_set_data_full (GTK_OBJECT (admin), "t_retrywait", t_retrywait,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_retrywait);
  gtk_box_pack_start (GTK_BOX (vbox4), t_retrywait, FALSE, FALSE, 0);

  alignment1 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_ref (alignment1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "alignment1", alignment1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (alignment1);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed5), alignment1, 8, 32);
  gtk_widget_set_uposition (alignment1, 8, 32);
#else
  gtk_fixed_put (GTK_FIXED (fixed5), alignment1, 8, 8);
  gtk_widget_set_uposition (alignment1, 8, 8);
#endif
  gtk_widget_set_usize (alignment1, 410, 248);

  frame11 = gtk_frame_new (" Connection Pooling Timeout ");
  gtk_widget_ref (frame11);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame11", frame11,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame11);
  gtk_container_add (GTK_CONTAINER (alignment1), frame11);
  gtk_widget_set_usize (frame11, 368, 248);

  fixed8 = gtk_fixed_new ();
  gtk_widget_ref (fixed8);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed8", fixed8,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed8);
  gtk_container_add (GTK_CONTAINER (frame11), fixed8);
  gtk_container_set_border_width (GTK_CONTAINER (fixed8), 6);

  l_odbcdrivers = gtk_label_new ("ODBC Drivers :");
  gtk_widget_ref (l_odbcdrivers);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_odbcdrivers",
      l_odbcdrivers, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_odbcdrivers);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed8), l_odbcdrivers, 16, 51);
  gtk_widget_set_uposition (l_odbcdrivers, 16, 51);
#else
  gtk_fixed_put (GTK_FIXED (fixed8), l_odbcdrivers, 8, 3);
  gtk_widget_set_uposition (l_odbcdrivers, 8, 3);
#endif
  gtk_widget_set_usize (l_odbcdrivers, 89, 16);

  scrolledwindow7 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow7);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow7",
      scrolledwindow7, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow7);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed8), scrolledwindow7, 16, 68);
  gtk_widget_set_uposition (scrolledwindow7, 16, 68);
#else
  gtk_fixed_put (GTK_FIXED (fixed8), scrolledwindow7, 8, 24);
  gtk_widget_set_uposition (scrolledwindow7, 8, 24);
#endif
  gtk_widget_set_usize (scrolledwindow7, 392, 200);

  clist7 = gtk_clist_new (3);
  gtk_widget_ref (clist7);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist7", clist7,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist7);
  gtk_container_add (GTK_CONTAINER (scrolledwindow7), clist7);
  gtk_clist_set_column_width (GTK_CLIST (clist7), 0, 210);
  gtk_clist_set_column_width (GTK_CLIST (clist7), 1, 80);
  gtk_clist_set_column_width (GTK_CLIST (clist7), 2, 150);
  gtk_clist_column_titles_show (GTK_CLIST (clist7));

  l_name = gtk_label_new (szCpLabels[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist7), 0, l_name);

  l_poolto = gtk_label_new (szCpLabels[1]);
  gtk_widget_ref (l_poolto);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_poolto", l_poolto,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_poolto);
  gtk_clist_set_column_widget (GTK_CLIST (clist7), 1, l_poolto);

  l_poolprobe = gtk_label_new (szCpLabels[2]);
  gtk_widget_ref (l_poolprobe);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_poolprobe", l_poolprobe,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_poolprobe);
  gtk_clist_set_column_widget (GTK_CLIST (clist7), 2, l_poolprobe);

  fpooling = gtk_label_new (szTabNames[4]);
  gtk_widget_ref (fpooling);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fpooling", fpooling,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fpooling);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 4), fpooling);

  connectionpool_t.driverlist = clist7;
  connectionpool_t.enperfmon_rb = br_enable;
  connectionpool_t.disperfmon_rb = br_disable;
  connectionpool_t.retwait_entry = t_retrywait;
  connectionpool_t.changed = FALSE;
  connectionpool_t.mainwnd = admin;

  /* Tracing panel */
  fixed6 = gtk_fixed_new ();
  gtk_widget_ref (fixed6);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed6", fixed6,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed6);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed6);
  gtk_container_set_border_width (GTK_CONTAINER (fixed6), 6);

  frame6 = gtk_frame_new (" When to trace ");
  gtk_widget_ref (frame6);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame6", frame6,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame6);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed6), frame6, 8, 32);
  gtk_widget_set_uposition (frame6, 8, 32);
#else
  gtk_fixed_put (GTK_FIXED (fixed6), frame6, 8, 8);
  gtk_widget_set_uposition (frame6, 8, 8);
#endif
  gtk_widget_set_usize (frame6, 280, 104);

  hbox1 = gtk_hbox_new (TRUE, 0);
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "hbox1", hbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_container_add (GTK_CONTAINER (frame6), hbox1);
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 6);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox2);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbox2", vbox2,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (hbox1), vbox2, TRUE, TRUE, 0);

  b_donottrace = gtk_radio_button_new_with_label (vbox2_group, "");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_donottrace)->child),
      "_Don't trace");
  gtk_widget_add_accelerator (b_donottrace, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  vbox2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (b_donottrace));
  gtk_widget_ref (b_donottrace);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_donottrace", b_donottrace,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_donottrace);
  gtk_box_pack_start (GTK_BOX (vbox2), b_donottrace, FALSE, FALSE, 0);
  gtk_widget_add_accelerator (b_donottrace, "clicked", accel_group,
      'D', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_donottrace), TRUE);

  b_allthetime = gtk_radio_button_new_with_label (vbox2_group, "");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_allthetime)->child),
      "All the t_ime");
  gtk_widget_add_accelerator (b_allthetime, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  vbox2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (b_allthetime));
  gtk_widget_ref (b_allthetime);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_allthetime", b_allthetime,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_allthetime);
  gtk_box_pack_start (GTK_BOX (vbox2), b_allthetime, FALSE, FALSE, 0);
  gtk_widget_add_accelerator (b_allthetime, "clicked", accel_group,
      'I', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_onetime = gtk_radio_button_new_with_label (vbox2_group, "");
  b_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_onetime)->child),
      "One-_time only");
  gtk_widget_add_accelerator (b_onetime, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  vbox2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (b_onetime));
  gtk_widget_ref (b_onetime);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_onetime", b_onetime,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_onetime);
  gtk_box_pack_start (GTK_BOX (vbox2), b_onetime, FALSE, FALSE, 0);
  gtk_widget_add_accelerator (b_onetime, "clicked", accel_group,
      'T', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox3);
  gtk_object_set_data_full (GTK_OBJECT (admin), "vbox3", vbox3,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox3);
  gtk_box_pack_start (GTK_BOX (hbox1), vbox3, FALSE, FALSE, 0);

  b_start = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_start)->child),
      "_Apply tracing settings");
  gtk_widget_add_accelerator (b_start, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_start);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_start", b_start,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_start);
  gtk_box_pack_start (GTK_BOX (vbox3), b_start, FALSE, FALSE, 0);
  gtk_widget_add_accelerator (b_start, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  frame7 = gtk_frame_new (" Log file path ");
  gtk_widget_ref (frame7);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame7", frame7,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame7);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed6), frame7, 296, 32);
  gtk_widget_set_uposition (frame7, 296, 32);
#else
  gtk_fixed_put (GTK_FIXED (fixed6), frame7, 296, 8);
  gtk_widget_set_uposition (frame7, 296, 8);
#endif
  gtk_widget_set_usize (frame7, 256, 104);

  table6 = gtk_table_new (2, 1, TRUE);
  gtk_widget_ref (table6);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table6", table6,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table6);
  gtk_container_add (GTK_CONTAINER (frame7), table6);
  gtk_container_set_border_width (GTK_CONTAINER (table6), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table6), 6);

  t_logfile = gtk_entry_new ();
  gtk_widget_ref (t_logfile);
  gtk_object_set_data_full (GTK_OBJECT (admin), "t_logfile", t_logfile,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_logfile);
  gtk_table_attach (GTK_TABLE (table6), t_logfile, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
      (GtkAttachOptions) (0), 0, 0);

  b_browse = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_browse)->child),
      "_Browse");
  gtk_widget_add_accelerator (b_browse, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_browse);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_browse", b_browse,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_browse);
  gtk_table_attach (GTK_TABLE (table6), b_browse, 0, 1, 1, 2,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_add_accelerator (b_browse, "clicked", accel_group,
      'B', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  frame8 = gtk_frame_new (" Custom trace library ");
  gtk_widget_ref (frame8);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame8", frame8,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame8);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed6), frame8, 8, 120);
  gtk_widget_set_uposition (frame8, 8, 144);
#else
  gtk_fixed_put (GTK_FIXED (fixed6), frame8, 8, 144);
  gtk_widget_set_uposition (frame8, 8, 120);
#endif
  gtk_widget_set_usize (frame8, 280, 96);

  table7 = gtk_table_new (2, 1, TRUE);
  gtk_widget_ref (table7);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table7", table7,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table7);
  gtk_container_add (GTK_CONTAINER (frame8), table7);
  gtk_container_set_border_width (GTK_CONTAINER (table7), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table7), 6);

  t_tracelib = gtk_entry_new ();
  gtk_widget_ref (t_tracelib);
  gtk_object_set_data_full (GTK_OBJECT (admin), "t_tracelib", t_tracelib,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_tracelib);
  gtk_table_attach (GTK_TABLE (table7), t_tracelib, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
      (GtkAttachOptions) (0), 0, 0);

  b_select = gtk_button_new_with_label ("");
  b_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_select)->child),
      "_Select library");
  gtk_widget_add_accelerator (b_select, "clicked", accel_group,
      b_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_select);
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_select", b_select,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_select);
  gtk_table_attach (GTK_TABLE (table7), b_select, 0, 1, 1, 2,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_add_accelerator (b_select, "clicked", accel_group,
      'S', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  frame5 = gtk_frame_new (NULL);
  gtk_widget_ref (frame5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame5", frame5,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame5);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed6), frame5, 8, 295);
  gtk_widget_set_uposition (frame5, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed6), frame5, 8, 264);
  gtk_widget_set_uposition (frame5, 8, 264);
#endif
  gtk_widget_set_usize (frame5, 546, 64);

  table5 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table5", table5,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table5);
  gtk_container_add (GTK_CONTAINER (frame5), table5);
  gtk_container_set_border_width (GTK_CONTAINER (table5), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table5), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table5), 6);

  l_explanation =
      gtk_label_new
      ("ODBC tracing allows you to create logs of the calls to ODBC drivers for\nuse by support personnel or to aid you in debugging your applications.\nNote: log files can become very large.");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table5), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap5 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap5);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap5", pixmap5,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap5);
  gtk_table_attach (GTK_TABLE (table5), pixmap5, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  ftracing = gtk_label_new (szTabNames[5]);
  gtk_widget_ref (ftracing);
  gtk_object_set_data_full (GTK_OBJECT (admin), "ftracing", ftracing,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ftracing);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 5), ftracing);

  tracing_t.logfile_entry = t_logfile;
  tracing_t.tracelib_entry = t_tracelib;
  tracing_t.b_start_stop = b_start;
  tracing_t.donttrace_rb = b_donottrace;
  tracing_t.allthetime_rb = b_allthetime;
  tracing_t.onetime_rb = b_onetime;
  tracing_t.changed = FALSE;

  /* About panel */
  fixed7 = gtk_fixed_new ();
  gtk_widget_ref (fixed7);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fixed7", fixed7,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed7);
  gtk_container_add (GTK_CONTAINER (notebook1), fixed7);
  gtk_container_set_border_width (GTK_CONTAINER (fixed7), 6);

  frame9 = gtk_frame_new (NULL);
  gtk_widget_ref (frame9);
  gtk_object_set_data_full (GTK_OBJECT (admin), "frame9", frame9,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame9);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed7), frame9, 8, 295);
  gtk_widget_set_uposition (frame9, 8, 295);
#else
  gtk_fixed_put (GTK_FIXED (fixed7), frame9, 8, 264);
  gtk_widget_set_uposition (frame9, 8, 264);
#endif
  gtk_widget_set_usize (frame9, 546, 64);

  table8 = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (table8);
  gtk_object_set_data_full (GTK_OBJECT (admin), "table8", table8,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table8);
  gtk_container_add (GTK_CONTAINER (frame9), table8);
  gtk_container_set_border_width (GTK_CONTAINER (table8), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table8), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table8), 6);

  l_explanation =
      gtk_label_new
      ("ODBC is a programming interface that enables applications to access\ndata in database management systems that use Structured Query\nLanguage (SQL) as a data access standard.");
  gtk_widget_ref (l_explanation);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_explanation",
      l_explanation, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_explanation);
  gtk_table_attach (GTK_TABLE (table8), l_explanation, 1, 2, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_explanation), GTK_JUSTIFY_LEFT);

  pixmap6 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap6);
  gtk_object_set_data_full (GTK_OBJECT (admin), "pixmap6", pixmap6,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap6);
  gtk_table_attach (GTK_TABLE (table8), pixmap6, 0, 1, 0, 1,
      (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

  l_about = gtk_label_new ("ODBC components installed on your system : ");
  gtk_widget_ref (l_about);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_about", l_about,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_about);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed7), l_about, 8, 32);
  gtk_widget_set_uposition (l_about, 8, 32);
#else
  gtk_fixed_put (GTK_FIXED (fixed7), l_about, 8, 8);
  gtk_widget_set_uposition (l_about, 8, 8);
#endif
  gtk_widget_set_usize (l_about, 280, 16);
  gtk_label_set_justify (GTK_LABEL (l_about), GTK_JUSTIFY_LEFT);

  scrolledwindow6 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow6);
  gtk_object_set_data_full (GTK_OBJECT (admin), "scrolledwindow6",
      scrolledwindow6, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow6);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed7), scrolledwindow6, 8, 48);
  gtk_widget_set_uposition (scrolledwindow6, 8, 48);
#else
  gtk_fixed_put (GTK_FIXED (fixed7), scrolledwindow6, 8, 24);
  gtk_widget_set_uposition (scrolledwindow6, 8, 24);
#endif
  gtk_widget_set_usize (scrolledwindow6, 536, 232);

  clist6 = gtk_clist_new (5);
  gtk_widget_ref (clist6);
  gtk_object_set_data_full (GTK_OBJECT (admin), "clist6", clist6,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist6);
  gtk_container_add (GTK_CONTAINER (scrolledwindow6), clist6);
  gtk_clist_set_column_width (GTK_CLIST (clist6), 0, 80);
  gtk_clist_set_column_width (GTK_CLIST (clist6), 1, 80);
  gtk_clist_set_column_width (GTK_CLIST (clist6), 2, 131);
  gtk_clist_set_column_width (GTK_CLIST (clist6), 3, 91);
  gtk_clist_set_column_width (GTK_CLIST (clist6), 4, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist6));

  l_name = gtk_label_new (szDriverColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist6), 0, l_name);

  l_version = gtk_label_new (szDriverColumnNames[4]);
  gtk_widget_ref (l_version);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_version", l_version,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_version);
  gtk_clist_set_column_widget (GTK_CLIST (clist6), 1, l_version);

  l_file = gtk_label_new (szDriverColumnNames[1]);
  gtk_widget_ref (l_file);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_file", l_file,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_file);
  gtk_clist_set_column_widget (GTK_CLIST (clist6), 2, l_file);

  l_date = gtk_label_new (szDriverColumnNames[2]);
  gtk_widget_ref (l_date);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_date", l_date,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_date);
  gtk_clist_set_column_widget (GTK_CLIST (clist6), 3, l_date);

  l_size = gtk_label_new (szDriverColumnNames[3]);
  gtk_widget_ref (l_size);
  gtk_object_set_data_full (GTK_OBJECT (admin), "l_size", l_size,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_size);
  gtk_clist_set_column_widget (GTK_CLIST (clist6), 4, l_size);

  fabout = gtk_label_new (szTabNames[6]);
  gtk_widget_ref (fabout);
  gtk_object_set_data_full (GTK_OBJECT (admin), "fabout", fabout,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fabout);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1),
      gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 6), fabout);

  dialog_action_area1 = GTK_DIALOG (admin)->action_area;
  gtk_object_set_data (GTK_OBJECT (admin), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (admin), "hbuttonbox1", hbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_ok", b_ok,
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
  gtk_object_set_data_full (GTK_OBJECT (admin), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Notebook events */
  gtk_signal_connect_after (GTK_OBJECT (notebook1), "switch_page",
      GTK_SIGNAL_FUNC (admin_switch_page), inparams);
  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_ok), "clicked",
      GTK_SIGNAL_FUNC (admin_ok_clicked), inparams);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (admin_cancel_clicked), inparams);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (admin), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), inparams);
  gtk_signal_connect (GTK_OBJECT (admin), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* Add user DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.uadd), "clicked",
      GTK_SIGNAL_FUNC (userdsn_add_clicked), &dsnchoose_t);
  /* Remove user DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.uremove), "clicked",
      GTK_SIGNAL_FUNC (userdsn_remove_clicked), &dsnchoose_t);
  /* Test user DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.utest), "clicked",
      GTK_SIGNAL_FUNC (userdsn_test_clicked), &dsnchoose_t);
  /* Configure user DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.uconfigure), "clicked",
      GTK_SIGNAL_FUNC (userdsn_configure_clicked), &dsnchoose_t);
  /* Add system DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.sadd), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_add_clicked), &dsnchoose_t);
  /* Remove system DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.sremove), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_remove_clicked), &dsnchoose_t);
  /* Test system DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.stest), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_test_clicked), &dsnchoose_t);
  /* Configure system DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.sconfigure), "clicked",
      GTK_SIGNAL_FUNC (systemdsn_configure_clicked), &dsnchoose_t);

  /* Add file DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.fadd), "clicked",
      GTK_SIGNAL_FUNC (filedsn_add_clicked),
      &dsnchoose_t);
  /* Remove file DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.fremove), "clicked",
      GTK_SIGNAL_FUNC (filedsn_remove_clicked),
      &dsnchoose_t);
  /* Test file DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.ftest), "clicked",
     GTK_SIGNAL_FUNC (filedsn_test_clicked),
     &dsnchoose_t);
  /* Configure file DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.fconfigure), "clicked",
     GTK_SIGNAL_FUNC (filedsn_configure_clicked),
     &dsnchoose_t);
  /* Configure file DSN button events */
  gtk_signal_connect (GTK_OBJECT (dsnchoose_t.fsetdir), "clicked",
     GTK_SIGNAL_FUNC (filedsn_setdir_clicked),
     &dsnchoose_t);
  /* Add driver button events */
  gtk_signal_connect (GTK_OBJECT (driverchoose_t.b_add), "clicked",
      GTK_SIGNAL_FUNC (driver_add_clicked), &driverchoose_t);
  /* Remove driver button events */
  gtk_signal_connect (GTK_OBJECT (driverchoose_t.b_remove), "clicked",
      GTK_SIGNAL_FUNC (driver_remove_clicked), &driverchoose_t);
  /* Configure driver button events */
  gtk_signal_connect (GTK_OBJECT (driverchoose_t.b_configure), "clicked",
      GTK_SIGNAL_FUNC (driver_configure_clicked), &driverchoose_t);
  /* User DSN list events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (userdsn_list_select), &dsnchoose_t);
  gtk_signal_connect (GTK_OBJECT (clist1), "unselect_row",
      GTK_SIGNAL_FUNC (userdsn_list_unselect), &dsnchoose_t);
  /* System DSN list events */
  gtk_signal_connect (GTK_OBJECT (clist2), "select_row",
      GTK_SIGNAL_FUNC (systemdsn_list_select), &dsnchoose_t);
  gtk_signal_connect (GTK_OBJECT (clist2), "unselect_row",
      GTK_SIGNAL_FUNC (systemdsn_list_unselect), &dsnchoose_t);
  /* Directories file DSN list events */
  gtk_signal_connect (GTK_OBJECT (clist3), "select_row",
     GTK_SIGNAL_FUNC (filedsn_dirlist_select),
     &dsnchoose_t);
  /* Files file DSN list events */
  gtk_signal_connect (GTK_OBJECT (clist4), "select_row",
     GTK_SIGNAL_FUNC (filedsn_filelist_select),
     &dsnchoose_t);
  gtk_signal_connect (GTK_OBJECT (clist4), "unselect_row",
     GTK_SIGNAL_FUNC (filedsn_filelist_unselect),
     &dsnchoose_t);
  /* Start tracing button events */
  gtk_signal_connect (GTK_OBJECT (b_start), "clicked",
      GTK_SIGNAL_FUNC (tracing_start_clicked), &tracing_t);
  /* Browse file entry events */
  gtk_signal_connect (GTK_OBJECT (b_browse), "clicked",
      GTK_SIGNAL_FUNC (tracing_browse_clicked), &tracing_t);
  /* Driver list events */
  gtk_signal_connect (GTK_OBJECT (clist5), "select_row",
      GTK_SIGNAL_FUNC (driver_list_select), &driverchoose_t);
  gtk_signal_connect (GTK_OBJECT (clist5), "unselect_row",
      GTK_SIGNAL_FUNC (driver_list_unselect), &driverchoose_t);
  /* Connection pooling list events */
  gtk_signal_connect (GTK_OBJECT (clist7), "select_row",
      GTK_SIGNAL_FUNC (cpdriver_list_select), &connectionpool_t);

  gtk_window_add_accel_group (GTK_WINDOW (admin), accel_group);

  SQLSetConfigMode (ODBC_BOTH_DSN);
  if (!SQLGetPrivateProfileString("ODBC", "FileDSNPath", "", 
      dsnchoose_t.curr_dir, sizeof(dsnchoose_t.curr_dir), "odbcinst.ini"))
    strcpy(dsnchoose_t.curr_dir, DEFAULT_FILEDSNPATH);

  adddsns_to_list (clist1, FALSE);
  component_t.componentlist = clist6;

  inparams[0] = &dsnchoose_t;
  inparams[1] = &driverchoose_t;
  inparams[2] = &tracing_t;
  inparams[3] = &component_t;
  inparams[4] = &connectionpool_t;
  inparams[5] = admin;

  gtk_widget_show_all (admin);
  gtk_main ();
}
