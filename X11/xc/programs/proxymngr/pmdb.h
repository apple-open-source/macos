/* $Xorg: pmdb.h,v 1.4 2001/02/09 02:05:34 xorgcvs Exp $ */

/*
Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.
*/


#include <X11/ICE/ICElib.h>


typedef struct _server_list {
    char *serverAddress;
    struct _server_list *next;
} server_list;


typedef struct _request_list {
    char *serviceName;
    char *serverAddress;
    char *hostAddress;
    char *startOptions;
    PMconn *requestor;
    char *listData;
    int authLen;
    char *authName;
    char *authData;
    struct _request_list *next;
} request_list;


typedef struct _running_proxy {
    Bool active;
    PMconn *pmConn;
    request_list *requests;
    server_list *servers;
    Bool refused_service;
    struct _running_proxy *next;
} running_proxy;


typedef struct _proxy_service {
    char *serviceName;
    int proxyCount;
    running_proxy *proxyList;
    struct _proxy_service *next;
} proxy_service;


typedef struct {
    int count;
    int current;
    running_proxy **list;
} running_proxy_list;



proxy_service
*FindProxyService (
	char *serviceName,
	Bool createIf);

running_proxy *
StartNewProxy (
	char *serviceName,
	char *startCommand);

running_proxy *
ConnectToProxy (
	int pmOpcode,
	char *serviceName,
	char *proxyAddress);

Status
ActivateProxyService (
	char *serviceName,
	PMconn *proxy);

void
ProxyGone (
    IceConn proxyIceConn,
    Bool *activeReqs);

running_proxy_list *
GetRunningProxyList (
	char *serviceName,
	char *serverAddress);

void
FreeProxyList (
	running_proxy_list *list);

Status
PushRequestorQueue (
	running_proxy *proxy,
	PMconn *requestor,
	running_proxy_list *runList,
	char *serviceName,
	char *serverAddress,
	char *hostAddress,
	char *startOptions,
	int authLen,
	char *authName,
	char *authData);


Status
PeekRequestorQueue (
	PMconn *proxy,
	PMconn **requestor,
	running_proxy_list **runList,
	char **serviceName,
	char **serverAddress,
	char **hostAddress,
	char **startOptions,
	int *authLen,
	char **authName,
	char **authData);

PMconn*
PopRequestorQueue (
	PMconn *pmConn,
	Bool addServer,
	Bool freeProxyList);

running_proxy *
ProxyForPMconn(
	PMconn *pmConn);
