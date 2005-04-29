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
* mod_filter.c: it's alive! it now handles all functions that
                used to be handled by mod_offline.c 
                mod_filter supports the following conditions:
                  unavailable - matches when you aren't online
                  from        - matches if the sender is the same
                                as the cdata in the <from/> tag
                  resource    - matches YOUR resource
                  subject     - matches the subject of the message
                  body        - matches the body of the message
                  show        - matches your <show/> in presence
                  type        - matches the type='' attrib
                and the following Actions:
                  offline     - stores the message offline
                  forward     - forwards the message to another jid
                  reply       - sends back an auto-reply message to
                                the sender
                  continue    - continues processing of other rules
                  settype     - change the incoming message type

                you may specify any number of conditions/actions,
                if there are more than one condition, ALL conditions
                must match for the rule to match, and ALL the listed
                actions will be taken.

                rules are checked in order, stopping when a match is
                found, unless a <continue/> flag is in place

                example rule:
                  <rule>
                    <unavailable/>
                    <offline/>
                    <reply>I'm not available right now</reply>
                    <forward>tsbandit@jabber.org</forward>
                  </rule>
*/

/* mod_filter must go first in module ordering */

/* basic principle of mod_filter
 * each user has their own filter
 * each filter contains zero or more rules
 * each rule contains one or more conditions and one or more actions
 * some conditions/actions may contain text to limit it further
 * first matching condition is applied and processing quits (unless an action is continue)
 * processed in order from client
 * NO ACTION IMPLIES DROP
 * FUTURE CONDITIONS: type, time, size
 * FUTURE ACTIONS: edit,error,settype
 */

xmlnode mod_filter__default = NULL;
jsmi    mod_filter__jsmi    = NULL;
#define MOD_FILTER_MAX_SIZE 100

typedef struct action_struct 
{
    pool p;
    int is_match, has_action;
    int offline, reply, settype, cont, error;
    jid forward;
    mapi m;
} _action, *action;


/* get the user's filter rules */
xmlnode mod_filter_get(udata u)
{
    xmlnode ret;

    /* get the existing rules */
    ret = xdb_get(u->si->xc, u->id, NS_FILTER);
    if(ret == NULL)
    {
        ret = xmlnode_new_tag("query");
        xmlnode_put_attrib(ret, "xmlns", NS_FILTER);
    }

    return ret;
}

void mod_filter_action_offline(mapi m, xmlnode rule)
{
    xmlnode cur;

   /* look for event messages */
    for(cur = xmlnode_get_firstchild(m->packet->x); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        if(NSCHECK(cur, NS_EVENT))
        {
            if(xmlnode_get_tag(cur, "id") != NULL)
                return; /* bah, we don't want to store events offline (XXX: do we?) */
            if(xmlnode_get_tag(cur, "offline") != NULL)
                break; /* cur remaining set is the flag */
        }

    log_debug("mod_filter","storing message for %s offline.",m->user->user);

    jutil_delay(m->packet->x,"Offline Storage");
    if(xdb_act(m->si->xc, m->user->id, NS_OFFLINE, "insert", NULL, m->packet->x))
        return;

    if(cur != NULL)
    { /* if there was an offline event to be sent, send it for gosh sakes! */
        xmlnode cur2;
        jutil_tofrom(m->packet->x);

        /* erease everything else in the message */
        for(cur2 = xmlnode_get_firstchild(m->packet->x); cur2 != NULL; cur2 = xmlnode_get_nextsibling(cur2))
            if(cur2 != cur)
                xmlnode_hide(cur2);

        /* erase any other events */
        for(cur2 = xmlnode_get_firstchild(cur); cur2 != NULL; cur2 = xmlnode_get_nextsibling(cur2))
            xmlnode_hide(cur2);

        /* fill it in and send it on */
        xmlnode_insert_tag(cur,"offline");
        xmlnode_insert_cdata(xmlnode_insert_tag(cur,"id"),xmlnode_get_attrib(m->packet->x,"id"), -1);
        js_deliver(m->si, jpacket_reset(m->packet));
    }
}

void mod_filter_action_reply(mapi m,xmlnode rule)
{
    char *reply=xmlnode_get_tag_data(rule,"reply");
    xmlnode x = xmlnode_get_tag(m->packet->x, "x?xmlns=jabber:x:envelope");
    int has_envelope = 0;

    /* check for infinite loops */
    if(x != NULL)
    {
        xmlnode cur = xmlnode_get_tag(x, "forwardedby");
        has_envelope = 1;
        for(; cur != NULL; cur = xmlnode_get_nextsibling(cur))
        {
            if(xmlnode_get_type(cur) != NTYPE_TAG)
                continue;

            if(j_strcmp(xmlnode_get_name(cur), "forwardedby") == 0)
            {
                char *fb = xmlnode_get_attrib(cur, "jid");
                jid j    = jid_new(m->packet->p, fb);

                if(jid_cmpx(j, m->packet->to, JID_USER | JID_SERVER) == 0)
                {
                    x = xmlnode_dup(m->packet->x);
                    xmlnode_put_attrib(x, "to", jid_full(j));
                    xmlnode_put_attrib(x, "from", jid_full(m->packet->to));
                    deliver_fail(dpacket_new(x), "Replying would result in infinite loop");
                    return;
                }
            }
        }
    }

    if(!has_envelope)
        xmlnode_put_attrib(x = xmlnode_insert_tag(m->packet->x, "x"), "xmlns", "jabber:x:envelope");
    xmlnode_put_attrib(xmlnode_insert_tag(x, "forwardedby"), "jid", jid_full(m->packet->to));
    xmlnode_put_attrib(xmlnode_insert_tag(x, "from"), "jid", jid_full(m->packet->to));
    xmlnode_put_attrib(xmlnode_insert_tag(x, "to"), "jid", jid_full(m->packet->from));

    if(jid_cmpx(m->packet->to, m->packet->from, JID_USER | JID_SERVER) == 0)
    { /* special case, we sent a msg to ourselves */
        /* try to find a session to deliver to... */
        session s = js_session_get(m->user, m->packet->to->resource);
        s = s ? s : js_session_primary(m->user);
        s = s ? s : m->s;

        if(s == NULL)
        { /* can't find a deliverable session, store offline */
            mod_filter_action_offline(m, rule);
            return;
        }
        
        /* just deliver to the session */
        x = xmlnode_dup(m->packet->x);
        jutil_tofrom(x);
        if(xmlnode_get_tag(x, "body") != NULL) 
            xmlnode_hide(xmlnode_get_tag(x, "body"));
        if(reply != NULL)
            xmlnode_insert_cdata(xmlnode_insert_tag(x, "body"), reply, -1);
        js_session_to(s, jpacket_new(x));
        return;
    }


    x = xmlnode_dup(m->packet->x);
    jutil_tofrom(x);
    if(xmlnode_get_tag(x, "body") != NULL) 
        xmlnode_hide(xmlnode_get_tag(x, "body"));
    if(reply != NULL)
        xmlnode_insert_cdata(xmlnode_insert_tag(x, "body"), reply, -1);
    deliver(dpacket_new(x),m->si->i);
}


void mod_filter_action_error(mapi m,xmlnode rule)
{
    xmlnode err = xmlnode_get_tag(rule, "error");
    log_debug(ZONE,"sending an error reply");
    
    if(err != NULL)
    {
        xmlnode_insert_tag_node(m->packet->x, err);
        xmlnode_put_attrib(m->packet->x, "type", "error");
        jpacket_reset(m->packet);
    }

    mod_filter_action_reply(m, rule);
}

void mod_filter_action_forward(mapi m,xmlnode rule,jid j)
{
    int has_envelope=0;
    jid cur;
    xmlnode x=xmlnode_get_tag(m->packet->x,"x?xmlns=jabber:x:envelope");

    /* check for infinite loops... */
    if(x!=NULL)
    {
        xmlnode cur=xmlnode_get_tag(x,"forwardedby");
        has_envelope=1;
        for(;cur!=NULL;cur=xmlnode_get_nextsibling(cur)) 
        {

            if(xmlnode_get_type(cur) != NTYPE_TAG)
                continue;

            if(j_strcmp(xmlnode_get_name(cur),"forwardedby")==0)
            {
                char *fb=xmlnode_get_attrib(cur,"jid");
                jid j=jid_new(m->packet->p,fb);
                if( ( j != NULL ) &&
                    ( jid_cmpx(j,m->packet->to,JID_USER|JID_SERVER)==0 ) )
                {
                    x=xmlnode_dup(m->packet->x);
                    xmlnode_put_attrib(x,"to",jid_full(j));
                    xmlnode_put_attrib(x,"from",jid_full(m->packet->to));
                    deliver_fail(dpacket_new(x),"Forwarding would result in infinite loop");
                    return;
                }
            }
        }
    }
    if(!has_envelope)
      xmlnode_put_attrib(x=xmlnode_insert_tag(m->packet->x,"x"),"xmlns","jabber:x:envelope");
    xmlnode_put_attrib(xmlnode_insert_tag(x,"forwardedby"),"jid",jid_full(m->packet->to));
    xmlnode_put_attrib(xmlnode_insert_tag(x,"from"),"jid",jid_full(m->packet->from));

    for(cur=j;cur!=NULL;cur=cur->next)
        xmlnode_put_attrib(xmlnode_insert_tag(x,"cc"),"jid",jid_full(cur));
    for(;j!=NULL;j=j->next)
    {
        x=xmlnode_dup(m->packet->x);
        xmlnode_put_attrib(x,"to",jid_full(j));
        xmlnode_put_attrib(x,"from",jid_full(m->packet->to));
        deliver(dpacket_new(x),m->si->i);
    }
}

void mod_filter_action_settype(mapi m,xmlnode rule)
{
    char *newtype=xmlnode_get_tag_data(rule,"settype");
    if(newtype==NULL) 
        xmlnode_hide_attrib(m->packet->x,"type");
    else
        xmlnode_put_attrib(m->packet->x,"type",newtype);
    jpacket_reset(m->packet);
}

mreturn mod_filter_handler(mapi m, void *arg)
{
    xmlnode rules,cur;
    xmlnode container;
    action cur_action;
    jpacket jp;
    pool p;

    jp = m->packet;
    if(m->packet->type == JPACKET_PRESENCE)
        return M_IGNORE;

    if(m->user == NULL) 
        return M_PASS;
    
    p = pool_new();
    cur_action = pmalloco(p,sizeof(_action));
    /* look through the user's rule set for a matching cond */

    container = mod_filter_get(m->user);
    xmlnode_insert_node(container, xmlnode_get_firstchild(mod_filter__default));

    rules = xmlnode_get_firstchild(container);


    log_debug(ZONE,"Looking at rules: %s", xmlnode2str(container));

    for(;rules!=NULL;rules=xmlnode_get_nextsibling(rules))
    {
        log_debug(ZONE, "rule: %s", xmlnode2str(rules));

        if(xmlnode_get_type(rules) != NTYPE_TAG)
            continue;

        cur=xmlnode_get_firstchild(rules);
        for(;cur!=NULL;)
        {
            /* iq packets may match the <ns/> condition */
            if(j_strcmp(xmlnode_get_name(cur), "ns") == 0)
            {
                log_debug(ZONE, "checking ns");
                if(m->packet->type != JPACKET_IQ)
                { /* ignore this rule, since the packet is not an IQ */
                    cur = NULL;
                    continue;
                }

                if(j_strcmp(xmlnode_get_attrib(m->packet->iq, "xmlns"), xmlnode_get_data(cur)) == 0)
                {
                    cur_action->is_match = 1;
                    log_debug(ZONE, "MATCH");
                }
                cur = xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur), "roster") == 0)
            {
                xmlnode roster = xdb_get(m->user->si->xc, m->user->id, NS_ROSTER);
                jid j = jid_new(m->packet->p, jid_full(m->packet->from));
                if ( j != NULL )
                {
                    jid_set(j, NULL, JID_RESOURCE);

                    log_debug(ZONE, "checking roster");

                    if(jid_nodescan(j, roster) != NULL)
                    {
                        cur_action->is_match = 1;
                        log_debug(ZONE, "MATCH");
                    }
                }
                else
                {
                    log_debug(ZONE, "Bogus return address on message");
                }
                xmlnode_free(roster);
                cur = xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur), "group") == 0)
            {
                xmlnode roster = xdb_get(m->user->si->xc, m->user->id, NS_ROSTER);
                xmlnode item;
                char *group = spools(m->packet->p, "item/group=", xmlnode_get_data(cur), m->packet->p);
                jid j = jid_new(m->packet->p, jid_full(m->packet->from));
                if ( j != NULL )
                {
                    jid_set(j, NULL, JID_RESOURCE);

                    log_debug(ZONE, "checking for group %s in %s", group, xmlnode2str(roster));

                    while((item = xmlnode_get_tag(roster, group)) != NULL)
                    {
                        log_debug(ZONE, "found match: %s", xmlnode2str(item));
                        if(jid_cmpx(j, jid_new(xmlnode_pool(item), xmlnode_get_attrib(item->parent, "jid")), JID_USER | JID_SERVER) == 0)
                        {
                            cur_action->is_match = 1;
                            log_debug(ZONE, "MATCH");
                            break;
                        }
                        xmlnode_hide(item);
                    }
                }
                else
                {
                    log_debug(ZONE, "Bogus Return address on message.");
                }
                xmlnode_free(roster);
                cur = xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur),"unavailable")==0)
            {
                log_debug(ZONE,"checking unavailalbe");
                if(js_session_primary(m->user)==NULL)
                    cur_action->is_match=1;
                else
                    break;
                log_debug(ZONE,"MATCH!");
                cur=xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur),"from")==0)
            {
                xmlnode f=cur;
                log_debug(ZONE,"checking from");
                cur_action->is_match=0;
                for(;f!=NULL;f=xmlnode_get_tag(rules,"from"))
                {
                    char *from=xmlnode_get_data(f);
                    log_debug(ZONE,"checking from: %s",from);
                    if(cur_action->is_match||from==NULL||jid_cmpx(jid_new(jp->from->p,from),jp->from,JID_USER|JID_SERVER)!=0)
                    {
                        log_debug(ZONE,"not a match, killing node");
                        if(cur==f)
                            cur=xmlnode_get_nextsibling(cur);
                        xmlnode_hide(f);
                        continue;
                    }
                    if(cur==f)cur=xmlnode_get_nextsibling(cur);
                    xmlnode_hide(f);
                    cur_action->is_match=1;
                }
                if(!cur_action->is_match) break;
                log_debug(ZONE,"MATCH!");
            }
            else if(j_strcmp(xmlnode_get_name(cur),"resource")==0)
            {
                xmlnode r=cur;
                log_debug(ZONE,"checking resource");
                cur_action->is_match=0;
                for(;r!=NULL;r=xmlnode_get_tag(rules,"resource"))
                {
                    char *res=xmlnode_get_data(r);
                    log_debug(ZONE,"checking res: %s",res);
                    if(cur_action->is_match||res==NULL||jp->to->resource==NULL||strcasecmp(res,jp->to->resource)!=0)
                    {
                        log_debug(ZONE,"not a match");
                        if(cur==r)
                            cur=xmlnode_get_nextsibling(cur);
                        xmlnode_hide(r);
                        continue;
                    }
                    if(cur==r)cur=xmlnode_get_nextsibling(cur);
                    xmlnode_hide(r);
                    cur_action->is_match=1;
                }
                if(!cur_action->is_match) break;
                log_debug(ZONE,"MATCH!");
            }
            else if(j_strcmp(xmlnode_get_name(cur),"subject")==0)
            {
                xmlnode s=cur;
                if(m->packet->type == JPACKET_IQ) 
                    break;
                log_debug(ZONE,"checking subject");
                cur_action->is_match=0;
                for(;s!=NULL;s=xmlnode_get_tag(rules,"subject"))
                {
                    char *subject=xmlnode_get_data(s);
                    log_debug(ZONE,"checking subject: %s",subject);
                    if(cur_action->is_match||subject==NULL||xmlnode_get_tag_data(jp->x,"subject")==NULL||strcasecmp(subject,xmlnode_get_tag_data(jp->x,"subject"))!=0)
                    {
                        log_debug(ZONE,"not a match");
                        if(cur==s)cur=xmlnode_get_nextsibling(cur);
                        xmlnode_hide(s);
                        continue;
                    }
                    if(cur==s)cur=xmlnode_get_nextsibling(cur);
                    xmlnode_hide(s);
                    cur_action->is_match=1;
                }
                if(!cur_action->is_match) break;
                log_debug(ZONE,"MATCH!");
            }
            else if(j_strcmp(xmlnode_get_name(cur),"body")==0)
            {
                xmlnode b=cur;
                if(m->packet->type == JPACKET_IQ)
                    break;
                log_debug(ZONE,"checking body");
                cur_action->is_match=0;
                for(;b!=NULL;b=xmlnode_get_tag(rules,"body"))
                {
                    char *body=xmlnode_get_data(b);
                    log_debug(ZONE,"checking body: %s",body);
                    if(cur_action->is_match||body==NULL||xmlnode_get_tag_data(jp->x,"body")==NULL||strcasecmp(body,xmlnode_get_tag_data(jp->x,"body"))!=0)
                    {
                        log_debug(ZONE,"not a match");
                        if(cur==b)cur=xmlnode_get_nextsibling(cur);
                        xmlnode_hide(b);
                        continue;
                    }
                    if(cur==b)cur=xmlnode_get_nextsibling(cur);
                    xmlnode_hide(b);
                    cur_action->is_match=1;
                }
                if(!cur_action->is_match) break;
                log_debug(ZONE,"MATCH!");
            }
            else if(j_strcmp(xmlnode_get_name(cur),"show")==0)
            {
                xmlnode sh=cur;
                session s;
                if(m->packet->to->resource!=NULL)
                    s=js_session_get(m->user,m->packet->to->resource);
                else
                    s=js_session_primary(m->user);
                cur_action->is_match=0;
                for(;sh!=NULL;sh=xmlnode_get_tag(rules,"show"))
                {
                    char *show=xmlnode_get_data(sh);
                    log_debug(ZONE,"checking show: %s",show);
                    if(cur_action->is_match||show==NULL||s==NULL||j_strcmp(show,xmlnode_get_tag_data(s->presence,"show"))!=0)
                    {
                        log_debug(ZONE,"not a match");
                        if(cur==sh)cur=xmlnode_get_nextsibling(cur);
                        xmlnode_hide(sh);
                        continue;
                    }
                    if(cur==sh)cur=xmlnode_get_nextsibling(cur);
                    xmlnode_hide(sh);
                    cur_action->is_match=1;
                }
                if(!cur_action->is_match) break;
                log_debug(ZONE,"MATCH!");
            }
            else if(j_strcmp(xmlnode_get_name(cur),"type")==0)
            {
                xmlnode t=cur;
                char norm[7]="normal\0";
                char *xtype=xmlnode_get_attrib(jp->x,"type");

                if(m->packet->type == JPACKET_IQ)
                    break;
                log_debug(ZONE,"checking type");
                if(xtype==NULL) xtype=norm;
                cur_action->is_match=0;
                for(;t!=NULL;t=xmlnode_get_tag(rules,"type"))
                {
                    char *type=xmlnode_get_data(t);
                    log_debug(ZONE,"checking type: %s",type);
                    if(cur_action->is_match||(type==NULL&&jpacket_subtype(m->packet)!=JPACKET__NONE)||(j_strcmp(type,xtype)!=0))
                    {
                        log_debug(ZONE,"not a match");
                        if(cur==t)cur=xmlnode_get_nextsibling(cur);
                        xmlnode_hide(t);
                        continue;
                    }
                    if(cur==t)cur=xmlnode_get_nextsibling(cur);
                    xmlnode_hide(t);
                    cur_action->is_match=1;
                }
                if(!cur_action->is_match) break;
                log_debug(ZONE,"MATCH");
            }
            else if(j_strcmp(xmlnode_get_name(cur),"settype")==0)
            {
                if(m->packet->type == JPACKET_IQ)
                    break;
                cur_action->has_action=1;
                cur_action->settype=1;
                log_debug(ZONE,"settype: %s",xmlnode_get_data(cur));
                cur=xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur),"reply")==0)
            {
                if(m->packet->type == JPACKET_IQ)
                    break;
                cur_action->has_action=1;
                cur_action->reply=1;
                log_debug(ZONE,"reply: %s",xmlnode_get_data(cur));
                cur=xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur),"forward")==0)
            {
                jid new=jid_new(p,xmlnode_get_data(cur));
                if ( ! new )
                {
                    log_debug(ZONE, "Ignoring illegal forwarding address: %s",
                                      xmlnode_get_data(cur));
                    //NPS:
                    //This if statement deals w/ the immediate case of
                    //the jid specified in the rule is bogus (can't
                    //be converted to a jid struct.  However, it
                    //does so by just ignoring that there ever was a
                    //<forward> tag in the rule, which is probably
                    //confusing to an end user.  I'd like to see a
                    //deliver_fail() call used, but util I have a chance
                    //to figure out an appropriate way to use it,
                    //this will have to do.
                }
                else
                {
                    if(m->packet->type == JPACKET_IQ)
                        break;
                    cur_action->has_action=1;
                    new->next=cur_action->forward;
                    cur_action->forward=new;
                    log_debug(ZONE,"forward: %s",xmlnode_get_data(cur));
                }
                cur=xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur),"offline")==0)
            {
                if(m->packet->type == JPACKET_IQ)
                    break;
                cur_action->has_action=1;
                cur_action->offline=1;
                log_debug(ZONE,"offline storage");
                cur=xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur),"continue")==0)
            {
                cur_action->has_action=1;
                cur_action->cont=1;
                log_debug(ZONE,"continue processing");
                cur=xmlnode_get_nextsibling(cur);
            }
            else if(j_strcmp(xmlnode_get_name(cur), "error") == 0)
            {
                cur_action->has_action = 1;
                cur_action->error = 1;
                log_debug(ZONE, "reply with error");
                cur = xmlnode_get_nextsibling(cur);
            }
            else
            {
                /* we don't know this tag.. how did we get here then?? */
                cur=xmlnode_get_nextsibling(cur);
            }

        }
        if(!cur_action->is_match)
        {
            memset(cur_action,0,sizeof(_action));
            continue;
        }

        if(cur_action->reply)
            mod_filter_action_reply(m,rules);
        if(cur_action->error)
            mod_filter_action_error(m, rules);
        if(cur_action->settype)
            mod_filter_action_settype(m,rules);
        if(cur_action->forward!=NULL)
            mod_filter_action_forward(m,rules,cur_action->forward);
        if(cur_action->offline)
            mod_filter_action_offline(m,rules);
        if(cur_action->cont)
        { /* continue processing rules */
            memset(cur_action,0,sizeof(_action));
            continue;
        }
        else break;
    }

    xmlnode_free(container);
    if(cur_action->has_action)
    {
        xmlnode_free(jp->x);
        pool_free(p);
        return M_HANDLED;
    }
    else
    {
        pool_free(p);
        return M_PASS;
    }
    /* it will never get to this point, but just becuase... */
    return M_PASS;
}

mreturn mod_filter_iq(mapi m)
{
    xmlnode opts, cur;
    int max_rule_size;
    pool p;

    if(!NSCHECK(m->packet->iq, NS_FILTER) || m->packet->to != NULL)
        return M_PASS;

    log_debug(ZONE, "FILTER RULE SET: iq %s", xmlnode2str(m->packet->x));
    max_rule_size = j_atoi(xmlnode_get_tag_data(js_config(mod_filter__jsmi, "filter"), "max_size"), MOD_FILTER_MAX_SIZE);

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__SET:
        /* check packet max size, and validity */

        log_debug(ZONE, "FILTER RULE SET: rule max size %d: %s", max_rule_size, xmlnode2str(m->packet->x));

        p = pool_new();
        for(cur = xmlnode_get_firstchild(m->packet->iq); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        {
            xmlnode tag;
            if(xmlnode_get_type(cur) != NTYPE_TAG)
                continue;

            max_rule_size--;
            log_debug(ZONE, "only %d left..", max_rule_size);

            if(max_rule_size <= 0 || j_strcmp(xmlnode_get_name(cur), "rule") != 0)
            { /* invalid tag used */
                jutil_iqresult(m->packet->x);
                xmlnode_put_attrib(m->packet->x, "type", "error");
                xmlnode_put_attrib(xmlnode_insert_tag(m->packet->x, "error"), "code", "406");
                xmlnode_insert_cdata(xmlnode_get_tag(m->packet->x, "error"), "Invalid rule, check rule size and tags", -1);
                xmlnode_hide(m->packet->iq);
                jpacket_reset(m->packet);
                js_session_to(m->s, m->packet);
                pool_free(p);
                return M_HANDLED;
            }

            for(tag = xmlnode_get_firstchild(cur); tag != NULL; tag = xmlnode_get_nextsibling(tag))
            {
                char *c, *a;
                xmlnode config;
                if(xmlnode_get_type(tag) != NTYPE_TAG)
                    continue;
                config = js_config(mod_filter__jsmi, "filter");
                config = xmlnode_get_tag(config, "allow");

                /* if ns is used, offline, reply and settype cannot be used */
                if(j_strcmp(xmlnode_get_name(tag), "ns") == 0 && (xmlnode_get_tag(tag->parent, "offline") != NULL || xmlnode_get_tag(tag->parent, "reply") == 0 || xmlnode_get_tag(tag->parent, "settype") == 0))
                {
                    jutil_iqresult(m->packet->x);
                    xmlnode_put_attrib(m->packet->x, "type", "error");
                    xmlnode_put_attrib(xmlnode_insert_tag(m->packet->x, "error"), "code", "406");
                    xmlnode_insert_cdata(xmlnode_get_tag(m->packet->x, "error"), spools(p, "ns tag cannot be used this way", p), -1);
                    xmlnode_hide(m->packet->iq);
                    jpacket_reset(m->packet);
                    js_session_to(m->s, m->packet);
                    pool_free(p);
                    return M_HANDLED;
                }


                c = spools(p, "conditions/", xmlnode_get_name(tag), p);
                a = spools(p, "actions/", xmlnode_get_name(tag), p);
                if(xmlnode_get_tag(config, c) == NULL && xmlnode_get_tag(config, a) == NULL)
                { /* invalid tag used */
                    jutil_iqresult(m->packet->x);
                    xmlnode_put_attrib(m->packet->x, "type", "error");
                    xmlnode_put_attrib(xmlnode_insert_tag(m->packet->x, "error"), "code", "406");
                    xmlnode_insert_cdata(xmlnode_get_tag(m->packet->x, "error"), spools(p, "tag type '", xmlnode_get_name(tag), "' can not be used on this server", p), -1);
                    xmlnode_hide(m->packet->iq);
                    jpacket_reset(m->packet);
                    js_session_to(m->s, m->packet);
                    pool_free(p);
                    return M_HANDLED;
                }
            }
        }
        pool_free(p);

        xdb_set(m->si->xc, m->user->id, NS_FILTER, m->packet->iq);
        jutil_iqresult(m->packet->x);
        xmlnode_hide(m->packet->iq);
        jpacket_reset(m->packet);
        js_session_to(m->s, m->packet);
        break;
    case JPACKET__GET:
        opts = mod_filter_get(m->user);
        xmlnode_put_attrib(m->packet->x, "type", "result");
        xmlnode_insert_node(m->packet->iq, xmlnode_get_firstchild(opts));
        jpacket_reset(m->packet);
        js_session_to(m->s, m->packet);
        xmlnode_free(opts);
        break;
    default:
        xmlnode_free(m->packet->x);
    }
    return M_HANDLED;
}

mreturn mod_filter_out(mapi m, void *arg)
{
    log_debug(ZONE, "\n packet out from mod_filter\n");
    switch(m->packet->type)
    {
    case JPACKET_IQ:
        return mod_filter_iq(m);
        break;
    default:
        return M_IGNORE;
    }
    return M_PASS;
}

/* sets up the per-session listeners */
mreturn mod_filter_session(mapi m, void *arg)
{
    js_mapi_session(es_OUT, m->s, mod_filter_out, NULL);

    return M_PASS;
}

void mod_filter(jsmi si)
{
    xmlnode rule;
    mod_filter__jsmi = si;

    js_mapi_register(si, e_DELIVER, mod_filter_handler, NULL);
    js_mapi_register(si, e_SESSION, mod_filter_session, NULL);

    /* setup the default built-in rule */
    rule = js_config(si, "filter");
    rule = xmlnode_get_tag(rule, "default");

    mod_filter__default = xmlnode_new_tag("query");
    xmlnode_put_attrib(mod_filter__default, "xmlns", NS_FILTER);
    xmlnode_insert_node(mod_filter__default, xmlnode_get_firstchild(rule));

    log_debug("mod_filter", "mod_filter startup up... default server rule: %s", xmlnode2str(mod_filter__default));
}

