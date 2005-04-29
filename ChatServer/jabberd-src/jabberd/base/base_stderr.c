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

#include "jabberd.h"

result base_stderr_display(instance i, dpacket p, void* args)
{   
    char* message = NULL;
    
    /* Get the raw data from the packet */
    message = xmlnode_get_data(p->x);

    if(message == NULL)
    {
        log_debug(ZONE,"base_stderr_deliver: no message available to print.");
        return r_ERR;
    }

    /* We know message is non-null so fprintf is okay. */
    fprintf(stderr, "%s\n", message);

    pool_free(p->p);
    return r_DONE;
}

result base_stderr_config(instance id, xmlnode x, void *arg)
{
    if(id == NULL)
        return r_PASS;

    if(id->type != p_LOG)
    {
        log_alert(NULL, "ERROR in instance %s: <stderr/> element only allowed in log sections", id->id);
        return r_ERR;
    }

    log_debug(ZONE,"base_stderr configuring instnace %s",id->id);

    /* Register the handler, for this instance */
    register_phandler(id, o_DELIVER, base_stderr_display, NULL);

    return r_DONE;
}

void base_stderr(void)
{
    log_debug(ZONE,"base_stderr loading...");
    register_config("stderr", base_stderr_config, NULL);
}
