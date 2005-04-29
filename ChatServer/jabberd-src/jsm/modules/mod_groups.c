/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "License").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the License.  You
 * may obtain a copy of the License at http://www.jabber.com/license/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2000 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Schuyler Heath.
 *                    (c) 2001      Philip Anderson.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * --------------------------------------------------------------------------*/
#include "jsm.h"

#define NS_XGROUPS "jabber:xdb:groups"
#define NS_XINFO   "jabber:xdb:groups:info" /* info about the group, name, edit/write perms, etc... */
#define GROUP_GET(mi,gid) (gt = (grouptab) xhash_get(mi->groups,gid)) ? gt : mod_groups_tab_add(mi,gid)

typedef struct
{
    pool p;
    xdbcache xc;
    xht groups;
    xht config; /* hash of group specfic config */
    char *inst; /* register instructions */
} *mod_groups_i, _mod_groups_i;

typedef struct
{
    xht to;
    xht from;
} *grouptab, _grouptab;

xmlnode mod_groups_get_info(mod_groups_i mi, pool p, char *host, char *gid)
{
    xmlnode info, xinfo, cur;
    jid id;

    if (gid == NULL) return NULL;

    log_debug("mod_groups","Getting info %s",gid);

    id = jid_new(p,host);
    jid_set(id,gid,JID_RESOURCE);
    xinfo = xdb_get(mi->xc,id,NS_XINFO);

    info = xmlnode_get_tag((xmlnode) xhash_get(mi->config,gid),"info");
    if (info != NULL)
        info = xmlnode_dup(info);
    else
        return xinfo;

    for (cur = xmlnode_get_firstchild(xinfo); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        if (xmlnode_get_tag(info,xmlnode_get_name(cur)) == NULL) /* config overrides */
            xmlnode_insert_node(info,cur);

    xmlnode_free(xinfo);

    return info;
}

xmlnode mod_groups_get_users(mod_groups_i mi, pool p, char *host, char *gid)
{
    xmlnode group, users;
    jid id;

    if (gid == NULL) return NULL;

    log_debug("mod_groups","getting users %s",gid);

    /* check config for specfic group before xdb */
    group = (xmlnode) xhash_get(mi->config,gid);

    if (group != NULL && (users = xmlnode_get_tag(group,"users")) != NULL)
        return xmlnode_dup(users);

    log_debug("mod_groups","%d %d",group != NULL,users!= NULL);

    id = jid_new(p,host);
    jid_set(id,gid,JID_RESOURCE);

    return xdb_get(mi->xc,id,NS_XGROUPS);
}

void mod_groups_top_walk(xht h, const char *gid, void *val, void *arg)
{
    if (strchr(gid,'/') == NULL)
    {
        xmlnode result = (xmlnode) arg;
        xmlnode group, info;
        pool p;

        p = xmlnode_pool(result);

        /* config overrides xdb */
        xmlnode_hide(xmlnode_get_tag(result,spools(p,"group?id=",gid,p)));

        /* bah, vattrib hack */
        info = mod_groups_get_info((mod_groups_i) xmlnode_get_vattrib(result,"mi"),p,xmlnode_get_attrib(result,"host"),(char *) gid);

        group = xmlnode_insert_tag(result,"group");
        xmlnode_put_attrib(group,"name",xmlnode_get_tag_data(info,"name"));
        xmlnode_put_attrib(group,"id",gid);

        xmlnode_free(info);
    }
}

/* returns toplevel groups */
xmlnode mod_groups_get_top(mod_groups_i  mi, pool p, char *host)
{
    xmlnode result;

    result = xdb_get(mi->xc,jid_new(p,host),NS_XGROUPS);

    if (result == NULL)
        result = xmlnode_new_tag("query");

    xmlnode_put_vattrib(result,"mi",(void *) mi);
    xmlnode_put_attrib(result,"host",host);

    /* insert toplevel groups from config */
    xhash_walk(mi->config,mod_groups_top_walk,(void *) result);

    xmlnode_hide_attrib(result,"mi");
    xmlnode_hide_attrib(result,"host");

    return result;
}

/* inserts required groups into result */
void mod_groups_current_walk(xht h, const char *gid, void *val, void *arg)
{
    xmlnode info;

    info = xmlnode_get_tag((xmlnode) val,"info");

    if (xmlnode_get_tag(info,"require") != NULL)
    {
        xmlnode result = (xmlnode) arg;
        xmlnode group;
        pool p;

        log_debug("mod_groups","required group %s",gid);

        p = xmlnode_pool(result);
        group = xmlnode_get_tag(result,spools(p,"?id=",gid,p));

        if (group == NULL)
        {
            group = xmlnode_insert_tag(result,"group");
            xmlnode_put_attrib(group,"id",gid);

            /* remember the jid attrib is "?jid=<jid>" */
            if (xmlnode_get_tag(xmlnode_get_tag(info,"users"),xmlnode_get_attrib(result,"jid")) != NULL)
                xmlnode_put_attrib(group,"type","both");
        }
        else
            xmlnode_put_attrib(group,"type","both");  
    }
}

/* get the list of groups a user is currently a member of */
xmlnode mod_groups_get_current(mod_groups_i mi, jid id)
{
    xmlnode result;
    pool p;

    id = jid_user(id);
    result = xdb_get(mi->xc,id,NS_XGROUPS);

    if (result == NULL)
        result = xmlnode_new_tag("query");

    p = xmlnode_pool(result);

    xmlnode_put_attrib(result,"jid",spools(p,"?jid=",jid_full(id),p));
    xhash_walk(mi->config,mod_groups_current_walk,(void *) result);
    xmlnode_hide_attrib(result,"jid");

    return result;
}

grouptab mod_groups_tab_add(mod_groups_i mi, char *gid)
{
    grouptab gt;

    log_debug("mod_groups","new group entry %s",gid);
    gt = pmalloco(mi->p,sizeof(_grouptab));
    gt->to = xhash_new(509);
    gt->from = xhash_new(509);
    xhash_put(mi->groups,pstrdup(mi->p,gid),gt);

    return gt;
}

void mod_groups_presence_to_walk(xht h, const char *key, void *val, void *arg)
{
    session from;

    from = js_session_primary((udata) val);

    if (from != NULL)
        js_session_to((session) arg,jpacket_new(xmlnode_dup(from->presence)));
}

/* send presence to a session from the group members */
void mod_groups_presence_to(session s, grouptab gt)
{
    xhash_put(gt->to,jid_full(s->u->id),(void *) s->u); /* we don't care if it replaces the old entry */
    xhash_walk(gt->from,mod_groups_presence_to_walk,(void *) s);
}

void mod_groups_presence_from_walk(xht h, const char *key, void *val, void *arg)
{
    xmlnode x = (xmlnode) arg;
    udata u = (udata) val;
    session s;

    s = xmlnode_get_vattrib(x,"s");
    if (s->u != u)
    {
        xmlnode pres;

        log_debug("mod_groups","delivering presence to %s",jid_full(u->id));

        pres = xmlnode_dup(x);
        xmlnode_put_attrib(pres,"to",jid_full(u->id));
        xmlnode_hide_attrib(pres,"s");
        js_session_from(s,jpacket_new(pres));
    }
}

/* send presence from a session to online members of a group */
void mod_groups_presence_from(session s, grouptab gt, xmlnode pres)
{
    udata u = s->u;

    log_debug("mod_groups","brodcasting");

    if (xhash_get(gt->from,jid_full(u->id)) == NULL)
        xhash_put(gt->from,jid_full(u->id),u);

    /* send our presence to online users subscribed to this group */
    xmlnode_hide_attrib(pres,"to");
    xmlnode_put_vattrib(pres,"s",s);
    xhash_walk(gt->to,mod_groups_presence_from_walk,(void *) pres);
    xmlnode_hide_attrib(pres,"s");
}

void mod_groups_roster_insert(udata u, xmlnode roster, xmlnode group, char *gn, int add)
{
    xmlnode item, cur, q;
    char *id, *user;

    user = jid_full(u->id);
    q = xmlnode_get_tag(roster,"query");

    /* loop through each item in the group */
    for (cur = xmlnode_get_firstchild(group); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        id = xmlnode_get_attrib(cur,"jid");
        if (id == NULL || strcmp(id,user) == 0)  /* don't push ourselves */
            continue;

        /* add them to the roster */
        item = xmlnode_insert_tag(q,"item");
        xmlnode_put_attrib(item,"jid",id);
        xmlnode_put_attrib(item,"subscription",add ? "to":"remove");
        xmlnode_put_attrib(item,"name",xmlnode_get_attrib(cur,"name"));

        xmlnode_insert_cdata(xmlnode_insert_tag(item,"group"),gn,-1);
    }

    xmlnode_free(group);
}

/* push updated roster to all sessions or a specfic session */
void mod_groups_roster_push(session s, xmlnode roster, int all)
{
    session cur;

    if (all)
    {
        /* send a copy to all session that have a roster */
        for(cur = s->u->sessions; cur != NULL; cur = cur->next)
            if(cur->roster)
                js_session_to(cur,jpacket_new(cur->next ? xmlnode_dup(roster):roster));
    }
    else
        js_session_to(s,jpacket_new(roster));
}

void mod_groups_update_walk(xht h, const char *key, void *val, void *arg)
{
    xmlnode packet = (xmlnode) arg;
    udata u = (udata) val;
    mod_groups_roster_push(js_session_primary(u),xmlnode_dup(packet),1);
}

/* updates every members roster with the new user */
void mod_groups_update_rosters(grouptab gt, jid uid, char *un, char *gn, int add)
{
    xmlnode packet, item, q;

    packet = xmlnode_new_tag("iq");
    xmlnode_put_attrib(packet, "type", "set");
    q = xmlnode_insert_tag(packet, "query");
    xmlnode_put_attrib(q,"xmlns",NS_ROSTER);

    item = xmlnode_insert_tag(q,"item");
    xmlnode_put_attrib(item,"jid",jid_full(uid));
    xmlnode_put_attrib(item,"name",un);
    xmlnode_put_attrib(item,"subscription",add ? "to" : "remove");
    xmlnode_insert_cdata(xmlnode_insert_tag(item,"group"),gn,-1);

    xhash_walk(gt->to,mod_groups_update_walk,(void *) packet);

    xmlnode_free(packet);
}

/* adds a user to the master group list and to their personal list */
int mod_groups_xdb_add(mod_groups_i mi, pool p, jid uid, char *un, char *gid, char *gn, int both)
{
    xmlnode groups, user, group;
    jid xid;

    xid = jid_new(p,uid->server);
    jid_set(xid,gid,JID_RESOURCE);

    user = xmlnode_new_tag("user");
    xmlnode_put_attrib(user,"jid",jid_full(uid));
    xmlnode_put_attrib(user,"name",un);

    if(both && xdb_act(mi->xc,xid,NS_XGROUPS,"insert",spools(p,"?jid=",jid_full(uid),p),user))
    {
        log_debug(ZONE,"Failed to insert user");
        xmlnode_free(user);
        return 1;
    }
    xmlnode_free(user);

    /* get the groups this user is currently part of */
    groups = mod_groups_get_current(mi,uid);
    if (groups == NULL)
    {
        groups = xmlnode_new_tag("query");
        xmlnode_put_attrib(groups,"xmlns",NS_XGROUPS);
    }

    /* check if the user already as the group listed */
    group = xmlnode_get_tag(groups,spools(p,"?id=",gid,p));
    if (group == NULL)
    {
        group = xmlnode_insert_tag(groups,"group");
        xmlnode_put_attrib(group,"id",gid);
    }
    else if (j_strcmp(xmlnode_get_attrib(group,"type"),"both") == 0 && both)
    {
        /* the group is already there */
        xmlnode_free(groups);
        return 0;
    }
    else if (both == 0)
    {
        xmlnode_free(groups);
        return 0;
    }

    /* save the new group in the users list groups */
    if (both)
        xmlnode_put_attrib(group,"type","both");

    xdb_set(mi->xc,uid,NS_XGROUPS,groups);
    xmlnode_free(groups);

    return 0;
}

/* removes a user from the master group list and from their personal list */
int mod_groups_xdb_remove(mod_groups_i mi, pool p, jid uid, char *host, char *gid)
{
    xmlnode groups, group, info;
    jid xid;

    xid = jid_new(p,uid->server);
    jid_set(xid,gid,JID_RESOURCE);

    if(xdb_act(mi->xc,xid,NS_XGROUPS,"insert",spools(p,"?jid=",jid_full(uid),p),NULL))
    {
        log_debug(ZONE,"Failed to remove user");
        return 1;
    }

    info = mod_groups_get_info(mi, p, host, gid);
    if (xmlnode_get_tag(info,"require") != NULL)
        return 0;

    /* get the groups this user is currently part of */
    groups = mod_groups_get_current(mi,uid);
    if (groups == NULL)
    {
        groups = xmlnode_new_tag("query");
        xmlnode_put_attrib(groups,"xmlns",NS_XGROUPS);
    }

    /* check if the user already as the group listed */
    group = xmlnode_get_tag(groups,spools(p,"?id=",gid,p));
    if (group == NULL)
    {
        /* the group isn't there */
        xmlnode_free(groups);
        return 0;
    }

    /* Delete Node */
    xmlnode_hide(group);

    xdb_set(mi->xc,uid,NS_XGROUPS,groups);
    xmlnode_free(groups);

    return 0;
}


void mod_groups_register_set(mod_groups_i mi, mapi m)
{
    jpacket jp = m->packet;
    pool p = jp->p;
    grouptab gt;
    xmlnode info, roster, users;
    jid uid;
    char *gid, *host, *key, *un, *gn;
    int add, both;

    /* make sure it's a valid register query */
    key = xmlnode_get_tag_data(jp->iq,"key");
    gid = strchr(pstrdup(p,jp->to->resource),'/') + 1;
    if (gid == NULL || key == NULL || jutil_regkey(key,jid_full(jp->from)) == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTACCEPTABLE);
        return;
    }

    host = jp->from->server;
    info = mod_groups_get_info(mi,p,host,gid);
    if (info == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTFOUND);
        return;
    }

    uid = jid_user(jp->from);
    un = xmlnode_get_tag_data(jp->iq,"name");
    gn = xmlnode_get_tag_data(info,"name");

    add = (xmlnode_get_tag(jp->iq, "remove") == NULL);
    both = (xmlnode_get_tag(info,"static") == NULL);

    if (add)
    {
        log_debug("mod_groups","register GID %s",gid);
        if (mod_groups_xdb_add(mi,p,uid,un ? un : jid_full(uid),gid,gn,both))
        {
            js_bounce(m->si,jp->x,TERROR_UNAVAIL);
            xmlnode_free(info);
            return;
        }
    }
    else
    {
        log_debug("mod_groups","unregister GID %s",gid);
        if (mod_groups_xdb_remove(mi,p,uid,host,gid))
        {
            js_bounce(m->si,jp->x,TERROR_UNAVAIL);
            xmlnode_free(info);
            return;
        }
    }

    gt = GROUP_GET(mi,gid);

    /* push the group to the user */
    if (add || xmlnode_get_tag(info,"require") == NULL)
    {
        users = mod_groups_get_users(mi,p,host,gid);
        if (users != NULL)
        {
            roster = jutil_iqnew(JPACKET__SET,NS_ROSTER);
            mod_groups_roster_insert(m->user,roster,users,gn,add);
            mod_groups_roster_push(m->s,roster,add);
        }
    }

    /* push/remove the new user to the other members */
    if (both)
        mod_groups_update_rosters(gt,uid,un,gn,add);

    /* send presnce to everyone */
    if (add && both)
    {
        mod_groups_presence_from(m->s,gt,m->s->presence);
        mod_groups_presence_to(m->s,gt);
    }

    jutil_iqresult(jp->x);
    jpacket_reset(jp);
    js_session_to(m->s,jp);

    xmlnode_free(info);
}

void mod_groups_register_get(mod_groups_i mi, mapi m)
{
    jpacket jp = m->packet;
    xmlnode q;
    char *gid, *name = "";
    xmlnode members, user;
    jid uid = m->user->id;

    gid = strchr(pstrdup(jp->p, jp->to->resource),'/');

    if (gid != NULL && ++gid != NULL) /* Check that it is somewhat valid */
    {
        jutil_iqresult(jp->x);
        q = xmlnode_insert_tag(jp->x,"query");
        xmlnode_put_attrib(q,"xmlns",NS_REGISTER);

        /* Search to see if this users is already registered */
        members = mod_groups_get_users(mi,jp->p,jp->from->server,gid);
        user =  xmlnode_get_tag(members,spools(jp->p,"?jid=",uid->full,jp->p));
        if (user)
        {
            name = xmlnode_get_attrib(user, "name");
            xmlnode_insert_tag(q,"registered");

        }
        xmlnode_free(members);

        xmlnode_insert_cdata(xmlnode_insert_tag(q,"name"),name,-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"key"),jutil_regkey(NULL,jid_full(jp->from)),-1);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),mi->inst,-1);

        jpacket_reset(jp);
        js_session_to(m->s,jp);
    }
    else
        js_bounce(m->si,jp->x,TERROR_NOTACCEPTABLE);
}

void mod_groups_browse_set(mod_groups_i mi, mapi m)
{
    jpacket jp = m->packet;
    pool p = jp->p;
    grouptab gt;
    xmlnode info, user;
    jid uid;
    char *gid, *gn, *un, *host, *action;
    int add;

    log_debug(ZONE,"Setting");

    gid = strchr(jp->to->resource,'/');
    if (gid == NULL || ++gid == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTACCEPTABLE);
        return;
    }

    user = xmlnode_get_tag(jp->iq,"user");
    uid = jid_new(p,xmlnode_get_attrib(user,"jid"));
    un = xmlnode_get_attrib(user,"name");
    action = xmlnode_get_attrib(user, "action");
    add = ( ( action == NULL ) || j_strcmp(action, "remove") );

    if (uid == NULL || un == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTACCEPTABLE);
        return;
    }

    info = mod_groups_get_info(mi,p,jp->to->server,gid);
    if (info == NULL ||  xmlnode_get_tag(info,spools(p,"edit/user=",jid_full(jp->from),p)) == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTALLOWED);
        return;
    }
    gn = xmlnode_get_tag_data(info,"name");

    if ( add )
    {
        log_debug("mod_groups", "Adding");
        if (mod_groups_xdb_add(mi,p,uid,un,gid,gn,1))
        {
            js_bounce(m->si,jp->x,TERROR_UNAVAIL);
            xmlnode_free(info);
            return;
        }
    }
    else
    {
        log_debug("mod_groups", "Removing");
        host = jp->from->server;
        if (mod_groups_xdb_remove(mi,p,uid,host,gid))
        {
            js_bounce(m->si,jp->x,TERROR_UNAVAIL);
            xmlnode_free(info);
            return;
        }
    }

    gt = GROUP_GET(mi,gid);

    /* push the new user to the other members */
    mod_groups_update_rosters(gt,uid,un,gn,add);

    /* XXX how can we push the roster to the new user and send their presence?  lookup their session? */

    xmlnode_free(info);
    jutil_iqresult(jp->x);
    jpacket_reset(jp);
    js_session_to(m->s,jp);
}

void mod_groups_browse_result(pool p, jpacket jp, xmlnode group, char *host, char *gn)
{
    xmlnode q, cur, tag;
    char *id, *name;

    q = xmlnode_insert_tag(jutil_iqresult(jp->x),"item");
    xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
    xmlnode_put_attrib(q,"jid",jid_full(jp->to));
    xmlnode_put_attrib(q,"name",gn ? gn : "Toplevel groups");

    for (cur = xmlnode_get_firstchild(group); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if (xmlnode_get_type(cur) != NTYPE_TAG) continue;

        name = xmlnode_get_name(cur);

        if (j_strcmp(name,"group") == 0)
        {
            tag = xmlnode_insert_tag(q,"item");
            xmlnode_put_attrib(tag,"name",xmlnode_get_attrib(cur,"name"));
            id = spools(p,host,"/groups/",xmlnode_get_attrib(cur,"id"),p);
            xmlnode_put_attrib(tag,"jid",id);
        }
        else if (j_strcmp(name,"user") == 0)
        {
            xmlnode_insert_node(q,cur);
        }
    }
}

void mod_groups_browse_get(mod_groups_i mi, mapi m)
{
    jpacket jp = m->packet;
    xmlnode group;
    pool p = jp->p;
    xmlnode info = NULL;
    char *gid, *gn, *host = jp->to->server;

    log_debug("mod_groups","Browse request");

    gid = strchr(jp->to->resource,'/');
    if (gid != NULL && ++gid != NULL)
    {
        group = mod_groups_get_users(mi,p,host,gid);
        info = mod_groups_get_info(mi,p,host,gid);
        gn = xmlnode_get_tag_data(info,"name");
    }
    else
    {
        group = mod_groups_get_top(mi,p,host);
        gn = NULL;
    }

    if (group == NULL && gn == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTFOUND);
        return;
    }

    if (group)
    {
        mod_groups_browse_result(p,jp,group,host,gn);
        xmlnode_free(group);
    }
    else
    {
        xmlnode q;

        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"item");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(q,"jid",jid_full(jp->to));
        xmlnode_put_attrib(q,"name",gn);
    }

    jpacket_reset(jp);

    if (gid)
    {
        xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"ns"),NS_REGISTER,-1);
        xmlnode_free(info);
    }

    js_session_to(m->s,jp);
}

void mod_groups_roster(mod_groups_i mi, mapi m)
{
    xmlnode groups, users, cur, roster;
    pool p;
    udata u = m->user;
    char *gid, *host = m->user->id->server;

    /* get group the user is a member of */
    if ((groups = mod_groups_get_current(mi,u->id)) == NULL)
        return;

    p = xmlnode_pool(groups);
    roster = jutil_iqnew(JPACKET__SET,NS_ROSTER);

    /* push each group */
    for (cur = xmlnode_get_firstchild(groups); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if (xmlnode_get_type(cur) != NTYPE_TAG) continue;

        gid = xmlnode_get_attrib(cur,"id");
        users = mod_groups_get_users(mi,p,host,gid);

        if (users != NULL)
        {
            xmlnode info;
            char *gn;

            info = mod_groups_get_info(mi,p,host,gid);
            gn = xmlnode_get_tag_data(info,"name");
            mod_groups_roster_insert(u,roster,users,gn ? gn : gid,1);
            xmlnode_free(info);
        }
        else
            log_debug("mod_groups","Failed to get users for group");
    }

    mod_groups_roster_push(m->s,roster,0);
    xmlnode_free(groups);
}

mreturn mod_groups_iq(mod_groups_i mi, mapi m)
{
    char *ns, *res;
    int type;

    ns = xmlnode_get_attrib(m->packet->iq,"xmlns");

    /* handle roster gets */
    type = jpacket_subtype(m->packet);
    if (j_strcmp(ns,NS_ROSTER) == 0)
    {
        if (jpacket_subtype(m->packet) == JPACKET__GET)
        {
            log_debug("mod_groups","Roster request");
            mod_groups_roster(mi,m);
        }
        return M_PASS;
    }

    /* handle iq's to groups */
    res = m->packet->to ? m->packet->to->resource : NULL;
    if (res && strncmp(res,"groups",6) == 0 && (strlen(res) == 6 || res[6] == '/'))
    {
        if (j_strcmp(ns,NS_BROWSE) == 0)
        {
            log_debug("mod_groups","Browse request");

            if (type == JPACKET__GET)
                mod_groups_browse_get(mi,m);
            else if (type == JPACKET__SET)
                mod_groups_browse_set(mi,m);
            else
                xmlnode_free(m->packet->x);
        }
        else if (j_strcmp(ns,NS_REGISTER) == 0)
        {
            log_debug("mod_groups","Register request");

            if (type == JPACKET__GET)
                mod_groups_register_get(mi,m);
            else if (type == JPACKET__SET)
                mod_groups_register_set(mi,m);
            else
                xmlnode_free(m->packet->x);
        }
        else
            js_bounce(m->si,m->packet->x,TERROR_NOTALLOWED);

        return M_HANDLED;
    }

    return M_PASS;
}

void mod_groups_presence(mod_groups_i mi, mapi m)
{
    grouptab gt;
    session s = m->s;
    udata u = m->user;
    xmlnode groups, cur;

    if ((groups = mod_groups_get_current(mi,u->id)) == NULL)
        return;

    log_debug("mod_groups","Getting groups for %s",jid_full(u->id));

    /* get each group */
    for (cur = xmlnode_get_firstchild(groups); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        char *gid;

        if ((gid = xmlnode_get_attrib(cur,"id")) == NULL) continue;

        gt = GROUP_GET(mi,gid);

        if(j_strcmp(xmlnode_get_attrib(cur,"type"),"both") == 0)
            mod_groups_presence_from(s,gt,m->packet->x);

        /* if we are new or our old priority was less then zero then "probe" the group members */
        if (js_session_primary(m->user) || m->s->priority < 0)
            mod_groups_presence_to(s,gt);
    }

    xmlnode_free(groups);
}

mreturn mod_groups_out(mapi m, void *arg)
{
    mod_groups_i mi = (mod_groups_i) arg;

    if (m->packet->type == JPACKET_PRESENCE)
    {
        if (m->packet->to == NULL) mod_groups_presence(mi,m);
        return M_PASS;
    }
    else if (m->packet->type == JPACKET_IQ)
        return mod_groups_iq(mi,m);

    return M_IGNORE;
}

mreturn mod_groups_end(mapi m, void *arg)
{
    mod_groups_i mi = (mod_groups_i) arg;
    xmlnode groups, cur;
    udata u = m->user;
    jid id = u->id;
    grouptab gt;

    if (js_session_primary(u) != NULL || (groups = mod_groups_get_current(mi,id)) == NULL)
        return M_PASS;

    log_debug("mod_groups","removing user from table");
    for (cur = xmlnode_get_firstchild(groups); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        gt = (grouptab) xhash_get(mi->groups,xmlnode_get_attrib(cur,"id"));
        if (gt == NULL) continue;

        if(j_strcmp(xmlnode_get_attrib(cur,"type"),"both") == 0)
            xhash_zap(gt->from,jid_full(id));

        xhash_zap(gt->to,jid_full(id));
    }

    xmlnode_free(groups);
    return M_PASS;
}

mreturn mod_groups_session(mapi m, void *arg)
{
    js_mapi_session(es_OUT,m->s,mod_groups_out,arg);
    js_mapi_session(es_END,m->s,mod_groups_end,arg);
    return M_PASS;
}

/* messages to groups */
void mod_groups_message_walk(xht h, const char *key, void *val, void *arg)
{
    xmlnode m = (xmlnode) arg;
    udata u = (udata) val;

    m = xmlnode_dup(m);
    xmlnode_put_attrib(m,"to",jid_full(u->id));
    js_deliver(u->si,jpacket_new(m));
}

void mod_groups_message_online(mod_groups_i mi, xmlnode msg, char *gid)
{
    grouptab gt;

    log_debug("mod_groups","broadcast message to '%s'",gid);

    gt = (grouptab) xhash_get(mi->groups,gid);
    if (gt != NULL)
    {
        xmlnode_put_attrib(msg,"from",xmlnode_get_attrib(msg,"to"));
        xmlnode_hide_attrib(msg,"to");
        xhash_walk(gt->from,mod_groups_message_walk,(void *) msg);
    }
    xmlnode_free(msg);
}

mreturn mod_groups_message(mapi m, void *arg)
{
    mod_groups_i mi = (mod_groups_i) arg;
    xmlnode info;
    jpacket jp = m->packet;
    char *gid;

    if(jp->type != JPACKET_MESSAGE) return M_IGNORE;
    if(jp->to == NULL || j_strncmp(jp->to->resource,"groups/",7) != 0) return M_PASS;

    /* circular safety */
    if(xmlnode_get_tag(jp->x,"x?xmlns=" NS_DELAY) != NULL)
    {
        xmlnode_free(jp->x);
        return M_HANDLED;
    }

    gid = strchr(jp->to->resource,'/');
    if (gid == NULL || ++gid == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTACCEPTABLE);
        return M_HANDLED;
    }

    info = mod_groups_get_info(mi,jp->p,jp->to->server,gid);
    if (info == NULL)
    {
        js_bounce(m->si,jp->x,TERROR_NOTFOUND);
        return M_HANDLED;
    }

    if (xmlnode_get_tag(info,spools(jp->p,"write/user=",jid_full(jp->from),jp->p)) != NULL)
        mod_groups_message_online(mi,jp->x,gid);
    else
        js_bounce(m->si,jp->x,TERROR_NOTALLOWED);

    xmlnode_free(info);
    return M_HANDLED;
}

void mod_groups_destroy(xht h, const char *key, void *val, void *arg)
{
    grouptab gt = (grouptab) val;

    xhash_free(gt->to);
    xhash_free(gt->from);
}

mreturn mod_groups_shutdown(mapi m, void *arg)
{
    mod_groups_i mi = (mod_groups_i) arg;

    xhash_walk(mi->groups,mod_groups_destroy,NULL);
    xhash_free(mi->groups);
    xhash_free(mi->config);
    pool_free(mi->p);

    return M_PASS;
}

void mod_groups(jsmi si)
{
    pool p;
    mod_groups_i mi;
    xmlnode cur, config;
    char *gid, *id = si->i->id;

    log_debug("mod_groups","initing");

    p = pool_new();
    mi = pmalloco(p,sizeof(_mod_groups_i));
    mi->p = p;
    mi->groups = xhash_new(67);
    mi->xc = si->xc;

    config = js_config(si,"groups");
    mi->inst = xmlnode_get_tag_data(config,"instructions");
    if (mi->inst == NULL)
        mi->inst = pstrdup(p,"This will add the group to your roster");

    if (config != NULL)
    {
        mi->config = xhash_new(67);
        for (cur = xmlnode_get_firstchild(config); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        {
            if (j_strcmp(xmlnode_get_name(cur),"group") != 0) continue;
            gid = xmlnode_get_attrib(cur,"id");
            if (gid == NULL)
            {
                log_error(id,"mod_groups: Error loading, no id attribute on group");
                pool_free(p);
                return;
            }
            else if (xhash_get(mi->config,gid) != NULL)
            {
                log_error(si->i->id,"mod_groups: Error loading, group '%s' configured twice",gid);
                pool_free(p);
                return;
            }

            if (xmlnode_get_tag(cur,"info") || xmlnode_get_tag(cur,"users"))
                xhash_put(mi->config,pstrdup(p,gid),cur);
        }
    }

    js_mapi_register(si,e_SERVER,mod_groups_message,(void *) mi);
    js_mapi_register(si,e_SESSION,mod_groups_session,(void *) mi);
    js_mapi_register(si,e_SHUTDOWN,mod_groups_shutdown,(void *) mi);
}
