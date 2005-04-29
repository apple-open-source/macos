/*
 *  filedsn.c
 *
 *  $Id: filedsn.c,v 1.3 2004/11/11 01:52:39 luesang Exp $
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

#include "gui.h"

static void
filedsn_finish_clicked (GtkWidget *widget, TFILEDSN *filedsn_t)
{
  if (filedsn_t)
    {
      filedsn_t->name =
	  (char *) malloc (sizeof (char) *
	  (STRLEN (gtk_entry_get_text (GTK_ENTRY (filedsn_t->name_entry))) +
	      1));
      if (filedsn_t->name)
	strcpy (filedsn_t->name,
	    gtk_entry_get_text (GTK_ENTRY (filedsn_t->name_entry)));

      gtk_signal_disconnect_by_func (GTK_OBJECT (filedsn_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (filedsn_t->mainwnd);
    }
}


static void
filedsn_cancel_clicked (GtkWidget *widget, TFILEDSN *filedsn_t)
{
  if (filedsn_t)
    {
      filedsn_t->name = NULL;

      gtk_signal_disconnect_by_func (GTK_OBJECT (filedsn_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (filedsn_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget, GdkEvent *event, TFILEDSN *filedsn_t)
{
  filedsn_cancel_clicked (widget, filedsn_t);

  return FALSE;
}


LPSTR
create_filedsn (HWND hwnd)
{
  GtkWidget *filedsn, *dialog_vbox1, *fixed1, *l_question;
  GtkWidget *t_dsn, *dialog_action_area1, *hbuttonbox1;
  GtkWidget *b_finish, *b_cancel;
  guint b_finish_key, b_cancel_key;
  GtkAccelGroup *accel_group;
  TFILEDSN filedsn_t;

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd))
    return NULL;

  accel_group = gtk_accel_group_new ();

  filedsn = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (filedsn), "filedsn", filedsn);
  gtk_window_set_title (GTK_WINDOW (filedsn), "Create a File DSN");
  gtk_window_set_position (GTK_WINDOW (filedsn), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (filedsn), TRUE);
  gtk_window_set_policy (GTK_WINDOW (filedsn), FALSE, FALSE, FALSE);

  dialog_vbox1 = GTK_DIALOG (filedsn)->vbox;
  gtk_object_set_data (GTK_OBJECT (filedsn), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (filedsn), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  l_question =
      gtk_label_new
      ("You have now to specify the name of the DSN you want to setup :");
  gtk_widget_ref (l_question);
  gtk_object_set_data_full (GTK_OBJECT (filedsn), "l_question", l_question,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_question);
  gtk_fixed_put (GTK_FIXED (fixed1), l_question, 8, 8);
  gtk_widget_set_uposition (l_question, 8, 8);
  gtk_widget_set_usize (l_question, 376, 24);
  gtk_label_set_justify (GTK_LABEL (l_question), GTK_JUSTIFY_LEFT);

  t_dsn = gtk_entry_new ();
  gtk_widget_ref (t_dsn);
  gtk_object_set_data_full (GTK_OBJECT (filedsn), "t_dsn", t_dsn,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_dsn);
  gtk_fixed_put (GTK_FIXED (fixed1), t_dsn, 40, 40);
  gtk_widget_set_uposition (t_dsn, 40, 40);
  gtk_widget_set_usize (t_dsn, 340, 22);

  dialog_action_area1 = GTK_DIALOG (filedsn)->action_area;
  gtk_object_set_data (GTK_OBJECT (filedsn), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (filedsn), "hbuttonbox1", hbuttonbox1,
      (GtkDestroyNotify) gtk_widget_unref);
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
  gtk_object_set_data_full (GTK_OBJECT (filedsn), "b_finish", b_finish,
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
  gtk_object_set_data_full (GTK_OBJECT (filedsn), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Finish button events */
  gtk_signal_connect (GTK_OBJECT (b_finish), "clicked",
      GTK_SIGNAL_FUNC (filedsn_finish_clicked), &filedsn_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (filedsn_cancel_clicked), &filedsn_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (filedsn), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), &filedsn_t);
  gtk_signal_connect (GTK_OBJECT (filedsn), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  gtk_window_add_accel_group (GTK_WINDOW (filedsn), accel_group);

  filedsn_t.name_entry = t_dsn;
  filedsn_t.mainwnd = filedsn;
  filedsn_t.name = NULL;

  gtk_widget_show_all (filedsn);
  gtk_main ();

  return filedsn_t.name;
}
