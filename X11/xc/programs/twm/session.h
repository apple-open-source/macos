/* $XFree86: xc/programs/twm/session.h,v 1.2 2004/02/13 23:58:53 dawes Exp $ */
/*
 * Copyright (C) 1998 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _SESSION_H
#define _SESSION_H
#include <X11/SM/SMlib.h>

extern void ConnectToSessionManager ( char *previous_id );
extern void DieCB ( SmcConn smcConn, SmPointer clientData );
extern char * GetClientID ( Window window );
extern int GetWindowConfig ( TwmWindow *theWindow, short *x, short *y, unsigned short *width, unsigned short *height, Bool *iconified, Bool *icon_info_present, short *icon_x, short *icon_y, Bool *width_ever_changed_by_user, Bool *height_ever_changed_by_user );
extern char * GetWindowRole ( Window window );
extern void ProcessIceMsgProc ( XtPointer client_data, int *source, XtInputId *id );
extern int ReadWinConfigEntry ( FILE *configFile, unsigned short version, TWMWinConfigEntry **pentry );
extern void ReadWinConfigFile ( char *filename );
extern int read_byte ( FILE *file, unsigned char *bp );
extern int read_counted_string ( FILE *file, char **stringp );
extern int read_short ( FILE *file, short *shortp );
extern int read_ushort ( FILE *file, unsigned short *shortp );
extern void SaveCompleteCB ( SmcConn smcConn, SmPointer clientData );
extern void SaveYourselfCB ( SmcConn smcConn, SmPointer clientData, int saveType, Bool shutdown, int interactStyle, Bool fast );
extern void SaveYourselfPhase2CB ( SmcConn smcConn, SmPointer clientData );
extern void ShutdownCancelledCB ( SmcConn smcConn, SmPointer clientData );
extern int WriteWinConfigEntry ( FILE *configFile, TwmWindow *theWindow, char *clientId, char *windowRole );
extern int write_byte ( FILE *file, unsigned char b );
extern int write_counted_string ( FILE *file, char *string );
extern int write_short ( FILE *file, short s );
extern int write_ushort ( FILE *file, unsigned short s );
 
extern SmcConn smcConn;

#endif
