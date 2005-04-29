/*
 * MU-Conference - Multi-User Conference Service
 * Copyright (c) 2002 David Sutton
 *
 *
 * This program is free software; you can redistribute it and/or drvify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#include "conference.h"
#include <sys/utsname.h>

void con_server_browsewalk(gpointer key, gpointer data, gpointer arg)
{
    cnr room = (cnr)data;
    jpacket jp = (jpacket)arg;
    char users[10];
    char maxu[10];
    xmlnode x;

    if(room == NULL)
    {
        log_warn(NAME, "[%s] Aborting: NULL room %s", FZONE, key);
	return;
    }

    /* We can only show the private rooms that the user already knows about */
    if(room->public == 0 && !in_room(room, jp->to) && !is_admin(room, jp->to) && !is_member(room, jp->to))
        return;

    /* Unconfigured rooms don't exist either */
    if(room->locked == 1)
        return;

    x = xmlnode_insert_tag(jp->iq, "item");

    xmlnode_put_attrib(x, "category", "conference");

    if(room->public == 0)
        xmlnode_put_attrib(x, "type", "private");
    else
        xmlnode_put_attrib(x, "type", "public");

    xmlnode_put_attrib(x, "jid", jid_full(room->id));

    if(room->maxusers > 0)
        xmlnode_put_attrib(x, "name", spools(jp->p, room->name, " (", itoa(room->count, users), "/", itoa(room->maxusers, maxu), ")", jp->p));
    else
        xmlnode_put_attrib(x, "name", spools(jp->p, room->name, " (", itoa(room->count, users), ")", jp->p));

}

void _server_discowalk(gpointer key, gpointer data, gpointer arg)
{
    cnr room = (cnr)data;
    jpacket jp = (jpacket)arg;
    xmlnode x;

    if(room == NULL)
    {
        log_warn(NAME, "[%s] Aborting: NULL room %s", FZONE, key);
	return;
    }

    /* if we're a private server, we can only show the rooms the user already knows about */
    if(room->public == 0 && !in_room(room, jp->to) && !is_admin(room, jp->to) && !is_member(room, jp->to))
        return;

    /* Unconfigured rooms don't exist either */
    if(room->locked == 1)
	return;

    x = xmlnode_insert_tag(jp->iq, "item");

    xmlnode_put_attrib(x, "jid", jid_full(room->id));

    xmlnode_put_attrib(x, "name", spools(jp->p, room->name, jp->p));
}

void con_server(cni master, jpacket jp)
{
    char *str;
    struct utsname un;
    xmlnode x;
    int start, status;
    time_t t;
    char *from;
    char nstr[10];
    jid user;

    log_debug(NAME, "[%s] server packet", FZONE);

    if(jp->type == JPACKET_PRESENCE)
    {
        log_debug(NAME, "[%s] Server packet: Presence - Not Implemented", FZONE);
        jutil_error(jp->x, TERROR_NOTIMPL);
        deliver(dpacket_new(jp->x),NULL);
	return;
    }

    if(jp->type != JPACKET_IQ)
    {
        log_debug(NAME, "[%s] Server packet: Dropping non-IQ packet", FZONE);
        xmlnode_free(jp->x);
        return;
    }

    /* Action by subpacket type */
    if(jpacket_subtype(jp) == JPACKET__SET)
    {
        log_debug(NAME, "[%s] Server packet - IQ Set", FZONE);

	if(NSCHECK(jp->iq,NS_REGISTER))
	{
	/* Disabled until checked */
        log_debug(NAME, "[%s] TERROR_NOTIMPL", FZONE);
        jutil_error(jp->x, TERROR_NOTIMPL);
        deliver(dpacket_new(jp->x),NULL);
	return;

            log_debug(NAME, "[%s] Server packet - Registration Handler", FZONE);
	    str = xmlnode_get_tag_data(jp->iq, "name");
	    x = xmlnode_get_tag(jp->iq, "remove");

	    from = xmlnode_get_attrib(jp->x, "from");

	    user = jid_new(jp->p, from);
	    status = is_registered(master, jid_full(jid_user(user)), str);

	    if(x)
	    {
	        log_debug(NAME, "[%s] Server packet - UnReg Submission", FZONE);
		set_data(master, str, from, NULL, 1);
		jutil_iqresult(jp->x);
	    }
	    else if(status == -1)
	    {
                log_debug(NAME, "[%s] Server packet - Registration Submission : Already taken", FZONE);
		jutil_error(jp->x, TERROR_MUC_NICKREG);
	    }
	    else if(status == 0)
	    {
                log_debug(NAME, "[%s] Server packet - Registration Submission", FZONE);
		set_data(master, str, from, NULL, 0);
		jutil_iqresult(jp->x);
	    }
	    else
	    {
                log_debug(NAME, "[%s] Server packet - Registration Submission : already set", FZONE);
		jutil_iqresult(jp->x);
	    }

            deliver(dpacket_new(jp->x), NULL);
	}

    }
    else if(jpacket_subtype(jp) == JPACKET__GET)
    {
        /* now we're all set */

	if(NSCHECK(jp->iq,NS_REGISTER))
	{
            log_debug(NAME, "[%s] Server packet - Registration Request", FZONE);
	/* Disabled until checked */
        log_debug(NAME, "[%s] TERROR_NOTIMPL", FZONE);
        jutil_error(jp->x, TERROR_NOTIMPL);
        deliver(dpacket_new(jp->x),NULL);
	return;

	    x = get_data_byjid(master, xmlnode_get_attrib(jp->x, "from"));
	    str = xmlnode_get_attrib(x, "nick") ? xmlnode_get_attrib(x, "nick") : "";
	    
	    jutil_iqresult(jp->x);
	    xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "query"), "xmlns", NS_REGISTER);
	    jpacket_reset(jp);

            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "instructions"), "Enter the nickname you wish to reserve for this conference service", -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "name"), str, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "key"), jutil_regkey(NULL, jid_full(jp->to)), -1);

	    if(x != NULL) 
	        xmlnode_insert_tag(jp->iq, "registered");
	    
            deliver(dpacket_new(jp->x), NULL);
	}
	else if(NSCHECK(jp->iq,NS_TIME))
        {
	    /* Compliant with JEP-0090 */ 

            log_debug(NAME, "[%s] Server packet - Time Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "query"), "xmlns", NS_TIME);
            jpacket_reset(jp);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "utc"), jutil_timestamp(), -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "tz"), tzname[0], -1);

            /* create nice display time */
            t = time(NULL);
            str = ctime(&t);

            str[strlen(str) - 1] = '\0'; /* cut off newline */
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "display"), pstrdup(jp->p, str), -1);

            deliver(dpacket_new(jp->x),NULL);
        }
	else if(NSCHECK(jp->iq,NS_VERSION))
        {
	    /* Compliant with JEP-0092 */

            log_debug(NAME, "[%s] Server packet - Version Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "query"), "xmlns", NS_VERSION);
            jpacket_reset(jp);

            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "name"), NAME, -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "version"), VERSION, -1);

            uname(&un);
            x = xmlnode_insert_tag(jp->iq,"os");
            xmlnode_insert_cdata(x, pstrdup(jp->p, un.sysname),-1);
            xmlnode_insert_cdata(x," ",1);
            xmlnode_insert_cdata(x,pstrdup(jp->p, un.release),-1);

            deliver(dpacket_new(jp->x),NULL);
        }
	else if(NSCHECK(jp->iq,NS_BROWSE))
        {
	    /* Compliant with JEP-0011 */

            log_debug(NAME, "[%s] Server packet - Browse Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "item"), "xmlns", NS_BROWSE);
            jpacket_reset(jp);

            xmlnode_put_attrib(jp->iq, "category", "conference");
            xmlnode_put_attrib(jp->iq, "type", "public");
            xmlnode_put_attrib(jp->iq, "jid", master->i->id);

            /* pull name from the server vCard */
            xmlnode_put_attrib(jp->iq, "name", xmlnode_get_tag_data(master->config, "vCard/FN"));
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_MUC, -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_DISCO, -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_BROWSE, -1);
            /* xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_REGISTER, -1); */
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_VERSION, -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_TIME, -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_LAST, -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_VCARD, -1);
            
	    /* Walk room hashtable and report available rooms */
            g_hash_table_foreach(master->rooms, con_server_browsewalk, (void*)jp);

            deliver(dpacket_new(jp->x), NULL);
        }
	else if(NSCHECK(jp->iq, NS_DISCO_INFO))
	{
            log_debug(NAME, "[%s] Server packet - Disco Info Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"), "xmlns", NS_DISCO_INFO);
            jpacket_reset(jp);

	    x = xmlnode_insert_tag(jp->iq,"identity");
	    xmlnode_put_attrib(x, "category", "conference");
	    xmlnode_put_attrib(x, "type", "text");
	    xmlnode_put_attrib(x, "name", xmlnode_get_tag_data(master->config, "vCard/FN"));

            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_MUC);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_DISCO);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_BROWSE);
            /* xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_REGISTER); */
            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_VERSION);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_TIME);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_LAST);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_VCARD);

            deliver(dpacket_new(jp->x),NULL);
	}
	else if(NSCHECK(jp->iq, NS_DISCO_ITEMS))
        { 
            log_debug(NAME, "[%s] Server packet - Disco Items Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"),"xmlns", NS_DISCO_ITEMS);
            jpacket_reset(jp);

            g_hash_table_foreach(master->rooms,_server_discowalk, (void*)jp);

            deliver(dpacket_new(jp->x),NULL);
        }
	else if(NSCHECK(jp->iq, NS_LAST))
        {
            log_debug(NAME, "[%s] Server packet - Last Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"),"xmlns",NS_LAST);
            jpacket_reset(jp);

            start = time(NULL) - master->start;
            sprintf(nstr,"%d",start);
            xmlnode_put_attrib(jp->iq,"seconds", pstrdup(jp->p, nstr));

            deliver(dpacket_new(jp->x),NULL);
        }
	else if(NSCHECK(jp->iq,NS_VCARD))
        { 
            log_debug(NAME, "[%s] Server packet - VCard Request", FZONE);

            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"vCard"),"xmlns",NS_VCARD);
            jpacket_reset(jp);

            xmlnode_insert_node(jp->iq,xmlnode_get_firstchild(xmlnode_get_tag(master->config,"vCard")));

            deliver(dpacket_new(jp->x),NULL);
        }
    }
    else
    {
        log_debug(NAME, "[%s] TERROR_NOTIMPL", FZONE);
        jutil_error(jp->x, TERROR_NOTIMPL);
        deliver(dpacket_new(jp->x),NULL);
    }

    return;
}


void _con_packets(void *arg)
{
    jpacket jp = (jpacket)arg;
    cni master = (cni)jp->aux1;
    cnr room;
    cnu u, u2;
    char *s, *reason;
    xmlnode node;
    int priority = -1;
    int created = 0;
    time_t now = time(NULL);

#ifndef _JCOMP
    pth_mutex_acquire(&master->lock, 0, NULL);
#else
    g_mutex_lock(master->lock);
#endif

    /* first, handle all packets just to the server (browse, vcard, ping, etc) */
    if(jp->to->user == NULL)
    {
        con_server(master, jp);
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
        return;
    }

    log_debug(NAME, "[%s] processing packet %s", FZONE, xmlnode2str(jp->x));

    /* any other packets must have an associated room */
    for(s = jp->to->user; *s != '\0'; s++) 
	    *s = tolower(*s); /* lowercase the group name */

    if((room = g_hash_table_lookup(master->rooms, jid_full(jid_user(jid_fix(jp->to))))) == NULL)
    {
	log_debug(NAME, "[%s] Room not found (%s)", FZONE, jid_full(jid_user(jp->to)));

	if((master->roomlock == 1 && !is_sadmin(master, jp->from)) || master->loader == 0)
	{
	    log_debug(NAME, "[%s] Room building request denied", FZONE);
	    jutil_error(jp->x, TERROR_MUC_ROOM);

#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}
	else if(jp->type == JPACKET_IQ && jpacket_subtype(jp) == JPACKET__GET && NSCHECK(jp->iq, NS_MUC_OWNER))
        {
            room = con_room_new(master, jid_user(jp->to), jp->from, NULL, NULL, 1, 1, 0);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    xmlnode_free(jp->x);
	    return;
	}
	else if(jp->to->resource == NULL)
	{
	    log_debug(NAME, "[%s] Room %s doesn't exist: Returning Bad Request", FZONE, jp->to->user);
	    jutil_error(jp->x, TERROR_BAD);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}
	else if(jpacket_subtype(jp) == JPACKET__UNAVAILABLE)
	{
	    log_debug(NAME, "[%s] Room %s doesn't exist: dropping unavailable presence", FZONE, jp->to->user);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    xmlnode_free(jp->x);
	    return;
	}
	else
	{
	    if(master->dynamic == -1)
                room = con_room_new(master, jid_user(jp->to), jp->from, NULL, NULL, 1, 0, 1);
	    else
                room = con_room_new(master, jid_user(jp->to), jp->from, NULL, NULL, 1, 0, 0);

            /* fall through, so the presence goes to the room like normal */
	    created = 1;
	}
    }

    /* get the sending user entry, if any */
    u = g_hash_table_lookup(room->remote, jid_full(jid_fix(jp->from)));

    /* handle errors */
    if(jpacket_subtype(jp) == JPACKET__ERROR)
    {
	log_debug(NAME, "[%s] Error Handler: init", FZONE);

        /* only allow iq errors that are to a resource (direct-chat) */
        if(jp->to->resource == NULL || jp->type != JPACKET_IQ)
	{
	    if(u != NULL && u->localid != NULL)
	    {
		log_debug(NAME, "[%s] Error Handler: Zapping user", FZONE);
	        node = xmlnode_new_tag("reason");
	        xmlnode_insert_cdata(node, "Lost connection", -1);

                con_user_zap(u, node);
	    }
	    else
	    {
		log_debug(NAME, "[%s] Error Handler: No cnu/lid found for user", FZONE);
	    }
	}

        xmlnode_free(jp->x);
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
        return;
    }

    /* Block message from users not already in the room */
    if(jp->type == JPACKET_MESSAGE && u == NULL)
    {
	log_debug(NAME, "[%s] Blocking message from outsider (%s)", FZONE, jid_full(jp->to));

        jutil_error(jp->x, TERROR_MUC_OUTSIDE);
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
        deliver(dpacket_new(jp->x),NULL);
        return;
    }

    /* several things use this field below as a flag */
    if(jp->type == JPACKET_PRESENCE)
        priority = jutil_priority(jp->x);

    /* sending available presence will automatically get you a generic user, if you don't have one */
    if(u == NULL && priority >= 0)
        u = con_user_new(room, jp->from);

    /* update tracking stuff */
    room->last = now;
    room->packets++;

    if(u != NULL)
    {
        u->last = now;
        u->packets++;
    }

    /* handle join/rename */
    if(priority >= 0 && jp->to->resource != NULL)
    {
        u2 = con_room_usernick(room, jp->to->resource); /* existing user w/ this nick? */

        /* it's just us updating our presence */
        if(u2 == u)
        {
            jp->to = jid_user(jp->to);
            xmlnode_put_attrib(jp->x, "to", jid_full(jp->to));

	    if(u)
            {
                xmlnode_free(u->presence);
	        u->presence = xmlnode_dup(jp->x);
            }

            con_room_process(room, u, jp);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
            return;
        }

	/* User already exists, return conflict Error */
        if(u2 != NULL)
        {
	    log_debug(NAME, "[%s] Nick Conflict (%s)", FZONE, jid_full(jid_user(jp->to)));

            jutil_error(jp->x, TERROR_MUC_NICK);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
            deliver(dpacket_new(jp->x),NULL);
            return;
        }

	/* Nick already registered, return conflict Error */
        if(is_registered(master, jid_full(jid_user(jid_fix(u->realid))), jp->to->resource) == -1)
        {
	    log_debug(NAME, "[%s] Nick Conflict with registered nick (%s)", FZONE, jid_full(jid_fix(jp->to)));

            jutil_error(jp->x, TERROR_MUC_NICKREG);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
            deliver(dpacket_new(jp->x),NULL);
            return;
        }

        /* if from an existing user in the room, change the nick */
	if(is_outcast(room, u->realid) && !is_admin(room, u->realid))
	{
	    log_debug(NAME, "[%s] Blocking Banned user (%s)", FZONE, jid_full(jid_user(jid_fix(jp->to))));

	    jutil_error(jp->x, TERROR_MUC_BANNED);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}

	/* User is not invited, return invitation error */
	if(room->invitation == 1 && !is_member(room, u->realid) && !is_owner(room, u->realid))
	{
	    jutil_error(jp->x, TERROR_MUC_INVITED);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}

	/* Room is full, return full room error */
	if(room->count >= room->maxusers && room->maxusers != 0 && !is_admin(room, u->realid))
	{
	    log_debug(NAME, "[%s] Room over quota - disallowing entry", FZONE);

	    jutil_error(jp->x, TERROR_MUC_FULL);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}

	/* Room has been locked against entry */
	if(room->locked && !is_owner(room, u->realid))
	{
	    log_debug(NAME, "[%s] Room has been locked", FZONE);

	    jutil_error(jp->x, TERROR_NOTFOUND);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}

	/* User already in room, simply a nick change */
        if(u->localid != NULL)
        {
            xmlnode_free(u->presence);
	    u->presence = xmlnode_dup(jp->x);
            con_user_nick(u, jp->to->resource, NULL); /* broadcast nick rename */
	    xmlnode_free(jp->x);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    return;

        }
	else if(room->secret == NULL || is_sadmin(master, jp->from)) /* No password required, just go right in, or you're an sadmin */
	{
	    if(NSCHECK(xmlnode_get_tag(jp->x,"x"), NS_MUC))
	    {
		/* Set legacy value to room value */
	        u->legacy = 0;
		node = xmlnode_get_tag(jp->x,"x");
		xmlnode_hide(node);
              
                /* Enable room defaults automatically */ 
                if(master->roomlock == -1)
                {
                    created = 0;
                }

	    }
	    else
	    {
		u->legacy = 1;
		created = 0; /* Override created flag for non-MUC compliant clients */
	    } 

            xmlnode_free(u->presence);
	    u->presence = xmlnode_dup(jp->x);
	    jutil_delay(u->presence, NULL);

  	    log_debug(NAME, "[%s] About to enter room, legacy<%d>, presence [%s]", FZONE, u->legacy, xmlnode2str(u->presence));
            con_user_enter(u, jp->to->resource, created); /* join the room */

	    xmlnode_free(jp->x);
#ifndef _JCOMP
            pth_mutex_release(&master->lock);
#else
            g_mutex_unlock(master->lock);
#endif
	    return;

        }
	else if(jp->type == JPACKET_PRESENCE) /* Hopefully you are including a password, this room is locked */
	{
	    if(NSCHECK(xmlnode_get_tag(jp->x,"x"), NS_MUC))
	    {
	        log_debug(NAME, "[%s] Password?", FZONE);
		if(j_strcmp(room->secret, xmlnode_get_tag_data(xmlnode_get_tag(jp->x,"x"), "password")) == 0)
		{
		    /* Set legacy value to room value */
		    u->legacy = 0;
		    node = xmlnode_get_tag(jp->x,"x");
		    xmlnode_hide(node);

                    xmlnode_free(u->presence);
	            u->presence = xmlnode_dup(jp->x);

	            jutil_delay(u->presence, NULL);
		    con_user_enter(u, jp->to->resource, created); /* join the room */

		    xmlnode_free(jp->x);
#ifndef _JCOMP
                    pth_mutex_release(&master->lock);
#else
                    g_mutex_unlock(master->lock);
#endif
		    return;
		}
	    }
	}

	/* No password found, room is password protected. Return password error */
        jutil_error(jp->x, TERROR_MUC_PASSWORD);
        deliver(dpacket_new(jp->x), NULL);
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
        return;
    }

    /* kill any user sending unavailable presence */
    if(jpacket_subtype(jp) == JPACKET__UNAVAILABLE)
    {
	log_debug(NAME, "[%s] Calling user zap", FZONE);

	if(u != NULL) 
	{
	    reason = xmlnode_get_tag_data(jp->x, "status");

            xmlnode_free(u->presence);
	    u->presence = xmlnode_dup(jp->x);

	    node = xmlnode_new_tag("reason");
	    if (reason)
	        xmlnode_insert_cdata(node, reason, -1);

            con_user_zap(u, node);
	}

        xmlnode_free(jp->x);
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
        return;
    }

    /* not in the room yet? foo */
    if(u == NULL || u->localid == NULL)
    {
	if(u == NULL)
	{
		log_debug(NAME, "[%s] No cnu found for user", FZONE);
	}
	else
	{
		log_debug(NAME, "[%s] No lid found for %s", FZONE, jid_full(u->realid));
	}

        if(jp->to->resource != NULL)
        {
            jutil_error(jp->x, TERROR_NOTFOUND);
            deliver(dpacket_new(jp->x),NULL);
        }
	else
	{
            con_room_outsider(room, u, jp); /* non-participants get special treatment */
        }
	
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
        return;
    }

    /* packets to a specific resource?  one on one chats, browse lookups, etc */
    if(jp->to->resource != NULL)
    {
        if((u2 = g_hash_table_lookup(room->local, jp->to->resource)) == NULL && (u2 = con_room_usernick(room, jp->to->resource)) == NULL) /* gotta have a recipient */
        {
            jutil_error(jp->x, TERROR_NOTFOUND);
            deliver(dpacket_new(jp->x),NULL);
        }
	else
	{
            con_user_process(u2, u, jp);
        }
        
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
	return;
    }

    /* finally, handle packets just to a room from a participant, msgs, pres, iq browse/conferencing, etc */
    con_room_process(room, u, jp);

#ifndef _JCOMP
    pth_mutex_release(&master->lock);
#else
    g_mutex_unlock(master->lock);
#endif

}

/* phandler callback, send packets to another server */
result con_packets(instance i, dpacket dp, void *arg)
{
    cni master = (cni)arg;
    jpacket jp;

    if(dp == NULL)
    {
        log_warn(NAME, "[%s] Err: Sent a NULL dpacket!", FZONE);
	return r_DONE;
    }

    /* routes are from dnsrv w/ the needed ip */
    if(dp->type == p_ROUTE)
    {
	log_debug(NAME, "[%s] Rejecting ROUTE packet", FZONE);
        deliver_fail(dp,"Illegal Packet");
        return r_DONE;
    }
    else
    {
        jp = jpacket_new(dp->x);
    }

    /* if the delivery failed */
    if(jp == NULL)
    {
	log_warn(NAME, "[%s] Rejecting Illegal Packet", FZONE);
        deliver_fail(dp,"Illegal Packet");
        return r_DONE;
    }

    /* bad packet??? ick */
    if(jp->type == JPACKET_UNKNOWN || jp->to == NULL)
    {
	log_warn(NAME, "[%s] Bouncing Bad Packet", FZONE);
        jutil_error(jp->x, TERROR_BAD);
        deliver(dpacket_new(jp->x),NULL);
        return r_DONE;
    }

    /* we want things processed in order, and don't like re-entrancy! */
    jp->aux1 = (void*)master;
#ifdef _JCOMP
    _con_packets((void *)jp);
#else
    mtq_send(master->q, jp->p, _con_packets, (void *)jp);
#endif

    return r_DONE;
}

/** Save and clean out every room on shutdown */
void _con_shutdown_rooms(gpointer key, gpointer data, gpointer arg)
{
    cnr room = (cnr)data;

    if(room == NULL)
    {
        log_warn(NAME, "[%s] SHUTDOWN: Aborting attempt to clear %s", FZONE, key);
	return;
    }

#ifdef _JCOMP
    if(room->persistent == 1)
        xdb_room_set(room);
#endif
    
    con_room_cleanup(room);
}

/** Called to clean up system on shutdown */
void con_shutdown(void *arg)
{
    cni master = (cni)arg;

    if(master->shutdown == 1)
    {
        log_debug(NAME, "[%s] SHUTDOWN: Already commencing. Aborting attempt", FZONE);
	return;
    }
    else
    {
        master->shutdown = 1;
    }

    log_debug(NAME, "[%s] SHUTDOWN: Clearing configuration", FZONE);
    xmlnode_free(master->config);

    log_debug(NAME, "[%s] SHUTDOWN: Zapping sadmin table", FZONE);
    g_hash_table_destroy(master->sadmin);

    log_debug(NAME, "[%s] SHUTDOWN: Clear users from rooms", FZONE);
    g_hash_table_foreach(master->rooms, _con_shutdown_rooms, NULL);

    log_debug(NAME, "[%s] SHUTDOWN: Zapping rooms", FZONE);
    g_hash_table_destroy(master->rooms);

    free(master->day);

    log_debug(NAME, "[%s] SHUTDOWN: Sequence completed", FZONE);
}

/** Function called for walking each user in a room */
void _con_beat_user(gpointer key, gpointer data, gpointer arg)
{
    cnu user = (cnu)data;
    int now = (int)arg;

    if(user == NULL)
    {
        log_warn(NAME, "[%s] Aborting : NULL cnu for %s", FZONE, key);
	return;
    }

    if(user->localid == NULL && (now - user->last) > 120)
    {
	log_debug(NAME, "[%s] Marking zombie", FZONE);

        g_queue_push_tail(user->room->queue, g_strdup(jid_full(user->realid)));
    }
}

/* callback for walking each room */
void _con_beat_idle(gpointer key, gpointer data, gpointer arg)
{
    cnr room = (cnr)data;
    int now = (int)arg;
    xmlnode node;
    char *user_name;

    log_debug(NAME, "[%s] HBTICK: Idle check for >%s<", FZONE, key);

    if(room == NULL)
    {
        log_warn(NAME, "[%s] Aborting : NULL cnr for %s", FZONE, key);
	return;
    }
    
    /* Perform zombie user clearout */
    room->queue = g_queue_new();
    g_hash_table_foreach(room->remote, _con_beat_user, arg); /* makes sure nothing stale is in the room */

    while ((user_name = (char *)g_queue_pop_head(room->queue)) != NULL) 
    {
	node = xmlnode_new_tag("reason");
        xmlnode_insert_cdata(node, "Clearing zombie", -1);

        con_user_zap(g_hash_table_lookup(room->remote, user_name), node);

        log_debug(NAME, "[%s] HBTICK: removed zombie '%s' in the queue", FZONE, user_name);
        g_free(user_name);
    }
    g_queue_free(room->queue);

    /* Destroy timed-out dynamic room */
    if(room->persistent == 0 && room->count == 0 && (now - room->last) > 240)
    {
        log_debug(NAME, "[%s] HBTICK: Locking room and adding %s to remove queue", FZONE, key, now);
        room->locked = 1;
        g_queue_push_tail(room->master->queue, g_strdup(jid_full(room->id)));
    }
}

/* heartbeat checker for timed out idle rooms */
void _con_beat_logrotate(gpointer key, gpointer data, gpointer arg)
{
    cnr room = (cnr)data;

    if(room == NULL)
    {
        log_warn(NAME, "[%s] Aborting : NULL cnr for %s", FZONE, key);
	return;
    }
    
    if(room->logfile)
    {
	log_debug(NAME, "[%s] Rotating log for room %s", FZONE, jid_full(room->id));

	con_room_log_close(room);
	con_room_log_new(room);
    }
}

/* heartbeat checker for timed out idle rooms */
void _con_beat_logupdate(gpointer key, gpointer data, gpointer arg)
{
    cnr room = (cnr)data;
    char *timestamp = (char*)arg;

    if(room == NULL)
    {
        log_warn(NAME, "[%s] Aborting : NULL cnr for %s", FZONE, key);
	return;
    }
    
    if(room->logformat == LOG_XHTML && room->logfile)
    {
	log_debug(NAME, "[%s] Adding anchor >%s< for room %s", FZONE, timestamp, jid_full(room->id));
	fprintf(room->logfile, "<a name=\"%s\"></a>\n", timestamp);
	fflush(room->logfile);
    }
}

/* heartbeat checker for maintainance */
result con_beat_update(void *arg)
{
    cni master = (cni)arg;
    time_t t = time(NULL);
    int mins = minuteget(t);
    char *tstamp = timeget(t);
    char *dstamp = dateget(t);
    char *room_name;

    log_debug(NAME, "[%s] HBTICK", FZONE);

    /* Check for timed out idle rooms */
    if(mins % 2 == 0)
    {
#ifndef _JCOMP
        pth_mutex_acquire(&master->lock, 0, NULL);
#else
        g_mutex_lock(master->lock);
#endif
        log_debug(NAME, "[%s] HBTICK: Idle check started", FZONE);

        master->queue = g_queue_new();

  	g_hash_table_foreach(master->rooms, _con_beat_idle, (void*)t);

        while ((room_name = (char *)g_queue_pop_head(master->queue)) != NULL) 
	{
           log_debug(NAME, "[%s] HBTICK: removed room '%s' in the queue", FZONE, room_name);
           con_room_zap(g_hash_table_lookup(master->rooms, room_name));
           log_debug(NAME, "[%s] HBTICK: removed room '%s' in the queue", FZONE, room_name);
           g_free(room_name);
        }
        g_queue_free(master->queue);
        log_debug(NAME, "[%s] HBTICK: Idle check complete", FZONE);
#ifndef _JCOMP
        pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
    }

    /* Check for logfiles requiring updating */
    if(mins % 5 == 0)
    {
#ifndef _JCOMP
        pth_mutex_acquire(&master->lock, 0, NULL);
#else
        g_mutex_lock(master->lock);
#endif
        g_hash_table_foreach(master->rooms, _con_beat_logupdate, (void*)tstamp);
#ifndef _JCOMP
	pth_mutex_release(&master->lock);
#else
        g_mutex_unlock(master->lock);
#endif
    }

    /* Release malloc for tstamp */
    free(tstamp);

    if(j_strcmp(master->day, dstamp) == 0)
    {
        free(dstamp);
        return r_DONE;
    }

    free(master->day);
    master->day = j_strdup(dstamp);
    free(dstamp);

#ifndef _JCOMP
    pth_mutex_acquire(&master->lock, 0, NULL);
#else
    g_mutex_lock(master->lock);
#endif
    g_hash_table_foreach(master->rooms, _con_beat_logrotate, NULL);
#ifndef _JCOMP
    pth_mutex_release(&master->lock);
#else
    g_mutex_unlock(master->lock);
#endif

    return r_DONE;
}

/* heartbeat checker for miscellaneous tasks */
result con_beat_housekeep(void *arg)
{
    cni master = (cni)arg;

    master->loader = 1;

    xdb_rooms_get(master);

    /* Remove unwanted heartbeat */
    return r_UNREG;
}

/*** everything starts here ***/
void conference(instance i, xmlnode x)
{
#ifdef _JCOMP
    extern jcr_instance jcr;
#endif
    cni master;
    xmlnode cfg;
    jid sadmin;
    xmlnode current;
    xmlnode node;
    xmlnode tmp;
    pool tp;
    time_t now = time(NULL);

    log_debug(NAME, "[%s] mu-conference loading  - Service ID: %s", FZONE, i->id);

    /* Temporary pool for temporary jid creation */
    tp = pool_new();

    /* Allocate space for cni struct and link to instance */
    log_debug(NAME, "[%s] Malloc: _cni=%d", FZONE, sizeof(_cni));
    master = pmalloco(i->p, sizeof(_cni));
    master->i = i;
#ifndef _JCOMP
    /* Set up xdb interface */
    master->xdbc = xdb_cache(i);

    /* get the config */
    cfg = xdb_get(master->xdbc, jid_new(xmlnode_pool(x), "config@-internal"), "jabber:config:conference");

    /* Parse config and initialise variables */
    master->q = mtq_new(i->p);
#else
    /* get the config */
    cfg = xmlnode_get_tag(jcr->config, "conference");
#endif
    master->loader = 0;
    master->start = now;

    master->rooms = g_hash_table_new_full(g_str_hash, g_str_equal, ght_remove_key, ght_remove_cnr);

    master->history = j_atoi(xmlnode_get_tag_data(cfg,"history"),20);
    master->config = xmlnode_dup(cfg);					/* Store a copy of the config for later usage */
    master->day = dateget(now); 				/* Used to determine when to rotate logs */
    master->logdir = xmlnode_get_tag_data(cfg, "logdir");	/* Directory where to store logs */

    /* If requested, set default room state to 'public', otherwise will default to 'private */
    if(xmlnode_get_tag(cfg,"public"))
        master->public = 1;

    /* If requested, rooms are given a default configuration */
    if(xmlnode_get_tag(cfg,"defaults"))
        master->roomlock = -1;

    /* If requested, stop any new rooms being created */
    if(xmlnode_get_tag(cfg,"roomlock"))
        master->roomlock = 1;

    /* If requested, stop any new rooms being created */
    if(xmlnode_get_tag(cfg,"dynamic"))
        master->dynamic = 1;

    /* If requested, stop any new rooms being created */
    if(xmlnode_get_tag(cfg,"persistent"))
        master->dynamic = -1;

    master->sadmin = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_xmlnode);

    /* sadmin code */
    if(xmlnode_get_tag(cfg, "sadmin"))
    {
	node = xmlnode_get_tag(cfg, "sadmin");
	for(current = xmlnode_get_firstchild(node); current != NULL; current = xmlnode_get_nextsibling(current))
	{
	    sadmin = jid_new(tp, xmlnode_get_data(current));

	    if(sadmin != NULL)
	    {
                log_debug(NAME, "[%s] Adding sadmin %s", FZONE, jid_full(sadmin));
		/* use an xmlnode as the data value */
		tmp = xmlnode_new_tag("sadmin");
		g_hash_table_insert(master->sadmin, j_strdup(jid_full(jid_user(jid_fix(sadmin)))), (void*)tmp);
	    }
	}
    }
#ifndef _JCOMP
    register_phandler(i, o_DELIVER, con_packets, (void*)master);
    register_shutdown(con_shutdown,(void *) master);
    register_beat(60, con_beat_update, (void *)master);
    register_beat(1, con_beat_housekeep, (void *)master);
#else
    master->lock = g_mutex_new();
    master->loader = 1;
    xdb_rooms_get(master);
    register_phandler(i, o_DELIVER, con_packets, (void*)master);
    register_shutdown(con_shutdown,(void *) master);
    g_timeout_add(60000, (GSourceFunc)con_beat_update, (void *)master);
#endif

    pool_free(tp);
}
