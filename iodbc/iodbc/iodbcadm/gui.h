/*
 *  gui.h
 *
 *  $Id: gui.h,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
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
#include "iodbcadm.h"

#if defined(__BEOS__)
#  include "be/gui.h"
#elif defined(_MAC)
#  include "mac/gui.h"
#elif defined(__GTK__)
#  include "gtk/gui.h"
#elif defined(_MACX)
#  include "macosx/gui.h"
#else
#  error GUI for this platform not supported ...
#endif

#ifndef	_GUI_H
#define _GUI_H

#ifdef _MACX
BOOL create_confirmadm (HWND hwnd, LPCSTR dsn, LPCSTR text);
#else
BOOL create_confirm (HWND hwnd, LPCSTR dsn, LPCSTR text);
#endif

void create_login (HWND hwnd, LPCSTR username, LPCSTR password, LPCSTR dsn, TLOGIN *log_t);
void create_dsnchooser (HWND hwnd, TDSNCHOOSER *choose_t);
void create_driverchooser (HWND hwnd, TDRIVERCHOOSER *choose_t);
void create_administrator (HWND hwnd);
void create_error (HWND hwnd, LPCSTR dsn, LPCSTR text, LPCSTR errmsg);
void create_message (HWND hwnd, LPCSTR dsn, LPCSTR text);
LPSTR create_driversetup (HWND hwnd, LPCSTR driver, LPCSTR attrs, BOOL add);
LPSTR create_filedsn (HWND hwnd);
LPSTR create_connectionpool (HWND hwnd, LPCSTR driver, LPCSTR oldtimeout);

#endif
