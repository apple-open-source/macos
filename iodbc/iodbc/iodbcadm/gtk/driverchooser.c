/*
 *  driverchooser.c
 *
 *  $Id: driverchooser.c,v 1.3 2004/11/11 01:52:39 luesang Exp $
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
	  choose_t->driver = strdup (szDriver);
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
  gtk_widget_set_usize (l_diz, 325, 16);
  gtk_label_set_justify (GTK_LABEL (l_diz), GTK_JUSTIFY_LEFT);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (driverchooser), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 168, 32);
  gtk_widget_set_uposition (scrolledwindow1, 168, 32);
  gtk_widget_set_usize (scrolledwindow1, 320, 248);

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

  style = gtk_widget_get_style (GTK_WIDGET (hwnd));
  pixmap =
      gdk_pixmap_create_from_xpm_d (GTK_WIDGET (hwnd)->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
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
