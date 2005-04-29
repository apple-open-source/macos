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

#include <netinet/in.h>

#ifndef __CYGWIN__
#include <arpa/nameser.h>
#include <resolv.h>
#else 
#include "win32_resolv.h"
#endif

#include "srv_resolv.h"

#ifndef T_SRV
#define T_SRV 33
#endif

typedef struct __srv_list
{
     int   priority;
     char* port;
     char* host;
     struct __srv_list* next;
} *srv_list, _srv_list;

char* srv_inet_ntoa(pool p, unsigned char* addrptr)
{
     char result[16];
     result[15] = '\0';
     snprintf(result, 16, "%d.%d.%d.%d", addrptr[0],addrptr[1],addrptr[2],addrptr[3]);
     return pstrdup(p, result);
}


char* srv_port2str(pool p, unsigned short port)
{
     char* result = pmalloco(p, 6);
     snprintf(result, 5, "%d", port);
     return result;
}

char* srv_lookup(pool p, const char* service, const char* domain)
{
     unsigned char    reply[1024];         /* Reply buffer */
     int              replylen = 0;
     char             host[1024];
     register HEADER* rheader;             /* Reply header*/
     unsigned char*   rrptr;               /* Current Resource record ptr */
     int              exprc;               /* dn_expand return code */
     int              rrtype;
     long             rrpayloadsz;
     srv_list       svrlist  = NULL;
     srv_list       tempnode = NULL;
     srv_list       iternode = NULL;
     HASHTABLE        arr_table;           /* Hash of A records (name, ip) */
     spool            result;
     char*            ipname;
     char*            ipaddr;
     struct hostent*  hp;

     /* If no service is specified, use a standard gethostbyname call */
     if (service == NULL)
     {
          log_debug(ZONE, "srv: Standard resolution of %s", domain);
          hp = gethostbyname(domain);
          if (!hp)
          {
               log_debug(ZONE, "srv: Unable to resolve: %s", domain);
               return NULL;
          }
          else
          {
               return pstrdup(p, srv_inet_ntoa(p, hp->h_addr));
          }
     }

     log_debug(ZONE, "srv: SRV resolution of %s.%s", service, domain);

     /* Setup A record hash table */
     arr_table = ghash_create(11, (KEYHASHFUNC)str_hash_code, (KEYCOMPAREFUNC)j_strcmp);

     #ifndef __CYGWIN__
     /* Initialize lookup system if needed (check global _res structure) */
     if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
     {
          log_debug(ZONE, "srv: initialization failed on res_init.");
          return NULL;
     }
     #endif

     /* Run a SRV query against the specified domain */
     replylen = res_querydomain(service, domain, 
                                C_IN,      /* Class */
                                T_SRV,     /* Type */
                                (unsigned char*)&reply,    /* Answer buffer */
                                sizeof(reply)); /* Answer buffer sz */


     /* Setup a pointer to the reply header */
     rheader   = (HEADER*)reply;

     /* Process SRV response if all conditions are met per RFC 2052:
        1.) reply has some data available
        2.) no error occurred
        3.) there are 1 or more answers available */
     if ( (replylen > 0) && (ntohs(rheader->rcode) == NOERROR) && (ntohs(rheader->ancount) > 0) )
     {
          /* Parse out the Question section, and get to the following 
             RRs (see RFC 1035-4.1.2) */
          exprc = dn_expand(reply,                    /* Msg ptr */
                            reply + replylen,         /* End of msg ptr */
                            reply + sizeof(HEADER),  /* Offset into msg */
                            host, sizeof(host));      /* Dest buffer for expansion */
          if (exprc < 0)
          {
               log_debug(ZONE, "srv: DN expansion failed for Question section.");
               return NULL;
          }

          /* Determine offset of the first RR */
          rrptr = reply + sizeof(HEADER) + exprc + 4;

          /* Walk the RRs, building a list of targets */
          while (rrptr < (reply + replylen))
          {
               /* Expand the domain name */
               exprc = dn_expand(reply, reply + replylen, rrptr, host, sizeof(host));
               if (exprc < 0)
               {
                    log_debug(ZONE, "srv: Whoa nelly! DN expansion failed for RR.");
                    return NULL;
               }

               /* Jump to RR info */
               rrptr += exprc;
               rrtype      = (rrptr[0] << 8 | rrptr[1]);  /* Extract RR type */
               rrpayloadsz = (rrptr[8] << 8 | rrptr[9]);  /* Extract RR payload size */
               rrptr += 10;

               /* Process the RR */
               switch(rrtype)
               {
                    /* A records should be hashed for the duration of this lookup */
               case T_A: 
                    /* Allocate a new string to hold the IP address */
                    ipaddr = srv_inet_ntoa(p, rrptr);
                    /* Copy the domain name */
                    ipname = pstrdup(p, host);

                    /* Insert name/ip into hash table for future reference */
                    ghash_put(arr_table, ipname, ipaddr);
                    
                    break;
                    /* SRV records should be stored in a sorted list */
               case T_SRV:
                    /* Expand the target name */
                    exprc = dn_expand(reply, reply + replylen, rrptr + 6, host, sizeof(host));
                    if (exprc < 0)
                    {
                         log_debug(ZONE, "srv: DN expansion failed for SRV.");
                         return NULL;
                    }

                    /* Create a new node */
                    tempnode = pmalloco(p, sizeof(_srv_list));
                    tempnode->priority = (rrptr[0] << 8 | rrptr[1]);
                    tempnode->port     = srv_port2str(p, (rrptr[4] << 8 | rrptr[5]));
                    tempnode->host     = pstrdup(p, host);

                    /* Initialize iteration node */
                    iternode = svrlist;
                    /* Insert the node in the list */               
                    if (iternode == NULL)
                         svrlist = tempnode;
                    else
                    {
                         /* Find the first entry greater in priority that the tempnode */
                         while ( (tempnode->priority > iternode->priority) && (iternode->next != NULL) )
                              iternode = iternode->next;
                         /* If iternode == svrlist, update svrlist */
                         if (iternode == svrlist)
                         {
                              tempnode->next = svrlist;
                              svrlist = tempnode;
                         }
                         else
                         {
                              /* Insert the tempnode */
                              tempnode->next = iternode->next;
                              iternode->next = tempnode;
                         }
                    }
               } /* end..switch */

               /* Increment to next RR */
               rrptr += rrpayloadsz;
          }

          /* Now, walk the nicely sorted list and resolve the target's A records, sticking the resolved name in
             a spooler -- hopefully these have been pre-cached, and arrived along with the SRV reply */
          result = spool_new(p);

          iternode = svrlist;
          while (iternode != NULL)
          {
               /* HACK */
               if (result->len != 0)
                    spool_add(result, ",");
               /* Check the A record hash table first.. */
               ipaddr = (char*)ghash_get(arr_table, iternode->host);
               if (ipaddr != NULL)
               {
                    spooler(result, ipaddr, ":", iternode->port, result);
               }
               /* Otherwise, request the A record */
               else
               {
                    log_debug(ZONE, "srv: attempting A record lookup.");
                    /* FIXME: Is this really retrv'ing A record? */
                    hp = gethostbyname(iternode->host);
                    if (!hp)
                    {
                         log_debug(ZONE, "srv: Unable to resolve SRV reference to: %s\n", iternode->host);
                    }
                    else
                    {
                         spooler(result, srv_inet_ntoa(p, hp->h_addr), ":", iternode->port, result);
                    }
               }
               iternode = iternode->next;
          }
          /* Finally, turn the fully resolved list into a string <ip>:<host>,... */
          return spool_print(result);
     }
     /* Otherwise, return NULL -- it's for the caller to finish up by using
        standard A records */
     return NULL;       
}
 

