/* $Xorg: BuildReq.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
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

#include "RxI.h"

/* build the GET request to perform to launch the remote application */
char *
RxBuildRequest(RxReturnParams *params)
{
    char *request, *ptr;
    int len, action_len, embed_len, width_len, height_len,
	ui_len, print_len, x_ui_lbx_len, x_print_lbx_len;
    char width_str[10], height_str[10];

    /* the action parameter is mandatory */
    if (params->action == NULL)
	return NULL;

    /*
     * compute string size
     */
    action_len = embed_len = width_len = height_len = 0;
    ui_len = print_len = x_ui_lbx_len = x_print_lbx_len = 0;

    len = (action_len = strlen(params->action)) + 1; /* URL + delimiter */

    /* RX_EMBEDDED + "=" + value + delimiter,
       sizeof including '\0' no need to add anything for the delimiter */
    if (params->embedded != RxUndef)
	len += (embed_len = sizeof(RX_EMBEDDED) +
		(params->embedded == RxTrue ? sizeof(RX_YES) : sizeof(RX_NO)));

    /* RX_WIDTH + "=" + value + delimiter */
    if (params->width != RxUndef) {
	sprintf(width_str, "%d", params->width);
	len += (width_len = sizeof(RX_WIDTH) + strlen(width_str) + 1);
    }
    /* RX_HEIGHT + "=" + value + delimiter */
    if (params->height != RxUndef) {
	sprintf(height_str, "%d", params->height);
	len += (height_len = sizeof(RX_HEIGHT) + strlen(height_str) + 1);
    }
    if (params->ui != NULL)
	/* "UI=" + URL + delimiter */
	len += (ui_len = sizeof(RX_UI)+ strlen(params->ui) + 1);
    if (params->print != NULL)
	/* "PRINT=" + URL + delimiter */
	len += (print_len = sizeof(RX_PRINT) + strlen(params->print) + 1);

    /* RX_X_UI_LBX + "=" + value + delimiter,
       sizeof including '\0' no need to add anything for the delimiter */
    if (params->x_ui_lbx != RxUndef) {
	x_ui_lbx_len = sizeof(RX_X_UI_LBX);
	if (params->x_ui_lbx == RxTrue) {
	    x_ui_lbx_len += sizeof(RX_YES);
	    if (params->x_ui_lbx_auth != NULL) /* 6 for ";auth=" */
		x_ui_lbx_len += strlen(params->x_ui_lbx_auth) + 6;
	} else
	    x_ui_lbx_len += sizeof(RX_NO);
	len += x_ui_lbx_len;
    }

    /* RX_X_PRINT_LBX + "=" + value + delimiter,
       sizeof including '\0' no need to add anything for the delimiter */
    if (params->x_print_lbx != RxUndef) {
	x_print_lbx_len = sizeof(RX_X_PRINT_LBX);
	if (params->x_print_lbx == RxTrue) {
	    x_print_lbx_len += sizeof(RX_YES);
	    if (params->x_print_lbx_auth != NULL) /* 6 for ";auth=" */
		x_print_lbx_len += strlen(params->x_print_lbx_auth) + 6;
	} else
	    x_print_lbx_len += sizeof(RX_NO);
	len += x_print_lbx_len;
    }

    /*
     * malloc string and set it
     */
    request = ptr = (char *)Malloc(len);
    strcpy(ptr, params->action);
    ptr += action_len;
    if (embed_len != 0) {
	sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_EMBEDDED,
		(params->embedded == RxTrue ? RX_YES : RX_NO));
	ptr += embed_len;	/* be careful delimiter is included here */
    }
    if (width_len != 0) {
	sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_WIDTH, width_str);
	ptr += width_len;
    }
    if (height_len != 0) {
	sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_HEIGHT, height_str);
	ptr += height_len;
    }
    if (ui_len != 0) {
	sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_UI, params->ui);
	ptr += ui_len;
    }
    if (print_len != 0) {
	sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_PRINT, params->print);
	ptr += print_len;
    }
    if (x_ui_lbx_len != 0) {
	if (params->x_ui_lbx == RxTrue && params->x_ui_lbx_auth != NULL)
	    sprintf(ptr, "%c%s=%s;auth=%s", RX_QUERY_DELIMITER, RX_X_UI_LBX,
		    RX_YES, params->x_ui_lbx_auth);
	else
	    sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_X_UI_LBX,
		    (params->x_ui_lbx == RxTrue ? RX_YES : RX_NO));
	ptr += x_ui_lbx_len;
    }
    if (x_print_lbx_len != 0) {
	if (params->x_print_lbx == RxTrue
	    && params->x_print_lbx_auth != NULL)
	    sprintf(ptr, "%c%s=%s;auth=%s", RX_QUERY_DELIMITER,
		    RX_X_PRINT_LBX, RX_YES, params->x_print_lbx_auth);
	else
	    sprintf(ptr, "%c%s=%s", RX_QUERY_DELIMITER, RX_X_PRINT_LBX,
		    (params->x_print_lbx == RxTrue ? RX_YES : RX_NO));
	ptr += x_print_lbx_len;
    }

    return request;
}
