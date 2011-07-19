/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be
 used in advertising or publicity pertaining to distribution
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.

 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/

#define	DEBUG_VAR evargsDebug
#include "xkbevd.h"

/***====================================================================***/

static char *
AppendBellNotifyArg(char *sink, char *arg, XkbEvent *ev)
{
    if (uStringEqual(arg,"device")||uStringEqual(arg,"D"))
	sprintf(sink,"%d",ev->bell.device);
    else if (uStringEqual(arg,"volume")||uStringEqual(arg,"v"))
	sprintf(sink,"%d",ev->bell.percent);
    else if (uStringEqual(arg,"pitch")||uStringEqual(arg,"p"))
	sprintf(sink,"%d",ev->bell.pitch);
    else if (uStringEqual(arg,"duration")||uStringEqual(arg,"d"))
	sprintf(sink,"%d",ev->bell.duration);
    else if (uStringEqual(arg,"class")||uStringEqual(arg,"c"))
	sprintf(sink,"%d",ev->bell.bell_class);
    else if (uStringEqual(arg,"id")||uStringEqual(arg,"i"))
	sprintf(sink,"%d",ev->bell.bell_id);
    else if (uStringEqual(arg,"window")||uStringEqual(arg,"w"))
	sprintf(sink,"0x%x",(unsigned int)ev->bell.window);
    else if (uStringEqual(arg,"name")||uStringEqual(arg,"n"))
	sprintf(sink,"%s",XkbAtomText(dpy,ev->bell.name,XkbMessage));
    return sink;
}

static char *
AppendAccessXNotifyArg(char *sink, char *arg, XkbEvent *ev)
{
    if (uStringEqual(arg,"device")||uStringEqual(arg,"D"))
	sprintf(sink,"%d",ev->accessx.device);
    else if (uStringEqual(arg,"detail")||uStringEqual(arg,"d")) {
	sprintf(sink,"%s",XkbAccessXDetailText(ev->accessx.detail,XkbMessage));
    }
    else if (uStringEqual(arg,"keycode")||uStringEqual(arg,"key")||
						  uStringEqual(arg,"k"))
	sprintf(sink,"%d",ev->accessx.keycode);
    else if (uStringEqual(arg,"skdelay")||uStringEqual(arg,"s"))
	sprintf(sink,"%d",ev->accessx.sk_delay);
    else if (uStringEqual(arg,"bkdelay")||uStringEqual(arg,"b"))
	sprintf(sink,"%d",ev->accessx.debounce_delay);
    return sink;
}

static char *
AppendActionMessageArg(char *sink, char *arg, XkbEvent *ev)
{
    if (uStringEqual(arg,"device")||uStringEqual(arg,"D"))
	sprintf(sink,"%d",ev->message.device);
    else if (uStringEqual(arg,"message")||uStringEqual(arg,"m"))
	sprintf(sink,"%s",ev->message.message);
    else if (uStringEqual(arg,"keycode")||uStringEqual(arg,"key")||
						  uStringEqual(arg,"k"))
	sprintf(sink,"%d",ev->message.keycode);
    else if (uStringEqual(arg,"press")||uStringEqual(arg,"p"))
	sprintf(sink,"%s",(ev->message.press?"press":"release"));
    else if (uStringEqual(arg,"event")||uStringEqual(arg,"e"))
	sprintf(sink,"%s",(ev->message.key_event_follows?"event":"no_event"));
    return sink;
}

static char *
AppendEventArg(char *sink, char *arg, XkbEvent *ev)
{
    switch (ev->any.xkb_type) {
	case XkbBellNotify:
	    sink= AppendBellNotifyArg(sink,arg,ev);
	    break;
	case XkbAccessXNotify:
	    sink= AppendAccessXNotifyArg(sink,arg,ev);
	    break;
	case XkbActionMessage:
	    sink= AppendActionMessageArg(sink,arg,ev);
	    break;
	default:
	    return sink;
    }
    sink+= strlen(sink);
    return sink;
}

static void
CopyEventArg(char **sink_inout, char **source_inout, XkbEvent *ev)
{
char buf[1024];
char *source,*sink;
char *arg;

    arg= buf;
    source= *source_inout;
    sink= *sink_inout;
    if (*source=='$') {
	*sink++= '$';
	source++;
	*sink++= '\0';
	*sink_inout= sink;
	*source_inout= source;
	return;
    }
    else if (*source=='(') {
	source++;
	while ((*source!=')')&&(*source!='\0')) {
	    *arg++= *source++;
	}
	if (*source=='\0') {
	    *sink++= '$';
	    *sink++= '\0';
	    *sink_inout= sink;
	    return;
	}
	source++;
	*arg++= '\0';
	arg= buf;
    }
    else {
	arg[0]= *source++;
	arg[1]= '\0';
    }
    sink= AppendEventArg(sink,arg,ev);
    *sink_inout= sink;
    *source_inout= source;
    return;
}

char *
SubstituteEventArgs(char *cmd, XkbEvent *ev)
{
static char buf[1024];
char *source,*sink;

    if (index(cmd,'$')==NULL)
	return cmd;
    buf[0]= '\0';
    sink= buf;
    source= cmd;
    while (*source!='\0') {
	if (*source=='$') {
	    source++;
	    CopyEventArg(&sink,&source,ev);
	}
	*sink++= *source++;
    }
    return buf;
}

