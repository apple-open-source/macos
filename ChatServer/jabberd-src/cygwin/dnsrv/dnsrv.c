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
#include "srv_resolv.h"
#include <sys/wait.h>

#ifdef __CYGWIN__
#include <process.h>
#endif

/* Config format:
   <dnsrv xmlns='jabber:config:dnsrv'>
      <resend service="_jabber._tcp">foo.org</resend>
      ...
   </dnsrv>

   Notes:
   * You must specify the services in the order you want them tried
*/



/* ------------------------------------------------- */
/* Struct to store list of services and resend hosts */
typedef struct __dns_resend_list
{
     char* service;
     char* host;
     struct __dns_resend_list* next;
} *dns_resend_list, _dns_resend_list;


/* --------------------------------------- */
/* Struct to keep track of a DNS coprocess */
typedef struct
{
     int             in;                 /* Inbound data handle */
     int             out;                /* Outbound data handle */
     int             pid;                /* Coprocess PID */
     HASHTABLE       packet_table; /* Hash of dns_packet_lists */
     int             packet_timeout; /* how long to keep packets in the queue */
     HASHTABLE       cache_table; /* Hash of resolved IPs */
     int             cache_timeout; /* how long to keep resolutions in the cache */
     pool            mempool;
     dns_resend_list svclist;
} *dns_io, _dns_io;

typedef int (*RESOLVEFUNC)(dns_io di);

/* ----------------------------------------------------------- */
/* Struct to store list of dpackets which need to be delivered */
typedef struct __dns_packet_list
{
     dpacket           packet;
     int               stamp;
     struct __dns_packet_list* next;
} *dns_packet_list, _dns_packet_list;



// ---------------------------------------------------------------------------
// This code is not used in cygwin
#ifndef __CYGWIN__


/* ----------------------- */
/* Coprocess functionality */
void dnsrv_child_process_xstream_io(int type, xmlnode x, void* args)
{
     dns_io di = (dns_io)args;
     char*  hostname;
     char*  str = NULL;
     dns_resend_list iternode = NULL;

     if (type == XSTREAM_NODE)
     {
          /* Get the hostname out... */
          hostname = xmlnode_get_data(x);
          log_debug(ZONE, "dnsrv: Recv'd lookup request for %s", hostname);
          if (hostname != NULL)
          {
               /* For each entry in the svclist, try and resolve using
                  the specified service and resend it to the specified host */
               iternode = di->svclist;
               while (iternode != NULL)
               {
                    str = srv_lookup(x->p, iternode->service, hostname);
                    if (str != NULL)
                    {
                         log_debug(ZONE, "Resolved %s(%s): %s\tresend to:%s", hostname, iternode->service, str, iternode->host);
                         xmlnode_put_attrib(x, "ip", str);
                         xmlnode_put_attrib(x, "to", iternode->host);
                         break;
                    }
                    iternode = iternode->next;
               }
               str = xmlnode2str(x);
               write(di->out, str, strlen(str));
          }
     }
     xmlnode_free(x);
}

int dnsrv_child_main(dns_io di)
{
     pool    p   = pool_new();
     xstream xs  = xstream_new(p, dnsrv_child_process_xstream_io, di);
     int     len;
     char    readbuf[1024];
     sigset_t sigs;


     sigemptyset(&sigs);
     sigaddset(&sigs, SIGHUP);
     sigprocmask(SIG_BLOCK, &sigs, NULL);

     log_debug(ZONE,"DNSRV CHILD: starting");

     /* Transmit stream header */
     write(di->out, "<stream>", 8);

     /* Loop forever, processing requests and feeding them to the xstream*/     
     while (1)
     {
       len = read(di->in, &readbuf, 1024);
       if (len <= 0)
       {
           log_debug(ZONE,"dnsrv: Read error on coprocess(%d): %d %s",getppid(),errno,strerror(errno));
           break;
       }

       log_debug(ZONE, "DNSRV CHILD: Read from buffer: %.*s",len,readbuf);

       if (xstream_eat(xs, readbuf, len) > XSTREAM_NODE)
       {
           log_debug(ZONE, "DNSRV CHILD: xstream died");
           break;
       }
     }

     /* child is out of loop... normal exit so parent will start us again */
     log_debug(ZONE, "DNSRV CHILD: out of loop.. exiting normal");
     pool_free(p);
     exit(0);
     return 0;
}

/* Core functionality */
int dnsrv_fork_and_capture(RESOLVEFUNC f, dns_io di)
{
     int left_fds[2], right_fds[2];
     int pid;

     /* Create left and right pipes */
     if (pipe(left_fds) < 0 || pipe(right_fds) < 0)
          return -1;

     pid = pth_fork();
     if (pid < 0)
          return -1;
     else if (pid > 0)          /* Parent */
     {
          /* Close unneeded file handles */
          close(left_fds[STDIN_FILENO]);
          close(right_fds[STDOUT_FILENO]);
          /* Return the in and out file descriptors */
          di->in = right_fds[STDIN_FILENO];
          di->out = left_fds[STDOUT_FILENO];
          return pid;
     }
     else                       /* Child */
     {
          /* Close unneeded file handles */
          pth_kill();
          close(left_fds[STDOUT_FILENO]);
          close(right_fds[STDIN_FILENO]);
          /* Start the specified function, passing the in/out descriptors */
          di->in = left_fds[STDIN_FILENO]; di->out = right_fds[STDOUT_FILENO];
          return (*f)(di);
     }
}

#endif





// ---------------------------------------------------------------------------
// Replacement functions for cygwin

#ifdef __CYGWIN__

/**
 * Dummy dnsrv_child_main
 */
int dnsrv_child_main(dns_io di) {
  return 0;
}


/**
 * Spawn a separate process for ADNS 
 */
int dnsrv_fork_and_capture(RESOLVEFUNC f, dns_io di)
{
  int pid;
  int childToParent[2];
  int parentToChild[2];
  char childWriteHandle[10];  
  char childReadHandle[10];
  char debugging[10];
  int READ=0;
  int WRITE=1;
  
  // create pipes for communication with child
  pipe(childToParent);
  pipe(parentToChild);

  // convert the handles as they should be seen by the child to 
  // strings so we can pass them as arguments to it
  sprintf(childWriteHandle, "%i", childToParent[WRITE]);
  sprintf(childReadHandle, "%i", parentToChild[READ]);
  sprintf(debugging, "%i", get_debug_flag());
  
  // try and spawn the child process
  pid = spawnlp(_P_NOWAIT, "jabadns", "jabadns", 
                childReadHandle, childWriteHandle, debugging, NULL);
  if (pid < 0) {
    return -1;
  }
  
  // setup di structure
  di->pid = pid;
  di->in = childToParent[READ];
  di->out = parentToChild[WRITE];
  
  // OK!
  return pid;
}

#endif





void dnsrv_resend(xmlnode pkt, char *ip, char *to)
{
    if(ip != NULL)
    {
         pkt = xmlnode_wrap(pkt,"route");
         xmlnode_put_attrib(pkt, "to", to);
         xmlnode_put_attrib(pkt, "ip", ip);
    }else{
         jutil_error(pkt, (terror){502, "Unable to resolve hostname."});
         xmlnode_put_attrib(pkt, "iperror", "");
    }
    deliver(dpacket_new(pkt),NULL);
}


/* Hostname lookup requested */
void dnsrv_lookup(dns_io d, dpacket p)
{
    dns_packet_list l, lnew;
    xmlnode req;
    char *reqs;

    /* make sure we have a child! */
    if(d->out <= 0)
    {
        deliver_fail(p, "DNS Resolver Error");
        return;
    }

    /* Attempt to lookup this hostname in the packet table */
    l = (dns_packet_list)ghash_get(d->packet_table, p->host);

    /* IF: hashtable has the hostname, a lookup is already pending,
       so push the packet on the top of the list (most recent at the top) */
    if (l != NULL)
    {
         log_debug(ZONE, "dnsrv: Adding lookup request for %s to pending queue.", p->host);
         lnew = pmalloco(p->p, sizeof(_dns_packet_list));
         lnew->packet = p;
         lnew->stamp = time(NULL);
         lnew->next = l;
         ghash_put(d->packet_table, p->host, lnew);
         return;
    }

    /* insert the packet into the packet_table using the hostname
       as the key and send a request to the coprocess */
    log_debug(ZONE, "dnsrv: Creating lookup request queue for %s", p->host);
    l = pmalloco(p->p, sizeof(_dns_packet_list));
    l->packet = p;
    l->stamp  = time(NULL);
    ghash_put(d->packet_table, p->host, l);
    req = xmlnode_new_tag_pool(p->p,"host");
    xmlnode_insert_cdata(req,p->host,-1);

    reqs = xmlnode2str(req);
    log_debug(ZONE, "dnsrv: Transmitting lookup request: %s", reqs);
    pth_write(d->out, reqs, strlen(reqs));
}


result dnsrv_deliver(instance i, dpacket p, void* args)
{
     dns_io di = (dns_io)args;
     xmlnode c;
     int timeout = di->cache_timeout;
     char *ip;
     jid to;

     /* if we get a route packet, it has to be to *us* and have the child as the real packet */
     if(p->type == p_ROUTE)
     {
        if(j_strcmp(p->host,i->id) != 0 || (to = jid_new(p->p,xmlnode_get_attrib(xmlnode_get_firstchild(p->x),"to"))) == NULL)
            return r_ERR;
        p->x=xmlnode_get_firstchild(p->x);
        p->id = to;
        p->host = to->server;
     }

     /* Ensure this packet doesn't already have an IP */
     if(xmlnode_get_attrib(p->x, "ip") || xmlnode_get_attrib(p->x, "iperror"))
     {
        log_notice(p->host, "dropping looping dns lookup request: %s", xmlnode2str(p->x));
        xmlnode_free(p->x);
        return r_DONE;
     }

     /* try the cache first */
     if((c = ghash_get(di->cache_table, p->host)) != NULL)
     {
         /* if there's no IP, cached failed lookup, time those out 10 times faster! (weird, I know, *shrug*) */
         if((ip = xmlnode_get_attrib(c,"ip")) == NULL)
            timeout = timeout / 10;
         if((time(NULL) - (int)xmlnode_get_vattrib(c,"t")) > timeout)
         { /* timed out of the cache, lookup again */
             xmlnode_free(c);
             ghash_remove(di->cache_table,p->host);
         }else{
             /* yay, send back right from the cache */
             dnsrv_resend(p->x, ip, xmlnode_get_attrib(c,"to"));
             return r_DONE;
         }
     }

    dnsrv_lookup(di, p);
    return r_DONE;
}

void dnsrv_process_xstream_io(int type, xmlnode x, void* arg)
{
     dns_io di            = (dns_io)arg;
     char* hostname       = NULL;
     char* ipaddr         = NULL;
     char* resendhost     = NULL;
     dns_packet_list head = NULL;
     dns_packet_list heado = NULL;

     /* Node Format: <host ip="201.83.28.2">foo.org</host> */
     if (type == XSTREAM_NODE)
     {    
          log_debug(ZONE,"incoming resolution: %s",xmlnode2str(x));
          hostname = xmlnode_get_data(x);

          /* whatever the response was, let's cache it */
          xmlnode_free((xmlnode)ghash_get(di->cache_table,hostname)); /* free any old cache, shouldn't ever be any */
          xmlnode_put_vattrib(x,"t",(void*)time(NULL));
          ghash_put(di->cache_table,hostname,(void*)x);

          /* Get the hostname and look it up in the hashtable */
          head = ghash_get(di->packet_table, hostname);
          /* Process the packet list */
          if (head != NULL)
          {
               ipaddr = xmlnode_get_attrib(x, "ip");
               resendhost = xmlnode_get_attrib(x, "to");

               /* Remove the list from the hashtable */
               ghash_remove(di->packet_table, hostname);
               
               /* Walk the list and insert IPs */
               while(head != NULL)
               {
                    heado = head;
                    /* Move to next.. */
                    head = head->next;
                    /* Deliver the packet */
                    dnsrv_resend(heado->packet->x, ipaddr, resendhost);
               }
          }
          /* Host name was not found, something is _TERRIBLY_ wrong! */
          else
               log_debug(ZONE, "Resolved unknown host/ip request: %s\n", xmlnode2str(x));

          return; /* we cached x above, so we don't free it below :) */
     }
     xmlnode_free(x);
} 

void* dnsrv_process_io(void* threadarg)
{
     /* Get DNS IO info */
     dns_io di = (dns_io)threadarg;
     int  retcode       = 0;
     int  pid           = 0;
     int  readlen       = 0;
     char readbuf[1024];
     xstream  xs       = NULL;       
     sigset_t sigs;
     #ifdef __CYGWIN__
     dns_resend_list iternode = NULL;
     #endif


     sigemptyset(&sigs);
     sigaddset(&sigs, SIGHUP);
     sigprocmask(SIG_BLOCK, &sigs, NULL);

     /* Allocate an xstream for talking to the process */
     xs = xstream_new(di->mempool, dnsrv_process_xstream_io, di);

     /* Transmit root element to coprocess */
     pth_write(di->out, "<stream>", 8);

     /* Transmit resend entries to coprocess */
     #ifdef __CYGWIN__
     iternode = di->svclist;
     while (iternode != NULL) {
       if (iternode->service) {
         sprintf(readbuf, "<resend service=\"%s\">%s</resend>",
                 iternode->service, iternode->host);
       } else {
         sprintf(readbuf, "<resend>%s</resend>",
                 iternode->host);
       }
       pth_write(di->out, readbuf, strlen(readbuf));
       iternode = iternode->next;
     }
     #endif

     /* Loop forever */
     while (1)
     {
       /* Hostname lookup completed from coprocess */
       readlen = pth_read(di->in, readbuf, sizeof(readbuf));
       if (readlen <= 0)
       {
           log_debug(ZONE,"dnsrv: Read error on coprocess: %d %s",errno,strerror(errno));
           break;
       }

       if (xstream_eat(xs, readbuf, readlen) > XSTREAM_NODE)
           break;
     }

     /* If we reached this point, the coprocess probably is dead, so 
        process the SIG_CHLD */
     pid = pth_waitpid(di->pid, &retcode, 0);

     if(pid == -1)
     {
        log_debug(ZONE, "pth_waitpid returned -1: %s", strerror(errno));
     }
     else if(pid == 0)
     {
        log_debug(ZONE, "no child available to call waitpid on");
     }
     else
     {
        log_debug(ZONE, "pid %d, exit status: %d", pid, WEXITSTATUS(retcode));
     }

     /* Cleanup */
     close(di->in);
     close(di->out);
     di->out = 0;

     log_debug(ZONE,"child returned %d",WEXITSTATUS(retcode));

     if(WIFEXITED(retcode)) /* if the child exited normally */
     {
        log_debug(ZONE, "child being restarted...");
        /* Fork out resolver function/process */
        di->pid = dnsrv_fork_and_capture(dnsrv_child_main, di);

        /* Start IO thread */
        pth_spawn(PTH_ATTR_DEFAULT, dnsrv_process_io, (void*)di);
        return NULL;
     }

     log_debug(ZONE, "child dying...");
     return NULL;
}

void *dnsrv_thread(void *arg)
{
     dns_io di=(dns_io)arg;
     /* Fork out resolver function/process */
     di->pid = dnsrv_fork_and_capture(dnsrv_child_main, di);
     return NULL;
}

void dnsrv_shutdown(void *arg)
{
     dns_io di=(dns_io)arg;
     ghash_destroy(di->packet_table);

     /* spawn a thread that get's forked, and wait for it since it sets up the fd's */
}

/* callback for walking the connecting hash tree */
int _dnsrv_beat_packets(void *arg, const void *key, void *data)
{
    dns_io di = (dns_io)arg;
    dns_packet_list n, l = (dns_packet_list)data;
    int now = time(NULL);
    int reap = 0;

    /* first, check the head */
    if((now - l->stamp) > di->packet_timeout)
    {
        log_notice(l->packet->host,"timed out from dnsrv queue");
        ghash_remove(di->packet_table,l->packet->host);
        reap = 1;
    }else{
        while(l->next != NULL)
        {
            if((now - l->next->stamp) > di->packet_timeout)
            {
                reap = 1;
                n = l->next;
                l->next = NULL; /* chop off packets to be killed */
                l = n;
                break;
            }
            l = l->next;
        }
    }

    if(reap == 0) return 1;

    /* time out individual queue'd packets */
    while(l != NULL)
    {
        n = l->next;
        deliver_fail(l->packet,"Hostname Resolution Timeout");
        l = n;
    }

    return 1;
}

result dnsrv_beat_packets(void *arg)
{
    dns_io di = (dns_io)arg;
    ghash_walk(di->packet_table,_dnsrv_beat_packets,arg);
    return r_DONE;
}


void dnsrv(instance i, xmlnode x)
{
     xdbcache xc = NULL;
     xmlnode  config = NULL;
     xmlnode  iternode   = NULL;
     dns_resend_list tmplist = NULL;

     /* Setup a struct to hold dns_io handles */
     dns_io di;
     di = pmalloco(i->p, sizeof(_dns_io));

     di->mempool = i->p;

     /* Load config from xdb */
     xc = xdb_cache(i);
     config = xdb_get(xc, jid_new(xmlnode_pool(x), "config@-internal"), "jabber:config:dnsrv");

     /* Build a list of services/resend hosts */
     iternode = xmlnode_get_lastchild(config);
     while (iternode != NULL)
     {
          if (j_strcmp("resend", xmlnode_get_name(iternode)) != 0)
          {
               iternode = xmlnode_get_prevsibling(iternode);
               continue;
          }

          /* Allocate a new list node */
          tmplist = pmalloco(di->mempool, sizeof(_dns_resend_list));
          tmplist->service = pstrdup(di->mempool, xmlnode_get_attrib(iternode, "service"));
          tmplist->host    = pstrdup(di->mempool, xmlnode_get_data(iternode));
          /* Insert this node into the list */
          tmplist->next = di->svclist;    
          di->svclist = tmplist;
          /* Move to next child */
          iternode = xmlnode_get_prevsibling(iternode);
     }
     log_debug(ZONE, "dnsrv debug: %s\n", xmlnode2str(config));

     /* Setup the hash of dns_packet_list */
     di->packet_table = ghash_create(j_atoi(xmlnode_get_attrib(config,"queuemax"),101), (KEYHASHFUNC)str_hash_code, (KEYCOMPAREFUNC)j_strcmp);
     di->packet_timeout = j_atoi(xmlnode_get_attrib(config,"queuetimeout"),60);
     register_beat(di->packet_timeout, dnsrv_beat_packets, (void *)di);


     /* Setup the internal hostname cache */
     di->cache_table = ghash_create(j_atoi(xmlnode_get_attrib(config,"cachemax"),1999), (KEYHASHFUNC)str_hash_code, (KEYCOMPAREFUNC)j_strcmp);
     di->cache_timeout = j_atoi(xmlnode_get_attrib(config,"cachetimeout"),3600); /* 1 hour dns cache? XXX would be nice to get the right value from dns! */

     xmlnode_free(config);

     /* spawn a thread that get's forked, and wait for it since it sets up the fd's */
     pth_join(pth_spawn(PTH_ATTR_DEFAULT,(void*)dnsrv_thread,(void*)di),NULL);

     if(di->pid < 0)
     {
         log_error(i->id,"dnsrv failed to start, unable to fork and/or create pipes");
         return;
     }

     /* Start IO thread */
     pth_spawn(PTH_ATTR_DEFAULT, dnsrv_process_io, di);

     /* Register an incoming packet handler */
     register_phandler(i, o_DELIVER, dnsrv_deliver, (void*)di);
     /* register a cleanup function */
     pool_cleanup(i->p, dnsrv_shutdown, (void*)di);
}
