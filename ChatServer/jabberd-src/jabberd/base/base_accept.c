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

extern pool jabberd__runtime;

#define A_ERROR  -1
#define A_READY   1

typedef struct queue_struct
{
    int stamp;
    xmlnode x;
    struct queue_struct *next;
} *queue, _queue;

typedef struct accept_instance_st
{
    mio m;
    int state;
    char *id;
    pool p;
    instance i;
    char *ip;
    char *secret;
    int port;
    int timeout;
    int restrict_var;
    xdbcache offline;
    jid offjid;
    queue q;
    //dpacket dplast;
} *accept_instance, _accept_instance;

void base_accept_queue(accept_instance ai, xmlnode x)
{
    queue q;
    if(ai == NULL || x == NULL) return;

    q = pmalloco(xmlnode_pool(x),sizeof(_queue));
    q->stamp = time(NULL);
    q->x = x;
    q->next = ai->q;
    ai->q = q;
}

/* Write packets to a xmlio object */
result base_accept_deliver(instance i, dpacket p, void* arg)
{
    accept_instance ai = (accept_instance)arg;

    /* Insert the message into the write_queue if we don't have a MIO socket yet.. */
    if (ai->state == A_READY)
    {
        /*
         * TSBandit -- this doesn't work, since many components simply modify the node in-place
        if(ai->dplast == p) // don't return packets that they sent us! circular reference!
        {
            deliver_fail(p,"Circular Refernce Detected");
        }
        else
        */
            mio_write(ai->m, p->x, NULL, 0);
        return r_DONE;
    }

    base_accept_queue(ai, p->x);
    return r_DONE;
}


/* Handle incoming packets from the xstream associated with an MIO object */
void base_accept_process_xml(mio m, int state, void* arg, xmlnode x)
{
    accept_instance ai = (accept_instance)arg;
    xmlnode cur, off;
    queue q, q2;
    char hashbuf[41];
    jpacket jp;

    log_debug(ZONE, "process XML: m:%X state:%d, arg:%X, x:%X", m, state, arg, x);

    switch(state)
    {
        case MIO_XML_ROOT:
            /* Ensure request namespace is correct... */
            if (j_strcmp(xmlnode_get_attrib(x, "xmlns"), "jabber:component:accept") != 0)
            {
                /* Log that the connected component sent an invalid namespace */
                log_warn(ai->i->id, "Recv'd invalid namespace. Closing connection.");
                /* Notify component with stream:error */
                mio_write(m, NULL, SERROR_NAMESPACE, -1);
                /* Close the socket and cleanup */
                mio_close(m);
                break;
            }

            /* Send header w/ proper namespace, using instance i */
            cur = xstream_header("jabber:component:accept", NULL, ai->i->id);
            /* Save stream ID for auth'ing later */
            ai->id = pstrdup(ai->p, xmlnode_get_attrib(cur, "id"));
            mio_write(m, NULL, xstream_header_char(cur), -1);
            xmlnode_free(cur);
            break;

        case MIO_XML_NODE:
            /* If aio has been authenticated previously, go ahead and deliver the packet */
            if(ai->state == A_READY  && m == ai->m)
            {
                /* Hide 1.0 style transports etherx:* attribs */
                xmlnode_hide_attrib(x, "etherx:to");
                xmlnode_hide_attrib(x, "etherx:from");
                /*
                 * TSBandit -- this doesn't work.. since many components modify the node in-place
                ai->dplast = dpacket_new(x);
                deliver(ai->dplast, ai->i);
                ai->dplast = NULL;
                */

                /* if we are supposed to be careful about what comes from this socket */
                if(ai->restrict_var)
                {
                    jp = jpacket_new(x);
                    if(jp->type == JPACKET_UNKNOWN || jp->to == NULL || jp->from == NULL || deliver_hostcheck(jp->from->server) != ai->i)
                    {
                        jutil_error(x,TERROR_INTERNAL);
                        mio_write(m,x,NULL,0);
                        return;
                    }
                }

                deliver(dpacket_new(x), ai->i);
                return;
            }

            /* only other packets are handshakes */
            if(j_strcmp(xmlnode_get_name(x), "handshake") != 0)
            {
                mio_write(m, NULL, "<stream:error>Must send handshake first.</stream:error>", -1);
                mio_close(m);
                break;
            }

            /* Create and check a SHA hash of this instance's password & SID */
            shahash_r(spools(xmlnode_pool(x), ai->id, ai->secret, xmlnode_pool(x)), hashbuf);
            if(j_strcmp(hashbuf, xmlnode_get_data(x)) != 0)
            {
                mio_write(m, NULL, "<stream:error>Invalid handshake</stream:error>", -1);
                mio_close(m);
            }

            /* Send a handshake confirmation */
            mio_write(m, NULL, "<handshake/>", -1);

            /* check for existing conenction and kill it */
            if(ai->m != NULL)
            {
                log_warn(ai->i->id, "Socket override by another connection from %s",mio_ip(m));
                mio_write(ai->m, NULL, "<stream:error>Socket override by another connection.</stream:error>", -1);
                mio_close(ai->m);
            }

            /* hook us up! */
            ai->m = m;
            ai->state = A_READY;

            /* if offline, get anything stored and deliver */
            if(ai->offline != NULL)
            {
                off = xdb_get(ai->offline, ai->offjid, "base:accept:offline");
                for(cur = xmlnode_get_firstchild(off); cur != NULL; cur = xmlnode_get_nextsibling(cur))
                {
                    /* dup and deliver stored packets... XXX should probably handle NS_EXPIRE, I get lazy at 6am */
                    mio_write(m,xmlnode_dup(cur),NULL,0);
                    xmlnode_hide(cur);
                }
                xdb_set(ai->offline, ai->offjid, "base:accept:offline", off);
                xmlnode_free(off);
            }

            /* flush old queue */
            q = ai->q;
            while(q != NULL)
            {
                q2 = q->next;
                mio_write(m, q->x, NULL, 0);
                q = q2;
            }
            ai->q = NULL;

            break;

        case MIO_ERROR:
            /* make sure it's the important one */
            if(m != ai->m)
                return;

            ai->state = A_ERROR;

            /* clean up any tirds */
            while((cur = mio_cleanup(m)) != NULL)
                deliver_fail(dpacket_new(cur), "External Server Error");

            return;

        case MIO_CLOSED:
            /* make sure it's the important one */
            if(m != ai->m)
                return;

            log_debug(ZONE,"closing accepted socket");
            ai->m = NULL;
            ai->state = A_ERROR;

            return;
        default:
            return;
    }
    xmlnode_free(x);
}

/* bounce messages/pres-s10n differently if in offline mode */
void base_accept_offline(accept_instance ai, xmlnode x)
{
    jpacket p;

    if(ai->offline == NULL)
    {
        deliver_fail(dpacket_new(x),"Internal Timeout");
        return;
    }

    p = jpacket_new(x);
    switch(p->type)
    {
        case JPACKET_MESSAGE:
            /* XXX should probably handle offline events I guess, more lazy */
        case JPACKET_S10N:
            if(xdb_act(ai->offline, ai->offjid, "base:accept:offline", "insert", NULL, x) == 0)
            {
                xmlnode_free(x);
                return;
            }
            break;
        default:
            break;
    }

    deliver_fail(dpacket_new(x),"Internal Timeout");
}

/* check the packet queue for stale packets */
result base_accept_beat(void *arg)
{
    accept_instance ai = (accept_instance)arg;
    queue bouncer, lastgood, cur, next;
    int now = time(NULL);

    cur = ai->q;
    bouncer = lastgood = NULL;
    while(cur != NULL)
    {
        if( (now - cur->stamp) <= ai->timeout)
        {
            lastgood = cur;
            cur = cur->next;
            continue;
        }

        /* timed out sukkah! */
        next = cur->next;
        if(lastgood == NULL)
            ai->q = next;
        else
            lastgood->next = next;

        /* place in a special queue to bounce later on */
        cur->next = bouncer;
        bouncer = cur;

        cur = next;
    }

    while(bouncer != NULL)
    {
        next = bouncer->next;
        base_accept_offline(ai, bouncer->x);
        bouncer = next;
    }
    
    return r_DONE;
}

result base_accept_config(instance id, xmlnode x, void *arg)
{
    char *secret = xmlnode_get_tag_data(x, "secret");
    accept_instance inst;
    int port = j_atoi(xmlnode_get_tag_data(x, "port"),0);

    if(id == NULL)
    {
        log_debug(ZONE,"base_accept_config validating configuration...");
        if (port == 0 || (xmlnode_get_tag(x,"secret") == NULL))
        {
            xmlnode_put_attrib(x,"error","<accept> requires the following subtags: <port>, and <secret>");
            return r_ERR;
        }		
        return r_PASS;
    }

    log_debug(ZONE,"base_accept_config performing configuration %s\n",xmlnode2str(x));

    /* Setup the default sink for this instance */ 
    inst              = pmalloco(id->p, sizeof(_accept_instance));
    inst->p           = id->p;
    inst->i           = id;
    inst->secret      = secret;
    inst->ip          = xmlnode_get_tag_data(x,"ip");
    inst->port        = port;
    inst->timeout     = j_atoi(xmlnode_get_tag_data(x, "timeout"),10);
    if(xmlnode_get_tag(x,"restrict") != NULL)
        inst->restrict_var = 1;
    if(xmlnode_get_tag(x,"offline") != NULL)
    {
        inst->offline = xdb_cache(id);
        inst->offjid = jid_new(id->p,id->id);
    }

    /* Start a new listening thread and associate this <listen> tag with it */
    if(mio_listen(inst->port, inst->ip, base_accept_process_xml, (void*)inst, NULL, mio_handlers_new(NULL, NULL, MIO_XML_PARSER)) == NULL)
    {
        xmlnode_put_attrib(x,"error","<accept> unable to listen on the configured ip and port");
        return r_ERR;
    }

    /* Register a packet handler and cleanup heartbeat for this instance */
    register_phandler(id, o_DELIVER, base_accept_deliver, (void *)inst);

    /* timeout check */
    register_beat(inst->timeout, base_accept_beat, (void *)inst);

    return r_DONE;
}


void base_accept(void)
{
    log_debug(ZONE,"base_accept loading...\n");
    register_config("accept",base_accept_config,NULL);
}
