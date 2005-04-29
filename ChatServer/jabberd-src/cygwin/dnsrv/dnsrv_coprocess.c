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



int debug_flag;

int get_debug_flag() {
  return debug_flag;
}

void set_debug_flag(int v) {
  debug_flag = v;
}
                   

/* ----------------------- */
/* Coprocess functionality */
void dnsrv_child_process_xstream_io(int type, xmlnode x, void* args)
{
     dns_io di = (dns_io)args;
     char*  hostname;
     char* service;
     char*  str = NULL;
     dns_resend_list iternode = NULL;
     dns_resend_list tmplist = NULL;

     if (type == XSTREAM_NODE) {
       // is this an entry in the svclist? or a normal resolve query
       if (!strcmp(x->name, "resend")) {
         service = xmlnode_get_attrib(x, "service");
         hostname = xmlnode_get_data(x);
         if (hostname != NULL) {
           log_debug(NULL, "dnsrv: Recv'd service entry: %s (%s)", 
                     service ? service : "<None>", 
                     hostname);

           // add it in
           tmplist = pmalloco(di->mempool, sizeof(_dns_resend_list));
           tmplist->service = pstrdup(di->mempool, service);
           tmplist->host = pstrdup(di->mempool, hostname);
           tmplist->next = di->svclist;
           di->svclist = tmplist;
         }
       } else {
         /* Get the hostname out... */
         hostname = xmlnode_get_data(x);
         log_debug(NULL, "dnsrv: Recv'd lookup request for %s", hostname);
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
                      log_debug(NULL, "Resolved %s(%s): %s\tresend to:%s", hostname, iternode->service, str, iternode->host);
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


     // store the pool in di
     di->mempool = p;

     sigemptyset(&sigs);
     sigaddset(&sigs, SIGHUP);
     sigprocmask(SIG_BLOCK, &sigs, NULL);

     log_debug(NULL,"DNSRV CHILD: starting");

     /* Transmit stream header */
     write(di->out, "<stream>", 8);

     /* Loop forever, processing requests and feeding them to the xstream*/     
     while (1)
     {
       len = read(di->in, &readbuf, 1024);
       if (len <= 0)
       {
         log_debug(NULL,"dnsrv: Read error on coprocess(%d): %d %s",getppid(),errno,strerror(errno));
           break;
       }

       log_debug(NULL, "DNSRV CHILD: Read from buffer: %.*s",len,readbuf);

       if (xstream_eat(xs, readbuf, len) > XSTREAM_NODE)
       {
         log_debug(NULL, "DNSRV CHILD: xstream died");
         break;
       }
     }

     /* child is out of loop... normal exit so parent will start us again */
     log_debug(NULL, "DNSRV CHILD: out of loop.. exiting normal");
     pool_free(p);
     exit(0);
     return 0;
}


/** 
 * Generate log timestamp
 */
char *debug_log_timestamp(void)
{
    time_t t;
    int sz;
    char *tmp_str;

    t = time(NULL);

    if(t == (time_t)-1)
        return NULL;

    tmp_str = ctime(&t);
    sz = strlen(tmp_str);
    /* chop off the \n */
    tmp_str[sz-1]=' ';

    return tmp_str;
}

/**
 * Dummy debug logging function
 *
 * Just output it to STDERR
 */
void debug_log(char *zone, const char *msgfmt, ...) {
  va_list ap;
  char *pos, c = '\0';

  /* special per-zone filtering */
  if(zone != NULL) {
    pos = strchr(zone,'.');
    if(pos != NULL) {
      c = *pos;
      *pos = '\0'; /* chop */
    }
    if(pos != NULL)
      *pos = c; /* restore */
  }
  
  fprintf(stderr, "%s %s ", debug_log_timestamp(), zone);

  va_start(ap, msgfmt);
  vfprintf(stderr, msgfmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}


/**
 * Main() for the new async. DNS process
 */
int main(int argc, char* argv[]) {
  _dns_io di;

  // check args
  if (argc < 4) {
    fprintf(stderr, "Syntax: jabadns <read handle> <write handle>");
    fprintf(stderr, "<debug flag>\n");
    exit(1);
  }

  // get read & write handles
  di.in = atoi(argv[1]);
  di.out = atoi(argv[2]);
  set_debug_flag(atoi(argv[3]));

  // set this to NULL for the moment
  di.svclist = NULL;

  // these are not actually used in this process
  di.pid = -1;
  di.packet_table = NULL;
  di.packet_timeout = 0;
  di.cache_table = NULL;
  di.cache_timeout = 0;
  di.mempool = NULL;
  
  // call the core function
  dnsrv_child_main(&di);

  return 1;
}

