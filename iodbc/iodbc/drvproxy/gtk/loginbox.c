/*
 *  loginbox.c
 *
 *  $Id: loginbox.c,v 1.5 2007/02/02 11:58:14 source Exp $
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


static void
login_ok_clicked (GtkWidget *widget, TLOGIN *log_t)
{
  if (log_t)
    {
      log_t->user = (char *) malloc (sizeof (char) *
	  (STRLEN (gtk_entry_get_text (GTK_ENTRY (log_t->username))) + 1));
      log_t->pwd = (char *) malloc (sizeof (char) *
	  (STRLEN (gtk_entry_get_text (GTK_ENTRY (log_t->password))) + 1));

      if (log_t->user)
	strcpy (log_t->user,
	    gtk_entry_get_text (GTK_ENTRY (log_t->username)));
      if (log_t->pwd)
	strcpy (log_t->pwd, gtk_entry_get_text (GTK_ENTRY (log_t->password)));

      log_t->username = log_t->password = NULL;
      log_t->ok = TRUE;

      gtk_signal_disconnect_by_func (GTK_OBJECT (log_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (log_t->mainwnd);
    }
}


static void
login_cancel_clicked (GtkWidget *widget, TLOGIN *log_t)
{
  if (log_t)
    {
      log_t->user = log_t->pwd = NULL;
      log_t->username = log_t->password = NULL;
      log_t->ok = FALSE;

      gtk_signal_disconnect_by_func (GTK_OBJECT (log_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (log_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget, GdkEvent *event, TLOGIN *log_t)
{
  login_cancel_clicked (widget, log_t);

  return FALSE;
}


void
create_login (HWND hwnd, LPCSTR username, LPCSTR password, LPCSTR dsn,
    TLOGIN *log_t)
{
  GtkWidget *login, *dialog_vbox1, *table1, *l_user, *l_password;
  GtkWidget *t_user, *t_password, *dialog_action_area1, *hbuttonbox1;
  GtkWidget *b_ok, *b_cancel;
  GtkAccelGroup *accel_group;
  guint b_ok_key, b_cancel_key;
  char buff[1024];

  if (hwnd == (HWND) - 1L)
    {
      gtk_init (0, NULL);
      hwnd = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    }

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return;

  accel_group = gtk_accel_group_new ();

  login = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (login), "login", login);
  sprintf (buff, "Login for DSN %s ...", (dsn) ? dsn : "Unknown");
  gtk_window_set_title (GTK_WINDOW (login), buff);
  gtk_window_set_position (GTK_WINDOW (login), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (login), TRUE);
  gtk_window_set_policy (GTK_WINDOW (login), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (login);
#endif

  dialog_vbox1 = GTK_DIALOG (login)->vbox;
  gtk_object_set_data (GTK_OBJECT (login), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  table1 = gtk_table_new (2, 2, TRUE);
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (login), "table1", table1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 6);

  l_user = gtk_label_new ("Username : ");
  gtk_widget_ref (l_user);
  gtk_object_set_data_full (GTK_OBJECT (login), "l_user", l_user,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_user);
  gtk_table_attach (GTK_TABLE (table1), l_user, 0, 1, 0, 1,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_user), GTK_JUSTIFY_LEFT);

  l_password = gtk_label_new ("Password : ");
  gtk_widget_ref (l_password);
  gtk_object_set_data_full (GTK_OBJECT (login), "l_password", l_password,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_password);
  gtk_table_attach (GTK_TABLE (table1), l_password, 0, 1, 1, 2,
      (GtkAttachOptions) (0), (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (l_password), GTK_JUSTIFY_LEFT);

  t_user = gtk_entry_new ();
  gtk_widget_ref (t_user);
  gtk_object_set_data_full (GTK_OBJECT (login), "t_user", t_user,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_user);
  gtk_table_attach (GTK_TABLE (table1), t_user, 1, 2, 0, 1,
      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
      (GtkAttachOptions) (0), 0, 0);
  if (username && STRLEN (username))
    gtk_entry_set_text (GTK_ENTRY (t_user), username);

  t_password = gtk_entry_new ();
  gtk_widget_ref (t_password);
  gtk_object_set_data_full (GTK_OBJECT (login), "t_password", t_password,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_password);
  gtk_table_attach (GTK_TABLE (table1), t_password, 1, 2, 1, 2,
      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
      (GtkAttachOptions) (0), 0, 0);
  gtk_entry_set_visibility (GTK_ENTRY (t_password), FALSE);
  if (password && STRLEN (password))
    gtk_entry_set_text (GTK_ENTRY (t_password), password);

  dialog_action_area1 = GTK_DIALOG (login)->action_area;
  gtk_object_set_data (GTK_OBJECT (login), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (login), "hbuttonbox1", hbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE,
      0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 10);
  gtk_button_box_set_child_ipadding (GTK_BUTTON_BOX (hbuttonbox1), 0, 0);

  b_ok = gtk_button_new_with_label ("");
  b_ok_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (b_ok)->child), "_Ok");
  gtk_widget_add_accelerator (b_ok, "clicked", accel_group,
      b_ok_key, GDK_MOD1_MASK, 0);
  gtk_widget_ref (b_ok);
  gtk_object_set_data_full (GTK_OBJECT (login), "b_ok", b_ok,
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
  gtk_object_set_data_full (GTK_OBJECT (login), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Ok button events */
  gtk_signal_connect (GTK_OBJECT (b_ok), "clicked",
      GTK_SIGNAL_FUNC (login_ok_clicked), log_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (login_cancel_clicked), log_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (login), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), log_t);
  gtk_signal_connect (GTK_OBJECT (login), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  gtk_window_add_accel_group (GTK_WINDOW (login), accel_group);

  log_t->username = t_user;
  log_t->password = t_password;
  log_t->user = log_t->pwd = NULL;
  log_t->mainwnd = login;

  gtk_widget_show_all (login);
  gtk_main ();
}
