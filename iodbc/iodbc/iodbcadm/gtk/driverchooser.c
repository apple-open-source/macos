/*
 *  driverchooser.c
 *
 *  $Id: driverchooser.c,v 1.11 2007/02/02 11:58:14 source Exp $
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
#include <sys/stat.h>
#include <unistd.h>

#include "../gui.h"
#include "img.xpm"

#include "dlf.h"
#include "dlproc.h"


char *szDriverColumnNames[] = {
  "Name",
  "File",
  "Date",
  "Size",
  "Version"
};


void
adddrivers_to_list (GtkWidget *widget, GtkWidget *dlg)
{
  SQLCHAR drvdesc[1024], drvattrs[1024], driver[1024], size[64];
  SQLCHAR *data[4];
  void *handle;
  struct stat _stat;
  SQLSMALLINT len, len1;
  SQLRETURN ret;
  HENV henv, drv_henv;
  HDBC drv_hdbc;
  pSQLGetInfoFunc funcHdl;
  pSQLAllocHandle allocHdl;
  pSQLAllocEnv allocEnvHdl = NULL;
  pSQLAllocConnect allocConnectHdl = NULL;
  pSQLFreeHandle freeHdl;
  pSQLFreeEnv freeEnvHdl;
  pSQLFreeConnect freeConnectHdl;

  if (!GTK_IS_CLIST (widget))
    return;
  gtk_clist_clear (GTK_CLIST (widget));

  /* Create a HENV to get the list of drivers then */
  ret = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
  if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      _iodbcdm_nativeerrorbox (dlg, henv, SQL_NULL_HANDLE, SQL_NULL_HANDLE);
      goto end;
    }

  /* Set the version ODBC API to use */
  SQLSetEnvAttr (henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3,
      SQL_IS_INTEGER);

  /* Get the list of drivers */
  ret =
      SQLDrivers (henv, SQL_FETCH_FIRST, drvdesc,
      sizeof (drvdesc) / sizeof (SQLTCHAR), &len, drvattrs,
      sizeof (drvattrs) / sizeof (SQLTCHAR), &len1);
  if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO
      && ret != SQL_NO_DATA)
    {
      _iodbcdm_nativeerrorbox (dlg, henv, SQL_NULL_HANDLE, SQL_NULL_HANDLE);
      goto error;
    }

  while (ret != SQL_NO_DATA)
    {
      data[0] = drvdesc;

      /* Get the driver library name */
      SQLSetConfigMode (ODBC_BOTH_DSN);
      SQLGetPrivateProfileString (drvdesc, "Driver", "", driver,
	  sizeof (driver) / sizeof (SQLTCHAR), "odbcinst.ini");
      if (driver[0] == '\0')
	SQLGetPrivateProfileString ("Default", "Driver", "", driver,
	    sizeof (driver) / sizeof (SQLTCHAR), "odbcinst.ini");
      if (driver[0] == '\0')
	{
	  data[0] = NULL;
	  goto skip;
	}

      data[1] = driver;

      drv_hdbc = NULL;
      drv_henv = NULL;

      if ((handle = (void *) DLL_OPEN (driver)) != NULL)
	{
	  if ((allocHdl =
		  (pSQLAllocHandle) DLL_PROC (handle,
		      "SQLAllocHandle")) != NULL)
	    {
	      ret = allocHdl (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &drv_henv);
	      if (ret == SQL_ERROR)
		goto nodriverver;
	      ret = allocHdl (SQL_HANDLE_DBC, drv_henv, &drv_hdbc);
	      if (ret == SQL_ERROR)
		goto nodriverver;
	    }
	  else
	    {
	      if ((allocEnvHdl =
		      (pSQLAllocEnv) DLL_PROC (handle,
			  "SQLAllocEnv")) != NULL)
		{
		  ret = allocEnvHdl (&drv_henv);
		  if (ret == SQL_ERROR)
		    goto nodriverver;
		}
	      else
		goto nodriverver;

	      if ((allocConnectHdl =
		      (pSQLAllocConnect) DLL_PROC (handle,
			  "SQLAllocConnect")) != NULL)
		{
		  ret = allocConnectHdl (drv_henv, &drv_hdbc);
		  if (ret == SQL_ERROR)
		    goto nodriverver;
		}
	      else
		goto nodriverver;
	    }

	  if ((funcHdl =
		  (pSQLGetInfoFunc) DLL_PROC (handle, "SQLGetInfo")) != NULL)
	    {
	      /* Retrieve some informations */
	      ret =
		  funcHdl (drv_hdbc, SQL_DRIVER_VER, drvattrs,
		  sizeof (drvattrs), &len);
	      if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		{
		  unsigned int z;
		  /* Drop the description if one provided */
		  for (z = 0; ((char *) drvattrs)[z]; z++)
		    if (((char *) drvattrs)[z] == ' ')
		      ((char *) drvattrs)[z] = '\0';
		  data[2] = drvattrs;
		}
	      else
		goto nodriverver;
	    }
	  else
	    goto nodriverver;
	}
      else
	{
	nodriverver:
	  data[2] = "##.##";
	}

      if (drv_hdbc || drv_henv)
	{
	  if (allocConnectHdl &&
	      (freeConnectHdl =
		  (pSQLFreeConnect) DLL_PROC (handle,
		      "SQLFreeConnect")) != NULL)
	    {
	      freeConnectHdl (drv_hdbc);
	      drv_hdbc = NULL;
	    }

	  if (allocEnvHdl &&
	      (freeEnvHdl =
		  (pSQLFreeEnv) DLL_PROC (handle, "SQLFreeEnv")) != NULL)
	    {
	      freeEnvHdl (drv_henv);
	      drv_henv = NULL;
	    }
	}

      if ((drv_hdbc || drv_henv) &&
	  (freeHdl =
	      (pSQLFreeHandle) DLL_PROC (handle, "SQLFreeHandle")) != NULL)
	{
	  if (drv_hdbc)
	    freeHdl (SQL_HANDLE_DBC, drv_hdbc);
	  if (drv_henv)
	    freeHdl (SQL_HANDLE_ENV, drv_henv);
	}

      if (handle)
        DLL_CLOSE (handle);

      /* Get the size of the driver */
      if (!stat (driver, &_stat))
	{
	  sprintf (size, "%d Kb", (int) (_stat.st_size / 1024));
	  data[3] = size;
	}
      else
	data[3] = "-";

      gtk_clist_append (GTK_CLIST (widget), data);

    skip:
      ret = SQLDrivers (henv, SQL_FETCH_NEXT, drvdesc,
	  sizeof (drvdesc) / sizeof (SQLTCHAR), &len, drvattrs,
	  sizeof (drvattrs) / sizeof (SQLTCHAR), &len1);
      if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO
	  && ret != SQL_NO_DATA)
	{
	  _iodbcdm_nativeerrorbox (dlg, henv, SQL_NULL_HANDLE,
	      SQL_NULL_HANDLE);
	  goto error;
	}
    }

error:
  /* Clean all that */
  SQLFreeHandle (SQL_HANDLE_ENV, henv);

end:
  if (GTK_CLIST (widget)->rows > 0)
    {
      gtk_clist_columns_autosize (GTK_CLIST (widget));
      gtk_clist_sort (GTK_CLIST (widget));
    }
}


static void
driver_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDRIVERCHOOSER *choose_t)
{
  LPSTR driver = NULL;

  if (choose_t)
    {
      /* Get the directory name */
      gtk_clist_get_text (GTK_CLIST (choose_t->driverlist), row, 0, &driver);

      if (driver && event && event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit_by_name (GTK_OBJECT (choose_t->b_finish), "clicked",
	    choose_t);
    }
}


static void
driverchooser_ok_clicked (GtkWidget *widget, TDRIVERCHOOSER *choose_t)
{
  char *szDriver;

  if (choose_t)
    {
      if (GTK_CLIST (choose_t->driverlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->driverlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->driverlist)->selection->
		  data), 0, &szDriver);
	  choose_t->driver = dm_SQL_A2W(szDriver, SQL_NTS);
	}
      else
	choose_t->driver = NULL;

      choose_t->driverlist = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static void
driverchooser_cancel_clicked (GtkWidget *widget, TDRIVERCHOOSER *choose_t)
{
  if (choose_t)
    {
      choose_t->driverlist = NULL;
      choose_t->driver = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget, GdkEvent *event, TDRIVERCHOOSER *choose_t)
{
  driverchooser_cancel_clicked (widget, choose_t);

  return FALSE;
}


void
create_driverchooser (HWND hwnd, TDRIVERCHOOSER *choose_t)
{
  GtkWidget *driverchooser, *dialog_vbox1, *fixed1, *l_diz, *scrolledwindow1;
  GtkWidget *clist1, *l_name, *l_file, *l_date, *l_size, *dialog_action_area1;
  GtkWidget *hbuttonbox1, *b_finish, *b_cancel, *pixmap1;
  guint b_finish_key, b_cancel_key;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkAccelGroup *accel_group;

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return;

  accel_group = gtk_accel_group_new ();

  driverchooser = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (driverchooser), "driverchooser",
      driverchooser);
  gtk_window_set_title (GTK_WINDOW (driverchooser), "Choose an ODBC Driver");
  gtk_window_set_position (GTK_WINDOW (driverchooser), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (driverchooser), TRUE);
  gtk_window_set_policy (GTK_WINDOW (driverchooser), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (driverchooser);
#endif

  dialog_vbox1 = GTK_DIALOG (driverchooser)->vbox;
  gtk_object_set_data (GTK_OBJECT (driverchooser), "dialog_vbox1",
      dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  l_diz =
      gtk_label_new
      ("Select a driver for which you want to setup a data source.");
  gtk_widget_ref (l_diz);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "l_diz", l_diz,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_diz);
  gtk_fixed_put (GTK_FIXED (fixed1), l_diz, 168, 16);
  gtk_widget_set_uposition (l_diz, 168, 16);
  gtk_widget_set_usize (l_diz, 332, 16);
  gtk_label_set_justify (GTK_LABEL (l_diz), GTK_JUSTIFY_LEFT);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 168, 32);
  gtk_widget_set_uposition (scrolledwindow1, 168, 32);
  gtk_widget_set_usize (scrolledwindow1, 332, 248);

  clist1 = gtk_clist_new (4);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "clist1", clist1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clist1);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 165);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 118);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 2, 80);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 3, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_name = gtk_label_new (szDriverColumnNames[0]);
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_name);

  l_file = gtk_label_new (szDriverColumnNames[1]);
  gtk_widget_ref (l_file);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "l_file", l_file,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_file);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_file);

  l_date = gtk_label_new (szDriverColumnNames[2]);
  gtk_widget_ref (l_date);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "l_date", l_date,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_date);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 2, l_date);

  l_size = gtk_label_new (szDriverColumnNames[3]);
  gtk_widget_ref (l_size);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "l_size", l_size,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_size);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 3, l_size);

#if GTK_CHECK_VERSION(2,0,0)
  style = gtk_widget_get_style (driverchooser);
  pixmap =
      gdk_pixmap_create_from_xpm_d (driverchooser->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
#else
  style = gtk_widget_get_style (GTK_WIDGET (hwnd));
  pixmap =
      gdk_pixmap_create_from_xpm_d (GTK_WIDGET (hwnd)->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
#endif

  pixmap1 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap1);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "pixmap1", pixmap1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap1);
  gtk_fixed_put (GTK_FIXED (fixed1), pixmap1, 16, 16);
  gtk_widget_set_uposition (pixmap1, 16, 16);
  gtk_widget_set_usize (pixmap1, 136, 264);

  dialog_action_area1 = GTK_DIALOG (driverchooser)->action_area;
  gtk_object_set_data (GTK_OBJECT (driverchooser), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "hbuttonbox1",
      hbuttonbox1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE,
      0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 10);

  b_finish = gtk_button_new_with_label ("");
  b_finish_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_finish)->child),
      "_Finish");
  gtk_widget_add_accelerator (b_finish, "clicked", accel_group,
      b_finish_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_finish);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "b_finish", b_finish,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_finish);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_finish);
  GTK_WIDGET_SET_FLAGS (b_finish, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_finish, "clicked", accel_group,
      'F', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_cancel = gtk_button_new_with_label ("");
  b_cancel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_cancel)->child),
      "_Cancel");
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      b_cancel_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_cancel);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_finish), "clicked",
      GTK_SIGNAL_FUNC (driverchooser_ok_clicked), choose_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (driverchooser_cancel_clicked), choose_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (driverchooser), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), choose_t);
  gtk_signal_connect (GTK_OBJECT (driverchooser), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* Driver list events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (driver_list_select), choose_t);

  gtk_window_add_accel_group (GTK_WINDOW (driverchooser), accel_group);

  adddrivers_to_list (clist1, driverchooser);

  choose_t->driverlist = clist1;
  choose_t->driver = NULL;
  choose_t->mainwnd = driverchooser;
  choose_t->b_finish = b_finish;

  gtk_widget_show_all (driverchooser);
  gtk_main ();
}



static void
fdriverchooser_switch_page (GtkNotebook * notebook, GtkNotebookPage * page,
    gint page_num, TFDRIVERCHOOSER * choose_t)
{
  guint len;
  char  buff[1024];
  char *dsn = {""};
  char *drv = {""};
  char *curr;

  if (choose_t)
    {
      switch (page_num)
	{
	case 0:
	  if (choose_t->b_back)
	    gtk_widget_set_sensitive (choose_t->b_back, FALSE);
	  if (choose_t->b_continue)
            gtk_label_parse_uline(GTK_LABEL(GTK_BIN (choose_t->b_continue)->child),
               "Continue");
	  break;

	case 1:
	  if (choose_t->driverlist && choose_t->tab_panel && GTK_CLIST (choose_t->driverlist)->selection == NULL)
	    {
	      _iodbcdm_messagebox(choose_t->mainwnd, NULL, "Driver wasn't selected!");
              gtk_notebook_set_page (GTK_NOTEBOOK (choose_t->tab_panel), 0);
              break;
	    }
	  if (choose_t->b_back)
	    gtk_widget_set_sensitive (choose_t->b_back, TRUE);
	  if (choose_t->b_continue)
            gtk_label_parse_uline(GTK_LABEL(GTK_BIN (choose_t->b_continue)->child),
               "Continue");
	  break;
	case 2:
	  if (choose_t->driverlist && choose_t->tab_panel && choose_t->dsn_entry)
	    {
              if (GTK_CLIST (choose_t->driverlist)->selection != NULL)
                {
	          gtk_clist_get_text (GTK_CLIST (choose_t->driverlist),
	            GPOINTER_TO_INT(GTK_CLIST(choose_t->driverlist)->selection->data),
	            0, &drv);
	        }
	      else
	        {
	          _iodbcdm_messagebox(choose_t->mainwnd, NULL, "Driver wasn't selected!");
                  gtk_notebook_set_page (GTK_NOTEBOOK (choose_t->tab_panel), 0);
                  break;
	        }

              dsn = gtk_entry_get_text(GTK_ENTRY(choose_t->dsn_entry));
              if (strlen(dsn) < 1)
                {
	          _iodbcdm_messagebox(choose_t->mainwnd, NULL, "Enter File DSN Name...");
                  gtk_notebook_set_page (GTK_NOTEBOOK (choose_t->tab_panel), 1);
                  break;
                }
            }

	  if (choose_t->b_back)
	    gtk_widget_set_sensitive (choose_t->b_back, TRUE);
	  if (choose_t->b_continue)
            gtk_label_parse_uline(GTK_LABEL(GTK_BIN (choose_t->b_continue)->child),
                 "Finish");

          if (choose_t->mess_entry)
            {
#if GTK_CHECK_VERSION(2,0,0)
              GtkTextBuffer *gbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(choose_t->mess_entry));
              GtkTextIter *iter;

              gtk_text_buffer_set_text(gbuf, "", 0);

              if (strchr(dsn, '/') != NULL)
                snprintf(buff, sizeof(buff), "Filename: %s\n", dsn);
              else
                snprintf(buff, sizeof(buff), "Filename: %s/%s\n", choose_t->curr_dir, dsn);

              gtk_text_buffer_insert_at_cursor(gbuf, buff, -1);

              snprintf(buff, sizeof(buff), "Driver: %s\n", drv);
              gtk_text_buffer_insert_at_cursor(gbuf, buff, -1);

              gtk_text_buffer_insert_at_cursor(gbuf, "Driver-specific Keywords:\n", -1);

              if (choose_t->attrs)
                {
                  for (curr = choose_t->attrs; *curr; curr += (STRLEN (curr) + 1))
                    {
                      if (!strncasecmp (curr, "PWD=", STRLEN ("PWD=")))
	                continue;

                      if (curr)
                        gtk_text_buffer_insert_at_cursor(gbuf, curr, -1);

                      gtk_text_buffer_insert_at_cursor(gbuf, "\n", -1);
                    }
                }
#else
              gtk_text_set_point(GTK_TEXT(choose_t->mess_entry), 0);
              len = gtk_text_get_length(GTK_TEXT(choose_t->mess_entry));
              gtk_text_forward_delete(GTK_TEXT(choose_t->mess_entry), len);
              gtk_text_insert(GTK_TEXT(choose_t->mess_entry), NULL, NULL, NULL, "File Data Source\n", -1);

              if (strchr(dsn, '/') != NULL)
                snprintf(buff, sizeof(buff), "Filename: %s\n", dsn);
              else
                snprintf(buff, sizeof(buff), "Filename: %s/%s\n", choose_t->curr_dir, dsn);
              gtk_text_insert(GTK_TEXT(choose_t->mess_entry), NULL, NULL, NULL, buff, -1);

              snprintf(buff, sizeof(buff), "Driver: %s\n", drv);
              gtk_text_insert(GTK_TEXT(choose_t->mess_entry), NULL, NULL, NULL, buff, -1);

              gtk_text_insert(GTK_TEXT(choose_t->mess_entry), NULL, NULL, NULL, "Driver-specific Keywords:\n", -1);

              if (choose_t->attrs)
                {
                  for (curr = choose_t->attrs; *curr; curr += (STRLEN (curr) + 1))
                    {
                      if (!strncasecmp (curr, "PWD=", STRLEN ("PWD=")))
                        {
	                  continue;
   	                }
                      gtk_text_insert(GTK_TEXT(choose_t->mess_entry), NULL, NULL, NULL, curr, -1);
                      gtk_text_insert(GTK_TEXT(choose_t->mess_entry), NULL, NULL, NULL, "\n", -1);
                    }
                }
#endif
            }
	  break;
	}
    }
}


static void
fdriverchooser_finish_clicked (GtkWidget *widget, TFDRIVERCHOOSER *choose_t)
{
  char *szDriver;
  char *dsn;
  char buff[1024];

  if (choose_t)
    {
      if (GTK_CLIST (choose_t->driverlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->driverlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->driverlist)->selection->
		  data), 0, &szDriver);
	  choose_t->driver = dm_SQL_A2W(szDriver, SQL_NTS);
	}
      else
	choose_t->driver = NULL;

      dsn = gtk_entry_get_text(choose_t->dsn_entry);
      if (strchr(dsn, '/') != NULL)
        snprintf(buff, sizeof(buff), "%s", dsn);
      else
        snprintf(buff, sizeof(buff), "%s/%s", choose_t->curr_dir, dsn);

      choose_t->dsn = strdup(buff);
      choose_t->driverlist = NULL;
      choose_t->dsn_entry = NULL;
      choose_t->b_back = NULL;
      choose_t->b_continue = NULL;
      choose_t->mess_entry = NULL;
      choose_t->tab_panel = NULL;
      choose_t->browse_sel = NULL;

      choose_t->ok = (choose_t->driver ? TRUE : FALSE);

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static void
fdriverchooser_cancel_clicked (GtkWidget *widget, TFDRIVERCHOOSER *choose_t)
{
  if (choose_t)
    {
      choose_t->driverlist = NULL;
      choose_t->driver = NULL;
      choose_t->ok = FALSE;
      choose_t->driverlist = NULL;
      choose_t->dsn_entry = NULL;
      choose_t->b_back = NULL;
      choose_t->b_continue = NULL;
      choose_t->mess_entry = NULL;
      choose_t->tab_panel = NULL;
      choose_t->browse_sel = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static void
fdriverchooser_next_clicked (GtkWidget * widget, TFDRIVERCHOOSER * choose_t)
{
  if (choose_t)
    {
      gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK (choose_t->tab_panel));
      if (page == 2) /* Last Page */
        fdriverchooser_finish_clicked (widget, choose_t);
      else
        gtk_notebook_next_page (GTK_NOTEBOOK (choose_t->tab_panel));
    }
}

static void
fdriverchooser_prev_clicked (GtkWidget * widget, TFDRIVERCHOOSER * choose_t)
{
  if (choose_t)
    {
      gtk_notebook_prev_page (GTK_NOTEBOOK (choose_t->tab_panel));
    }
}


static gint
fdelete_event (GtkWidget *widget, GdkEvent *event, TFDRIVERCHOOSER *choose_t)
{
  fdriverchooser_cancel_clicked (widget, choose_t);

  return FALSE;
}


static void
fdriver_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TFDRIVERCHOOSER *choose_t)
{
  LPSTR driver = NULL;

  if (choose_t)
    {
      /* Get the directory name */
      gtk_clist_get_text (GTK_CLIST (choose_t->driverlist), row, 0, &driver);

      if (driver && event && event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit_by_name (GTK_OBJECT (choose_t->b_continue), "clicked",
	    choose_t);
    }
}


static void
fdsn_choosen(GtkWidget *widget, TFDRIVERCHOOSER *choose_t)
{
  if (choose_t)
    {
      gtk_entry_set_text (GTK_ENTRY (choose_t->dsn_entry),
	  gtk_file_selection_get_filename (GTK_FILE_SELECTION (choose_t->
		  browse_sel)));
      choose_t->browse_sel = NULL;
    }
}


static void
fdriverchooser_browse_clicked (GtkWidget * widget, TFDRIVERCHOOSER * choose_t)
{
  GtkWidget *filesel;
  char *dsn;
  char buff[1024];

  if (choose_t)
    {
      filesel = gtk_file_selection_new ("Save as ...");
      gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);

      dsn = gtk_entry_get_text(choose_t->dsn_entry);
      if (strchr(dsn, '/') != NULL)
        snprintf(buff, sizeof(buff), "%s", dsn);
      else
        snprintf(buff, sizeof(buff), "%s/%s", choose_t->curr_dir, dsn);

      gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel), buff);
      /* Ok button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button), 
          "clicked", GTK_SIGNAL_FUNC (fdsn_choosen), choose_t);
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button), 
          "clicked", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      /* Cancel button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button), 
          "clicked", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      /* Close window button events */
      gtk_signal_connect (GTK_OBJECT (filesel), "delete_event",
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

      choose_t->browse_sel = filesel;
      gtk_widget_show_all (filesel);
      gtk_main ();
      gtk_widget_destroy (filesel);

      choose_t->browse_sel = NULL;
    }
}


static void
fdriverchooser_advanced_clicked (GtkWidget * widget, TFDRIVERCHOOSER * choose_t)
{
  if (choose_t)
    {
      LPSTR attr_lst = NULL;
      LPSTR in_attrs = choose_t->attrs ? choose_t->attrs : "\0\0";

      attr_lst = create_keyval (choose_t->mainwnd, in_attrs, &choose_t->verify_conn);
      if (attr_lst && attr_lst != (LPSTR)-1L)
        {
          if (choose_t->attrs)
            free(choose_t->attrs);
          choose_t->attrs = attr_lst;
        }
    }
}


#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  gtk_widget_ref(widget); \
  gtk_object_set_data_full (GTK_OBJECT (component), name, \
      widget, (GtkDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  gtk_object_set_data (GTK_OBJECT (component), name, widget)

void
create_fdriverchooser (HWND hwnd, TFDRIVERCHOOSER *choose_t)
{
  GtkAccelGroup *accel_group;
  GtkWidget *driverchooser;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox5;
  GtkWidget *notebook2;
  GtkWidget *hbox8;
  GtkWidget *fixed5;
  GtkWidget *pixmap1;
  GtkWidget *vbox6;
  GtkWidget *label8;
  GtkWidget *scrolledwindow3;
  GtkWidget *clist1;
  GtkWidget *hbuttonbox7;
  GtkWidget *b_advanced;
  GtkWidget *label9;
  GtkWidget *hbox9;
  GtkWidget *fixed6;
  GtkWidget *pixmap2;
  GtkWidget *vbox7;
  GtkWidget *label10;
  GtkWidget *hbox10;
  GtkWidget *fdsn_entry;
  GtkWidget *b_browse;
  GtkWidget *label11;
  GtkWidget *hbox11;
  GtkWidget *hbox12;
  GtkWidget *fixed7;
  GtkWidget *pixmap3;
  GtkWidget *vbox8;
  GtkWidget *label12;
  GtkWidget *scrolledwindow4;
  GtkWidget *results_text;
  GtkWidget *label13;
  GtkWidget *hseparator2;
  GtkWidget *hbox13;
  GtkWidget *hbuttonbox8;
  GtkWidget *b_cancel;
  GtkWidget *hbuttonbox9;
  GtkWidget *b_back;
  GtkWidget *b_continue;
  GtkWidget *dialog_action_area1;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  GtkWidget *l_name, *l_file, *l_date, *l_size;
  guint b_cancel_key, b_continue_key, b_back_key, b_advanced_key, b_browse_key;


  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return;

  accel_group = gtk_accel_group_new ();

  driverchooser = gtk_dialog_new ();
  GLADE_HOOKUP_OBJECT_NO_REF (driverchooser, driverchooser, "driverchooser");
  gtk_window_set_title (GTK_WINDOW (driverchooser), "Create New File Data Source");
  gtk_window_set_position (GTK_WINDOW (driverchooser), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (driverchooser), TRUE);
  gtk_window_set_policy (GTK_WINDOW (driverchooser), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (driverchooser);
#endif

  dialog_vbox1 = GTK_DIALOG (driverchooser)->vbox;
  GLADE_HOOKUP_OBJECT_NO_REF (driverchooser, dialog_vbox1, "dialog_vbox1");
  gtk_widget_show (dialog_vbox1);

  vbox5 = gtk_vbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, vbox5, "vbox5");
  gtk_widget_show (vbox5);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox5, TRUE, TRUE, 0);

  notebook2 = gtk_notebook_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, notebook2, "notebook2");
  gtk_widget_show (notebook2);
  gtk_box_pack_start (GTK_BOX (vbox5), notebook2, TRUE, TRUE, 0);

  hbox8 = gtk_hbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, hbox8, "hbox8");
  gtk_widget_show (hbox8);
  gtk_container_add (GTK_CONTAINER (notebook2), hbox8);

  fixed5 = gtk_fixed_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, fixed5, "fixed5");
  gtk_widget_show (fixed5);
  gtk_box_pack_start (GTK_BOX (hbox8), fixed5, FALSE, TRUE, 5);
  gtk_widget_set_usize (fixed5, 141, 280);

#if GTK_CHECK_VERSION(2,0,0)
  style = gtk_widget_get_style (driverchooser);
  pixmap = gdk_pixmap_create_from_xpm_d (driverchooser->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
#else
  style = gtk_widget_get_style (GTK_WIDGET (hwnd));
  pixmap = gdk_pixmap_create_from_xpm_d (GTK_WIDGET (hwnd)->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
#endif

  pixmap1 = gtk_pixmap_new (pixmap, mask);
  GLADE_HOOKUP_OBJECT (driverchooser, pixmap1, "pixmap1");
  gtk_widget_show (pixmap1);
  gtk_fixed_put (GTK_FIXED (fixed5), pixmap1, 2, 5);
  gtk_widget_set_usize (pixmap1, 136, 264);

  vbox6 = gtk_vbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, vbox6, "vbox6");
  gtk_widget_show (vbox6);
  gtk_box_pack_start (GTK_BOX (hbox8), vbox6, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox8), 10);

  label8 = gtk_label_new ("Select a driver for which you want to setup a data source");
  GLADE_HOOKUP_OBJECT (driverchooser, label8, "label8");
  gtk_widget_show (label8);
  gtk_box_pack_start (GTK_BOX (vbox6), label8, FALSE, FALSE, 5);
  gtk_label_set_justify (GTK_LABEL (label8), GTK_JUSTIFY_LEFT);

  scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
  GLADE_HOOKUP_OBJECT (driverchooser, scrolledwindow3, "scrolledwindow3");
  gtk_widget_show (scrolledwindow3);
  gtk_box_pack_start (GTK_BOX (vbox6), scrolledwindow3, TRUE, TRUE, 5);

  clist1 = gtk_clist_new (4);
  GLADE_HOOKUP_OBJECT (driverchooser, clist1, "clist1");
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow3), clist1);
  gtk_container_set_border_width (GTK_CONTAINER (clist1), 3);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 165);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 118);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 2, 80);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 3, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_name = gtk_label_new (szDriverColumnNames[0]);
  GLADE_HOOKUP_OBJECT (driverchooser, l_name, "l_name");
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_name);

  l_file = gtk_label_new (szDriverColumnNames[1]);
  GLADE_HOOKUP_OBJECT (driverchooser, l_file, "l_file");
  gtk_widget_show (l_file);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_file);

  l_date = gtk_label_new (szDriverColumnNames[2]);
  GLADE_HOOKUP_OBJECT (driverchooser, l_date, "l_date");
  gtk_widget_show (l_date);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 2, l_date);

  l_size = gtk_label_new (szDriverColumnNames[3]);
  GLADE_HOOKUP_OBJECT (driverchooser, l_size, "l_size");
  gtk_widget_show (l_size);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 3, l_size);

  hbuttonbox7 = gtk_hbutton_box_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, hbuttonbox7, "hbuttonbox7");
  gtk_widget_show (hbuttonbox7);
  gtk_box_pack_start (GTK_BOX (vbox6), hbuttonbox7, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox7), 5);
  GTK_WIDGET_SET_FLAGS (hbuttonbox7, GTK_CAN_FOCUS);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox7), GTK_BUTTONBOX_END);
  gtk_box_set_spacing (GTK_BOX (hbuttonbox7), 1);

  b_advanced = gtk_button_new_with_label ("");
  b_advanced_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_advanced)->child),
      "_Advanced...");
  gtk_widget_add_accelerator (b_advanced, "clicked", accel_group,
      b_advanced_key, GDK_MOD1_MASK, 0);
  gtk_widget_add_accelerator (b_advanced, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  GLADE_HOOKUP_OBJECT (driverchooser, b_advanced, "b_advanced");
  gtk_widget_show (b_advanced);
  gtk_container_add (GTK_CONTAINER (hbuttonbox7), b_advanced);
  GTK_WIDGET_SET_FLAGS (b_advanced, GTK_CAN_DEFAULT);

  label9 = gtk_label_new ("  Drivers  ");
  GLADE_HOOKUP_OBJECT (driverchooser, label9, "label9");
  gtk_widget_show (label9);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook2), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook2), 0), label9);
  gtk_label_set_justify (GTK_LABEL (label9), GTK_JUSTIFY_LEFT);


  hbox9 = gtk_hbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, hbox9, "hbox9");
  gtk_widget_show (hbox9);
  gtk_container_add (GTK_CONTAINER (notebook2), hbox9);

  fixed6 = gtk_fixed_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, fixed6, "fixed6");
  gtk_widget_show (fixed6);
  gtk_box_pack_start (GTK_BOX (hbox9), fixed6, FALSE, TRUE, 5);
  gtk_widget_set_usize (fixed6, 141, 275);

  pixmap2 = gtk_pixmap_new (pixmap, mask);
  GLADE_HOOKUP_OBJECT (driverchooser, pixmap2, "pixmap2");
  gtk_widget_show (pixmap2);
  gtk_fixed_put (GTK_FIXED (fixed6), pixmap2, 2, 5);
  gtk_widget_set_usize (pixmap2, 136, 264);

  vbox7 = gtk_vbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, vbox7, "vbox7");
  gtk_widget_show (vbox7);
  gtk_box_pack_start (GTK_BOX (hbox9), vbox7, TRUE, TRUE, 0);

  label10 = gtk_label_new ("Type the name of the data source you want to save this connection to. Or, find the location to save to by clicking Browse.");
  GLADE_HOOKUP_OBJECT (driverchooser, label10, "label10");
  gtk_widget_show (label10);
  gtk_box_pack_start (GTK_BOX (vbox7), label10, FALSE, FALSE, 5);
  gtk_label_set_justify (GTK_LABEL (label10), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (label10), TRUE);

  hbox10 = gtk_hbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, hbox10, "hbox10");
  gtk_widget_show (hbox10);
  gtk_box_pack_start (GTK_BOX (vbox7), hbox10, FALSE, FALSE, 0);

  fdsn_entry = gtk_entry_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, fdsn_entry, "fdsn_entry");
  gtk_widget_show (fdsn_entry);
  gtk_box_pack_start (GTK_BOX (hbox10), fdsn_entry, TRUE, TRUE, 5);

  b_browse = gtk_button_new_with_label ("");
  b_browse_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_browse)->child),
      "  _Browse  ");
  gtk_widget_add_accelerator (b_browse, "clicked", accel_group,
      b_browse_key, GDK_MOD1_MASK, 0);
  gtk_widget_add_accelerator (b_browse, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  GLADE_HOOKUP_OBJECT (driverchooser, b_browse, "b_browse");
  gtk_widget_show (b_browse);
  gtk_box_pack_start (GTK_BOX (hbox10), b_browse, FALSE, FALSE, 5);

  label11 = gtk_label_new ("  FileDSN Name  ");
  GLADE_HOOKUP_OBJECT (driverchooser, label11, "label11");
  gtk_widget_show (label11);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook2), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook2), 1), label11);
  gtk_label_set_justify (GTK_LABEL (label11), GTK_JUSTIFY_LEFT);


  hbox11 = gtk_hbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, hbox11, "hbox11");
  gtk_widget_show (hbox11);
  gtk_container_add (GTK_CONTAINER (notebook2), hbox11);

  hbox12 = gtk_hbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, hbox12, "hbox12");
  gtk_widget_show (hbox12);
  gtk_box_pack_start (GTK_BOX (hbox11), hbox12, TRUE, TRUE, 0);

  fixed7 = gtk_fixed_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, fixed7, "fixed7");
  gtk_widget_show (fixed7);
  gtk_box_pack_start (GTK_BOX (hbox12), fixed7, FALSE, TRUE, 5);
  gtk_widget_set_usize (fixed7, 141, 275);

  pixmap3 = gtk_pixmap_new (pixmap, mask);
  GLADE_HOOKUP_OBJECT (driverchooser, pixmap3, "pixmap3");
  gtk_widget_show (pixmap3);
  gtk_fixed_put (GTK_FIXED (fixed7), pixmap3, 2, 5);
  gtk_widget_set_usize (pixmap3, 136, 264);

  vbox8 = gtk_vbox_new (FALSE, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, vbox8, "vbox8");
  gtk_widget_show (vbox8);
  gtk_box_pack_start (GTK_BOX (hbox12), vbox8, TRUE, TRUE, 0);

  label12 = gtk_label_new ("When you click Finish, you will create the data source which you have just configured. The driver may prompt you more information.");
  GLADE_HOOKUP_OBJECT (driverchooser, label12, "label12");
  gtk_widget_show (label12);
  gtk_box_pack_start (GTK_BOX (vbox8), label12, FALSE, FALSE, 5);
  gtk_label_set_justify (GTK_LABEL (label12), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (label12), TRUE);

  scrolledwindow4 = gtk_scrolled_window_new (NULL, NULL);
  GLADE_HOOKUP_OBJECT (driverchooser, scrolledwindow4, "scrolledwindow4");
  gtk_widget_show (scrolledwindow4);
  gtk_box_pack_start (GTK_BOX (vbox8), scrolledwindow4, TRUE, TRUE, 5);

#if GTK_CHECK_VERSION(2,0,0)
  results_text = gtk_text_view_new ();
#else
  results_text = gtk_text_new (NULL, NULL);
#endif
  GLADE_HOOKUP_OBJECT (driverchooser, results_text, "results_text");
  gtk_widget_show (results_text);
  gtk_container_add (GTK_CONTAINER (scrolledwindow4), results_text);

  label13 = gtk_label_new ("  Results  ");
  GLADE_HOOKUP_OBJECT (driverchooser, label13, "label13");
  gtk_widget_show (label13);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook2), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook2), 2), label13);
  gtk_label_set_justify (GTK_LABEL (label13), GTK_JUSTIFY_LEFT);

  dialog_action_area1 = GTK_DIALOG (driverchooser)->action_area;
  GLADE_HOOKUP_OBJECT_NO_REF (driverchooser, dialog_action_area1, "dialog_action_area1");
  gtk_widget_show (dialog_action_area1);


  hbuttonbox8 = gtk_hbutton_box_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, hbuttonbox8, "hbuttonbox8");
  gtk_widget_show (hbuttonbox8);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox8, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox8), 5);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox8), GTK_BUTTONBOX_START);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox8), 5);
  gtk_button_box_set_child_ipadding (GTK_BUTTON_BOX (hbuttonbox8), 0, -1);

  b_cancel = gtk_button_new_with_label ("");
  b_cancel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_cancel)->child),
      "Cancel");
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      b_cancel_key, GDK_MOD1_MASK, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, b_cancel, "b_cancel");
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox8), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);

  hbuttonbox9 = gtk_hbutton_box_new ();
  GLADE_HOOKUP_OBJECT (driverchooser, hbuttonbox9, "hbuttonbox9");
  gtk_widget_show (hbuttonbox9);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox9, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox9), 5);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox9), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox9), 5);
  gtk_button_box_set_child_ipadding (GTK_BUTTON_BOX (hbuttonbox9), 0, -1);

  b_back = gtk_button_new_with_label ("");
  b_back_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_back)->child),
      "Go _Back");
  gtk_widget_add_accelerator (b_back, "clicked", accel_group,
      b_back_key, GDK_MOD1_MASK, 0);
  gtk_widget_add_accelerator (b_back, "clicked", accel_group,
      'B', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  GLADE_HOOKUP_OBJECT (driverchooser, b_back, "b_back");
  gtk_widget_show (b_back);
  gtk_container_add (GTK_CONTAINER (hbuttonbox9), b_back);
  GTK_WIDGET_SET_FLAGS (b_back, GTK_CAN_DEFAULT);
  gtk_widget_set_sensitive (b_back, FALSE);

  b_continue = gtk_button_new_with_label ("");
  b_continue_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_continue)->child),
      "Continue");
  gtk_widget_add_accelerator (b_continue, "clicked", accel_group,
      b_continue_key, GDK_MOD1_MASK, 0);
  GLADE_HOOKUP_OBJECT (driverchooser, b_continue, "b_continue");
  gtk_widget_show (b_continue);
  gtk_container_add (GTK_CONTAINER (hbuttonbox9), b_continue);
  GTK_WIDGET_SET_FLAGS (b_continue, GTK_CAN_DEFAULT);


  /* Notebook events */
  gtk_signal_connect_after (GTK_OBJECT (notebook2), "switch_page",
      GTK_SIGNAL_FUNC (fdriverchooser_switch_page), choose_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (fdriverchooser_cancel_clicked), choose_t);
  /* Continue button events */
  gtk_signal_connect (GTK_OBJECT (b_continue), "clicked",
      GTK_SIGNAL_FUNC (fdriverchooser_next_clicked), choose_t);
  /* Back button events */
  gtk_signal_connect (GTK_OBJECT (b_back), "clicked",
      GTK_SIGNAL_FUNC (fdriverchooser_prev_clicked), choose_t);
  /* Browse button events */
  gtk_signal_connect (GTK_OBJECT (b_browse), "clicked",
      GTK_SIGNAL_FUNC (fdriverchooser_browse_clicked), choose_t);
  /* Advanced button events */
  gtk_signal_connect (GTK_OBJECT (b_advanced), "clicked",
      GTK_SIGNAL_FUNC (fdriverchooser_advanced_clicked), choose_t);
  /* Driver list events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (fdriver_list_select), choose_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (driverchooser), "delete_event",
      GTK_SIGNAL_FUNC (fdelete_event), choose_t);
  gtk_signal_connect (GTK_OBJECT (driverchooser), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  
  gtk_window_add_accel_group (GTK_WINDOW (driverchooser), accel_group);
  gtk_widget_grab_default (b_continue);

  adddrivers_to_list (clist1, driverchooser);

  choose_t->driverlist = clist1;
  choose_t->driver = NULL;
  choose_t->mainwnd = driverchooser;
  choose_t->b_continue = b_continue;
  choose_t->b_back = b_back;
  choose_t->tab_panel = notebook2;
  choose_t->dsn_entry = fdsn_entry;
  choose_t->mess_entry = results_text;

  gtk_widget_show_all (driverchooser);
  gtk_main ();

}
