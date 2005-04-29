/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "JOSL").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the JOSL. You
 * may obtain a copy of the JOSL at http://www.jabber.org/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the JOSL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the JOSL
 * for the specific language governing rights and limitations under the
 * JOSL.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2002 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the JOSL,
 * indicate your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.  If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the JOSL or the GPL. 
 * 
 * 
 * --------------------------------------------------------------------------*/
#include "jsm.h"


/*************
Welcome!

So... you want to play with the innards of jsm, huh?  Cool, this should be fun, and it isn't too hard either :)

The API that you're going to be working with is starting to show it's age, it was created back in the era of 0.9, in early 2000.
It's still quite functional and usable, but not as clean as a purist geek might like.  Don't hesistate to code against it though,
it's not going anywhere until the 2.0 release (probably 02), and even then we'll make sure it remains as compatible as possible (if not fully).

A very general overview:
 -> packets come into jsm
 -> jsm tracks "sessions", or logged in users
 -> jsm determins the right session, and the packet is delivered to that session
 -> packets can either be of the type the user generated and is sending (OUT) or receiving from others (IN)
 -> modules are called to process the packets

Modules can also hook in when a session starts, ends, and when packets are just send to="server" or when the user is offline.
Most of the module api symbols, defines, etc are defined in the jsm.h file, consult it frequently.

To get your module building, it's very ugly right now, but either compile it by hand to a .so and include it in the jabber.xml,
or edit the jsm/Makefile and jsm/modules/Makefile and include it in the jabber.xml just like the other modules (cut/paste/edit a few lines in all those files).

You can often follow the logic bottom-up, the first function that registers the module is at the bottom, and subsequent callbacks are above it.
So, head to the bottom of this file to get started!

**************/

/* check for packets that are sent to="servername/example" and reply */
mreturn mod_example_server(mapi m, void *arg)
{
    xmlnode body;

    /* we only handle messages, ignore the rest */    
    if(m->packet->type != JPACKET_MESSAGE) return M_IGNORE;

    /* second, is this message sent to the right resource? */
    if(m->packet->to->resource == NULL || strncasecmp(m->packet->to->resource,"example",7) != 0) return M_PASS;

    log_debug("mod_example","handling example request from %s",jid_full(m->packet->from));

    /* switch the to/from headers, using a utility */
    jutil_tofrom(m->packet->x);

    /* hide the old body */
    xmlnode_hide(xmlnode_get_tag(m->packet->x,"body"));

    /* insert our own and fill it up */
    body = xmlnode_insert_tag(m->packet->x,"body");
    xmlnode_insert_cdata(body,"this is the mod_example_server reply",-1);

    /* reset the packet and deliver it again */
    jpacket_reset(m->packet);
    js_deliver(m->si,m->packet);

    /* we handled the packet */
    return M_HANDLED;
}

/* this one isn't used, but is just here to show off what they all have in common */
mreturn mod_example_generic(mapi m, void *arg)
{
    /* every callback is passed the generic mapi data and the optional argument from when they were registered below */
    /* the mapi data contains pointers to common peices of data for the callback (each only sent with relevant events):
       m->si        the session instance data, see jsm.h and the jsmi struct
       m->packet    the packet that this callback is processing, see the jpacket_* functions in lib.h, and m->packet->x is the xmlnode of the actual data (xmlnode_* in lib.h too)
       m->e         the e_EVENT that this call is, not usually used unless you're overloading a function
       m->user      the udata struct, containing generic information about the user this packet is related to
       m->s         the session struct, data for the particular session
    */
    /* the callbacks can return different signals:
       M_PASS       I don't want to process this packet, or didn't do anything to it
       M_IGNORE     I never want to see this m->packet->type again, I don't handle those
       M_HANDLED    I consumed the m->packet and processed it, it is no longer valid, I've resent it or free'd it
    */

    /* the first thing you can do is filter out just the packet types we care about, ignore the rest */
    if(m->packet->type != JPACKET_MESSAGE) return M_IGNORE;

    /* second, you usually validate that it is a valid request, or one relevant to what you want to do */
    if( /* some condition */ 0) return M_PASS;

    /* it's usually useful at this point to add some debugging output, the first argument ZONE includes the mod_example.c:123, the rest is the same as a printf */
    log_debug(ZONE,"handling example request from %s",jid_full(m->packet->from));

    /* here you can perform some logic on the packet, modifying it, and free'ing or delivering it elsewhere */

    /* since we processed the packet, signal that back */
    return M_HANDLED;
}

/* the main startup/initialization function */
void mod_example(jsmi si)
{
    /* in here we register our callbacks with the "events" we are interested in */
    /* each callback can register an argument (we're passing NULL) that is passed to them again when called */
    /* before looking at specific callbacks above, take a chance now to look at the mod_example_generic one above that explains what they all have in common */

    /* this event is when packets are sent to the server address, to="jabber.org", often used for administrative purposes or special server/resources */
    js_mapi_register(si,e_SERVER,mod_example_server,NULL);
}


