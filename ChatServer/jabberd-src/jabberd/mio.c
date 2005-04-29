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
 * Portions Copyright (c) 2005 Apple Computer, Inc. 
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
   MIO -- Managed Input/Output
   ---------------------------

   The purpose of this file, is mainly to provide support, to any component
   of jabberd, for abstraced I/O functions.  This works much like tstreams,
   and will incorporate the functionality of io_select initially, but will
   be expanded to support any socket handling model, such as polld, SIGIO, etc

   This works to abstract the socket work, and hide it from the component,
   this way, the component does not have to deal with any complexeties of
   socket functions.
*/
#define MIO
#include <jabberd.h>
/********************************************************
 *************  Internal MIO Functions  *****************
 ********************************************************/

typedef struct mio_main_st
{
    pool p;             /* pool to hold this data */
    mio master__list;  /* a list of all the socks */
    pth_t t;            /* a pointer to thread for signaling */
    int shutdown;
    int zzz[2];
    struct karma *k; /* default karma */
    int rate_t, rate_p; /* default rate, if any */
    int limit_max_fd; /* limit the number of max_fd */
    long max_stanza_bytes;  /* default max # of bytes in any stanza */
    long max_message_bytes; /* default max # of bytes per message */
} _ios,*ios;

typedef struct mio_connect_st
{
    pool p;
    char *ip;
    int port;
    void *cb;
    void *cb_arg;
    mio_connect_func cf;
    mio_handlers mh;
    pth_t t;
    int connected;
} _connect_data,  *connect_data;

/* global object */
int mio__errno = 0;
int mio__ssl_reread = 0;
ios mio__data = NULL;
extern xmlnode greymatter__;

#ifdef WITH_IPV6
int _mio_compare_ipv6(const struct in6_addr *addr1, const struct in6_addr *addr2, int netsize)
{
    int i;
    u_int8_t mask;

    if(netsize > 128)
	netsize = 128;

    for(i = 0; i < netsize/8; i++)
    {
	if(addr1->s6_addr[i] != addr2->s6_addr[i])
	    return 0;
    }

    if (netsize%8 == 0)
	return 1;

    mask = 0xff << (8 - netsize%8);

    return ((addr1->s6_addr[i]&mask) == (addr2->s6_addr[i]&mask));
}

int _mio_netmask_to_ipv6(const char *netmask)
{
    struct in_addr addr;

    if (netmask == NULL)
    {
	return 128;
    }

    if (inet_pton(AF_INET, netmask, &addr))
    {
	uint32_t temp = ntohl(addr.s_addr);
	int netmask = 128;

	while (netmask>96 && temp%2==0)
	{
	    netmask--;
	    temp /= 2;
	}
	return netmask;
    }

    return atoi(netmask);
}
#endif

int _mio_allow_check(const char *address)
{
#ifdef WITH_IPV6
    char temp_address[INET6_ADDRSTRLEN];
    char temp_ip[INET6_ADDRSTRLEN];
    static struct in_addr tmpa;
#endif
    
    xmlnode io = xmlnode_get_tag(greymatter__, "io");
    xmlnode cur;

#ifdef WITH_IPV6
    if (inet_pton(AF_INET, address, &tmpa)) {
	strcpy(temp_address, "::ffff:");
	strcat(temp_address, address);
	address = temp_address;
    }
#endif

    if(xmlnode_get_tag(io, "allow") == NULL)
        return 1; /* if there is no allow section, allow all */

    for(cur = xmlnode_get_firstchild(io); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        char *ip, *netmask;
#ifdef WITH_IPV6
	struct in6_addr in_address, in_ip;
	int in_netmask;
#else
        struct in_addr in_address, in_ip, in_netmask;
#endif

        if(xmlnode_get_type(cur) != NTYPE_TAG)
            continue;

        if(j_strcmp(xmlnode_get_name(cur), "allow") != 0) 
            continue;

        ip = xmlnode_get_tag_data(cur, "ip");
        netmask = xmlnode_get_tag_data(cur, "mask");

        if(ip == NULL)
            continue;

#ifdef WITH_IPV6
	if (inet_pton(AF_INET, ip, &tmpa))
	{
	    strcpy(temp_ip, "::ffff:");
	    strcat(temp_ip, ip);
	    ip = temp_ip;
	}

	inet_pton(AF_INET6, address, &in_address);
#else
        inet_aton(address, &in_address);
#endif

        if(ip != NULL)
#ifdef WITH_IPV6
	    inet_pton(AF_INET6, ip, &in_ip);
#else
            inet_aton(ip, &in_ip);
#endif

        if(netmask != NULL)
        {
#ifdef WITH_IPV6
	    in_netmask = _mio_netmask_to_ipv6(netmask);

	    if(_mio_compare_ipv6(&in_address, &in_ip, in_netmask))
#else
            inet_aton(netmask, &in_netmask);
            if((in_address.s_addr & in_netmask.s_addr) == (in_ip.s_addr & in_netmask.s_addr))
#endif
            { /* this ip is in the allow network */
                return 1;
            }
        }
        else
        {
#ifdef WITH_IPV6
	    if(_mio_compare_ipv6(&in_ip, &in_address, 128))
#else
            if(in_ip.s_addr == in_address.s_addr)
#endif
                return 2; /* exact matches hold greater weight */
        }
    }

    /* deny the rest */
    return 0;
}

int _mio_deny_check(const char *address)
{
#ifdef WITH_IPV6
    char temp_address[INET6_ADDRSTRLEN];
    char temp_ip[INET6_ADDRSTRLEN];
    static struct in_addr tmpa;
#endif

    xmlnode io = xmlnode_get_tag(greymatter__, "io");
    xmlnode cur;

#ifdef WITH_IPV6
    if (inet_pton(AF_INET, address, &tmpa)) {
	strcpy(temp_address, "::ffff:");
	strcat(temp_address, address);
	address = temp_address;
    }
#endif

    if(xmlnode_get_tag(io, "deny") == NULL)
        return 0; /* if there is no allow section, allow all */

    for(cur = xmlnode_get_firstchild(io); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        char *ip, *netmask;
#ifdef WITH_IPV6
	struct in6_addr in_address, in_ip;
	int in_netmask;
#else
        struct in_addr in_address, in_ip, in_netmask;
#endif

        if(xmlnode_get_type(cur) != NTYPE_TAG)
            continue;

        if(j_strcmp(xmlnode_get_name(cur), "deny") != 0) 
            continue;

        ip = xmlnode_get_tag_data(cur, "ip");
        netmask = xmlnode_get_tag_data(cur, "mask");

        if(ip == NULL)
            continue;

#ifdef WITH_IPV6
	if (inet_pton(AF_INET, ip, &tmpa))
	{
	    strcpy(temp_ip, ":ffff:");
	    strcat(temp_ip, ip);
	    ip = temp_ip;
	}

	inet_pton(AF_INET6, address, &in_address);
#else
        inet_aton(address, &in_address);
#endif

        if(ip != NULL)
#ifdef WITH_IPV6
	    inet_pton(AF_INET6, ip, &in_ip);
#else
            inet_aton(ip, &in_ip);
#endif

        if(netmask != NULL)
        {
#ifdef WITH_IPV6
	    in_netmask = _mio_netmask_to_ipv6(netmask);

	    if (_mio_compare_ipv6(&in_address, &in_ip, in_netmask))
#else
            inet_aton(netmask, &in_netmask);
            if((in_address.s_addr & in_netmask.s_addr) == (in_ip.s_addr & in_netmask.s_addr))
#endif
            { /* this ip is in the deny network */
                return 1;
            }
        }
        else
        {
#ifdef WITH_IPV6
	    if(_mio_compare_ipv6(&in_ip, &in_address, 128))
#else
            if(in_ip.s_addr == in_address.s_addr)
#endif
                return 2; /* must be an exact match, if no netmask */
        }
    }

    return 0;
}

/*
 * callback for Heartbeat, increments karma, and signals the
 * select loop, whenever a socket's punishment is over
 */
result _karma_heartbeat(void*arg)
{
    mio cur;

    /* if there is nothing to do, just return */
    if(mio__data == NULL || mio__data->master__list == NULL) 
        return r_DONE;

    /* loop through the list, and add karma where appropriate */
    for(cur = mio__data->master__list; cur != NULL; cur = cur->next)
    {
        if(cur->k.dec != 0)
        { /* Karma is enabled for this connection */
            int was_negative = 0;
            /* don't update if we are closing, or pre-initilized */
            if(cur->state == state_CLOSE || cur->k.init == 0) 
                continue;
     
            /* if we are being punished, set the flag */
            if(cur->k.val < 0) was_negative = 1; 
     
            /* possibly increment the karma */
            karma_increment( &cur->k );
     
            /* punishment is over */
            if(was_negative && cur->k.val >= 0)  
            {
               log_debug(ZONE, "Punishment Over for socket %d: ", cur->fd);
               pth_write(mio__data->zzz[1]," ",1);
            }
        }
    }

    /* always return r_DONE, to keep getting heartbeats */
    return r_DONE;
}

/* 
 * unlinks a socket from the master list 
 */
void _mio_unlink(mio m)
{
    if(mio__data == NULL) 
        return;

    if(mio__data->master__list == m)
       mio__data->master__list = mio__data->master__list->next;

    if(m->prev != NULL) 
        m->prev->next = m->next;

    if(m->next != NULL) 
        m->next->prev = m->prev;

}

/* 
 * links a socket to the master list 
 */
void _mio_link(mio m)
{
    if(mio__data == NULL) 
        return;

    m->next = mio__data->master__list;
    m->prev = NULL;

    if(mio__data->master__list != NULL) 
        mio__data->master__list->prev = m;

    mio__data->master__list = m;
   
}

/* 
 * Dump this socket's write queue.  tries to write
 * as much of the write queue as it can, before the
 * write call would block the server
 * returns -1 on error, 0 on success, and 1 if more data to write
 */
int _mio_write_dump(mio m)
{
    int len;
    mio_wbq cur;

    /* try to write as much as we can */
    while(m->queue != NULL)
    {
        cur = m->queue;

        log_debug(ZONE, "write_dump writing data: %.*s", cur->len, cur->cur);

        /* write a bit from the current buffer */
        len = (*m->mh->write)(m, cur->cur, cur->len);
        /* we had an error on the write */
        if(len == 0)
        {
            if(m->cb != NULL)
                (*(mio_std_cb)m->cb)(m, MIO_ERROR, m->cb_arg);
            return -1;
        }
        if(len < 0)
        { 
            /* if we have an error, that isn't a blocking issue */ 
            if(errno != EWOULDBLOCK && errno != EINTR && errno != EAGAIN &&
               mio__errno != EAGAIN)
            { 
                /* bounce the queue */
                if(m->cb != NULL)
                    (*(mio_std_cb)m->cb)(m, MIO_ERROR, m->cb_arg);
                return -1;
            }
            return 1;
        }
        /* we didnt' write it all, move the current buffer up */
        else if(len < cur->len)
        { 

            cur->cur += len;
            cur->len -= len;
            return 1;
        } 
        /* we wrote the entire node, kill it and move on */
        else
        {  
            m->queue = m->queue->next;

            if(m->queue == NULL)
                m->tail = NULL;

            pool_free(cur->p);
        }
    } 
    return 0;
}

/* 
 * internal close function 
 * does a final write of the queue, bouncing and freeing all memory
 */
void _mio_close(mio m)
{
    int ret = 0;
    xmlnode cur;

    /* ensure that the state is set to CLOSED */
    m->state = state_CLOSE;

    /* take it off the master__list */
    _mio_unlink(m);

    /* try to write what's in the queue */
    if(m->queue != NULL) 
        ret = _mio_write_dump(m);

    if(ret == 1) /* still more data, bounce it all */
        if(m->cb != NULL)
            (*(mio_std_cb)m->cb)(m, MIO_ERROR, m->cb);

    /* notify of the close */
    if(m->cb != NULL)
        (*(mio_std_cb)m->cb)(m, MIO_CLOSED, m->cb_arg);

    /* close the socket, and free all memory */
    close(m->fd);
    

    if(m->rated) 
        jlimit_free(m->rate);

    pool_free(m->mh->p);

    /* cleanup the write queue */
    while((cur = mio_cleanup(m)) != NULL)
        xmlnode_free(cur);

    pool_free(m->p);

    log_debug(ZONE,"freed MIO socket");
}

/* 
 * accept an incoming connection from a listen sock 
 */
mio _mio_accept(mio m)
{
#ifdef WITH_IPV6
    struct sockaddr_in6 serv_addr;
    char addr_str[INET6_ADDRSTRLEN];
#else
    struct sockaddr_in serv_addr;
#endif
    size_t addrlen = sizeof(serv_addr);
    int fd;
    int allow, deny;
    mio new;

    log_debug(ZONE, "_mio_accept calling accept on fd #%d", m->fd);

    /* pull a socket off the accept queue */
    fd = (*m->mh->accept)(m, (struct sockaddr*)&serv_addr, (socklen_t*)&addrlen);
    if(fd <= 0)
    {
        return NULL;
    }

#ifdef WITH_IPV6
    allow = _mio_allow_check(inet_ntop(AF_INET6, &serv_addr.sin6_addr, addr_str, sizeof(addr_str)));
    deny = _mio_deny_check(addr_str);
#else
    allow = _mio_allow_check(inet_ntoa(serv_addr.sin_addr));
    deny  = _mio_deny_check(inet_ntoa(serv_addr.sin_addr));
#endif

    if(deny >= allow)
    {
#ifdef WITH_IPV6
	log_warn("mio", "%s was denied access, due to the allow list of IPs", addr_str);
#else
        log_warn("mio", "%s was denied access, due to the allow list of IPs", inet_ntoa(serv_addr.sin_addr));
#endif
        close(fd);
        return NULL;
    }
    int maxFDSET= mio__data->limit_max_fd;
    if (fd > maxFDSET)
    {
        log_warn("io_select", "%s(%d) is rejected - the server limit = %d (xml preference io:limit:mx_fd) has been exceeded ", inet_ntoa(serv_addr.sin_addr), fd, maxFDSET);
        close(fd);
	return NULL;
   } 


/* make sure that we aren't rate limiting this IP */
#ifdef WITH_IPV6
    if(m->rated && jlimit_check(m->rate, addr_str, 1))
    {
	log_warn("io_select", "%s(%d) is being connection rate limited - the connection attempts from this IP exceed the rate limit defined in jabberd config", addr_str, fd);
#else
    if(m->rated && jlimit_check(m->rate, inet_ntoa(serv_addr.sin_addr), 1))
    {
        log_warn("io_select", "%s(%d) is being connection rate limited - the connection attempts from this IP exceed the rate limit defined in jabberd config", inet_ntoa(serv_addr.sin_addr), fd);
#endif
        close(fd);
        return NULL;
    }

#ifdef WITH_IPV6
    log_debug(ZONE, "new socket accepted (fd: %d, ip%s, port: %d)", fd, addr_str, ntohs(serv_addr.sin6_port));
#else
    log_debug(ZONE, "new socket accepted (fd: %d, ip: %s, port: %d)", fd, inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
#endif

    /* create a new sock object for this connection */
    new      = mio_new(fd, m->cb, m->cb_arg, mio_handlers_new(m->mh->read, m->mh->write, m->mh->parser));
#ifdef WITH_IPV6
    new->ip  = pstrdup(new->p, addr_str);
#else
    new->ip  = pstrdup(new->p, inet_ntoa(serv_addr.sin_addr));
#endif
#ifdef HAVE_SSL
    new->ssl = m->ssl;
    
    /* XXX temas:  This is so messy, but I can't see a better way since I can't
     *             hook into the mio_cleanup routines.  MIO still needs some
     *             work.
     */
    pool_cleanup(new->p, _mio_ssl_cleanup, (void *)new->ssl);
#endif

    mio_karma2(new, &m->k);

    if(m->cb != NULL)
        (*(mio_std_cb)new->cb)(new, MIO_NEW, new->cb_arg);

    return new;
}

/* raise a signal on the connecting thread to time it out */
result _mio_connect_timeout(void *arg)
{
    connect_data cd = (connect_data)arg;

    if(cd->connected)
    {
        pool_free(cd->p);
        return r_UNREG;
    }

    log_debug(ZONE, "mio_connect taking too long connecting to %s, signaling to stop", cd->ip);
    if(cd->t != NULL)
        pth_raise(cd->t, SIGUSR2);

    return r_DONE; /* loop again */
}

void _mio_connect(void *arg)
{
    connect_data cd = (connect_data)arg;
#ifdef WITH_IPV6
    struct sockaddr_in6 sa;
    struct in6_addr	*saddr;
#else
    struct sockaddr_in sa;
    struct in_addr     *saddr;
#endif
    int                flag = 1,
                       flags;
    mio                new;
    pool               p;
    sigset_t           set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    pth_sigmask(SIG_BLOCK, &set, NULL);


    bzero((void*)&sa, sizeof(sa));

    /* create the new mio object, can't call mio_new.. don't want it in select yet */
    p           = pool_new();
    new         = pmalloco(p, sizeof(_mio));
    new->p      = p;
    new->type   = type_NORMAL;
    new->state  = state_ACTIVE;
    new->ip     = pstrdup(p,cd->ip);
    new->cb     = (void*)cd->cb;
    new->cb_arg = cd->cb_arg;
    mio_set_handlers(new, cd->mh);

    /* create a socket to connect with */
#ifdef WITH_IPV6
    new->fd = socket(PF_INET6, SOCK_STREAM,0);
#else
    new->fd = socket(PF_INET, SOCK_STREAM,0);
#endif

    /* set socket options */
    if(new->fd < 0 || setsockopt(new->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) < 0)
    {
        if(cd->cb != NULL)
            (*(mio_std_cb)cd->cb)(new, MIO_CLOSED, cd->cb_arg);
        cd->connected = -1;

        mio_handlers_free(new->mh);
        if(new->fd > 0)
            close(new->fd);
        pool_free(p);
        return;
    }

    /* optionally bind to a local address */
    if(xmlnode_get_tag_data(greymatter__, "io/bind") != NULL)
    {
#ifdef WITH_IPV6
	struct sockaddr_in6 sa;
	char *addr_str = xmlnode_get_tag_data(greymatter__, "io/bind");
	char temp_addr[INET6_ADDRSTRLEN];
	struct in_addr tmp;

	if (inet_pton(AF_INET, addr_str, &tmp))
	{
	    strcpy(temp_addr, "::ffff:");
	    strcat(temp_addr, addr_str);
	    addr_str = temp_addr;
	}

	sa.sin6_family = AF_INET6;
	sa.sin6_port = 0;
	sa.sin6_flowinfo = 0;

	inet_pton(AF_INET6, addr_str, &sa.sin6_addr);
#else
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port   = 0;
        inet_aton(xmlnode_get_tag_data(greymatter__, "io/bind"), &sa.sin_addr);
#endif
        bind(new->fd, (struct sockaddr*)&sa, sizeof(sa));
    }

#ifdef WITH_IPV6
    saddr = make_addr_ipv6(cd->ip);
#else
    saddr = make_addr(cd->ip);
#endif
    if(saddr == NULL)
    {
        if(cd->cb != NULL)
            (*(mio_std_cb)cd->cb)(new, MIO_CLOSED, cd->cb_arg);
        cd->connected = -1;

        mio_handlers_free(new->mh);
        if(new->fd > 0)
            close(new->fd);
        pool_free(p);
        return;
    }

#ifdef WITH_IPV6
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(cd->port);
    sa.sin6_addr = *saddr;
#else
    sa.sin_family = AF_INET;
    sa.sin_port = htons(cd->port);
    sa.sin_addr.s_addr = saddr->s_addr;
#endif

    log_debug(ZONE, "calling the connect handler for mio object %X", new);
    if((*cd->cf)(new, (struct sockaddr*)&sa, sizeof sa) < 0)
    {
        if(cd->cb != NULL)
            (*(mio_std_cb)cd->cb)(new, MIO_CLOSED, cd->cb_arg);
        cd->connected = -1;

        if(new->fd > 0)
            close(new->fd);
        mio_handlers_free(new->mh);
        pool_free(p);
        return;
    }

    /* set the socket to non-blocking */
    flags =  fcntl(new->fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(new->fd, F_SETFL, flags);

    /* XXX pthreads race condition.. cd->connected may be checked in the timeout, and cd freed before these calls */

    /* set the default karma values */
    mio_karma2(new, mio__data->k);
    
    /* add to the select loop */
    _mio_link(new);
    cd->connected = 1; 

    /* notify the select loop */
    if(mio__data != NULL)
        pth_write(mio__data->zzz[1]," ",1);

    /* notify the client that the socket is born */
    if(new->cb != NULL)
        (*(mio_std_cb)new->cb)(new, MIO_NEW, new->cb_arg);

}

/* 
 * main select loop thread 
 */
void _mio_main(void *arg)
{
    fd_set      wfds,       /* fd set for current writes   */
                rfds,       /* fd set for current reads    */
                all_wfds,   /* fd set for all writes       */
                all_rfds;   /* fd set for all reads        */
    mio         cur,
                temp;    
    char        buf[8192]; /* max socket read buffer      */
    int         maxlen,
                len,
                retval,
                bcast=-1,
                maxfd=0;
#ifdef WITH_IPV6
    char	addr_str[INET6_ADDRSTRLEN];
#endif

    log_debug(ZONE, "MIO is starting up");

    log_debug(ZONE, "MIO FD_SETSIZE=%d", FD_SETSIZE);
    /* init the socket junk */
    maxfd = mio__data->zzz[0];
    FD_ZERO(&all_wfds);
    FD_ZERO(&all_rfds);

    /* the optional local broadcast receiver */
    if(xmlnode_get_tag(greymatter__,"io/announce") != NULL)
    {
        bcast = make_netsocket(j_atoi(xmlnode_get_attrib(xmlnode_get_tag(greymatter__,"io/announce"),"port"),5222),NULL,NETSOCKET_UDP);
        if(bcast < 0)
        {
            log_notice("mio","failed to create network announce handler socket");
        }else if(bcast > maxfd){
            maxfd = bcast;
        }
        log_debug(ZONE,"started announcement handler");
    }

    /* loop forever -- will only exit when
     * mio__data->master__list is NULL */
    while (1)
    {
        // reset the local errno
        mio__errno = 0;
        rfds = all_rfds;
        wfds = all_wfds;

        log_debug(ZONE,"mio while loop top");

        /* if we are closing down, exit the loop */
        if(mio__data->shutdown == 1 && mio__data->master__list == NULL)
            break;

        /* wait for a socket event */
        FD_SET(mio__data->zzz[0],&rfds); /* include our wakeup socket */
        if(bcast > 0)
            FD_SET(bcast,&rfds); /* optionally include our announcements socket */
        retval = pth_select(maxfd+1, &rfds, &wfds, NULL, NULL);
        /* if retval is -1, fd sets are undefined across all platforms */

        log_debug(ZONE,"mio while loop, working");

        /* reset maxfd, in case it changes */
        maxfd=mio__data->zzz[0];

        /* check our zzz */
        if(FD_ISSET(mio__data->zzz[0],&rfds))
        {
            while(pth_read(mio__data->zzz[0], buf, sizeof(buf)) > 0);
        }

        /* check our pending announcements */
        if(bcast > 0 && FD_ISSET(bcast,&rfds))
        {
#ifdef WITH_IPV6
	    struct sockaddr_in6 remote_addr;
#else
            struct sockaddr_in remote_addr;
#endif
            int addrlen = sizeof(remote_addr);
            xmlnode curx;
            curx = xmlnode_get_firstchild(xmlnode_get_tag(greymatter__,"io/announce"));
            /* XXX pth <1.4 doesn't have pth_* wrapper for recvfrom or sendto! */
            len = recvfrom(bcast,buf,sizeof(buf),0,(struct sockaddr*)&remote_addr,&addrlen);
#ifdef WITH_IPV6
            log_debug(ZONE,"ANNOUNCER: received some data from %s: %.*s",inet_ntop(AF_INET, &remote_addr.sin6_addr, addr_str, sizeof(addr_str)),len,buf);
#else
            log_debug(ZONE,"ANNOUNCER: received some data from %s: %.*s",inet_ntoa(remote_addr.sin_addr),len,buf);
#endif
            /* sending our data out */
            for(; curx != NULL; curx = xmlnode_get_nextsibling(curx))
            {
                if(xmlnode_get_type(curx) != NTYPE_TAG) continue;
                len = snprintf(buf,sizeof(buf),"%s",xmlnode2str(curx));
                log_debug(ZONE,"announcement packet: %.*s",len,buf);
                sendto(bcast,buf,len,0,(struct sockaddr*)&remote_addr,addrlen);
            }
        }

        /* loop through the sockets, check for stuff to do */
        for(cur = mio__data->master__list; cur != NULL;)
        {

            /* if this socket needs to close */
            if(cur->state == state_CLOSE)
            {
                temp = cur;
                cur = cur->next;
                FD_CLR(temp->fd, &all_rfds);
                FD_CLR(temp->fd, &all_wfds);
                _mio_close(temp);
                continue;
            }

            /* find the max fd */
            if(cur->fd > maxfd)
                maxfd = cur->fd;

            /* if the sock is not in the read set, and has good karma and we got a non read event,
             * or if we need to initialize this socket */
            if(cur->k.init == 0 || (!FD_ISSET(cur->fd, &all_rfds) && cur->k.val >= 0) )
            {
                if(cur->k.init == 0)
                {
                    /* set to intialized */
                    cur->k.init = 1;
                    log_debug(ZONE, "socket %d has been intialized with starting karma %d ", cur->fd, cur->k.val);
                }
                else
                {
                    /* reset the karma to restore val */
                    log_debug(ZONE, "socket %d has restore karma %d byte meter %d", cur->fd, cur->k.val, cur->k.bytes);
                }
                /* and make sure that they are in the read set */
                FD_SET(cur->fd,&all_rfds);
            }

            /* pause while the rest of jabberd catches up */
            pth_yield(NULL);

            if(retval == -1) 
            { /* we can't check anything else, and be XP on all platforms here.. */
                cur = cur->next;
                continue;
            }

            /* if this socket needs to be read from */
            if(FD_ISSET(cur->fd, &rfds)) /* do not read if select returned error */
            {
                /* new connection */
                if(cur->type == type_LISTEN)
                {
                    mio m = _mio_accept(cur);

                    /* this is needed here too */
                    if(cur->fd > maxfd)
                        maxfd = cur->fd;

                    if(m != NULL)
                    {
                        FD_SET(m->fd, &all_rfds);
                        if(m->fd > maxfd)
                            maxfd=m->fd;
                    }

                    cur = cur->next;
                    continue;
                }

                do
                {
                    maxlen = KARMA_READ_MAX(cur->k.val);

                    if(maxlen > 8191) maxlen = 8191;

                    len = (*(cur->mh->read))(cur, buf, maxlen);

                    /* if we had a bad read */
                    if(len == 0 && maxlen > 0)
                    { 
                        mio_close(cur);
                        continue; /* loop on the same socket to kill it for real */
                    }
                    else if(len < 0)
                    {
                        if(errno != EWOULDBLOCK && errno != EINTR && 
                           errno != EAGAIN && mio__errno != EAGAIN) 
                        {
                            /* kill this socket and move on */
                            mio_close(cur);
                            continue;  /* loop on the same socket to kill it for real */
                        }
                    }
                    else 
                    {
                        if(cur->k.dec != 0)
                        { /* karma is enabled */
                            karma_decrement(&cur->k, len);
                            /* Check if that socket ran out of karma */
                            if(cur->k.val <= 0)
                            { /* ran out of karma */
                                log_notice("MIO_XML_READ", "socket from %s is out of karma: traffic exceeds karma limits defined in jabberd configuration", cur->ip);
                                FD_CLR(cur->fd, &all_rfds); /* this fd is being punished */
                            }
                        }

                        buf[len] = '\0';

                        log_debug(ZONE, "MIO read from socket %d: %s", cur->fd, buf);
                        (*cur->mh->parser)(cur, buf, len);
                    }
                }
                while (mio__ssl_reread == 1);
            } 

            /* we could have gotten a bad parse, and want to close */
            if(cur->state == state_CLOSE)
            { /* loop again to close the socket */
                continue;
            }

            /* if we need to write to this socket */
            if((FD_ISSET(cur->fd, &wfds) || cur->queue != NULL))
            {   
                int ret;

                /* write the current buffer */
                ret = _mio_write_dump(cur);

                /* if an error occured */
                if(ret == -1)
                {
                    mio_close(cur);
                    continue; /* loop on the same socket to kill it for real */
                }
                /* if we are done writing */
                else if(ret == 0) 
                    FD_CLR(cur->fd, &all_wfds);
                /* if we still have more to write */
                else if(ret == 1)
                    FD_SET(cur->fd, &all_wfds);
            }

            /* we may have wanted the socket closed after this operation */
            if(cur->state == state_CLOSE)
                continue; /* loop on the same socket to kill it for real */

            /* check the next socket */
            cur = cur->next;
        } 
        
        /* XXX 
         * *tsbandit pokes jer: Why are we doing this again? i forget
         * yes, spin through the entire list again, 
         * otherwise you can't write to a socket 
         * from another socket's read call) if 
         * there are packets to be written, wait 
         * for a write slot */
        for(cur = mio__data->master__list; cur != NULL; cur = cur->next)
            if(cur->queue != NULL) FD_SET(cur->fd, &all_wfds);
            else FD_CLR(cur->fd, &all_wfds);
    }
}

static long _mio_scale_limit(xmlnode tag, const char *key)
{
    const char *scale = xmlnode_get_attrib(tag, "scale");
    long bytes = j_atoi(xmlnode_get_data(tag), 0);
    
    if(scale != NULL)
    {
        if(!strcasecmp(scale, "K"))
            bytes <<= 10;
        else if (!strcasecmp(scale, "M"))
            bytes <<= 20;
        else if (!strcasecmp(scale, "G"))
            bytes <<= 30;
        else
        {
            log_warn("mio", "xml preference %s[scale]=%s is invalid, setting to default", key, scale);
            bytes = 0;
        }
    }
    if((bytes < 0) || (bytes > LONG_MAX))
    {
        log_warn("mio", "xml preference %s=%ld is invalid, setting to default", key, bytes);
        bytes = 0;
    }
    return bytes;
}


/***************************************************\
*      E X T E R N A L   F U N C T I O N S          *
\***************************************************/

/* 
   starts the _mio_main() loop
*/
void mio_init(void)
{
    struct rlimit rl;
    pool p;
    pth_attr_t attr;
    xmlnode io = xmlnode_get_tag(greymatter__, "io");
    xmlnode karma = xmlnode_get_tag(io, "karma");
    xmlnode lims = xmlnode_get_tag(io, "stanza_limits");

#ifdef HAVE_SSL
    if(xmlnode_get_tag(io, "ssl") != NULL)
        mio_ssl_init(xmlnode_get_tag(io, "ssl"));
#endif

    if(mio__data == NULL)
    {
        register_beat(KARMA_HEARTBEAT, _karma_heartbeat, NULL);

        /* malloc our instance object */
        p            = pool_new();
        mio__data    = pmalloco(p, sizeof(_ios));
        mio__data->p = p;
        mio__data->k = karma_new(p);
        pipe(mio__data->zzz);
        pth_fdmode(mio__data->zzz[0], PTH_FDMODE_NONBLOCK);
        pth_fdmode(mio__data->zzz[1], PTH_FDMODE_NONBLOCK);

        /* start main accept/read/write thread */
        attr = pth_attr_new();
        pth_attr_set(attr,PTH_ATTR_JOINABLE,FALSE);
#ifdef __CYGWIN__
        pth_attr_set(attr,PTH_ATTR_STACK_SIZE, 128*1024);
#endif
        mio__data->t=pth_spawn(attr,(void*)_mio_main,NULL);
        pth_attr_destroy(attr);

        /* give time to init the signal handlers */
        pth_yield(NULL);
    }

    if(karma != NULL)
    {
        mio__data->k->val        = j_atoi(xmlnode_get_tag_data(karma, "init"), KARMA_INIT);
        mio__data->k->max         = j_atoi(xmlnode_get_tag_data(karma, "max"), KARMA_MAX);
        mio__data->k->inc         = j_atoi(xmlnode_get_tag_data(karma, "inc"), KARMA_INC);
        mio__data->k->dec         = j_atoi(xmlnode_get_tag_data(karma, "dec"), KARMA_DEC);
        mio__data->k->penalty     = j_atoi(xmlnode_get_tag_data(karma, "penalty"), KARMA_PENALTY);
        mio__data->k->restore     = j_atoi(xmlnode_get_tag_data(karma, "restore"), KARMA_RESTORE);
        mio__data->k->reset_meter = j_atoi(xmlnode_get_tag_data(karma, "resetmeter"), KARMA_RESETMETER);
    }
    mio__data->rate_t = j_atoi(xmlnode_get_attrib(xmlnode_get_tag(io, "rate"), "time"), 0);
    mio__data->rate_p = j_atoi(xmlnode_get_attrib(xmlnode_get_tag(io, "rate"), "points"), 0);

    if(lims != NULL)
    {
        mio__data->max_stanza_bytes  = _mio_scale_limit(xmlnode_get_tag(lims, "default"), "io:stanza_limit:default");
        mio__data->max_message_bytes = _mio_scale_limit(xmlnode_get_tag(lims, "message"), "io:stanza_limit:message"); 
        if(mio__data->max_message_bytes > mio__data->max_stanza_bytes)
        {
            log_warn("mio", "xml preference io:stanza_limit:message=%d is invalid, setting to default", mio__data->max_message_bytes);
            mio__data->max_message_bytes = 0;
        }
        if(mio__data->max_stanza_bytes)
            log_debug(ZONE, "default stanza limit is %ld; message limit is %d", mio__data->max_stanza_bytes, mio__data->max_message_bytes);
    }

    /* get the max # of file descriptors the process can use */
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    getrlimit(RLIMIT_NOFILE,  &rl);
	if(rl.rlim_max > FD_SETSIZE)
		rl.rlim_max = FD_SETSIZE;

    mio__data->limit_max_fd = j_atoi(xmlnode_get_tag_data(io, "max_connections"), (int)rl.rlim_max);
    if((mio__data->limit_max_fd > rl.rlim_max) || (mio__data->limit_max_fd < 0))
    {  
        log_warn("mio", "xml preference io:limit:max_fd=%d is invalid, setting to rlim_max=%lu", mio__data->limit_max_fd, (long unsigned)rl.rlim_max);
        mio__data->limit_max_fd = (int)rl.rlim_max ;
    }
}

/*
 * Cleanup function when server is shutting down, closes
 * all sockets, so that everything can be cleaned up
 * properly.
 */
void mio_stop(void)
{
    mio cur, mnext;

    log_debug(ZONE, "MIO is shutting down");

    /* no need to do anything if mio__data hasn't been used yet */
    if(mio__data == NULL) 
        return;

    /* flag that it is okay to exit the loop */
    mio__data->shutdown = 1;

    /* loop each socket, and close it */
    for(cur = mio__data->master__list; cur != NULL;)
    {
        mnext = cur->next;
        _mio_close(cur);
    	cur = mnext;
    }

    /* signal the loop to end */
    pth_abort(mio__data->t);

    pool_free(mio__data->p);
    mio__data = NULL;
}

/* 
   creates a new mio object from a file descriptor
*/
mio mio_new(int fd, void *cb, void *arg, mio_handlers mh)
{
    mio   new    =  NULL;
    pool  p      =  NULL;
    int   flags  =  0;

    if(fd <= 0) 
        return NULL;
    
    /* create the new MIO object */
    p           = pool_new();
    new         = pmalloco(p, sizeof(_mio));
    new->p      = p;
    new->type   = type_NORMAL;
    new->state  = state_ACTIVE;
    new->fd     = fd;
    new->cb     = (void*)cb;
    new->cb_arg = arg;
    mio_set_handlers(new, mh);

    /* set the default karma values */
    mio_karma2(new, mio__data->k);
    mio_rate(new, mio__data->rate_t, mio__data->rate_p);
    mio_stanza_limits(new, mio__data->max_stanza_bytes, mio__data->max_message_bytes);

    /* set the socket to non-blocking */
    flags =  fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    /* add to the select loop */
    _mio_link(new);

    /* notify the select loop */
    if(mio__data != NULL)
        pth_write(mio__data->zzz[1]," ",1);

    return new;
}

/*
   resets the callback function
*/
mio mio_reset(mio m, void *cb, void *arg)
{
    if(m == NULL) 
        return NULL;

    m->cb     = cb;
    m->cb_arg = arg;

    return m;
}

/* 
 * client call to close the socket 
 */
void mio_close(mio m) 
{
    if(m == NULL) 
        return;

    m->state = state_CLOSE;
    if(mio__data != NULL)
        pth_write(mio__data->zzz[1]," ",1);
}

/* 
 * writes a str, or xmlnode to the client socket 
 */
void mio_write(mio m, xmlnode x, char *buffer, int len)
{
    mio_wbq new;
    pool p;


    if(m == NULL) 
        return;

    /* if there is nothing to write */
    if(x == NULL && buffer == NULL)
    {
        log_debug("mio", "[%s] mio_write called without x or buffer", ZONE);
        return;
    }

    /* create the pool for this wbq */
    if(x != NULL)
        p = xmlnode_pool(x);
    else
        p = pool_new();

    /* create the wbq */
    new    = pmalloco(p, sizeof(_mio_wbq));
    new->p = p;

    /* set the queue item type */
    if(buffer != NULL)
    {
        new->type = queue_CDATA;

        if (len == -1)
            len = strlen(buffer);

        /* XXX more hackish code to print the stream header right on a NUL xmlnode socket */
        if(m->type == type_NUL && strncmp(buffer,"<?xml ",6) == 0)
        {
            new->data = pmalloco(p,len+2);
            memcpy(new->data,buffer,len);
            memcpy((new->data + len) - 1, "/>",3);
            len++;
            /* THIS WAS DUMB, I'm just leaving it here to remind me of how dumb it was :)
            sprintf(new->data,"%.*s/>",len-2,buffer); */
        }else{
            new->data = pmalloco(p,len+1);
            memcpy(new->data,buffer,len);
        }
    }
    else
    {
        new->type = queue_XMLNODE;
        if((new->data = xmlnode2str(x)) == NULL)
        {
            pool_free(p);
            return;
        }
        len = strlen(new->data);
    }

    /* include the \0 if we're special */
    if(m->type == type_NUL)
    {
        len++;
    }

    /* assign values */
    new->x    = x;
    new->cur  = new->data;

    new->len = len;

    /* put at end of queue */
    if(m->tail == NULL)
        m->queue = new;
    else
        m->tail->next = new;
    m->tail = new;

    log_debug(ZONE, "mio_write called on x: %X buffer: %.*s", x, len, buffer);
    /* notify the select loop that a packet needs writing */
    if(mio__data != NULL)
        pth_write(mio__data->zzz[1]," ",1);
}

/*
   sets karma values
*/
void mio_karma(mio m, int val, int max, int inc, int dec, int penalty, int restore)
{
    if(m == NULL)
       return;

    m->k.val     = val;
    m->k.max     = max;
    m->k.inc     = inc;
    m->k.dec     = dec;
    m->k.penalty = penalty;
    m->k.restore = restore;
}

void mio_karma2(mio m, struct karma *k)
{
    if(m == NULL)
       return;

    karma_copy(&m->k, k);
}

/*
   sets connection rate limits
*/
void mio_rate(mio m, int rate_time, int max_points)
{
    if(m == NULL || rate_time == 0) 
        return;

    m->rated = 1;
    if(m->rate != NULL)
        jlimit_free(m->rate);

    m->rate = jlimit_new(rate_time, max_points);
}

/*
   sets stanza size limits
*/
void mio_stanza_limits(mio m, long max_stanza, long max_msg)
{
    if(m == NULL) 
        return;

    m->max_stanza_bytes = max_stanza;
    m->max_message_bytes = max_msg;
    m->message = 0;
    m->bytes_read = 0;
}

/*
   pops the last xmlnode from the queue 
*/
xmlnode mio_cleanup(mio m)
{
    mio_wbq     cur;
    
    if(m == NULL || m->queue == NULL) 
        return NULL;

    /* find the first queue item with a xmlnode attached */
    for(cur = m->queue; cur != NULL;)
    {

        /* move the queue up */
        m->queue = cur->next;

        /* set the tail pointer if needed */
        if(m->queue == NULL)
            m->tail = NULL;

        /* if there is no node attached */
        if(cur->x == NULL)
        {
            /* just kill this item, and move on..
             * only pop xmlnodes 
             */
            mio_wbq next = m->queue;
            pool_free(cur->p);
            cur = next;
            continue;
        }

        /* and pop this xmlnode */
        return cur->x;
    }

    /* no xmlnodes found */
    return NULL;
}

/* 
 * request to connect to a remote host 
 */
void mio_connect(char *host, int port, void *cb, void *cb_arg, int timeout, mio_connect_func f, mio_handlers mh)
{
    connect_data cd = NULL;
    pool         p  = NULL;
    pth_attr_t   attr;

    /* verify data */
    if(host == NULL || port == 0) 
        return;

    if(timeout <= 0)
        timeout = 30; /* default timeout */

    if(f == NULL)
        f = MIO_RAW_CONNECT;

    if(mh == NULL)
        mh = mio_handlers_new(NULL, NULL, NULL);

    /* create the connect struct */
    p          = pool_new();
    cd         = pmalloco(p, sizeof(_connect_data));
    cd->p      = p;
    cd->ip     = pstrdup(p, host);
    cd->port   = port;
    cd->cb     = cb;
    cd->cb_arg = cb_arg;
    cd->cf     = f;
    cd->mh     = mh;

#ifdef WITH_IPV6
    if(!strchr(host,':'))
    {
	char *temp = pmalloco(p, strlen(host)+8);
	strcpy(temp, "::ffff:");
	strcat(temp, host);
	host = temp;
    }
#endif

    attr = pth_attr_new();
    pth_attr_set(attr,PTH_ATTR_JOINABLE,FALSE);
    cd->t      = pth_spawn(attr, (void*)_mio_connect, (void*)cd);
    pth_attr_destroy(attr);

    register_beat(timeout, _mio_connect_timeout, (void*)cd);

}

/* 
 * call to start listening with select 
 */
mio mio_listen(int port, char *listen_host, void *cb, void *arg, mio_accept_func f, mio_handlers mh)
{
    mio        new;
    int        fd;

    if(f == NULL)
        f = MIO_RAW_ACCEPT;

    if(mh == NULL)
        mh = mio_handlers_new(NULL, NULL, NULL);

    mh->accept = f;

    log_debug(ZONE, "io_select to listen on %d [%s]",port, listen_host);

    /* attempt to open a listening socket */
    fd = make_netsocket(port, listen_host, NETSOCKET_SERVER);

    /* if we got a bad fd we can't listen */
    if(fd < 0)
    {
        log_alert(NULL, "io_select unable to listen on %d [%s]: jabberd already running or invalid interface?", port, listen_host);
        return NULL;
    }

    /* start listening with a max accept queue of 10 */
    if(listen(fd, 10) < 0)
    {
        log_alert(NULL, "io_select unable to listen on %d [%s]: jabberd already running or invalid interface?", port, listen_host);
        return NULL;
    }

    /* create the sock object, and assign the values */
    new       = mio_new(fd, cb, arg, mh);
    new->type = type_LISTEN;
    new->ip   = pstrdup(new->p, listen_host);

    log_debug(ZONE, "io_select starting to listen on %d [%s]", port, listen_host);

    return new;
}

mio_handlers mio_handlers_new(mio_read_func rf, mio_write_func wf, mio_parser_func pf)
{
    pool p = pool_new();
    mio_handlers new;

    new = pmalloco(p, sizeof(_mio_handlers));

    new->p = p;

    /* yay! a chance to use the tertiary operator! */
    new->read   = rf ? rf : MIO_RAW_READ;
    new->write  = wf ? wf : MIO_RAW_WRITE;
    new->parser = pf ? pf : MIO_RAW_PARSER;

    return new;
}

void mio_handlers_free(mio_handlers mh)
{
    if(mh == NULL)
        return;

    pool_free(mh->p);
}

void mio_set_handlers(mio m, mio_handlers mh)
{
    mio_handlers old;

    if(m == NULL || mh == NULL)
        return;

    old = m->mh;
    m->mh = mh;

    mio_handlers_free(old);
}
