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

/*
    <service id="pthsock client">
      <host>pth-csock.127.0.0.1</host> <!-- Can be anything -->
      <load>
	    <pthsock_client>../load/pthsock_client.so</pthsock_client>
      </load>
      <pthcsock xmlns='jabber:config:pth-csock'>
        <alias to="main.host.com">alias.host.com</alias>
        <alias to="default.host.com"/>
        <listen>5222</listen>            <!-- Port to listen on -->
        <!-- allow 25 connects per 5 seconts -->
        <rate time="5" points="25"/> 
      </pthcsock>
    </service>
*/

#include <jabberd.h>
#define DEFAULT_AUTH_TIMEOUT 0
#define DEFAULT_HEARTBEAT 0

/* socket manager instance */
typedef struct smi_st
{
    instance i;
    int auth_timeout;
    int heartbeat;
    HASHTABLE aliases;
    HASHTABLE users;
    xmlnode cfg;
    char *host;
} *smi, _smi;

typedef enum { state_UNKNOWN, state_AUTHD } user_state;
typedef struct cdata_st
{
    smi si;
    int aliased;
    jid session_id;
    jid sending_id;
    user_state state;
    char *client_id, *sid, *res, *auth_id;
    time_t connect_time;
    time_t last_activity;
    mio m;
    pth_msgport_t pre_auth_mp;
} _cdata,*cdata;


/* makes a route packet, intelligently */
xmlnode pthsock_make_route(xmlnode x, char *to, char *from, char *type)
{
    xmlnode new;
    new = x ? xmlnode_wrap(x, "route") : xmlnode_new_tag("route");

    if(type != NULL) 
        xmlnode_put_attrib(new, "type", type);

    if(to != NULL) 
        xmlnode_put_attrib(new, "to", to);

    if(from != NULL) 
        xmlnode_put_attrib(new, "from", from);

    return new;
}

/* incoming jabberd deliver()ed packets */
result pthsock_client_packets(instance id, dpacket p, void *arg)
{
    smi s__i = (smi)arg;
    cdata cdcur;
    mio m;
    int fd = 0;

    if(p->id->user != NULL)
        fd = atoi(p->id->user); 
    
    if(p->type != p_ROUTE || fd == 0)
    { /* we only want <route/> packets or ones with a valid connection */
        log_warn(p->host, "pthsock_client bouncing invalid %s packet from %s", xmlnode_get_name(p->x), xmlnode_get_attrib(p->x,"from"));
        deliver_fail(p, "invalid client packet");
        return r_DONE;
    }


    if ((cdcur = ghash_get(s__i->users, xmlnode_get_attrib(p->x,"to"))) == NULL)
    {
        if (!j_strcmp(xmlnode_get_attrib(p->x, "type"),  "session"))
        {
            jutil_tofrom(p->x);
            xmlnode_put_attrib(p->x, "type", "error");
            deliver(dpacket_new(p->x), s__i->i);
        } else {
            xmlnode_free(p->x);
        }
        return r_DONE;
    }



    if (fd != cdcur->m->fd || cdcur->m->state != state_ACTIVE)
        m = NULL;
    else if (j_strcmp(p->id->resource,cdcur->res) != 0)
        m = NULL;
    else
        m = cdcur->m;

    if(m == NULL)
    { 
        if (j_strcmp(xmlnode_get_attrib(p->x, "type"), "error") == 0)
        { /* we got a 510, but no session to end */
            log_debug("c2s", "[%s] received Session close for non-existant session: %s", ZONE, xmlnode_get_attrib(p->x, "from"));
            xmlnode_free(p->x);
            return r_DONE;
        }

        log_debug("c2s", "[%s] connection not found for %s, closing session", ZONE, xmlnode_get_attrib(p->x, "from"));

        jutil_tofrom(p->x);
        xmlnode_put_attrib(p->x, "type", "error");

        deliver(dpacket_new(p->x), s__i->i);
        return r_DONE;
    }

    log_debug("c2s", "[%s] %s has an active session, delivering packet", ZONE, xmlnode_get_attrib(p->x, "from"));
    if (j_strcmp(xmlnode_get_attrib(p->x, "type"), "error") == 0)
    { /* <route type="error" means we were disconnected */
        log_debug("c2s", "[%s] closing down session %s at request of session manager", ZONE, xmlnode_get_attrib(p->x, "from"));
        mio_write(m, NULL, "<stream:error>Disconnected</stream:error></stream:stream>", -1);
        mio_close(m);
        xmlnode_free(p->x);
        return r_DONE;
    }
    else if(cdcur->state == state_UNKNOWN && j_strcmp(xmlnode_get_attrib(p->x, "type"), "auth") == 0)
    { /* look for our auth packet back */
        char *type = xmlnode_get_attrib(xmlnode_get_firstchild(p->x), "type");
        char *id   = xmlnode_get_attrib(xmlnode_get_tag(p->x, "iq"), "id");
        if((j_strcmp(type, "result") == 0) && j_strcmp(cdcur->auth_id, id) == 0)
        { /* update the cdata status if it's a successfull auth */
            xmlnode x;
            log_debug("c2s", "[%s], auth for user successful", ZONE);
            /* notify SM to start a session */
            x = pthsock_make_route(NULL, jid_full(cdcur->session_id), cdcur->client_id, "session");
            log_debug("c2s", "[%s] requesting Session Start for %s", ZONE, xmlnode_get_attrib(p->x, "from"));
            deliver(dpacket_new(x), s__i->i);
        } 
        else if(j_strcmp(type,"error") == 0)
        {
            log_record(jid_full(jid_user(cdcur->session_id)), "login", "fail", "%s %s %s", mio_ip(cdcur->m), xmlnode_get_attrib(xmlnode_get_tag(p->x, "iq/error"),"code"), cdcur->session_id->resource);
        }
    } 
    else if(cdcur->state == state_UNKNOWN && j_strcmp(xmlnode_get_attrib(p->x, "type"), "session") == 0)
    { /* got a session reply from the server */
        mio_wbq q;

        cdcur->state = state_AUTHD;
        log_record(jid_full(jid_user(cdcur->session_id)), "login", "ok", "%s %s", mio_ip(cdcur->m), cdcur->session_id->resource);
        /* change the host id */
        cdcur->session_id = jid_new(m->p, xmlnode_get_attrib(p->x, "from"));
        xmlnode_free(p->x);
        /* if we have packets in the queue, write them */
        while((q = (mio_wbq)pth_msgport_get(cdcur->pre_auth_mp)) != NULL)
        {
            q->x = pthsock_make_route(q->x, jid_full(cdcur->session_id), cdcur->client_id, NULL);
            deliver(dpacket_new(q->x), s__i->i);
        }
        pth_msgport_destroy(cdcur->pre_auth_mp);
        cdcur->pre_auth_mp = NULL;
        return r_DONE;
    }


    if(xmlnode_get_firstchild(p->x) == NULL || ghash_get(s__i->users, xmlnode_get_attrib(p->x, "to")) == NULL)
    {
        xmlnode_free(p->x);
    }
    else
    {
        /* change the to name to match what the client wants to hear 
	 * jer: I have no sympathy for clients that can't handle this right, they should check the stream from="" anyway
        if(cdcur->aliased)
        {
            jid j = jid_new(p->p, xmlnode_get_attrib(xmlnode_get_firstchild(p->x), "to"));
            if(j != NULL && j_strcmp(j->server, cdcur->sending_id->server) != 0)
            {
                jid_set(j, cdcur->sending_id->server, JID_SERVER);
                xmlnode_put_attrib(xmlnode_get_firstchild(p->x), "to", jid_full(j));
            }

            j = jid_new(p->p, xmlnode_get_attrib(xmlnode_get_firstchild(p->x), "from"));
            if(j != NULL && j->user == NULL && j_strcmp(j->server, cdcur->session_id->server) == 0)
            {
                jid_set(j, cdcur->sending_id->server, JID_SERVER);
                xmlnode_put_attrib(xmlnode_get_firstchild(p->x), "from", jid_full(j));
            }
        }*/
        log_debug("c2s", "[%s] Writing packet to MIO: %s", ZONE, xmlnode2str(xmlnode_get_firstchild(p->x)));
        mio_write(m, xmlnode_get_firstchild(p->x), NULL, 0);
        cdcur->last_activity = time(NULL);
    }

    return r_DONE;
}

cdata pthsock_client_cdata(mio m, smi s__i)
{
    cdata cd;
    char *buf;

    cd               = pmalloco(m->p, sizeof(_cdata));
    cd->pre_auth_mp  = pth_msgport_create("pre_auth_mp");
    cd->state        = state_UNKNOWN;
    cd->connect_time = time(NULL);
    cd->last_activity = cd->connect_time;
    cd->m            = m;
    cd->si           = s__i;

    buf = pmalloco(m->p, 100);

    /* HACK to fix race conditon */
    snprintf(buf, 99, "%X", m);
    cd->res = pstrdup(m->p, buf);

    /* we use <fd>@host to identify connetions */
    snprintf(buf, 99, "%d@%s/%s", m->fd, s__i->host, cd->res);
    cd->client_id = pstrdup(m->p, buf);

    return cd;
}

void pthsock_client_read(mio m, int flag, void *arg, xmlnode x)
{
    cdata cd = (cdata)arg;
    xmlnode h;
    char *alias, *to;

    if(cd == NULL) 
        return;

    log_debug("c2s", "[%s] pthsock_client_read called with: m:%X flag:%d arg:%X", ZONE, m, flag, arg);
    switch(flag)
    {
    case MIO_CLOSED:

        log_debug("c2s", "[%s] io_select Socket %d close notification", ZONE, m->fd);
        ghash_remove(cd->si->users, cd->client_id);
        if(cd->state == state_AUTHD)
        {
            h = pthsock_make_route(NULL, jid_full(cd->session_id), cd->client_id, "error");
            deliver(dpacket_new(h), cd->si->i);
        }

        if(cd->pre_auth_mp != NULL)
        { /* if there is a pre_auth queue still */
            mio_wbq q;

            while((q = (mio_wbq)pth_msgport_get(cd->pre_auth_mp)) != NULL)
            {
                log_debug("c2s", "[%s] freeing unsent packet due to disconnect with no auth: %s", ZONE, xmlnode2str(q->x));
                xmlnode_free(q->x);
            }

            pth_msgport_destroy(cd->pre_auth_mp);
            cd->pre_auth_mp = NULL;
        } 
        break;
    case MIO_ERROR:
        while((h = mio_cleanup(m)) != NULL)
            deliver_fail(dpacket_new(h), "Socket Error to Client");

        break;
    case MIO_XML_ROOT:
        log_debug("c2s", "[%s] root received for %d", ZONE, m->fd);
        to = xmlnode_get_attrib(x, "to");
        cd->sending_id = jid_new(cd->m->p, to);

        /* check for a matching alias or use default alias */
        log_debug("c2s", "[%s] Recieved connection to: %s", ZONE, jid_full(cd->sending_id));
        alias = ghash_get(cd->si->aliases, to);
        alias = alias ? alias : ghash_get(cd->si->aliases, "default");

        /* set host to that alias, or to the given host */
        cd->session_id = alias ? jid_new(m->p, alias) : cd->sending_id;

        /* if we are using an alias, set the alias flag */
        if(j_strcmp(jid_full(cd->session_id), jid_full(cd->sending_id)) != 0) cd->aliased = 1;
        if(cd->aliased) log_debug("c2s", "[%s] using alias %s --> %s", ZONE, jid_full(cd->sending_id), jid_full(cd->session_id));

        /* write header */
        h = xstream_header("jabber:client", NULL, jid_full(cd->session_id));
        cd->sid = pstrdup(m->p, xmlnode_get_attrib(h, "id"));
        /* XXX hack in the style that jabber.com uses for flash mode support */
        if(j_strncasecmp(xmlnode_get_attrib(x, "xmlns:flash"), "http://www.jabber.com/streams/flash",35) == 0)
        {
            h = xmlnode_new_tag_pool(xmlnode_pool(h),"flash:stream");
            xmlnode_put_attrib(h, "xmlns:flash", "http://www.jabber.com/streams/flash"); 
            xmlnode_put_attrib(h, "xmlns:stream", "http://etherx.jabber.org/streams"); 
            xmlnode_put_attrib(h, "xmlns", "jabber:client"); 
            xmlnode_put_attrib(h, "id", cd->sid); 
            xmlnode_put_attrib(h, "from", jid_full(cd->session_id)); 
            /* put real stream declaration on incoming root */
            xmlnode_put_attrib(x, "xmlns:stream", "http://etherx.jabber.org/streams"); 
        }
        mio_write(m, NULL, xstream_header_char(h), -1);
        xmlnode_free(h);

        if(j_strcmp(xmlnode_get_attrib(x, "xmlns"), "jabber:client") != 0)
        { /* if they sent something other than jabber:client */
            mio_write(m, NULL, "<stream:error>Invalid Namespace</stream:error></stream:stream>", -1);
            mio_close(m);
        }
        else if(cd->session_id == NULL)
        { /* they didn't send a to="" and no valid alias */
            mio_write(m, NULL, "<stream:error>Did not specify a valid to argument</stream:error></stream:stream>", -1);
            mio_close(m);
        }
        else if(j_strncasecmp(xmlnode_get_attrib(x, "xmlns:stream"), "http://etherx.jabber.org/streams", 32) != 0)
        {
            mio_write(m, NULL, "<stream:error>Invalid Stream Namespace</stream:error></stream:stream>", -1);
            mio_close(m);
        }


        xmlnode_free(x);
        break;
    case MIO_XML_NODE:
        /* make sure alias is upheld */
        if(cd->aliased)
        {
            jid j = jid_new(xmlnode_pool(x), xmlnode_get_attrib(x, "to"));
            if(j != NULL && j_strcmp(j->server, cd->sending_id->server) == 0)
            {
                jid_set(j, cd->session_id->server, JID_SERVER);
                xmlnode_put_attrib(x, "to", jid_full(j));
            }
            j = jid_new(xmlnode_pool(x), xmlnode_get_attrib(x, "from"));
            if(j != NULL && j_strcmp(j->server, cd->sending_id->server) == 0)
            {
                jid_set(j, cd->session_id->server, JID_SERVER);
                xmlnode_put_attrib(x, "from", jid_full(j));
            }
        }

        cd = (cdata)arg;
        if (cd->state == state_UNKNOWN)
        { /* only allow auth and registration queries at this point */
            xmlnode q = xmlnode_get_tag(x, "query");
            if (!NSCHECK(q, NS_AUTH) && !NSCHECK(q, NS_REGISTER))
            {
                mio_wbq q;
                /* queue packet until authed */
                q = pmalloco(xmlnode_pool(x), sizeof(_mio_wbq));
                q->x = x;
                pth_msgport_put(cd->pre_auth_mp, (void*)q);
                return;
            }
            else if (NSCHECK(q, NS_AUTH))
            {
                if(j_strcmp(xmlnode_get_attrib(x, "type"), "set") == 0)
                { /* if we are authing against the server */
                    xmlnode_put_attrib(xmlnode_get_tag(q, "digest"), "sid", cd->sid);
                    cd->auth_id = pstrdup(m->p, xmlnode_get_attrib(x, "id"));
                    if(cd->auth_id == NULL) 
                    {
                        cd->auth_id = pstrdup(m->p, "pthsock_client_auth_ID");
                        xmlnode_put_attrib(x, "id", "pthsock_client_auth_ID");
                    }
                    jid_set(cd->session_id, xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x, "query?xmlns=jabber:iq:auth"), "username")), JID_USER);
                    jid_set(cd->session_id, xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x, "query?xmlns=jabber:iq:auth"), "resource")), JID_RESOURCE);

                    x = pthsock_make_route(x, jid_full(cd->session_id), cd->client_id, "auth");
                    deliver(dpacket_new(x), cd->si->i);
                }
                else if(j_strcmp(xmlnode_get_attrib(x, "type"), "get") == 0)
                { /* we are just doing an auth get */
                    /* just deliver the packet */
                    jid_set(cd->session_id, xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x, "query?xmlns=jabber:iq:auth"), "username")), JID_USER);
                    x = pthsock_make_route(x, jid_full(cd->session_id), cd->client_id, "auth");
                    deliver(dpacket_new(x), cd->si->i);
                }
            }
            else if (NSCHECK(q, NS_REGISTER))
            {
                jid_set(cd->session_id, xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x, "query?xmlns=jabber:iq:register"), "username")), JID_USER);
                x = pthsock_make_route(x, jid_full(cd->session_id), cd->client_id, "auth");
                deliver(dpacket_new(x), cd->si->i);
            }
        }
        else
        {   /* normal delivery of packets after authed */
            x = pthsock_make_route(x, jid_full(cd->session_id), cd->client_id, NULL);
            deliver(dpacket_new(x), cd->si->i);
            cd->last_activity = time(NULL);
        }
        break;
    }
}


void pthsock_client_listen(mio m, int flag, void *arg, xmlnode x)
{
    smi s__i = (void*)arg;
    cdata cd;

    if(flag != MIO_NEW)
        return;

    s__i = (smi)arg;
    cd = pthsock_client_cdata(m, s__i);
    ghash_put(cd->si->users, cd->client_id, cd);
    mio_reset(m, pthsock_client_read, (void*)cd);
}


int _pthsock_client_timeout(void *arg, const void *key, void *data)
{
    time_t timeout;
    cdata cd = (cdata)data;
    if(cd->state == state_AUTHD) 
        return 1;

    timeout = time(NULL) - cd->si->auth_timeout;
    log_debug("c2s", "[%s] timeout: %d, connect time %d: fd %d", ZONE, timeout, cd->connect_time, cd->m->fd);

    if(cd->connect_time < timeout)
    {
        mio_write(cd->m, NULL, "<stream:error>Timeout waiting for authentication</stream:error></stream:stream>", -1);
        ghash_remove(cd->si->users, mio_ip(cd->m));
        mio_close(cd->m);
    }
    return 1;
}

/* auth timeout beat function */
result pthsock_client_timeout(void *arg)
{
    smi s__i = (smi)arg;

    if(s__i->users == NULL)
        return r_UNREG;

    ghash_walk(s__i->users, _pthsock_client_timeout, NULL);
    return r_DONE;
}

int _pthsock_client_heartbeat(void *arg, const void *key, void *data)
{
    time_t skipbeat;
    cdata cd = (cdata)data;

    skipbeat = time(NULL) - cd->si->heartbeat;
    if ( (cd->state == state_AUTHD) &&
         (cd->last_activity < skipbeat) )
    {
       log_debug("c2s", "[%s] heartbeat on fd %d", ZONE, cd->m->fd);
       mio_write(cd->m, NULL, " \n", -1);
    }
    return 1;
}

/* auth timeout beat function */
result pthsock_client_heartbeat(void *arg)
{
    smi s__i = (smi)arg;

    if(s__i->users == NULL)
        return r_UNREG;

    ghash_walk(s__i->users, _pthsock_client_heartbeat, NULL);
    return r_DONE;
}


int _pthsock_client_shutdown(void *arg, const void *key, void *data)
{
    cdata cd = (cdata)data;
    log_debug("c2s", "[%s] closing down user %s from ip: %s", ZONE, jid_full(cd->session_id), mio_ip(cd->m));
    mio_close(cd->m);
    return 1;
}

/* cleanup function */
void pthsock_client_shutdown(void *arg)
{
    smi s__i = (smi)arg;
    xmlnode_free(s__i->cfg);
    log_debug("c2s", "[%s] Shutting Down", ZONE);
    ghash_walk(s__i->users, _pthsock_client_shutdown, NULL);
    s__i->users = NULL;
}

/* everything starts here */
void pthsock_client(instance i, xmlnode x)
{
    smi s__i;
    xdbcache xc;
    xmlnode cur;
    int set_rate = 0; /* Default false; did they want to change the rate parameters */
    int rate_time, rate_points;
    char *host;
    struct karma *k = karma_new(i->p); /* Get new inialized karma */
    int set_karma = 0; /* Default false; Did they want to change the karma parameters */

    log_debug("c2s", "[%s] pthsock_client loading", ZONE);

    s__i               = pmalloco(i->p, sizeof(_smi));
    s__i->auth_timeout = DEFAULT_AUTH_TIMEOUT;
    s__i->heartbeat    = DEFAULT_HEARTBEAT;
    s__i->i            = i;
    s__i->aliases      = ghash_create_pool(i->p, 7, (KEYHASHFUNC)str_hash_code, (KEYCOMPAREFUNC)j_strcmp);
    s__i->users        = ghash_create_pool(i->p, 503, (KEYHASHFUNC)str_hash_code, (KEYCOMPAREFUNC)j_strcmp);

    /* get the config */
    xc = xdb_cache(i);
    s__i->cfg = xdb_get(xc, jid_new(xmlnode_pool(x), "config@-internal"), "jabber:config:pth-csock");

    s__i->host = host = i->id;

    for(cur = xmlnode_get_firstchild(s__i->cfg); cur != NULL; cur = cur->next)
    {
        if(cur->type != NTYPE_TAG) 
            continue;
        
        if(j_strcmp(xmlnode_get_name(cur), "alias") == 0)
        {
           char *host, *to;
           if((to = xmlnode_get_attrib(cur, "to")) == NULL) 
               continue;

           host = xmlnode_get_data(cur);
           if(host != NULL)
           {
               ghash_put(s__i->aliases, host, to);
           }
           else
           {
               ghash_put(s__i->aliases, "default", to);
           }
        }
        else if(j_strcmp(xmlnode_get_name(cur), "authtime") == 0)
        {
            s__i->auth_timeout = j_atoi(xmlnode_get_data(cur), 0);
        }
        else if(j_strcmp(xmlnode_get_name(cur), "heartbeat") == 0)
        {
            s__i->heartbeat = j_atoi(xmlnode_get_data(cur), 0);
        }
        else if(j_strcmp(xmlnode_get_name(cur), "rate") == 0)
        {
            rate_time   = j_atoi(xmlnode_get_attrib(cur, "time"), 0);
            rate_points = j_atoi(xmlnode_get_attrib(cur, "points"), 0);
            set_rate = 1; /* set to true */
        }
        else if(j_strcmp(xmlnode_get_name(cur), "karma") == 0)
        {
            k->val     = j_atoi(xmlnode_get_tag_data(cur, "init"), KARMA_INIT);
            k->max     = j_atoi(xmlnode_get_tag_data(cur, "max"), KARMA_MAX);
            k->inc     = j_atoi(xmlnode_get_tag_data(cur, "inc"), KARMA_INC);
            k->dec     = j_atoi(xmlnode_get_tag_data(cur, "dec"), KARMA_DEC);
            k->restore = j_atoi(xmlnode_get_tag_data(cur, "restore"), KARMA_RESTORE);
            k->penalty = j_atoi(xmlnode_get_tag_data(cur, "penalty"), KARMA_PENALTY);
            k->reset_meter = j_atoi(xmlnode_get_tag_data(cur, "resetmeter"), KARMA_RESETMETER);
            set_karma = 1; /* set to true */
        }
    }

    /* start listening */
    if((cur = xmlnode_get_tag(s__i->cfg, "ip")) != NULL)
    {
        for(; cur != NULL; xmlnode_hide(cur), cur = xmlnode_get_tag(s__i->cfg, "ip"))
        {
            mio m;
            m = mio_listen(j_atoi(xmlnode_get_attrib(cur, "port"), 5222), xmlnode_get_data(cur), pthsock_client_listen, (void*)s__i, MIO_LISTEN_XML);
            if(m == NULL)
                return;

            /* Set New rate and points */
            if(set_rate == 1) mio_rate(m, rate_time, rate_points);
            /* Set New karma values */
            if(set_karma == 1) mio_karma2(m, k);
        }
    }

#ifdef HAVE_SSL
    /* listen on SSL sockets */
    if((cur = xmlnode_get_tag(s__i->cfg, "ssl")) != NULL)
    {
        for(; cur != NULL; xmlnode_hide(cur), cur = xmlnode_get_tag(s__i->cfg, "ssl"))
        {
            mio m;
            m = mio_listen(j_atoi(xmlnode_get_attrib(cur, "port"), 5223), xmlnode_get_data(cur), pthsock_client_listen, (void*)s__i, MIO_SSL_ACCEPT, mio_handlers_new(MIO_SSL_READ, MIO_SSL_WRITE, MIO_XML_PARSER));
            if(m == NULL)
                return;
            /* Set New rate and points */
            if(set_rate == 1) mio_rate(m, rate_time, rate_points);
            /* set karma valuse */
            if(set_karma == 1) mio_karma2(m, k);
        }
    }
                   
#endif

    /* register data callbacks */
    register_phandler(i, o_DELIVER, pthsock_client_packets, (void*)s__i);
    pool_cleanup(i->p, pthsock_client_shutdown, (void*)s__i);
    if(s__i->auth_timeout)
        register_beat(5, pthsock_client_timeout, (void*)s__i);

    if(s__i->heartbeat)
    {
        log_debug("c2s", "Registering heartbeat: %d", s__i->heartbeat);
        //Register a heartbeat to catch dead sockets.
        register_beat(s__i->heartbeat, pthsock_client_heartbeat, (void*)s__i);
    }
}

