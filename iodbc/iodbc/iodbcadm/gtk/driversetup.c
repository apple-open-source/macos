/*
 *  driversetup.c
 *
 *  $Id: driversetup.c,v 1.6 2007/02/02 11:58:14 source Exp $
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


#include "gui.h"


static char* STRCONN = "%s\0Driver=%s\0Setup=%s\0\0";
static int STRCONN_NB_TOKENS = 3;

char *szKeysColumnNames[] = {
  "Keyword",
  "Value"
};

char *szKeysButtons[] = {
  "_Add",
  "_Update"
};


static void
addkeywords_to_list (GtkWidget *widget, LPCSTR attrs,
    TDRIVERSETUP *driversetup_t)
{
  char *curr, *cour;
  char *data[2];

  if (!GTK_IS_CLIST (widget))
    return;
  gtk_clist_clear (GTK_CLIST (widget));

  for (curr = (LPSTR) attrs; *curr; curr += (STRLEN (curr) + 1))
    {
      if (!strncasecmp (curr, "Driver=", STRLEN ("Driver=")))
	{
	  gtk_entry_set_text (GTK_ENTRY (driversetup_t->driver_entry),
	      curr + STRLEN ("Driver="));
	  continue;
	}

      if (!strncasecmp (curr, "Setup=", STRLEN ("Setup=")))
	{
	  gtk_entry_set_text (GTK_ENTRY (driversetup_t->setup_entry),
	      curr + STRLEN ("Setup="));
	  continue;
	}

      data[0] = curr;

      if ((cour = strchr (curr, '=')))
	{
	  data[1] = cour + 1;
	  *cour = 0;
	  gtk_clist_append (GTK_CLIST (widget), data);
	  *cour = '=';
	}
      else
	{
	  data[1] = "";
	  gtk_clist_append (GTK_CLIST (widget), data);
	}
    }

  if (GTK_CLIST (widget)->rows > 0)
    gtk_clist_sort (GTK_CLIST (widget));
}


static void
parse_attribute_line (TDRIVERSETUP *driversetup_t, LPCSTR driver,
    LPCSTR attrs, BOOL add)
{
  if (driver)
    {
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->name_entry), driver);
      if (add)
	gtk_entry_set_editable (GTK_ENTRY (driversetup_t->name_entry), FALSE);
      else
	gtk_entry_set_editable (GTK_ENTRY (driversetup_t->name_entry), TRUE);
    }

  addkeywords_to_list (driversetup_t->key_list, attrs, driversetup_t);
}


static void
driver_file_choosen (GtkWidget *widget, TDRIVERSETUP *driversetup_t)
{
  if (driversetup_t)
    {
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->driver_entry),
	  gtk_file_selection_get_filename (GTK_FILE_SELECTION (driversetup_t->
		  filesel)));
      driversetup_t->filesel = NULL;
    }
}


static void
setup_file_choosen (GtkWidget *widget, TDRIVERSETUP *driversetup_t)
{
  if (driversetup_t)
    {
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->setup_entry),
	  gtk_file_selection_get_filename (GTK_FILE_SELECTION (driversetup_t->
		  filesel)));
      driversetup_t->filesel = NULL;
    }
}


static void
driversetup_browsedriver_clicked (GtkWidget *widget,
    TDRIVERSETUP *driversetup_t)
{
  GtkWidget *filesel;

  if (driversetup_t)
    {
      filesel = gtk_file_selection_new ("Choose your driver library ...");
      gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
	  gtk_entry_get_text (GTK_ENTRY (driversetup_t->driver_entry)));
      /* Ok button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      ok_button), "clicked", GTK_SIGNAL_FUNC (driver_file_choosen),
	  driversetup_t);
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      ok_button), "clicked", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      /* Cancel button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      cancel_button), "clicked", GTK_SIGNAL_FUNC (gtk_main_quit),
	  NULL);
      /* Close window button events */
      gtk_signal_connect (GTK_OBJECT (filesel), "delete_event",
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

      driversetup_t->filesel = filesel;

      gtk_widget_show_all (filesel);
      gtk_main ();
      gtk_widget_destroy (filesel);

      driversetup_t->filesel = NULL;
    }
}


static void
driversetup_browsesetup_clicked (GtkWidget *widget,
    TDRIVERSETUP *driversetup_t)
{
  GtkWidget *filesel;

  if (driversetup_t)
    {
      filesel = gtk_file_selection_new ("Choose your setup library ...");
      gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
	  gtk_entry_get_text (GTK_ENTRY (driversetup_t->setup_entry)));
      /* Ok button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      ok_button), "clicked", GTK_SIGNAL_FUNC (setup_file_choosen),
	  driversetup_t);
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      ok_button), "clicked", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      /* Cancel button events */
      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->
	      cancel_button), "clicked", GTK_SIGNAL_FUNC (gtk_main_quit),
	  NULL);
      /* Close window button events */
      gtk_signal_connect (GTK_OBJECT (filesel), "delete_event",
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

      driversetup_t->filesel = filesel;

      gtk_widget_show_all (filesel);
      gtk_main ();
      gtk_widget_destroy (filesel);

      driversetup_t->filesel = NULL;
    }
}


static void
driversetup_add_clicked (GtkWidget *widget, TDRIVERSETUP *driversetup_t)
{
  char *szKey;
  char *data[2];
  int i = 0;

  if (driversetup_t)
    {
      data[0] = gtk_entry_get_text (GTK_ENTRY (driversetup_t->key_entry));
      if (!STRLEN (data[0]))
	goto done;
      data[1] = gtk_entry_get_text (GTK_ENTRY (driversetup_t->value_entry));

      for (i = 0; i < GTK_CLIST (driversetup_t->key_list)->rows; i++)
	{
	  gtk_clist_get_text (GTK_CLIST (driversetup_t->key_list), i, 0,
	      &szKey);
	  if (!strcmp (szKey, data[0]))
	    {
	      gtk_clist_remove (GTK_CLIST (driversetup_t->key_list), i);
	      break;
	    }
	}

      gtk_clist_append (GTK_CLIST (driversetup_t->key_list), data);
      if (GTK_CLIST (driversetup_t->key_list)->rows > 0)
	gtk_clist_sort (GTK_CLIST (driversetup_t->key_list));

    done:
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->key_entry), "");
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->value_entry), "");
    }
}


static void
driversetup_update_clicked (GtkWidget *widget, TDRIVERSETUP *driversetup_t)
{
  char *data[2];

  if (driversetup_t && GTK_CLIST (driversetup_t->key_list)->selection != NULL)
    {
      data[0] = gtk_entry_get_text (GTK_ENTRY (driversetup_t->key_entry));
      data[1] = gtk_entry_get_text (GTK_ENTRY (driversetup_t->value_entry));
      gtk_clist_remove (GTK_CLIST (driversetup_t->key_list),
	  GPOINTER_TO_INT (GTK_CLIST (driversetup_t->key_list)->selection->
	      data));

      if (STRLEN (data[0]))
	{
	  gtk_clist_append (GTK_CLIST (driversetup_t->key_list), data);
	  if (GTK_CLIST (driversetup_t->key_list)->rows > 0)
	    gtk_clist_sort (GTK_CLIST (driversetup_t->key_list));
	}

      gtk_entry_set_text (GTK_ENTRY (driversetup_t->key_entry), "");
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->value_entry), "");
    }
}


static void
driversetup_list_select (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDRIVERSETUP *driversetup_t)
{
  char *szKey, *szValue;

  if (driversetup_t && GTK_CLIST (driversetup_t->key_list)->selection != NULL)
    {
      gtk_clist_get_text (GTK_CLIST (driversetup_t->key_list),
	  GPOINTER_TO_INT (GTK_CLIST (driversetup_t->key_list)->selection->
	      data), 0, &szKey);
      gtk_clist_get_text (GTK_CLIST (driversetup_t->key_list),
	  GPOINTER_TO_INT (GTK_CLIST (driversetup_t->key_list)->selection->
	      data), 1, &szValue);
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->key_entry), szKey);
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->value_entry), szValue);
      gtk_widget_set_sensitive (driversetup_t->bupdate, TRUE);
    }
}


static void
driversetup_list_unselect (GtkWidget *widget, gint row, gint column,
    GdkEvent *event, TDRIVERSETUP *driversetup_t)
{
  if (driversetup_t)
    {
      gtk_widget_set_sensitive (driversetup_t->bupdate, FALSE);
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->key_entry), "");
      gtk_entry_set_text (GTK_ENTRY (driversetup_t->value_entry), "");
    }
}


static void
driversetup_ok_clicked (GtkWidget *widget, TDRIVERSETUP *driversetup_t)
{
  char *curr, *cour, *szKey, *szValue;
  int i = 0, size;

  if (driversetup_t)
    {
      driversetup_t->connstr = (char *) malloc (sizeof (char) * (size =
	      ((STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				  name_entry))) ?
		      STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				  name_entry))) + 1 : 0) +
		  (STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				  driver_entry))) ?
		      STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				  driver_entry))) + STRLEN ("Driver=") +
		      1 : 0) +
		  (STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				  setup_entry))) ?
		      STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				  setup_entry))) + STRLEN ("Setup=") +
		      1 : 0) + 1)));

      if (driversetup_t->connstr)
	{
	  for (curr = STRCONN, cour = driversetup_t->connstr;
	      i < STRCONN_NB_TOKENS; i++, curr += (STRLEN (curr) + 1))
	    switch (i)
	      {
	      case 0:
		if (STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				name_entry))))
		  {
		    sprintf (cour, curr,
			gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				name_entry)));
		    cour += (STRLEN (cour) + 1);
		  }
		break;
	      case 1:
		if (STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				driver_entry))))
		  {
		    sprintf (cour, curr,
			gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				driver_entry)));
		    cour += (STRLEN (cour) + 1);
		  }
		break;
	      case 2:
		if (STRLEN (gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				setup_entry))))
		  {
		    sprintf (cour, curr,
			gtk_entry_get_text (GTK_ENTRY (driversetup_t->
				setup_entry)));
		    cour += (STRLEN (cour) + 1);
		  }
		break;
	      };

	  for (i = 0; i < GTK_CLIST (driversetup_t->key_list)->rows; i++)
	    {
	      gtk_clist_get_text (GTK_CLIST (driversetup_t->key_list), i, 0,
		  &szKey);
	      gtk_clist_get_text (GTK_CLIST (driversetup_t->key_list), i, 1,
		  &szValue);

	      cour = (char *) driversetup_t->connstr;
	      driversetup_t->connstr =
		  (LPSTR) malloc (size + STRLEN (szKey) + STRLEN (szValue) +
		  2);
	      if (driversetup_t->connstr)
		{
		  memcpy (driversetup_t->connstr, cour, size);
		  sprintf (driversetup_t->connstr + size - 1, "%s=%s", szKey,
		      szValue);
		  free (cour);
		  size += STRLEN (szKey) + STRLEN (szValue) + 2;
		}
	      else
		driversetup_t->connstr = cour;
	    }

	  driversetup_t->connstr[size - 1] = 0;
	}

      driversetup_t->name_entry = driversetup_t->driver_entry =
	  driversetup_t->setup_entry = NULL;
      driversetup_t->key_list = driversetup_t->filesel = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (driversetup_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (driversetup_t->mainwnd);
    }
}


static void
driversetup_cancel_clicked (GtkWidget *widget, TDRIVERSETUP *driversetup_t)
{
  if (driversetup_t)
    {
      driversetup_t->connstr = (LPSTR) - 1L;

      driversetup_t->name_entry = driversetup_t->driver_entry =
	  driversetup_t->setup_entry = NULL;
      driversetup_t->key_list = driversetup_t->filesel = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (driversetup_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (driversetup_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget, GdkEvent *event,
    TDRIVERSETUP *driversetup_t)
{
  driversetup_cancel_clicked (widget, driversetup_t);

  return FALSE;
}


LPSTR
create_driversetup (HWND hwnd, LPCSTR driver, LPCSTR attrs, BOOL add, BOOL user)
{
  GtkWidget *driversetup, *dialog_vbox1, *fixed1, *t_name, *t_driver,
      *t_keyword;
  GtkWidget *t_value, *l_name, *b_browse, *t_setup, *l_driver, *b_browse1;
  GtkWidget *l_setup, *scrolledwindow1, *clist1, *l_key, *l_value, *l_keyword;
  GtkWidget *l_valeur, *vbuttonbox1, *b_add, *b_update, *l_copyright;
  GtkWidget *dialog_action_area1, *hbuttonbox1, *b_ok, *b_cancel;
  guint b_add_key, b_update_key, b_ok_key, b_cancel_key;
  GtkAccelGroup *accel_group;
  TDRIVERSETUP driversetup_t;

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return (LPSTR) attrs;

  accel_group = gtk_accel_group_new ();

  driversetup = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (driversetup), "driversetup", driversetup);
  gtk_window_set_title (GTK_WINDOW (driversetup), "ODBC Driver Add/Setup");
  gtk_window_set_position (GTK_WINDOW (driversetup), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (driversetup), TRUE);
  gtk_window_set_policy (GTK_WINDOW (driversetup), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (driversetup);
#endif

  dialog_vbox1 = GTK_DIALOG (driversetup)->vbox;
  gtk_object_set_data (GTK_OBJECT (driversetup), "dialog_vbox1",
      dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  t_name = gtk_entry_new ();
  gtk_widget_ref (t_name);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "t_name", t_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_name);
  gtk_fixed_put (GTK_FIXED (fixed1), t_name, 160, 56);
  gtk_widget_set_uposition (t_name, 160, 56);
  gtk_widget_set_usize (t_name, 250, 0);

  t_driver = gtk_entry_new ();
  gtk_widget_ref (t_driver);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "t_driver", t_driver,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_driver);
  gtk_fixed_put (GTK_FIXED (fixed1), t_driver, 160, 88);
  gtk_widget_set_uposition (t_driver, 160, 88);
  gtk_widget_set_usize (t_driver, 250, 0);

  t_keyword = gtk_entry_new ();
  gtk_widget_ref (t_keyword);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "t_keyword", t_keyword,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_keyword);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), t_keyword, 88, 360);
  gtk_widget_set_uposition (t_keyword, 88, 360);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), t_keyword, 88, 352);
  gtk_widget_set_uposition (t_keyword, 88, 352);
#endif
  gtk_widget_set_usize (t_keyword, 301, 22);

  t_value = gtk_entry_new ();
  gtk_widget_ref (t_value);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "t_value", t_value,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_value);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), t_value, 88, 392);
  gtk_widget_set_uposition (t_value, 88, 392);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), t_value, 88, 384);
  gtk_widget_set_uposition (t_value, 88, 384);
#endif
  gtk_widget_set_usize (t_value, 301, 22);

  l_name = gtk_label_new ("Description of the driver : ");
  gtk_widget_ref (l_name);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_name", l_name,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_name);
  gtk_fixed_put (GTK_FIXED (fixed1), l_name, 8, 59);
  gtk_widget_set_uposition (l_name, 8, 59);
  gtk_widget_set_usize (l_name, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_name), GTK_JUSTIFY_LEFT);

  b_browse = gtk_button_new_with_label ("Browse ...");
  gtk_widget_ref (b_browse);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "b_browse", b_browse,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_browse);
  gtk_fixed_put (GTK_FIXED (fixed1), b_browse, 424, 88);
  gtk_widget_set_uposition (b_browse, 424, 88);
  gtk_widget_set_usize (b_browse, 65, 22);

  t_setup = gtk_entry_new ();
  gtk_widget_ref (t_setup);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "t_setup", t_setup,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_setup);
  gtk_fixed_put (GTK_FIXED (fixed1), t_setup, 160, 120);
  gtk_widget_set_uposition (t_setup, 160, 120);
  gtk_widget_set_usize (t_setup, 250, 0);

  l_driver = gtk_label_new ("Driver file name : ");
  gtk_widget_ref (l_driver);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_driver", l_driver,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_driver);
  gtk_fixed_put (GTK_FIXED (fixed1), l_driver, 55, 92);
  gtk_widget_set_uposition (l_driver, 55, 92);
  gtk_widget_set_usize (l_driver, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_driver), GTK_JUSTIFY_LEFT);

  b_browse1 = gtk_button_new_with_label ("Browse ...");
  gtk_widget_ref (b_browse1);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "b_browse1", b_browse1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_browse1);
  gtk_fixed_put (GTK_FIXED (fixed1), b_browse1, 424, 120);
  gtk_widget_set_uposition (b_browse1, 424, 120);
  gtk_widget_set_usize (b_browse1, 65, 22);

  l_setup = gtk_label_new ("Setup file name : ");
  gtk_widget_ref (l_setup);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_setup", l_setup,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_setup);
  gtk_fixed_put (GTK_FIXED (fixed1), l_setup, 56, 123);
  gtk_widget_set_uposition (l_setup, 56, 123);
  gtk_widget_set_usize (l_setup, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_setup), GTK_JUSTIFY_LEFT);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 8, 152);
  gtk_widget_set_uposition (scrolledwindow1, 8, 152);
  gtk_widget_set_usize (scrolledwindow1, 480, 192);

  clist1 = gtk_clist_new (2);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "clist1", clist1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clist1);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 134);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_key = gtk_label_new (szKeysColumnNames[0]);
  gtk_widget_ref (l_key);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_key", l_key,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_key);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_key);

  l_value = gtk_label_new (szKeysColumnNames[1]);
  gtk_widget_ref (l_value);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_value", l_value,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_value);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_value);

  l_keyword = gtk_label_new ("Keyword : ");
  gtk_widget_ref (l_keyword);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_keyword", l_keyword,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_keyword);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), l_keyword, 16, 363);
  gtk_widget_set_uposition (l_keyword, 16, 363);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), l_keyword, 16, 355);
  gtk_widget_set_uposition (l_keyword, 16, 355);
#endif
  gtk_widget_set_usize (l_keyword, 69, 16);
  gtk_label_set_justify (GTK_LABEL (l_keyword), GTK_JUSTIFY_LEFT);

  l_valeur = gtk_label_new ("Value : ");
  gtk_widget_ref (l_valeur);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_valeur", l_valeur,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_valeur);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), l_valeur, 32, 396);
  gtk_widget_set_uposition (l_valeur, 32, 396);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), l_valeur, 32, 388);
  gtk_widget_set_uposition (l_valeur, 32, 388);
#endif
  gtk_widget_set_usize (l_valeur, 51, 16);
  gtk_label_set_justify (GTK_LABEL (l_valeur), GTK_JUSTIFY_LEFT);

  vbuttonbox1 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "vbuttonbox1",
      vbuttonbox1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox1);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), vbuttonbox1, 400, 357);
  gtk_widget_set_uposition (vbuttonbox1, 400, 357);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), vbuttonbox1, 400, 344);
  gtk_widget_set_uposition (vbuttonbox1, 400, 344);
#endif
  gtk_widget_set_usize (vbuttonbox1, 85, 67);

  b_add = gtk_button_new_with_label ("");
  b_add_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szKeysButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      b_add_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "b_add", b_add,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_update = gtk_button_new_with_label ("");
  b_update_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_update)->child),
      szKeysButtons[1]);
  gtk_widget_add_accelerator (b_update, "clicked", accel_group,
      b_update_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_update);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "b_update", b_update,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_update);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_update);
  GTK_WIDGET_SET_FLAGS (b_update, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_update, "clicked", accel_group,
      'U', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  l_copyright = gtk_label_new ("ODBC Driver Add/Setup");
  gtk_widget_ref (l_copyright);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "l_copyright",
      l_copyright, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_copyright);
  gtk_fixed_put (GTK_FIXED (fixed1), l_copyright, 6, 6);
  gtk_widget_set_uposition (l_copyright, 0, 0);
  gtk_widget_set_usize (l_copyright, 482, 42);

  dialog_action_area1 = GTK_DIALOG (driversetup)->action_area;
  gtk_object_set_data (GTK_OBJECT (driversetup), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "hbuttonbox1",
      hbuttonbox1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE,
      0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 10);

  b_ok = gtk_button_new_with_label ("");
  b_ok_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_ok)->child), "_Ok");
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      b_ok_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_ok);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "b_ok", b_ok,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_ok);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_ok);
  GTK_WIDGET_SET_FLAGS (b_ok, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      'O', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_cancel = gtk_button_new_with_label ("");
  b_cancel_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_cancel)->child),
      "_Cancel");
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      b_cancel_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_cancel);
  gtk_object_set_data_full (GTK_OBJECT (driversetup), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_ok), "clicked",
      GTK_SIGNAL_FUNC (driversetup_ok_clicked), &driversetup_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (driversetup_cancel_clicked), &driversetup_t);
  /* Add button events */
  gtk_signal_connect (GTK_OBJECT (b_add), "clicked",
      GTK_SIGNAL_FUNC (driversetup_add_clicked), &driversetup_t);
  /* Update button events */
  gtk_signal_connect (GTK_OBJECT (b_update), "clicked",
      GTK_SIGNAL_FUNC (driversetup_update_clicked), &driversetup_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (driversetup), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), &driversetup_t);
  gtk_signal_connect (GTK_OBJECT (driversetup), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* List events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (driversetup_list_select), &driversetup_t);
  gtk_signal_connect (GTK_OBJECT (clist1), "unselect_row",
      GTK_SIGNAL_FUNC (driversetup_list_unselect), &driversetup_t);
  /* Browse button events */
  gtk_signal_connect (GTK_OBJECT (b_browse), "clicked",
      GTK_SIGNAL_FUNC (driversetup_browsedriver_clicked), &driversetup_t);
  gtk_signal_connect (GTK_OBJECT (b_browse1), "clicked",
      GTK_SIGNAL_FUNC (driversetup_browsesetup_clicked), &driversetup_t);

  gtk_window_add_accel_group (GTK_WINDOW (driversetup), accel_group);

  driversetup_t.name_entry = t_name;
  driversetup_t.driver_entry = t_driver;
  driversetup_t.key_list = clist1;
  driversetup_t.bupdate = b_update;
  driversetup_t.key_entry = t_keyword;
  driversetup_t.value_entry = t_value;
  driversetup_t.mainwnd = driversetup;
  driversetup_t.setup_entry = t_setup;

  /* Parse the attributes line */
  parse_attribute_line (&driversetup_t, driver, attrs, add);

  gtk_widget_show_all (driversetup);
  gtk_main ();

  return driversetup_t.connstr;
}
