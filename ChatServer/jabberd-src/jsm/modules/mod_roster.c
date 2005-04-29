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

xmlnode mod_roster_get(udata u)
{
    xmlnode ret;

    log_debug("mod_roster","getting %s's roster",u->user);

    /* get the existing roster */
    ret = xdb_get(u->si->xc, u->id, NS_ROSTER);
    if(ret == NULL)
    { /* there isn't one, sucky, create a container node and let xdb manage it */
        log_debug("mod_roster","creating");
        ret = xmlnode_new_tag("query");
        xmlnode_put_attrib(ret,"xmlns",NS_ROSTER);
    }

    return ret;
}

xmlnode mod_roster_get_item(xmlnode roster, jid id, char *name, int *newflag)
{
    xmlnode ret;

    log_debug("mod_roster","getting item %s",jid_full(id));

    ret = jid_nodescan(id,roster);

    if(ret == NULL)
    { /* there isn't one, brew one up */
        log_debug("mod_roster","creating");
        ret = xmlnode_insert_tag(roster,"item");
        xmlnode_put_attrib(ret,"jid",jid_full(id));
        if(name != NULL)
            xmlnode_put_attrib(ret,"name",name);
        xmlnode_put_attrib(ret,"subscription","none");
        *newflag = 1;
    }

    return ret;
}

void mod_roster_push(udata user, xmlnode item)
{ /* push the item to all session */
    session cur;
    xmlnode packet, query;

    log_debug("mod_roster","pushing %s",xmlnode2str(item));

    if(xmlnode_get_attrib(item,"hidden") != NULL) return;

    /* create a jpacket roster item push */
    packet = xmlnode_new_tag("iq");
    xmlnode_put_attrib(packet, "type", "set");
    query = xmlnode_insert_tag(packet, "query");
    xmlnode_put_attrib(query,"xmlns",NS_ROSTER);
    xmlnode_insert_tag_node(query,item);
    xmlnode_hide_attrib(xmlnode_get_firstchild(query),"subscribe"); /* hide the server tirds */

    /* send a copy to all session that have a roster */
    for(cur = user->sessions; cur != NULL; cur = cur->next)
        if(cur->roster)
            js_session_to(cur, jpacket_new(xmlnode_dup(packet)));

    xmlnode_free(packet);
}

#define S10N_ADD_FROM 1
#define S10N_ADD_TO 2
#define S10N_REM_FROM 3
#define S10N_REM_TO 4

void mod_roster_set_s10n(int set, xmlnode item)
{
    switch(set)
    { /* LAZY ALERT, yeah, redundant code, gak! */
    case S10N_ADD_FROM:
        if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"to") == 0 || j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0)
            xmlnode_put_attrib(item,"subscription","both");
        else
            xmlnode_put_attrib(item,"subscription","from");
        break;
    case S10N_ADD_TO:
        if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"from") == 0 || j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0)
            xmlnode_put_attrib(item,"subscription","both");
        else
            xmlnode_put_attrib(item,"subscription","to");
        break;
    case S10N_REM_FROM:
        if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0 || j_strcmp(xmlnode_get_attrib(item,"subscription"),"to") == 0)
            xmlnode_put_attrib(item,"subscription","to");
        else
            xmlnode_put_attrib(item,"subscription","none");
        break;
    case S10N_REM_TO:
        if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0 || j_strcmp(xmlnode_get_attrib(item,"subscription"),"from") == 0)
            xmlnode_put_attrib(item,"subscription","from");
        else
            xmlnode_put_attrib(item,"subscription","none");
        break;
    }
}

/* force presence updates through all sessions, thanks to 'smarter' presence logic */
void mod_roster_pforce(udata u, jid to, int uflag)
{
    session s;
    xmlnode x;

    log_debug(ZONE,"brute forcing presence updates");

    /* loop through all the sessions */
    for(s = u->sessions; s != NULL; s = s->next)
    {
        if(uflag)
            x = jutil_presnew(JPACKET__UNAVAILABLE,NULL,NULL);
        else
            x = xmlnode_dup(s->presence);
        xmlnode_put_attrib(x,"to",jid_full(to));
        js_session_from(s,jpacket_new(x));
    }
}

mreturn mod_roster_out_s10n(mapi m)
{
    xmlnode roster, item;
    int newflag, to, from;
    jid curr;

    if(m->packet->to == NULL) return M_PASS;
    if(jid_cmpx(jid_user(m->s->id),m->packet->to,JID_USER|JID_SERVER) == 0) return M_PASS; /* vanity complex */

    log_debug("mod_roster","handling outgoing s10n");

    newflag = to = from = 0;
    roster = mod_roster_get(m->user);
    item = mod_roster_get_item(roster,m->packet->to,NULL,&newflag);

    /* vars */
    if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"to") == 0)
        to = 1;
    if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"from") == 0)
        from = 1;
    if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0)
        to = from = 1;

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__SUBSCRIBE:
        if(!to)
        {
            xmlnode_put_attrib(item,"ask","subscribe");
            mod_roster_push(m->user, item);
        }
        break;
    case JPACKET__SUBSCRIBED:
        mod_roster_set_s10n(S10N_ADD_FROM,item); /* update subscription */
        jid_append(js_trustees(m->user),m->packet->to); /* make them trusted now */
        xmlnode_hide_attrib(item,"subscribe"); /* cancel any pending requests */
        xmlnode_hide_attrib(item,"hidden"); /* don't hide it anymore */
        mod_roster_pforce(m->user, m->packet->to, 0); /* they are now subscribed to us, send them our presence */
        mod_roster_push(m->user, item);
        break;
    case JPACKET__UNSUBSCRIBE:
        if(to)
        {
            xmlnode_put_attrib(item,"ask","unsubscribe");
            mod_roster_push(m->user, item);
        }else if(newflag){
            xmlnode_hide(item);
        }
        break;
    case JPACKET__UNSUBSCRIBED:
        if(from)
        {
            mod_roster_set_s10n(S10N_REM_FROM,item); /* update subscription */
            /* remove them from the user trusted list */
            for(curr = js_trustees(m->user);curr != NULL && jid_cmp(curr->next,m->packet->to) != 0;curr = curr->next);
            if(curr != NULL && curr->next != NULL)
                curr->next = curr->next->next;
            mod_roster_pforce(m->user, m->packet->to, 1); /* make us offline */
            mod_roster_push(m->user, item);
        }else if(newflag){
            xmlnode_hide(item);
        }else{
            if(xmlnode_get_attrib(item,"hidden") != NULL)
                xmlnode_hide(item); /* remove it for good */
            else
                xmlnode_hide_attrib(item,"subscribe"); /* just cancel any pending requests */
        }
        break;
    }

    /* save the roster */
    /* XXX what do we do if the set fails?  hrmf... */
    xdb_set(m->si->xc, m->user->id, NS_ROSTER, roster);

    /* make sure it's sent from the *user*, not the resource */
    xmlnode_put_attrib(m->packet->x,"from",jid_full(jid_user(m->s->id)));
    jpacket_reset(m->packet);

    xmlnode_free(roster);

    return M_PASS;
}

mreturn mod_roster_out_iq(mapi m)
{
    xmlnode roster, cur, pres, item;
    int newflag;
    jid id;

    if(!NSCHECK(m->packet->iq,NS_ROSTER)) return M_PASS;

    roster = mod_roster_get(m->user);

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__GET:
        log_debug("mod_roster","handling get request");
        xmlnode_put_attrib(m->packet->x,"type","result");
        m->s->roster = 1;

        /* insert the roster into the result */
        xmlnode_hide(m->packet->iq);
        xmlnode_insert_tag_node(m->packet->x, roster);
        jpacket_reset(m->packet);

        /* filter out pending subscribes */
        for(cur = xmlnode_get_firstchild(m->packet->iq); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        {
            if(xmlnode_get_attrib(cur,"subscribe") != NULL)
                xmlnode_hide_attrib(cur,"subscribe");
            if(xmlnode_get_attrib(cur,"hidden") != NULL)
                xmlnode_hide(cur);
        }

        /* send to the user */
        js_session_to(m->s,m->packet);

        /* redeliver those subscribes */
        for(cur = xmlnode_get_firstchild(roster); cur != NULL; cur = xmlnode_get_nextsibling(cur))
            if(xmlnode_get_attrib(cur,"subscribe") != NULL)
            {
                pres = xmlnode_new_tag("presence");
                xmlnode_put_attrib(pres,"type","subscribe");
                xmlnode_put_attrib(pres,"from",xmlnode_get_attrib(cur,"jid"));
                if(strlen(xmlnode_get_attrib(cur,"subscribe")) > 0)
                    xmlnode_insert_cdata(xmlnode_insert_tag(pres,"status"),xmlnode_get_attrib(cur,"subscribe"),-1);
                js_session_to(m->s,jpacket_new(pres));
            }

        break;
    case JPACKET__SET:
        log_debug("mod_roster","handling set request");

        /* loop through the incoming items updating or creating */
        for(cur = xmlnode_get_firstchild(m->packet->iq); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        {
            if(xmlnode_get_type(cur) != NTYPE_TAG || xmlnode_get_attrib(cur,"jid") == NULL)
                continue;

            id = jid_new(m->packet->p,xmlnode_get_attrib(cur,"jid"));
            if(id == NULL || jid_cmpx(jid_user(m->s->id),id,JID_USER|JID_SERVER) == 0) continue;

            /* zoom to find the existing item in the current roster, and hide it */
            item = mod_roster_get_item(roster, id, NULL, &newflag);
            xmlnode_hide(item);

            /* drop you sukkah */
            if(j_strcmp(xmlnode_get_attrib(cur,"subscription"),"remove") == 0)
            {
                /* cancel our subscription to them */
                if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0 || j_strcmp(xmlnode_get_attrib(item,"subscription"),"to") == 0 || j_strcmp(xmlnode_get_attrib(item,"ask"),"subscribe") == 0)
                    js_session_from(m->s,jpacket_new(jutil_presnew(JPACKET__UNSUBSCRIBE,xmlnode_get_attrib(cur,"jid"),NULL)));

                /* tell them their subscription to us is toast */
                if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0 || j_strcmp(xmlnode_get_attrib(item,"subscription"),"from") == 0)
                    js_session_from(m->s,jpacket_new(jutil_presnew(JPACKET__UNSUBSCRIBED,xmlnode_get_attrib(cur,"jid"),NULL)));

                /* push this remove out */
                mod_roster_push(m->user,cur);
                continue;
            }

            /* copy the old stuff into the new one and insert it into the roster */
            xmlnode_put_attrib(cur,"subscription",xmlnode_get_attrib(item,"subscription"));
            xmlnode_put_attrib(cur,"ask",xmlnode_get_attrib(item,"ask")); /* prolly not here, but just in case */
            xmlnode_insert_tag_node(roster,cur);

            /* push the new item */
            mod_roster_push(m->user,cur);
        }

        /* send to the user */
        jutil_iqresult(m->packet->x);
        jpacket_reset(m->packet);
        js_session_to(m->s,m->packet);

        /* save the changes */
        log_debug(ZONE,"SROSTER: %s",xmlnode2str(roster));
        /* XXX what do we do if the set fails?  hrmf... */
        xdb_set(m->si->xc, m->user->id, NS_ROSTER, roster);

        break;
    default:
        /* JPACKET__RESULT: result from a roster push to the client */
        xmlnode_free(m->packet->x);
        break;
    }

    xmlnode_free(roster);
    return M_HANDLED;
}

mreturn mod_roster_out(mapi m, void *arg)
{
    if(m->packet->type == JPACKET_IQ) return mod_roster_out_iq(m);
    if(m->packet->type == JPACKET_S10N) return mod_roster_out_s10n(m);

    return M_IGNORE;
}

mreturn mod_roster_session(mapi m, void *arg)
{
    js_mapi_session(es_OUT,m->s,mod_roster_out,NULL);
    return M_PASS;
}

mreturn mod_roster_s10n(mapi m, void *arg)
{
    xmlnode roster, item, reply, reply2;
    char *status;
    session top;
    int newflag, drop, to, from, push;

    push = newflag = drop = to = from = 0;

    /* check for incoming s10n (subscription) requests */
    if(m->packet->type != JPACKET_S10N) return M_IGNORE;

    if(m->user == NULL) return M_PASS;
    if(jid_cmpx(m->packet->from,m->packet->to,JID_USER|JID_SERVER) == 0) return M_PASS; /* vanity complex */

    /* now we can get to work and handle this user's incoming subscription crap */
    roster = mod_roster_get(m->user);
    item = mod_roster_get_item(roster,m->packet->from,xmlnode_get_attrib(m->packet->x,"name"),&newflag);
    reply2 = reply = NULL;
    jid_set(m->packet->to,NULL,JID_RESOURCE); /* make sure we're only dealing w/ the user id */

    log_debug("mod_roster","s10n %s request from %s with existing item %s",xmlnode_get_attrib(m->packet->x,"type"),jid_full(m->packet->from),xmlnode2str(item));

    /* vars */
    if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"to") == 0)
        to = 1;
    if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"from") == 0)
        from = 1;
    if(j_strcmp(xmlnode_get_attrib(item,"subscription"),"both") == 0)
        to = from = 1;

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__SUBSCRIBE:
        if(from)
        {
            /* already subscribed, respond automatically */
            reply = jutil_presnew(JPACKET__SUBSCRIBED,jid_full(m->packet->from),"Already Subscribed");
            jid_set(m->packet->to,NULL,JID_RESOURCE);
            xmlnode_put_attrib(reply,"from",jid_full(m->packet->to));
            drop = 1;

            /* the other person obviously is re-adding them to their roster, and should be told of the current presence */
            reply2 = jutil_presnew(JPACKET__PROBE,jid_full(m->packet->to),NULL);
            xmlnode_put_attrib(reply2,"from",jid_full(m->packet->from));

        }else{
            /* tuck request in the roster */
            status = xmlnode_get_tag_data(m->packet->x,"status");
            if(status == NULL)
                xmlnode_put_attrib(item,"subscribe","");
            else
                xmlnode_put_attrib(item,"subscribe",status);
            if(newflag) /* SPECIAL CASE: special flag so that we can hide these incoming subscribe requests */
                xmlnode_put_attrib(item,"hidden","");
        }
        break;
    case JPACKET__SUBSCRIBED:
        if(to)
        { /* already subscribed, drop */
            drop = 1;
        }else{
            /* cancel any ask, s10n=to */
            xmlnode_hide_attrib(item,"ask");
            mod_roster_set_s10n(S10N_ADD_TO,item);
            push = 1;
        }
        break;
    case JPACKET__UNSUBSCRIBE:
        if(from)
        {
            /* remove s10n=from */
            xmlnode_hide_attrib(item,"subscribe");
            mod_roster_set_s10n(S10N_REM_FROM,item);
            if(xmlnode_get_attrib(item,"hidden") != NULL)
                xmlnode_hide(item);
            else
                push = 1;
        }else{
            if(newflag)
                xmlnode_hide(item);
            drop = 1;
        }
        /* respond automatically */
        reply = jutil_presnew(JPACKET__UNSUBSCRIBED,jid_full(m->packet->from),"Autoreply");
        jid_set(m->packet->to,NULL,JID_RESOURCE);
        xmlnode_put_attrib(reply,"from",jid_full(m->packet->to));
        break;
    case JPACKET__UNSUBSCRIBED:
        if(to || xmlnode_get_attrib(item,"ask") != NULL)
        {
            /* cancel any ask, remove s10n=to */
            xmlnode_hide_attrib(item,"ask");
            mod_roster_set_s10n(S10N_REM_TO,item);
            push = 1;
        }else{
            if(newflag)
                xmlnode_hide(item);
            drop = 1;
        }
    }

    /* XXX what do we do if the set fails?  hrmf... */
    xdb_set(m->si->xc, m->user->id, NS_ROSTER, roster);

    /* these are delayed until after we check the roster back in, avoid rancid race conditions */
    if(reply != NULL)
        js_deliver(m->si,jpacket_new(reply));
    if(reply2 != NULL)
        js_deliver(m->si,jpacket_new(reply2));

    /* find primary session */
    top = js_session_primary(m->user);

    /* if we can, deliver this to that session */
    if(!drop && top != NULL && top->roster)
        js_session_to(top,m->packet);
    else
        xmlnode_free(m->packet->x);

    if(push)
        mod_roster_push(m->user,item);

    xmlnode_free(roster);
    return M_HANDLED;
}

void mod_roster(jsmi si)
{
    /* we just register for new sessions */
    js_mapi_register(si,e_SESSION,mod_roster_session,NULL);
    js_mapi_register(si,e_DELIVER,mod_roster_s10n,NULL);
}


