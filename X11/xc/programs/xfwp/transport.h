/* $Xorg: transport.h,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
/*

Copyright "1986-1997, 1998  The Open Group 

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and the following permission notice
shall be included in all copies of the Software:

THE SOFTWARE IS PROVIDED "AS IS ", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
AND NON-INFRINGEMENT. IN NO EVENT SHALL THE OPEN GROUP BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER SIABILITIY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN
CONNNECTION WITH THE SOFTWARE OR THE USE OF OTHER DEALINGS IN
THE SOFTWARE.

Except as contained in this notice, the name of The Open Group
shall not be used in advertising or otherwise to promote the use
or other dealings in this Software without prior written
authorization from The Open Group.

X Window System is a trademark of The Open Group.

*/

#ifndef _TRANSPORT_H
#define _TRANSPORT_H

#ifndef SOMAXCONN
#define SOMAXCONN 		128
#endif

#define MAX_HOSTNAME_LEN  	256

#define X_SERVER_PORT_BASE	6000


extern int
doSetupRemClientListen(
    char ** listen_port_string,
    struct clientDataStruct * program_data,
    char * server_address);

extern void 
doSelect(struct
    config * config_info,
    int * nfds,
    int * nready,
    fd_set * readable,
    fd_set * writable);

extern int 
doServerConnectSetup(
    char * x_server_hostport,
    int * server_connect_fd,
    struct sockaddr_in * server_sockaddr_in);

extern int
doServerConnect(
    int * server_connect_fd,
    struct sockaddr_in * server_sockaddr_in);

#endif /* _TRANSPORT_H */
