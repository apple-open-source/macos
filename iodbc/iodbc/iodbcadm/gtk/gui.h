/*
 *  gui.h
 *
 *  $Id: gui.h,v 1.1.1.1 2002/04/08 22:48:11 miner Exp $
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
#include <iodbcinst.h>
#include <gtk/gtk.h>

#ifndef	_GTKGUI_H
#define	_GTKGUI_H

extern char* szDSNColumnNames[];
extern char* szTabNames[];
extern char* szDSNButtons[];
extern char* szDriverColumnNames[];

typedef struct TLOGIN {
  GtkWidget *username,*password,*mainwnd;
  char *user, *pwd;
} TLOGIN;

typedef struct TFILEDSN {
  GtkWidget *name_entry,*mainwnd;
  char *name;
} TFILEDSN;

typedef struct TDSNCHOOSER {
	GtkWidget *mainwnd,*udsnlist,*sdsnlist;
	GtkWidget *uadd,*uremove,*utest,*uconfigure;
	GtkWidget *sadd,*sremove,*stest,*sconfigure;
	GtkWidget *fadd,*fremove,*ftest,*fconfigure;
	GtkWidget *dir_list,*file_list,*file_entry,*dir_combo;
  char *dsn, *curr_dir;
	int type_dsn;
} TDSNCHOOSER;

typedef struct TDRIVERCHOOSER {
	GtkWidget *driverlist,*mainwnd,*b_add,*b_remove,*b_configure,*b_finish;
  char *driver;
} TDRIVERCHOOSER;

typedef struct TCONNECTIONPOOLING {
	GtkWidget *driverlist,*mainwnd,*enperfmon_rb,*disperfmon_rb,*retwait_entry,*timeout_entry;
	BOOL changed;
	char *timeout;
} TCONNECTIONPOOLING;

typedef struct TTRANSLATORCHOOSER {
	GtkWidget *translatorlist,*mainwnd,*b_finish;
  char *translator;
} TTRANSLATORCHOOSER;

typedef struct TCOMPONENT {
	GtkWidget *componentlist;
} TCOMPONENT;

typedef struct TTRACING {
	GtkWidget *logfile_entry, *tracelib_entry, *b_start_stop;
	GtkWidget *donttrace_rb, *allthetime_rb, *onetime_rb;
	GtkWidget *filesel;
	BOOL changed;
} TTRACING;

typedef struct TCONFIRM {
  GtkWidget *mainwnd;
  BOOL yes_no;
} TCONFIRM;

typedef struct TDRIVERSETUP {
	GtkWidget *name_entry, *driver_entry, *setup_entry, *key_list, *bupdate;
	GtkWidget *key_entry, *value_entry;
	GtkWidget *mainwnd, *filesel;
	LPSTR connstr;
} TDRIVERSETUP;

void adddsns_to_list(GtkWidget* widget, BOOL systemDSN);
void userdsn_add_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void userdsn_remove_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void userdsn_configure_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void userdsn_test_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void systemdsn_add_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void systemdsn_remove_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void systemdsn_configure_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void systemdsn_test_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void filedsn_add_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void filedsn_remove_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void filedsn_configure_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void filedsn_test_clicked(GtkWidget* widget, TDSNCHOOSER *choose_t);
void userdsn_list_select(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void userdsn_list_unselect(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void systemdsn_list_select(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void systemdsn_list_unselect(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void filedsn_filelist_select(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void filedsn_filelist_unselect(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void filedsn_dirlist_select(GtkWidget* widget, gint row, gint column, GdkEvent *event, TDSNCHOOSER *choose_t);
void filedsn_lookin_clicked(GtkWidget* widget, void **array);
void adddrivers_to_list(GtkWidget* widget, BOOL isTrs);
void adddirectories_to_list(HWND hwnd, GtkWidget* widget, LPCSTR path);
void addfiles_to_list(HWND hwnd, GtkWidget* widget, LPCSTR path);
void addlistofdir_to_optionmenu(GtkWidget* widget, LPCSTR path, TDSNCHOOSER *choose_t);

#endif
