/*
 *  connectionpooling.c
 *
 *  $Id: connectionpooling.c,v 1.6 2007/02/02 11:58:14 source Exp $
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
connectionpool_finish_clicked (GtkWidget *widget,
    TCONNECTIONPOOLING *connectionpool_t)
{
  if (connectionpool_t)
    {
      connectionpool_t->changed = TRUE;

      memset(connectionpool_t->timeout, 0, sizeof(connectionpool_t->timeout)); 
      memset(connectionpool_t->probe, 0, sizeof(connectionpool_t->probe)); 
      strncpy (connectionpool_t->timeout,
	    gtk_entry_get_text (GTK_ENTRY (connectionpool_t->timeout_entry)),
	    sizeof(connectionpool_t->timeout)-1);
      strncpy (connectionpool_t->probe,
	    gtk_entry_get_text (GTK_ENTRY (connectionpool_t->probe_entry)),
	    sizeof(connectionpool_t->probe)-1);

      gtk_signal_disconnect_by_func (GTK_OBJECT (connectionpool_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (connectionpool_t->mainwnd);
    }
}


static void
connectionpool_cancel_clicked (GtkWidget *widget,
    TCONNECTIONPOOLING *connectionpool_t)
{
  if (connectionpool_t)
    {
      connectionpool_t->changed = FALSE;

      gtk_signal_disconnect_by_func (GTK_OBJECT (connectionpool_t->mainwnd),
	  GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
      gtk_main_quit ();
      gtk_widget_destroy (connectionpool_t->mainwnd);
    }
}


static gint
delete_event (GtkWidget *widget,
    GdkEvent *event, TCONNECTIONPOOLING *connectionpool_t)
{
  connectionpool_cancel_clicked (widget, connectionpool_t);

  return FALSE;
}


BOOL
create_connectionpool (HWND hwnd, TCONNECTIONPOOLING *choose_t)
{
  GtkWidget *connectionpool, *dialog_vbox1, *fixed1, *l_question;
  GtkWidget *t_cptimeout, *dialog_action_area1, *hbuttonbox1;
  GtkWidget *t_probe, *l_time, *l_probe;
  GtkWidget *b_finish, *b_cancel;
  guint b_finish_key, b_cancel_key;
  GtkAccelGroup *accel_group;
  char msg[1024];

  if (hwnd == NULL || !GTK_IS_WIDGET (hwnd) || !choose_t)
    return FALSE;

  accel_group = gtk_accel_group_new ();

  connectionpool = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (connectionpool), "connectionpool",
      connectionpool);
  gtk_window_set_title (GTK_WINDOW (connectionpool), "Connection pooling attributes");
  gtk_window_set_position (GTK_WINDOW (connectionpool), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (connectionpool), TRUE);
  gtk_window_set_policy (GTK_WINDOW (connectionpool), FALSE, FALSE, FALSE);

#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_show (connectionpool);
#endif

  dialog_vbox1 = GTK_DIALOG (connectionpool)->vbox;
  gtk_object_set_data (GTK_OBJECT (connectionpool), "dialog_vbox1",
      dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  fixed1 = gtk_fixed_new ();
  gtk_widget_ref (fixed1);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "fixed1", fixed1,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), fixed1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fixed1), 6);

  l_question =
      gtk_label_new
      ("You have now to specify the connection pooling timeout\nin seconds of the specified driver and probe query");
  gtk_widget_ref (l_question);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "l_question",
      l_question, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_question);
  gtk_fixed_put (GTK_FIXED (fixed1), l_question, 8, 8);
  gtk_widget_set_uposition (l_question, 8, 8);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_widget_set_usize (l_question, 376, 36);
#else
  gtk_widget_set_usize (l_question, 376, 24);
#endif
  gtk_label_set_justify (GTK_LABEL (l_question), GTK_JUSTIFY_LEFT);

  l_time = gtk_label_new ("Timeout:");
  gtk_widget_ref (l_time);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "l_time",
      l_time, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_time);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), l_time, 8, 48);
  gtk_widget_set_uposition (l_time, 8, 48);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), l_time, 8, 40);
  gtk_widget_set_uposition (l_time, 8, 40);
#endif
  gtk_widget_set_usize (l_time, 60, 24);
  gtk_label_set_justify (GTK_LABEL (l_time), GTK_JUSTIFY_RIGHT);

  t_cptimeout = gtk_entry_new ();
  gtk_widget_ref (t_cptimeout);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "t_cptimeout",
      t_cptimeout, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_cptimeout);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), t_cptimeout, 80, 48);
  gtk_widget_set_uposition (t_cptimeout, 80, 48);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), t_cptimeout, 80, 40);
  gtk_widget_set_uposition (t_cptimeout, 80, 40);
#endif
  gtk_widget_set_usize (t_cptimeout, 300, 22);

  if (choose_t)
    gtk_entry_set_text (GTK_ENTRY (t_cptimeout), choose_t->timeout);

  l_probe = gtk_label_new ("Query:");
  gtk_widget_ref (l_probe);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "l_probe",
      l_probe, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (l_probe);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), l_probe, 8, 78);
  gtk_widget_set_uposition (l_probe, 8, 78);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), l_probe, 8, 70);
  gtk_widget_set_uposition (l_probe, 8, 70);
#endif
  gtk_widget_set_usize (l_probe, 60, 24);
  gtk_label_set_justify (GTK_LABEL (l_probe), GTK_JUSTIFY_RIGHT);

  t_probe = gtk_entry_new ();
  gtk_widget_ref (t_probe);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "t_probe",
      t_probe, (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (t_probe);
#if GTK_CHECK_VERSION(2,0,0)
  gtk_fixed_put (GTK_FIXED (fixed1), t_probe, 80, 78);
  gtk_widget_set_uposition (t_probe, 80, 78);
#else
  gtk_fixed_put (GTK_FIXED (fixed1), t_probe, 80, 70);
  gtk_widget_set_uposition (t_probe, 80, 70);
#endif
  gtk_widget_set_usize (t_probe, 300, 22);

  if (choose_t)
    gtk_entry_set_text (GTK_ENTRY (t_probe), choose_t->probe);

  dialog_action_area1 = GTK_DIALOG (connectionpool)->action_area;
  gtk_object_set_data (GTK_OBJECT (connectionpool), "dialog_action_area1",
      dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "hbuttonbox1",
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
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "b_finish", b_finish,
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
  gtk_object_set_data_full (GTK_OBJECT (connectionpool), "b_cancel", b_cancel,
      (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (b_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), b_cancel);
  GTK_WIDGET_SET_FLAGS (b_cancel, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (b_cancel, "clicked", accel_group,
      'C', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

  /* Finish button events */
  gtk_signal_connect (GTK_OBJECT (b_finish), "clicked",
      GTK_SIGNAL_FUNC (connectionpool_finish_clicked), choose_t);
  /* Cancel button events */
  gtk_signal_connect (GTK_OBJECT (b_cancel), "clicked",
      GTK_SIGNAL_FUNC (connectionpool_cancel_clicked), choose_t);
  /* Close window button events */
  gtk_signal_connect (GTK_OBJECT (connectionpool), "delete_event",
      GTK_SIGNAL_FUNC (delete_event), choose_t);
  gtk_signal_connect (GTK_OBJECT (connectionpool), "destroy",
      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  gtk_window_add_accel_group (GTK_WINDOW (connectionpool), accel_group);

  choose_t->timeout_entry = t_cptimeout;
  choose_t->probe_entry = t_probe;
  choose_t->mainwnd = connectionpool;

  gtk_widget_show_all (connectionpool);
  gtk_main ();

  return choose_t->changed;
}
