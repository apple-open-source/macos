/* $Xorg: xfwp.c,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */

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

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>		
#include <netinet/in.h>

#include <X11/ICE/ICElib.h>

#include "xfwp.h"
#include "io.h"
#include "misc.h"
#include "pm.h"
#include "transport.h"

/*
 * Global variables
 */
struct clientDataStruct 	global_data;  	/* for ICE callbacks */

struct pm_conn_buf 		** pm_conn_array;
struct server_list 		** server_array;
struct client_conn_buf 		** client_conn_array;

char **SitePolicies    = NULL;	/* list of site security policy strings */

int    SitePolicyCount = 0;	/* count of elements in SitePolicies */

Bool   SitePolicyPermit = 0;	/* True  := permit iff server supports 
				 *      at least one listed policy,
			         *  False := deny if server has any of 
				 *      the listed policies.
			         */

int 
main (
    int 			argc, 
    char 			* argv[])
{
    int 			* pm_listen_array;
    fd_set 			readable, writable, rinit, winit;
    int				nfds = 0;
    int 			nready = 0;
    struct ICE_setup_info 	pm_conn_setup;
    IceListenObj *		listen_objects;
    struct config *		config_info;

    /*
     * setup the global client data struct; we need to do this in order
     * to access program data in the ICE FWPprocessMessages() callback 
     * without making everything global!  See FWPprotocolSetupProc() for
     * the rest of what we are doing
     */
    config_info = (struct config *) Malloc(sizeof(struct config));

    global_data.config_info = config_info;
    global_data.nfds =  &nfds;
    global_data.rinit = &rinit;
    global_data.winit = &winit;

    /*
     * complete the setup
     */
    doProcessInputArgs(config_info, argc, argv);

    if ((doHandleConfigFile(config_info)) == FAILURE)
	exit(1);

    if ((doInitDataStructs(config_info, 
			   &pm_conn_setup)) == FAILURE)
	exit(1);

    /*
     * install the ICE i/o error handler
     */
    doInstallIOErrorHandler();

    /*
     * zero the select() read/write masks
     */
    FD_ZERO(&readable);
    FD_ZERO(&writable);
    FD_ZERO(&rinit);
    FD_ZERO(&winit);

    /*
     * create listener socket(s) for PM connections
     */
    if (!doSetupPMListen(config_info->pm_listen_port, 
			 &config_info->num_pm_listen_ports,
			 &pm_listen_array, 
			 &listen_objects, 
			 &nfds, 
			 &rinit))
	exit(1);

    while(1)
    {
	readable = rinit;
	writable = winit;

	doSelect (config_info, 
		  &nfds, 
		  &nready, 
		  &readable, 
		  &writable);

	doCheckTimeouts (config_info, 
			 &nready, 
			 &rinit, 
			 &winit, 
			 &readable, 
			 &writable);

	doProcessSelect (&nfds, 
			 &nready, 
			 &readable, 
			 &writable, 
			 &rinit, 
			 &winit, 
			 pm_listen_array, 
			 config_info, 
			 &listen_objects);
    } 
}
