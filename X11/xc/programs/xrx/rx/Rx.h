/* $Xorg: Rx.h,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
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
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#ifndef _Rx_h
#define _Rx_h

/* a fancy Boolean type */
typedef enum { RxUndef=-1, RxFalse=0, RxTrue=1 } RxBool;


/* various enums, starting from index 1,
   with a dummy last value allowing to easily get the size,
   new values must be inserted before the last one */
typedef enum { UI=1, Print, LASTService } RxService;
typedef enum { XUI=1, LASTUIProtocol } RxUIProtocol;
typedef enum { XPrint=1, LASTPrintProtocol } RxPrintProtocol;

typedef enum { MitMagicCookie1=1, LASTXAuthentication } RxXAuthentication;

/* for each of the above enum define the corresponding max array size,
   counting 1 for NULL terminating */
#define MAX_SERVICES LASTService
#define MAX_UIPROTOS LASTUIProtocol
#define MAX_PRINTPROTOS LASTPrintProtocol

#define MAX_XAUTHENTICATIONS LASTXAuthentication

/* RxParams structure,
   designed so it can handle whatever parameter is defined in an RX document */
typedef struct {
    short version;		/* document version number */
    short revision;		/* document revision number */

    char *action;		/* URL */
    RxBool embedded;
    RxBool auto_start;
    int width;
    int height;
    char *app_group;		/* application group name */

    /* NULL terminated "lists" */
    RxService required_services[MAX_SERVICES];
    RxUIProtocol ui[MAX_UIPROTOS];
    RxPrintProtocol print[MAX_PRINTPROTOS];

    /* private params */
    RxBool x_ui_input_method;
    char *x_ui_input_method_url;

    RxBool x_ui_lbx;
    RxBool x_print_lbx;

    RxXAuthentication x_auth[MAX_XAUTHENTICATIONS];
    char *x_auth_data[MAX_XAUTHENTICATIONS];

    RxXAuthentication x_ui_auth[MAX_XAUTHENTICATIONS];
    char *x_ui_auth_data[MAX_XAUTHENTICATIONS];

    RxXAuthentication x_print_auth[MAX_XAUTHENTICATIONS];
    char *x_print_auth_data[MAX_XAUTHENTICATIONS];

    RxXAuthentication x_ui_lbx_auth[MAX_XAUTHENTICATIONS];
    char *x_ui_lbx_auth_data[MAX_XAUTHENTICATIONS];

    RxXAuthentication x_print_lbx_auth[MAX_XAUTHENTICATIONS];
    char *x_print_lbx_auth_data[MAX_XAUTHENTICATIONS];

} RxParams;


typedef struct {
    RxBool embedded;
    int width;
    int height;
    char *action;
    char *ui;			/* URL */
    char *print;		/* URL */

    /* private params */
    RxBool x_ui_lbx;
    char *x_ui_lbx_auth;
    RxBool x_print_lbx;
    char *x_print_lbx_auth;
} RxReturnParams;


/* functions */
extern int
RxReadParams(char *stream,
	     char **argn_ret[], char **argv_ret[], int *argc_ret);

extern void RxInitializeParams(RxParams *params);

extern int
RxParseParams(char *argn[], char *argv[], int argc, RxParams *params,
	      int debug);	/* (1/0) */

extern char *RxBuildRequest(RxReturnParams *params);

extern int RxFreeParams(RxParams *params);
extern int RxFreeReturnParams(RxReturnParams *params);

#endif /* _Rx_h */
