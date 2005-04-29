/* $XConsortium: os2_select.h /main/1 1996/05/13 16:38:30 kaleb $ */
/*
 * (c) Copyright 1996 by Sebastien Marineau
 *			<marineau@genie.uottawa.ca>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
 * HOLGER VEIT  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_select.h,v 3.2 2004/02/14 00:10:18 dawes Exp $ */

/* Header file for os2_select.c */

#define MAX_TCP 256

#define MOUSE_SEM_KEY 0x0F01
#define KBD_SEM_KEY 0x0F02
#define PIPE_SEM_KEY 0x0F03
#define SOCKET_SEM_KEY 0x0F04
#define SWITCHTO_SEM_KEY 0x0F05


struct select_data
{
   fd_set read_copy;
   fd_set write_copy;
   BOOL have_read;
   BOOL have_write;
   int tcp_select_mask[MAX_TCP];
   int tcp_emx_handles[MAX_TCP];
   int tcp_select_copy[MAX_TCP];
   int tcp_select_monitor[MAX_TCP];
   int socket_nread;
   int socket_nwrite;
   int socket_ntotal;
   int pipe_ntotal;
   int pipe_have_write;
   int max_fds;
};



