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

result xdb_results(instance id, dpacket p, void *arg)
{
    xdbcache xc = (xdbcache)arg;
    xdbcache curx;
    int idnum;
    char *idstr;

    if(p->type != p_NORM || *(xmlnode_get_name(p->x)) != 'x') return r_PASS; /* yes, we are matching ANY <x*> element */

    log_debug(ZONE,"xdb_results checking xdb packet %s",xmlnode2str(p->x));

    if((idstr = xmlnode_get_attrib(p->x,"id")) == NULL) return r_ERR;

    idnum = atoi(idstr);

    pth_mutex_acquire(&(xc->mutex), FALSE, NULL);
    for(curx = xc->next; curx->id != idnum && curx != xc; curx = curx->next); /* spin till we are where we started or find our id# */

    /* we got an id we didn't have cached, could be a dup, ignore and move on */
    if(curx->id != idnum)
    {
        pool_free(p->p);
        pth_mutex_release(&(xc->mutex));
        return r_DONE;
    }

    /* associte only a non-error packet w/ waiting cache */
    if(j_strcmp(xmlnode_get_attrib(p->x,"type"),"error") == 0)
        curx->data = NULL;
    else
        curx->data = p->x;

    /* remove from ring */
    curx->prev->next = curx->next;
    curx->next->prev = curx->prev;


    /* set the flag to not block, and signal */
    curx->preblock = 0;
    pth_cond_notify(&(curx->cond), FALSE);

    /* Now release the master xc mutex */
    pth_mutex_release(&(xc->mutex));

    return r_DONE; /* we processed it */
}

/* actually deliver the xdb request */
/* Should be called while holding the xc mutex */
void xdb_deliver(instance i, xdbcache xc)
{
    xmlnode x;
    char ids[9];

    x = xmlnode_new_tag("xdb");
    xmlnode_put_attrib(x,"type","get");
    if(xc->set)
    {
        xmlnode_put_attrib(x,"type","set");
        xmlnode_insert_tag_node(x,xc->data); /* copy in the data */
        if(xc->act != NULL)
            xmlnode_put_attrib(x,"action",xc->act);
        if(xc->match != NULL)
            xmlnode_put_attrib(x,"match",xc->match);
    }
    xmlnode_put_attrib(x,"to",jid_full(xc->owner));
    xmlnode_put_attrib(x,"from",i->id);
    xmlnode_put_attrib(x,"ns",xc->ns);
    sprintf(ids,"%d",xc->id);
    xmlnode_put_attrib(x,"id",ids); /* to track response */
    deliver(dpacket_new(x), i);
}

result xdb_thump(void *arg)
{
    xdbcache xc = (xdbcache)arg;
    xdbcache cur, next;
    int now = time(NULL);

    pth_mutex_acquire(&(xc->mutex), FALSE, NULL);
    /* spin through the cache looking for stale requests */
    cur = xc->next;
    while(cur != xc)
    {
        next = cur->next;

        /* really old ones get wacked */
        if((now - cur->sent) > 30)
        {
            /* remove from ring */
            cur->prev->next = cur->next;
            cur->next->prev = cur->prev;

            /* make sure it's null as a flag for xdb_set's */
            cur->data = NULL;

            /* free the thread! */
            if (cur->preblock)
            {
                cur->preblock = 0;
                pth_cond_notify(&(cur->cond), FALSE);
            }

            cur = next;
            continue;
        }

        /* resend the waiting ones every so often */
        if((now - cur->sent) > 10)
            xdb_deliver(xc->i, cur);

        /* cur could have been free'd already on it's thread */
        cur = next;
    }

    pth_mutex_release(&(xc->mutex));
    return r_DONE;
}

xdbcache xdb_cache(instance id)
{
    xdbcache newx;

    if(id == NULL)
    {
        fprintf(stderr, "Programming Error: xdb_cache() called with NULL\n");
        return NULL;
    }

    newx = pmalloco(id->p, sizeof(_xdbcache));
    newx->i = id; /* flags it as the top of the ring too */
    newx->next = newx->prev = newx; /* init ring */
    pth_mutex_init(&(newx->mutex));

    /* register the handler in the instance to filter out xdb results */
    register_phandler(id, o_PRECOND, xdb_results, (void *)newx);

    /* heartbeat to keep a watchful eye on xdb_cache */
    register_beat(10,xdb_thump,(void *)newx);

    return newx;
}

/* blocks until namespace is retrieved, host must map back to this service! */
xmlnode xdb_get(xdbcache xc, jid owner, char *ns)
{
    _xdbcache newx;
    xmlnode x;
    //pth_cond_t cond = PTH_COND_INIT;

    if(xc == NULL || owner == NULL || ns == NULL)
    {
        fprintf(stderr,"Programming Error: xdb_get() called with NULL\n");
        return NULL;
    }

    /* init this newx */
    newx.i = NULL;
    newx.set = 0;
    newx.data = NULL;
    newx.ns = ns;
    newx.owner = owner;
    newx.sent = time(NULL);
    newx.preblock = 1; /* flag */
    pth_cond_init(&(newx.cond));

    /* in the future w/ real threads, would need to lock xc to make these changes to the ring */
    pth_mutex_acquire(&(xc->mutex), FALSE, NULL);
    newx.id = xc->id++;
    newx.next = xc->next;
    newx.prev = xc;
    newx.next->prev = &newx;
    xc->next = &newx;

    /* send it on it's way, holding the lock */
    xdb_deliver(xc->i, &newx);

    log_debug(ZONE,"xdb_get() waiting for %s %s",jid_full(owner),ns);
    if (newx.preblock)
        pth_cond_await(&(newx.cond), &(xc->mutex), NULL); /* blocks thread */
    pth_mutex_release(&(xc->mutex));

    /* we got signalled */
    log_debug(ZONE,"xdb_get() done waiting for %s %s",jid_full(owner),ns);

    /* newx.data is now the returned xml packet */
    /* return the xmlnode inside <xdb>...</xdb> */
    for(x = xmlnode_get_firstchild(newx.data); x != NULL && xmlnode_get_type(x) != NTYPE_TAG; x = xmlnode_get_nextsibling(x));

    /* there were no children (results) to the xdb request, free the packet */
    if(x == NULL)
        xmlnode_free(newx.data);

    return x;
}

/* sends new xml xdb action, data is NOT freed, app responsible for freeing it */
/* act must be NULL or "insert" for now, insert will either blindly insert data into the parent (creating one if needed) or use match */
/* match will find a child in the parent, and either replace (if it's an insert) or remove (if data is NULL) */
int xdb_act(xdbcache xc, jid owner, char *ns, char *act, char *match, xmlnode data)
{
    _xdbcache newx;

    if(xc == NULL || owner == NULL || ns == NULL)
    {
        fprintf(stderr,"Programming Error: xdb_set() called with NULL\n");
        return 1;
    }

    /* init this newx */
    newx.i = NULL;
    newx.set = 1;
    newx.data = data;
    newx.ns = ns;
    newx.act = act;
    newx.match = match;
    newx.owner = owner;
    newx.sent = time(NULL);
    newx.preblock = 1; /* flag */
    pth_cond_init(&(newx.cond));

    /* in the future w/ real threads, would need to lock xc to make these changes to the ring */
    pth_mutex_acquire(&(xc->mutex), FALSE, NULL);
    newx.id = xc->id++;
    newx.next = xc->next;
    newx.prev = xc;
    newx.next->prev = &newx;
    xc->next = &newx;

    /* send it on it's way */
    xdb_deliver(xc->i, &newx);

    /* wait for the condition var */
    log_debug(ZONE,"xdb_set() waiting for %s %s",jid_full(owner),ns);
    /* preblock is set to 0 if it beats us back here */
    if (newx.preblock)
        pth_cond_await(&(newx.cond), &(xc->mutex), NULL); /* blocks thread */
    pth_mutex_release(&(xc->mutex));

    /* we got signalled */
    log_debug(ZONE,"xdb_set() done waiting for %s %s",jid_full(owner),ns);

    /* newx.data is now the returned xml packet or NULL if it was unsuccessful */
    /* if it didn't actually get set, flag that */
    if(newx.data == NULL)
        return 1;

    xmlnode_free(newx.data);
    return 0;
}

/* sends new xml to replace old, data is NOT freed, app responsible for freeing it */
int xdb_set(xdbcache xc, jid owner, char *ns, xmlnode data)
{
    return xdb_act(xc, owner, ns, NULL, NULL, data);
}
