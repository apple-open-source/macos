/*
 *  translator.c
 *
 *  $Id: translatorchooser.c,v 1.7 2007/02/02 11:58:14 source Exp $
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


#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "gui.h"
#include "img.xpm"


static void
translator_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TTRANSLATORCHOOSER *choose_t)
{
  LPSTR translator = NULL;

  if (choose_t)
    {
      /* Get the directory name */
      gtk_clist_get_text (GTK_CLIST (choose_t->translatorlist), row, 0,
	  &translator);

      if (translator && event && event->type == GDK_2BUTTON_PRESS)
	gtk_signal_emit_by_name (GTK_OBJECT (choose_t->b_finish), "clicked",
	    choose_t);
    }
}


static void
translatorchooser_ok_clicked (GtkWidget *widget,
    TTRANSLATORCHOOSER *choose_t)
{
  char *szTranslator;

  if (choose_t)
    {
      if (GTK_CLIST (choose_t->translatorlist)->selection != NULL)
	{
	  gtk_clist_get_text (GTK_CLIST (choose_t->translatorlist),
	      GPOINTER_TO_INT (GTK_CLIST (choose_t->translatorlist)->
		  selection->data), 0, &szTranslator);
	  choose_t->translator = dm_SQL_A2W (szTranslator, SQL_NTS);
	}
      else
	choose_t->translator = NULL;

      choose_t->translatorlist = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static void
translatorchooser_cancel_clicked (GtkWidget *widget,
    TTRANSLATORCHOOSER *choose_t)
{
  if (choose_t)
    {
      choose_t->translatorlist = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (choose_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (choose_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget,
    GdkEvent *event, TTRANSLATORCHOOSER *choose_t)
{
  translatorchooser_cancel_clicked (widget, choose_t);

  return FALSE;
}

void
addtranslators_to_list (GtkWidget *widget, GtkWidget *dlg)
{
  char *curr, *buffer = (char *) malloc (sizeof (char) * 65536), *szDriver;
  char driver[1024], _date[1024], _size[1024];
  char *data[4];
  int len, i;
  BOOL careabout;
  UWORD confMode = ODBC_USER_DSN;
  struct stat _stat;

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
	  SQLGetPrivateProfileString ("ODBC 32 bit Translators",
	  NULL, "", buffer, 65535, "odbcinst.ini");
#else
      len =
	  SQLGetPrivateProfileString ("ODBC Translators",
	  NULL, "", buffer, 65535, "odbcinst.ini");
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
	  SQLGetPrivateProfileString ("ODBC 32 bit Translators",
	      curr, "", driver, sizeof (driver), "odbcinst.ini");
#else
	  SQLGetPrivateProfileString ("ODBC Translators",
	      curr, "", driver, sizeof (driver), "odbcinst.ini");
#endif

	  /* Check if the driver is installed */
	  if (strcasecmp (driver, "Installed"))
	    goto end;

	  /* Get the driver library name */
	  SQLSetConfigMode (confMode);
	  if (!SQLGetPrivateProfileString (curr,
		  "Translator", "", driver, sizeof (driver), "odbcinst.ini"))
	    {
	      SQLSetConfigMode (confMode);
	      SQLGetPrivateProfileString ("Default",
		  "Translator", "", driver, sizeof (driver), "odbcinst.ini");
	    }

	  if (STRLEN (curr) && STRLEN (driver))
	    {
	      data[0] = curr;
	      data[1] = driver;

	      /* Get some information about the driver */
	      if (!stat (driver, &_stat))
		{
		  sprintf (_size, "%lu Kb",
		      (unsigned long) _stat.st_size / 1024L);
		  sprintf (_date, "%s", ctime (&_stat.st_mtime));
		  data[2] = _date;
		  data[3] = _size;
		  gtk_clist_append (GTK_CLIST (widget), data);
		}
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


void
create_translatorchooser (HWND hwnd, TTRANSLATORCHOOSER *choose_t)
{
  GtkWidget *translatorchooser, *dialog_vbox1, *fixed1, *l_diz,
      *scrolledwindow1;
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

  translatorchooser = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (translatorchooser), "translatorchooser",
      translatorchooser);
  gtk_window_set_title (GTK_WINDOW (translatorchooser),
      "Choose a Translator");
  gtk_window_set_position (GTK_WINDOW (translatorchooser),
      GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (translatorchooser), TRUE);
  gtk_window_set_policy (GTK_WINDOW (translatorchooser), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (translatorchooser);
#endif

  dialog_vbox1 = GTK_DIALOG (translatorchooser)->vbox;
  gtk_object_set_data (GTK_OBJECT (translatorchooser), "dialog_vbox1",
      dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  l_diz = gtk_label_new ("Select which ODBC Translator you want to use.");
  gtk_widget_ref (l_diz);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "l_diz", l_diz,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_diz);
  gtk_fixed_put (GTK_FIXED (fixed1), l_diz, 168, 16);
  gtk_widget_set_uposition (l_diz, 168, 16);
  gtk_widget_set_usize (l_diz, 325, 16);
  gtk_label_set_justify (GTK_LABEL (l_diz), GTK_JUSTIFY_LEFT);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 168, 32);
  gtk_widget_set_uposition (scrolledwindow1, 168, 32);
  gtk_widget_set_usize (scrolledwindow1, 320, 248);

  clist1 = gtk_clist_new (4);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "clist1", clist1,
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
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_name);

  l_file = gtk_label_new (szDriverColumnNames[1]);
  gtk_widget_ref (l_file);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "l_file", l_file,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_file);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_file);

  l_date = gtk_label_new (szDriverColumnNames[2]);
  gtk_widget_ref (l_date);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "l_date", l_date,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_date);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 2, l_date);

  l_size = gtk_label_new (szDriverColumnNames[3]);
  gtk_widget_ref (l_size);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "l_size", l_size,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_size);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 3, l_size);

#if GTK_CHECK_VERSION(2,0,0)
  style = gtk_widget_get_style (translatorchooser);
  pixmap =
      gdk_pixmap_create_from_xpm_d (translatorchooser->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
#else
  style = gtk_widget_get_style (GTK_WIDGET (hwnd));
  pixmap =
      gdk_pixmap_create_from_xpm_d (GTK_WIDGET (hwnd)->window, &mask,
      &style->bg[GTK_STATE_NORMAL], (gchar **) img_xpm);
#endif

  pixmap1 = gtk_pixmap_new (pixmap, mask);
  gtk_widget_ref (pixmap1);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "pixmap1",
      pixmap1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap1);
  gtk_fixed_put (GTK_FIXED (fixed1), pixmap1, 16, 16);
  gtk_widget_set_uposition (pixmap1, 16, 16);
  gtk_widget_set_usize (pixmap1, 136, 264);

  dialog_action_area1 = GTK_DIALOG (translatorchooser)->action_area;
  gtk_object_set_data (GTK_OBJECT (translatorchooser), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "hbuttonbox1",
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
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "b_finish",
      b_finish, (GtkDestroyNotify) gtk_widget_unref);
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
  gtk_object_set_data_full (GTK_OBJECT (translatorchooser), "b_cancel",
      b_cancel, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_finish), "clicked",
      GTK_SIGNAL_FUNC (translatorchooser_ok_clicked), choose_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (translatorchooser_cancel_clicked), choose_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (translatorchooser), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), choose_t);
  gtk_signal_connect (GTK_OBJECT (translatorchooser), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* Translator list events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (translator_list_select), choose_t);

  gtk_window_add_accel_group (GTK_WINDOW (translatorchooser), accel_group);

  addtranslators_to_list (clist1, translatorchooser);

  choose_t->translatorlist = clist1;
  choose_t->translator = NULL;
  choose_t->mainwnd = translatorchooser;
  choose_t->b_finish = b_finish;

  gtk_widget_show_all (translatorchooser);
  gtk_main ();
}
