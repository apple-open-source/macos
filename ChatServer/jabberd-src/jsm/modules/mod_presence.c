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

/*
OUR JOB:

three sets of jids

T: trusted (roster s10ns)
A: availables (who knows were available)
I: invisibles (who were invisible to)


action points:

*** broadcasting available presence: intersection of T and A
        (don't broadcast updates to them if they don't think we're
available any more, either we told them that or their jid returned a
presence error)

*** broadcasting unavailable presence: union of A and I
        (even invisible jids need to be notified when going unavail,
since invisible is still technically an available presence and may be
used as such by a transport or other remote service)

*** allowed to return presence to a probe when available: compliment of I in T
        (all trusted jids, except the ones were invisible to, may poll
our presence any time)

*** allowed to return presence to a probe when invisible: intersection of T and A
        (of the trusted jids, only the ones we've sent availability to can poll,
and we return a generic available presence)

*** individual avail presence: forward, add to A, remove from I 

*** individual unavail presence: forward and remove from A, remove from I

*** individual invisible presence: add to I, remove from A

*** first avail: populate A with T and broadcast

*/

/* track our lists */
typedef struct modpres_struct
{
    int invisible;
    jid A;
    jid I;
    jid bcc;
} *modpres, _modpres;


/* util to check if someone knows about us */
int _mod_presence_search(jid id, jid ids)
{
    jid cur;
    for(cur = ids; cur != NULL; cur = cur->next)
        if(jid_cmp(cur,id) == 0)
            return 1;
    return 0;
}

/* remove a jid from a list, returning the new list */
jid _mod_presence_whack(jid id, jid ids)
{
    jid curr;

    if(id == NULL || ids == NULL) return NULL;

    /* check first */
    if(jid_cmp(id,ids) == 0) return ids->next;

    /* check through the list, stopping at the previous list entry to a matching one */
    for(curr = ids;curr != NULL && jid_cmp(curr->next,id) != 0;curr = curr->next);

    /* clip it out if found */
    if(curr != NULL)
        curr->next = curr->next->next;

    return ids;
}

/* just brute force broadcast the presence packets to whoever should be notified */
void _mod_presence_broadcast(session s, jid notify, xmlnode x, jid intersect)
{
    jid cur;
    xmlnode pres;

    for(cur = notify; cur != NULL; cur = cur->next)
    {
        if(intersect != NULL && !_mod_presence_search(cur,intersect)) continue; /* perform insersection search, must be in both */
        s->c_out++;
        pres = xmlnode_dup(x);
        xmlnode_put_attrib(pres, "to",jid_full(cur));
        js_deliver(s->si,jpacket_new(pres));
    }
}

/* filter the incoming presence to this session */
mreturn mod_presence_in(mapi m, void *arg)
{
    modpres mp = (modpres)arg;
    xmlnode pres;

    if(m->packet->type != JPACKET_PRESENCE) return M_IGNORE;

    log_debug("mod_presence","incoming filter for %s",jid_full(m->s->id));

    if(jpacket_subtype(m->packet) == JPACKET__PROBE)
    { /* reply with our presence */
        if(m->s->presence == NULL)
        {
            log_debug("mod_presence","probe from %s and no presence to return",jid_full(m->packet->from));
        }else if(!mp->invisible && js_trust(m->user,m->packet->from) && !_mod_presence_search(m->packet->from,mp->I)){ /* compliment of I in T */
            log_debug("mod_presence","got a probe, responding to %s",jid_full(m->packet->from));
            pres = xmlnode_dup(m->s->presence);
            xmlnode_put_attrib(pres,"to",jid_full(m->packet->from));
            js_session_from(m->s, jpacket_new(pres));
        }else if(mp->invisible && js_trust(m->user,m->packet->from) && _mod_presence_search(m->packet->from,mp->A)){ /* when invisible, intersection of A and T */
            log_debug("mod_presence","got a probe when invisible, responding to %s",jid_full(m->packet->from));
            pres = jutil_presnew(JPACKET__AVAILABLE,jid_full(m->packet->from),NULL);
            js_session_from(m->s, jpacket_new(pres));
        }else{
            log_debug("mod_presence","%s attempted to probe by someone not qualified",jid_full(m->packet->from));
        }
        xmlnode_free(m->packet->x);
        return M_HANDLED;
    }

    if(m->packet->from == NULL || jid_cmp(m->packet->from,m->s->id) == 0)
    { /* this is our presence, don't send to ourselves */
        xmlnode_free(m->packet->x);
        return M_HANDLED;
    }

    /* if a presence packet bounced, remove from the A list */
    if(jpacket_subtype(m->packet) == JPACKET__ERROR)
        mp->A = _mod_presence_whack(m->packet->from, mp->A);

    /* doh! this is a user, they should see invisibles as unavailables */
    if(jpacket_subtype(m->packet) == JPACKET__INVISIBLE)
        xmlnode_put_attrib(m->packet->x,"type","unavailable");

    return M_PASS;
}

/* process the roster to probe outgoing s10ns, and populate a list of the jids that should be notified */
void mod_presence_roster(mapi m, jid notify)
{
    xmlnode roster, cur, pnew;
    jid id;
    int to, from;

    /* do our roster setup stuff */
    roster = xdb_get(m->si->xc, m->user->id, NS_ROSTER);
    for(cur = xmlnode_get_firstchild(roster); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        id = jid_new(m->packet->p,xmlnode_get_attrib(cur,"jid"));
        if(id == NULL) continue;

        log_debug(ZONE,"roster item %s s10n=%s",jid_full(id),xmlnode_get_attrib(cur,"subscription"));

        /* vars */
        to = from = 0;
        if(j_strcmp(xmlnode_get_attrib(cur,"subscription"),"to") == 0)
            to = 1;
        if(j_strcmp(xmlnode_get_attrib(cur,"subscription"),"from") == 0)
            from = 1;
        if(j_strcmp(xmlnode_get_attrib(cur,"subscription"),"both") == 0)
            to = from = 1;

        /* curiosity phase */
        if(to)
        {
            log_debug(ZONE,"we're new here, probe them");
            pnew = jutil_presnew(JPACKET__PROBE,jid_full(id),NULL);
            xmlnode_put_attrib(pnew,"from",jid_full(jid_user(m->s->id)));
            js_session_from(m->s, jpacket_new(pnew));
        }

        /* notify phase, only if it's global presence */
        if(from && notify != NULL)
        {
            log_debug(ZONE,"we need to notify them");
            jid_append(notify, id);
        }

    }

    xmlnode_free(roster);

}

mreturn mod_presence_out(mapi m, void *arg)
{
    xmlnode pnew, delay;
    modpres mp = (modpres)arg;
    session top;
    int oldpri;

    if(m->packet->type != JPACKET_PRESENCE) return M_IGNORE;

    if(m->packet->to != NULL || jpacket_subtype(m->packet) == JPACKET__PROBE || jpacket_subtype(m->packet) == JPACKET__ERROR) return M_PASS;

    log_debug("mod_presence","new presence from %s of  %s",jid_full(m->s->id),xmlnode2str(m->packet->x));

    /* pre-existing conditions (no, we are not an insurance company) */
    top = js_session_primary(m->user);
    oldpri = m->s->priority;

    /* invisible mode is special, don't you wish you were special too? */
    if(jpacket_subtype(m->packet) == JPACKET__INVISIBLE)
    {
        log_debug(ZONE,"handling invisible mode request");

        /* if we get this and we're available, it means go unavail first then reprocess this packet, nifty trick :) */
        if(oldpri >= 0)
	{
            js_session_from(m->s, jpacket_new(jutil_presnew(JPACKET__UNAVAILABLE,NULL,NULL)));
            js_session_from(m->s, m->packet);
            return M_HANDLED;
	}

        /* now, pretend we come online :) */
        mp->invisible = 1;
        mod_presence_roster(m, NULL);
        m->s->priority = j_atoi(xmlnode_get_tag_data(m->packet->x,"priority"),0);

        xmlnode_free(m->packet->x);
        return M_HANDLED;
    }

    /* our new presence */
    xmlnode_free(m->s->presence);
    m->s->presence = xmlnode_dup(m->packet->x);
    m->s->priority = jutil_priority(m->packet->x);

    /* stamp the sessions presence */
    delay = xmlnode_insert_tag(m->s->presence,"x");
    xmlnode_put_attrib(delay,"xmlns",NS_DELAY);
    xmlnode_put_attrib(delay,"from",jid_full(m->s->id));
    xmlnode_put_attrib(delay,"stamp",jutil_timestamp());

    log_debug(ZONE,"presence oldp %d newp %d top %X",oldpri,m->s->priority,top);

    /* if we're going offline now, let everyone know */
    if(m->s->priority < 0)
    {
        if(!mp->invisible) /* bcc's don't get told if we were invisible */
            _mod_presence_broadcast(m->s,mp->bcc,m->packet->x,NULL);
        _mod_presence_broadcast(m->s,mp->A,m->packet->x,NULL);
        _mod_presence_broadcast(m->s,mp->I,m->packet->x,NULL);

        /* reset vars */
        mp->invisible = 0;
        if(mp->A != NULL)
            mp->A->next = NULL;
        mp->I = NULL;

        xmlnode_free(m->packet->x);
        return M_HANDLED;
    }

    /* available presence updates, intersection of A and T */
    if(oldpri >= 0 && !mp->invisible)
    {
        _mod_presence_broadcast(m->s,mp->A,m->packet->x,js_trustees(m->user));
        xmlnode_free(m->packet->x);
        return M_HANDLED;
    }

    /* at this point we're coming out of the closet */
    mp->invisible = 0;

    /* make sure we get notified for any presence about ourselves */
    pnew = jutil_presnew(JPACKET__PROBE,jid_full(jid_user(m->s->id)),NULL);
    xmlnode_put_attrib(pnew,"from",jid_full(jid_user(m->s->id)));
    js_session_from(m->s, jpacket_new(pnew));

    /* probe s10ns and populate A */
    mod_presence_roster(m,mp->A);

    /* we broadcast this baby! */
    _mod_presence_broadcast(m->s,mp->bcc,m->packet->x,NULL);
    _mod_presence_broadcast(m->s,mp->A,m->packet->x,NULL);
    xmlnode_free(m->packet->x);
    return M_HANDLED;
}

mreturn mod_presence_avails(mapi m, void *arg)
{
    modpres mp = (modpres)arg;

    if(m->packet->type != JPACKET_PRESENCE) return M_IGNORE;

    if(m->packet->to == NULL) return M_PASS;

    log_debug(ZONE,"track presence sent to jids");

    /* handle invisibles: put in I and remove from A */
    if(jpacket_subtype(m->packet) == JPACKET__INVISIBLE)
    {
        if(mp->I == NULL)
            mp->I = jid_new(m->s->p,jid_full(m->packet->to));
        else
            jid_append(mp->I, m->packet->to);
        mp->A = _mod_presence_whack(m->packet->to,mp->A);
        return M_PASS;
    }

    /* ensure not invisible from before */
    mp->I = _mod_presence_whack(m->packet->to,mp->I);

    /* avails to A */
    if(jpacket_subtype(m->packet) == JPACKET__AVAILABLE)
        jid_append(mp->A, m->packet->to);

    /* unavails from A */
    if(jpacket_subtype(m->packet) == JPACKET__UNAVAILABLE)
        mp->A = _mod_presence_whack(m->packet->to,mp->A);

    return M_PASS;
}

mreturn mod_presence_avails_end(mapi m, void *arg)
{
    modpres mp = (modpres)arg;

    log_debug("mod_presence","avail tracker guarantee checker");

    /* send  the current presence (which the server set to unavail) */
    xmlnode_put_attrib(m->s->presence, "from",jid_full(m->s->id));
    _mod_presence_broadcast(m->s, mp->bcc, m->s->presence, NULL);
    _mod_presence_broadcast(m->s, mp->A, m->s->presence, NULL);
    _mod_presence_broadcast(m->s, mp->I, m->s->presence, NULL);

    return M_PASS;
}

mreturn mod_presence_session(mapi m, void *arg)
{
    jid bcc = (jid)arg;
    modpres mp;

    /* track our session stuff */
    mp = pmalloco(m->s->p, sizeof(_modpres));
    mp->A = jid_user(m->s->id);
    mp->bcc = bcc; /* no no, it's ok, these live longer than us */

    js_mapi_session(es_IN, m->s, mod_presence_in, mp);
    js_mapi_session(es_OUT, m->s, mod_presence_avails, mp); /* must come first, it passes, _out handles */
    js_mapi_session(es_OUT, m->s, mod_presence_out, mp);
    js_mapi_session(es_END, m->s, mod_presence_avails_end, mp);

    return M_PASS;
}

mreturn mod_presence_deliver(mapi m, void *arg)
{
    session cur;

    if(m->packet->type != JPACKET_PRESENCE) return M_IGNORE;

    log_debug("mod_presence","deliver phase");

    /* only if we HAVE a user, and it was sent to ONLY the user@server, and there is at least one session available */
    if(m->user != NULL && m->packet->to->resource == NULL && js_session_primary(m->user) != NULL)
    {
        log_debug("mod_presence","broadcasting to %s",m->user->user);

        /* broadcast */
        for(cur = m->user->sessions; cur != NULL; cur = cur->next)
        {
            if(cur->priority < 0) continue;
            js_session_to(cur, jpacket_new(xmlnode_dup(m->packet->x)));
        }

        if(jpacket_subtype(m->packet) != JPACKET__PROBE)
        { /* probes get handled by the offline thread as well? */
            xmlnode_free(m->packet->x);
            return M_HANDLED;
        }
    }

    return M_PASS;
}

void mod_presence(jsmi si)
{
    xmlnode cfg = js_config(si, "presence");
    jid bcc = NULL;

    log_debug("mod_presence","init");

    for(cfg = xmlnode_get_firstchild(cfg); cfg != NULL; cfg = xmlnode_get_nextsibling(cfg))
    {
        if(xmlnode_get_type(cfg) != NTYPE_TAG || j_strcmp(xmlnode_get_name(cfg),"bcc") != 0) continue;
        if(bcc == NULL)
            bcc = jid_new(si->p,xmlnode_get_data(cfg));
        else
            jid_append(bcc,jid_new(si->p,xmlnode_get_data(cfg)));
    }

    js_mapi_register(si,e_DELIVER, mod_presence_deliver, NULL);
    js_mapi_register(si,e_SESSION, mod_presence_session, (void*)bcc);
}

