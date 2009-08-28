/*
 *  gensetup.c
 *
 *  $Id: gensetup.c,v 1.5 2007/02/02 11:58:14 source Exp $
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


static char* STRCONN = "DSN=%s\0Description=%s\0\0";
static int STRCONN_NB_TOKENS = 2;

static char *szKeysColumnNames[] = {
  "Keyword",
  "Value"
};

static char *szKeysButtons[] = {
  "_Add",
  "_Update"
};


static void
addkeywords_to_list(GtkWidget* widget, LPCSTR attrs, TGENSETUP *gensetup_t)
{
  char *curr, *cour;
  char *data[2];

  if (!GTK_IS_CLIST (widget))
    return;
  gtk_clist_clear (GTK_CLIST (widget));

  for (curr = (LPSTR) attrs; *curr; curr += (STRLEN (curr) + 1))
    {
      if (!strncasecmp (curr, "DSN=", STRLEN ("DSN=")) ||
	  !strncasecmp (curr, "Driver=", STRLEN ("Driver=")) ||
	  !strncasecmp (curr, "Description=", STRLEN ("Description=")))
	continue;

      if ((cour = strchr (curr, '=')))
	{
	  *cour = '\0';
	  data[0] = curr;
	  data[1] = cour + 1;
	  gtk_clist_append (GTK_CLIST (widget), data);
	  *cour = '=';
	}
      else
	{
	  data[0] = "";
	  gtk_clist_append (GTK_CLIST (widget), data);
	}
    }

  if (GTK_CLIST (widget)->rows > 0)
    gtk_clist_sort (GTK_CLIST (widget));
}


static void
parse_attribute_line(TGENSETUP *gensetup_t, LPCSTR dsn, LPCSTR attrs, BOOL add)
{
  if (dsn && gensetup_t->dsn_entry)
    {
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->dsn_entry), dsn);
      if (add)
	gtk_widget_set_sensitive (gensetup_t->dsn_entry, TRUE);
      else
	gtk_widget_set_sensitive (gensetup_t->dsn_entry, FALSE);
    }

  addkeywords_to_list (gensetup_t->key_list, attrs, gensetup_t);
}


static void
gensetup_add_clicked(GtkWidget* widget, TGENSETUP *gensetup_t)
{
  char *szKey;
  char *data[2];
  int i = 0;

  if (gensetup_t)
    {
      data[0] = gtk_entry_get_text (GTK_ENTRY (gensetup_t->key_entry));
      if (STRLEN (data[0]))
	{
	  data[1] = gtk_entry_get_text (GTK_ENTRY (gensetup_t->value_entry));

	  /* Try to see if the keyword already exists */
	  for (i = 0; i < GTK_CLIST (gensetup_t->key_list)->rows; i++)
	    {
	      gtk_clist_get_text (GTK_CLIST (gensetup_t->key_list), i, 0,
		  &szKey);
	      if (STREQ (data[0], szKey))
		goto done;
	    }

	  /* An update operation */
	  if (i < GTK_CLIST (gensetup_t->key_list)->rows)
	    {
	      gtk_clist_set_text (GTK_CLIST (gensetup_t->key_list), i, 1,
		  data[1]);
	    }
	  else if (STRLEN (data[1]))
	    {
	      gtk_clist_append (GTK_CLIST (gensetup_t->key_list), data);
	    }
	}

      gtk_clist_sort (GTK_CLIST (gensetup_t->key_list));

    done:
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->key_entry), "");
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->value_entry), "");
    }
}


static void
gensetup_update_clicked(GtkWidget* widget, TGENSETUP *gensetup_t)
{
  char *data[2];
  int i;

  if (gensetup_t)
    {
      data[0] = gtk_entry_get_text (GTK_ENTRY (gensetup_t->key_entry));
      if (STRLEN (data[0]))
	{
	  data[1] = gtk_entry_get_text (GTK_ENTRY (gensetup_t->value_entry));

	  if (GTK_CLIST (gensetup_t->key_list)->selection != NULL)
	    i = GPOINTER_TO_INT (GTK_CLIST (gensetup_t->key_list)->selection->
		data);
	  else
	    i = 0;

	  /* An update operation */
	  if (i < GTK_CLIST (gensetup_t->key_list)->rows)
	    {
	      gtk_clist_set_text (GTK_CLIST (gensetup_t->key_list), i, 0,
		  data[0]);
	      gtk_clist_set_text (GTK_CLIST (gensetup_t->key_list), i, 1,
		  data[1]);
	    }
	}

      gtk_entry_set_text (GTK_ENTRY (gensetup_t->key_entry), "");
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->value_entry), "");
    }
}


static void
gensetup_list_select(GtkWidget* widget, gint row, gint column, GdkEvent *event, TGENSETUP *gensetup_t)
{
  char *szKey, *szValue;

  if (gensetup_t && GTK_CLIST (gensetup_t->key_list)->selection != NULL)
    {
      gtk_clist_get_text (GTK_CLIST (gensetup_t->key_list),
	  GPOINTER_TO_INT (GTK_CLIST (gensetup_t->key_list)->selection->data),
	  0, &szKey);
      gtk_clist_get_text (GTK_CLIST (gensetup_t->key_list),
	  GPOINTER_TO_INT (GTK_CLIST (gensetup_t->key_list)->selection->data),
	  1, &szValue);
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->key_entry), szKey);
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->value_entry), szValue);
      gtk_widget_set_sensitive (gensetup_t->bupdate, TRUE);
    }
}


static void
gensetup_list_unselect(GtkWidget* widget, gint row, gint column, GdkEvent *event, TGENSETUP *gensetup_t)
{
  if (gensetup_t)
    {
      gtk_widget_set_sensitive (gensetup_t->bupdate, FALSE);
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->key_entry), "");
      gtk_entry_set_text (GTK_ENTRY (gensetup_t->value_entry), "");
    }
}


static void
gensetup_ok_clicked(GtkWidget* widget, TGENSETUP *gensetup_t)
{
  char *curr, *cour, *szKey, *szValue;
  int i = 0, size = 0;

  if (gensetup_t)
    {
      /* What is the size of the block to malloc */
      if (gensetup_t->dsn_entry)
        {
          size +=
	      STRLEN (gtk_entry_get_text (GTK_ENTRY (gensetup_t->dsn_entry))) +
	      STRLEN ("DSN=") + 1;
          size += STRLEN ("Description=") + 1;
        }
      else
        {
          size = 1;
        }
      /* Malloc it (+1 for list-terminating NUL) */
      if ((gensetup_t->connstr = (char *) calloc (sizeof(char), ++size)))
	{
	  if (gensetup_t->dsn_entry)
	    {
	      for (curr = STRCONN, cour = gensetup_t->connstr;
	           i < STRCONN_NB_TOKENS; i++, curr += (STRLEN (curr) + 1))
	        switch (i)
	          {
	          case 0:
		    sprintf (cour, curr,
		        gtk_entry_get_text (GTK_ENTRY (gensetup_t->dsn_entry)));
		    cour += (STRLEN (cour) + 1);
		    break;
	          case 1:
		    sprintf (cour, curr, "");
		    cour += (STRLEN (cour) + 1);
		    break;
	          };
	    }
	  else
	    size = 1;

	  for (i = 0; i < GTK_CLIST (gensetup_t->key_list)->rows; i++)
	    {
	      gtk_clist_get_text (GTK_CLIST (gensetup_t->key_list), i, 0,
		  &szKey);
	      gtk_clist_get_text (GTK_CLIST (gensetup_t->key_list), i, 1,
		  &szValue);

	      cour = gensetup_t->connstr;
	      gensetup_t->connstr =
		  (char *) malloc (size + STRLEN (szKey) + STRLEN (szValue) +
		  2);
	      if (gensetup_t->connstr)
		{
		  memcpy (gensetup_t->connstr, cour, size);
		  sprintf (gensetup_t->connstr + size - 1, "%s=%s", szKey, szValue);
		  free (cour);
		  size += STRLEN (szKey) + STRLEN (szValue) + 2;
		}
	      else
		gensetup_t->connstr = cour;
	    }

	  /* add list-terminating NUL */
	  gensetup_t->connstr[size - 1] = '\0';
	}

      gensetup_t->dsn_entry = NULL;
      gensetup_t->key_list = NULL;
      gensetup_t->verify_conn = gtk_toggle_button_get_active(gensetup_t->verify_conn_cb);

      gtk_signal_disconnect_by_func (GTK_OBJECT (gensetup_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (gensetup_t->mainwnd);
    }
}


static void
gensetup_cancel_clicked(GtkWidget* widget, TGENSETUP *gensetup_t)
{
  if (gensetup_t)
    {
      gensetup_t->connstr = (LPSTR) -1L;

      gensetup_t->dsn_entry = NULL;
      gensetup_t->key_list = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (gensetup_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (gensetup_t->mainwnd);
    }
}


static gint delete_event( GtkWidget *widget,
	GdkEvent *event, TGENSETUP *gensetup_t)
{
  gensetup_cancel_clicked (widget, gensetup_t);

  return FALSE;
}


LPSTR
create_fgensetup (HWND hwnd, LPCSTR dsn, LPCSTR attrs, BOOL add, BOOL *verify_conn)
{
  GtkWidget *gensetup, *dialog_vbox1, *fixed1, *t_dsn, *l_dsn;
  GtkWidget *l_comment, *scrolledwindow1, *clist1;
  GtkWidget *l_key, *l_value, *t_keyword, *t_value, *l_copyright;
  GtkWidget *vbuttonbox1, *b_add, *b_update, *l_keyword, *l_valeur;
  GtkWidget *dialog_action_area1, *hbuttonbox1, *b_ok, *b_cancel;
  GtkWidget *cb_verify;
  guint button_key;
  GtkAccelGroup *accel_group;
  TGENSETUP gensetup_t;
  char buff[1024];

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return (LPSTR) attrs;

  accel_group = gtk_accel_group_new ();

  gensetup = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (gensetup), "gensetup", gensetup);
  gtk_window_set_title (GTK_WINDOW (gensetup), "File DSN Generic Setup");
  gtk_window_set_position (GTK_WINDOW (gensetup), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (gensetup), TRUE);
  gtk_window_set_policy (GTK_WINDOW (gensetup), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (gensetup);
#endif

  dialog_vbox1 = GTK_DIALOG (gensetup)->vbox;
  gtk_object_set_data (GTK_OBJECT (gensetup), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  t_dsn = gtk_entry_new ();
  gtk_widget_ref (t_dsn);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "t_dsn", t_dsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_dsn);
  gtk_fixed_put (GTK_FIXED (fixed1), t_dsn, 168, 16); 
  gtk_widget_set_uposition (t_dsn, 168, 16);
  gtk_widget_set_usize (t_dsn, 0, 0);

  l_dsn = gtk_label_new ("File Data Source Name : ");
  gtk_widget_ref (l_dsn);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_dsn", l_dsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_dsn);
  gtk_fixed_put (GTK_FIXED (fixed1), l_dsn, 8, 19); 
  gtk_widget_set_uposition (l_dsn, 8, 19);
  gtk_widget_set_usize (l_dsn, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_dsn), GTK_JUSTIFY_LEFT);

  l_comment = gtk_label_new ("If you know the driver-specific keywords for this data source, you can type them and their values here. For more information on driver-specific keywords, please consult your ODBC driver documentation.");
  gtk_widget_ref (l_comment);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_comment", l_comment,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_comment);
  gtk_fixed_put (GTK_FIXED (fixed1), l_comment, 8, 47); 
  gtk_widget_set_uposition (l_comment, 8, 47);
  gtk_widget_set_usize (l_comment, 330, 70);
  gtk_label_set_justify (GTK_LABEL (l_comment), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (l_comment), TRUE);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 8, 128); 
  gtk_widget_set_uposition (scrolledwindow1, 8, 128);
  gtk_widget_set_usize (scrolledwindow1, 320, 184);

  clist1 = gtk_clist_new (2);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "clist1", clist1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clist1);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 137);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_key = gtk_label_new (szKeysColumnNames[0]);
  gtk_widget_ref (l_key);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_key", l_key,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_key);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_key);

  l_value = gtk_label_new (szKeysColumnNames[1]);
  gtk_widget_ref (l_value);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_value", l_value,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_value);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_value);

  t_keyword = gtk_entry_new ();
  gtk_widget_ref (t_keyword);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "t_keyword", t_keyword,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_keyword);
  gtk_fixed_put (GTK_FIXED (fixed1), t_keyword, 80, 328); 
  gtk_widget_set_uposition (t_keyword, 80, 328);
  gtk_widget_set_usize (t_keyword, 158, 22);

  t_value = gtk_entry_new ();
  gtk_widget_ref (t_value);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "t_value", t_value,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_value);
  gtk_fixed_put (GTK_FIXED (fixed1), t_value, 80, 360); 
  gtk_widget_set_uposition (t_value, 80, 360);
  gtk_widget_set_usize (t_value, 158, 22);


  vbuttonbox1 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "vbuttonbox1", vbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox1);
  gtk_fixed_put (GTK_FIXED (fixed1), vbuttonbox1, 248, 320); 
  gtk_widget_set_uposition (vbuttonbox1, 248, 320);
  gtk_widget_set_usize (vbuttonbox1, 85, 69);

  b_add = gtk_button_new_with_label ("");
  button_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szKeysButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      button_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_add", b_add,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_update = gtk_button_new_with_label ("");
  button_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_update)->child),
      szKeysButtons[1]);
  gtk_widget_add_accelerator (b_update, "clicked", accel_group,
      button_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_update);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_update", b_update,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_update);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_update);
  GTK_WIDGET_SET_FLAGS (b_update, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_update, "clicked", accel_group,
      'U', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_update, FALSE);

  l_keyword = gtk_label_new ("Keyword : ");
  gtk_widget_ref (l_keyword);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_keyword", l_keyword,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_keyword);
  gtk_fixed_put (GTK_FIXED (fixed1), l_keyword, 8, 330);  
  gtk_widget_set_uposition (l_keyword, 8, 330);
  gtk_widget_set_usize (l_keyword, 69, 16);
  gtk_label_set_justify (GTK_LABEL (l_keyword), GTK_JUSTIFY_LEFT);

  l_valeur = gtk_label_new ("Value : ");
  gtk_widget_ref (l_valeur);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_valeur", l_valeur,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_valeur);
  gtk_fixed_put (GTK_FIXED (fixed1), l_valeur, 8, 363);  
  gtk_widget_set_uposition (l_valeur, 8, 363);
  gtk_widget_set_usize (l_valeur, 51, 16);
  gtk_label_set_justify (GTK_LABEL (l_valeur), GTK_JUSTIFY_LEFT);

  cb_verify = gtk_check_button_new_with_label ("Verify this connection(recommended)");
  gtk_widget_ref (cb_verify);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "cb_verify",
      cb_verify, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cb_verify);
  gtk_fixed_put (GTK_FIXED (fixed1), cb_verify, 8, 390);
  gtk_widget_set_uposition (cb_verify, 8, 390);
  gtk_widget_set_usize (cb_verify, 240, 24);

  dialog_action_area1 = GTK_DIALOG (gensetup)->action_area;
  gtk_object_set_data (GTK_OBJECT (gensetup), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "hbuttonbox1", hbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE,
      0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 10);

  b_ok = gtk_button_new_with_label ("");
  button_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_ok)->child), "_Ok");
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group, button_key,
      GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_ok);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_ok", b_ok,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_ok);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_ok);
  GTK_WIDGET_SET_FLAGS (b_ok, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      'O', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_cancel = gtk_button_new_with_label ("");
  button_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_cancel)->child),
      "_Cancel");
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      button_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_cancel);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_ok), "clicked",
      GTK_SIGNAL_FUNC (gensetup_ok_clicked), &gensetup_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (gensetup_cancel_clicked), &gensetup_t);
  /* Add button events */
  gtk_signal_connect (GTK_OBJECT (b_add), "clicked",
      GTK_SIGNAL_FUNC (gensetup_add_clicked), &gensetup_t);
  /* Update button events */
  gtk_signal_connect (GTK_OBJECT (b_update), "clicked",
      GTK_SIGNAL_FUNC (gensetup_update_clicked), &gensetup_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (gensetup), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), &gensetup_t);
  gtk_signal_connect (GTK_OBJECT (gensetup), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* List events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (gensetup_list_select), &gensetup_t);
  gtk_signal_connect (GTK_OBJECT (clist1), "unselect_row",
      GTK_SIGNAL_FUNC (gensetup_list_unselect), &gensetup_t);

  gtk_window_add_accel_group (GTK_WINDOW (gensetup), accel_group);

  gensetup_t.dsn_entry = t_dsn;
  gensetup_t.key_list = clist1;
  gensetup_t.bupdate = b_update;
  gensetup_t.key_entry = t_keyword;
  gensetup_t.value_entry = t_value;
  gensetup_t.mainwnd = gensetup;
  gensetup_t.verify_conn_cb = cb_verify;
  gensetup_t.verify_conn = *verify_conn;

  gtk_toggle_button_set_active(cb_verify, *verify_conn);

  /* Parse the attributes line */
  parse_attribute_line (&gensetup_t, dsn, attrs, add);

  gtk_widget_show_all (gensetup);
  gtk_main ();

  *verify_conn = gensetup_t.verify_conn;

  return gensetup_t.connstr;
}



LPSTR
create_keyval (HWND hwnd, LPCSTR attrs, BOOL *verify_conn)
{
  GtkWidget *gensetup, *dialog_vbox1, *fixed1;
  GtkWidget *l_comment, *scrolledwindow1, *clist1;
  GtkWidget *l_key, *l_value, *t_keyword, *t_value, *l_copyright;
  GtkWidget *vbuttonbox1, *b_add, *b_update, *l_keyword, *l_valeur;
  GtkWidget *dialog_action_area1, *hbuttonbox1, *b_ok, *b_cancel;
  GtkWidget *cb_verify;
  guint button_key;
  GtkAccelGroup *accel_group;
  TGENSETUP gensetup_t;
  char buff[1024];

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return (LPSTR) attrs;

  accel_group = gtk_accel_group_new ();

  gensetup = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (gensetup), "gensetup", gensetup);
  gtk_window_set_title (GTK_WINDOW (gensetup), "Advanced File DSN Creation Settings");
  gtk_window_set_position (GTK_WINDOW (gensetup), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (gensetup), TRUE);
  gtk_window_set_policy (GTK_WINDOW (gensetup), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (gensetup);
#endif

  dialog_vbox1 = GTK_DIALOG (gensetup)->vbox;
  gtk_object_set_data (GTK_OBJECT (gensetup), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);


  l_comment = gtk_label_new ("If you know the driver-specific keywords for this data source, you can type them and their values here. For more information on driver-specific keywords, please consult your ODBC driver documentation.");
  gtk_widget_ref (l_comment);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_comment", l_comment,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_comment);
  gtk_fixed_put (GTK_FIXED (fixed1), l_comment, 8, 7); 
  gtk_widget_set_uposition (l_comment, 8, 7);
  gtk_widget_set_usize (l_comment, 330, 70);
  gtk_label_set_justify (GTK_LABEL (l_comment), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (l_comment), TRUE);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "scrolledwindow1",
      scrolledwindow1, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixed1), scrolledwindow1, 8, 88); 
  gtk_widget_set_uposition (scrolledwindow1, 8, 88);
  gtk_widget_set_usize (scrolledwindow1, 320, 184);

  clist1 = gtk_clist_new (2);
  gtk_widget_ref (clist1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "clist1", clist1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clist1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clist1);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 0, 137);
  gtk_clist_set_column_width (GTK_CLIST (clist1), 1, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clist1));

  l_key = gtk_label_new (szKeysColumnNames[0]);
  gtk_widget_ref (l_key);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_key", l_key,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_key);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 0, l_key);

  l_value = gtk_label_new (szKeysColumnNames[1]);
  gtk_widget_ref (l_value);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_value", l_value,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_value);
  gtk_clist_set_column_widget (GTK_CLIST (clist1), 1, l_value);

  t_keyword = gtk_entry_new ();
  gtk_widget_ref (t_keyword);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "t_keyword", t_keyword,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_keyword);
  gtk_fixed_put (GTK_FIXED (fixed1), t_keyword, 80, 288); 
  gtk_widget_set_uposition (t_keyword, 80, 288);
  gtk_widget_set_usize (t_keyword, 158, 22);

  t_value = gtk_entry_new ();
  gtk_widget_ref (t_value);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "t_value", t_value,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_value);
  gtk_fixed_put (GTK_FIXED (fixed1), t_value, 80, 320); 
  gtk_widget_set_uposition (t_value, 80, 320);
  gtk_widget_set_usize (t_value, 158, 22);


  vbuttonbox1 = gtk_vbutton_box_new ();
  gtk_widget_ref (vbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "vbuttonbox1", vbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbuttonbox1);
  gtk_fixed_put (GTK_FIXED (fixed1), vbuttonbox1, 248, 280); 
  gtk_widget_set_uposition (vbuttonbox1, 248, 280);
  gtk_widget_set_usize (vbuttonbox1, 85, 69);

  b_add = gtk_button_new_with_label ("");
  button_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_add)->child),
      szKeysButtons[0]);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      button_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_add);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_add", b_add,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_add);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_add);
  GTK_WIDGET_SET_FLAGS (b_add, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_add, "clicked", accel_group,
      'A', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_update = gtk_button_new_with_label ("");
  button_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_update)->child),
      szKeysButtons[1]);
  gtk_widget_add_accelerator (b_update, "clicked", accel_group,
      button_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_update);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_update", b_update,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_update);
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), b_update);
  GTK_WIDGET_SET_FLAGS (b_update, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_update, "clicked", accel_group,
      'U', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (b_update, FALSE);

  l_keyword = gtk_label_new ("Keyword : ");
  gtk_widget_ref (l_keyword);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_keyword", l_keyword,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_keyword);
  gtk_fixed_put (GTK_FIXED (fixed1), l_keyword, 8, 290);  
  gtk_widget_set_uposition (l_keyword, 8, 290);
  gtk_widget_set_usize (l_keyword, 69, 16);
  gtk_label_set_justify (GTK_LABEL (l_keyword), GTK_JUSTIFY_LEFT);

  l_valeur = gtk_label_new ("Value : ");
  gtk_widget_ref (l_valeur);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "l_valeur", l_valeur,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_valeur);
  gtk_fixed_put (GTK_FIXED (fixed1), l_valeur, 8, 323);  
  gtk_widget_set_uposition (l_valeur, 8, 323);
  gtk_widget_set_usize (l_valeur, 51, 16);
  gtk_label_set_justify (GTK_LABEL (l_valeur), GTK_JUSTIFY_LEFT);

  cb_verify = gtk_check_button_new_with_label ("Verify this connection(recommended)");
  gtk_widget_ref (cb_verify);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "cb_verify",
      cb_verify, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cb_verify);
  gtk_fixed_put (GTK_FIXED (fixed1), cb_verify, 8, 350);
  gtk_widget_set_uposition (cb_verify, 8, 350);
  gtk_widget_set_usize (cb_verify, 230, 24);

  dialog_action_area1 = GTK_DIALOG (gensetup)->action_area;
  gtk_object_set_data (GTK_OBJECT (gensetup), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "hbuttonbox1", hbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE,
      0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 10);

  b_ok = gtk_button_new_with_label ("");
  button_key =
      gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_ok)->child), "_Ok");
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group, button_key,
      GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_ok);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_ok", b_ok,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_ok);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_ok);
  GTK_WIDGET_SET_FLAGS (b_ok, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      'O', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  b_cancel = gtk_button_new_with_label ("");
  button_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_cancel)->child),
      "_Cancel");
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      button_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_cancel);
  gtk_object_set_data_full (GTK_OBJECT (gensetup), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_ok), "clicked",
      GTK_SIGNAL_FUNC (gensetup_ok_clicked), &gensetup_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (gensetup_cancel_clicked), &gensetup_t);
  /* Add button events */
  gtk_signal_connect (GTK_OBJECT (b_add), "clicked",
      GTK_SIGNAL_FUNC (gensetup_add_clicked), &gensetup_t);
  /* Update button events */
  gtk_signal_connect (GTK_OBJECT (b_update), "clicked",
      GTK_SIGNAL_FUNC (gensetup_update_clicked), &gensetup_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (gensetup), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), &gensetup_t);
  gtk_signal_connect (GTK_OBJECT (gensetup), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  /* List events */
  gtk_signal_connect (GTK_OBJECT (clist1), "select_row",
      GTK_SIGNAL_FUNC (gensetup_list_select), &gensetup_t);
  gtk_signal_connect (GTK_OBJECT (clist1), "unselect_row",
      GTK_SIGNAL_FUNC (gensetup_list_unselect), &gensetup_t);

  gtk_window_add_accel_group (GTK_WINDOW (gensetup), accel_group);

  gensetup_t.dsn_entry = NULL;
  gensetup_t.key_list = clist1;
  gensetup_t.bupdate = b_update;
  gensetup_t.key_entry = t_keyword;
  gensetup_t.value_entry = t_value;
  gensetup_t.mainwnd = gensetup;
  gensetup_t.verify_conn_cb = cb_verify;
  gensetup_t.verify_conn = *verify_conn;

  gtk_toggle_button_set_active(cb_verify, *verify_conn);

  /* Parse the attributes line */
  parse_attribute_line (&gensetup_t, NULL, attrs, TRUE);

  gtk_widget_show_all (gensetup);
  gtk_main ();
  
  *verify_conn = gensetup_t.verify_conn;

  return gensetup_t.connstr;
}
