/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <netdb.h>
#include <memory.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif
#include <netinet/in.h>

#include "sio.h"
#include "str.h"
#include "addr.h"
#include "msg.h"
#include "util.h"
#include "xtimer.h"
#include "libportable.h"

#define OPEN_CURLY_BRACKET      '{'
#define CLOSED_CURLY_BRACKET    '}'
#define COMMA                   ','
#define DOT                     '.'

typedef enum { CANT_PARSE, PARSED, ERROR } result_e ;
typedef enum {   NUMERIC_ADDR, NET_ADDR, HOST_ADDR } address_e ;

/*
 * address types denote how the actual numeric address was obtained.
 * Currently they are only useful for debugging.
 * Note that NUMERIC_ADDR includes both simple (e.g. 128.138.91.1) and
 * factorized symbolic addresses (e.g. 128.138.91.{1,2,3}).
 */
struct comp_addr
{
   address_e       addr_type ;
   char            name[MAXHOSTNAMELEN+1] ;
   char            version;   /* v4 vs. v6 addresses/masks */
   union {
      struct in6_addr addr6 ;
      uint32_t        addr ;  /* host byte order */
   } a;
   union {
      struct in6_addr mask6 ;
      uint32_t        mask ;
   } m;
} ;

#define CAP( p )                ( (struct comp_addr *) (p) )
#define NEW_CAP()               NEW( struct comp_addr )
#define FREE_CAP( cap )         FREE( cap )


/* The addrlist_match function sets the mask for IPv6 addresses.
 * mask is a pointer to the in6_addr structure, bits is the
 * number of bits to set in the mask, and len is the length of mask.
 */
static void xsetmask(char *mask, unsigned int bits, unsigned int len)
{
   int i;
   int bytes = bits/8;
   int remain = bits - (bytes * 8);
   
   memset(mask, 0, len);

   /* This may be wrong for bigendian... */
   for(i=0; i < bytes; i++ ) {
      mask[i] = 0xFF;
   }

   if( remain > 0 )
      mask[i] = ( 0xFF << (8-remain) );

   return;
}


/* This is a helper function to make address matching with mask
 * work ok w/ipv6. The len parameter is in bytes, not bits. 
 * Returns TRUE if addr1&mask1 == addr2
 */
static bool_int xmatch(const char *addr1, const char *mask1, 
                       const char *addr2, int len)
{
   int i;

   for(i=0; i < len; i++ ) {
      if( (addr1[i] & mask1[i]) != ( addr2[i] & mask1[i] ) ) 
         return( FALSE );
   }
   return TRUE;
} 


/*
*   This function returns 0 if no match and the offset+1
*   to list which element in the list matched. The elements
*   in the addr_list are expected to be comp_addr structs.
*/
int addrlist_match( const pset_h addr_list, 
                    const struct sockaddr *addr )
{
   unsigned u, addr_count, length;
   char hname[NI_MAXHOST] ;

   if ( addr == NULL )
	   return 0;

   length = (addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : 
	   sizeof(struct sockaddr_in6);

   addr_count = pset_count( addr_list );
   if (addr_count == 0)
      return 0;
   
   hname[0] = 0;
      
   for ( u = 0 ; u < addr_count ; u++ ) 
   {
      struct comp_addr *cap = CAP( pset_pointer( addr_list, u ) ) ;
      if ( cap == NULL )
         continue ;
 
      if( (cap->addr_type == HOST_ADDR) ) 
      {
         char *tmpname = NULL;
         if ( hname[0] == 0 ) 
	 {
            memset(hname, 0, NI_MAXHOST);
            if ( getnameinfo(addr, length, hname, NI_MAXHOST, 
		 NULL, 0, NI_NAMEREQD) )
            {  /* 
	        * Name cannot be looked up if here. We should continue 
		* searching the list in case a IP address or net mask agrees 
		*/
                hname[0] = 0;
                continue ;
            }
	 }

         /* Parse the address as a domain portion */
         if( cap->name[0] == '.' )
         {
            tmpname = str_casefind( hname, cap->name );
            if( tmpname != NULL ) 
	    {
               if( strlen(cap->name) == strlen(tmpname) )
                  return( u+1 );
            }
         } 
	 else 
	 {
            if( (strlen(hname) == strlen(cap->name)) && 
	        (str_casefind( hname, cap->name ) == (char *)hname) )
               return( u+1 );
         }
      } /* End HOST_ADDR */ 
      else 
      { /* NUMERIC or NET addresses */ 
         if( (addr->sa_family == AF_INET) && (cap->version == 4) ) 
	 {
            const struct sockaddr_in *inp = SAIN(addr);
            if( ( ntohl(inp->sin_addr.s_addr) & cap->m.mask ) == 
			    ( cap->a.addr & cap->m.mask ) ) 
               return (u+1) ;
         } 
	 else if( (addr->sa_family == AF_INET6) && (cap->version == 6)) 
	 {
            if (cap->addr_type == NUMERIC_ADDR) {
	       if (IN6_ARE_ADDR_EQUAL(&SAIN6(addr)->sin6_addr, &cap->a.addr6))
                  return( u+1 );
            }
            else {  /* NET_ADDR */ 
               if ( xmatch( SAIN6(addr)->sin6_addr.s6_addr, 
	                (char *)&(cap->m.mask6), 
			(char *)&(cap->a.addr6), 16) == TRUE )
                  return( u+1 );
            }
         } 
	 else if (((addr->sa_family) == AF_INET6) && (cap->version == 4))
	 {  /* 
	     * If it's a mapped address, and a v4 address is specified, see
             * if the mapped address matches the v4 equivalent.
             */
            if( IN6_IS_ADDR_V4MAPPED( &SAIN6(addr)->sin6_addr ) ) 
	    {
               uint32_t *tmp_addr = (uint32_t *)&addr->sa_data[3];
               if( (ntohl(*tmp_addr) & cap->m.mask)
			       == ( cap->a.addr & cap->m.mask ) )
                  return (u+1);
            }
         }
      } /* End NUMERIC or NET address check */
   } /* End for loop */
   return ( 0 );
}


void addrlist_dump( const pset_h addr_list, int fd )
{
   unsigned u, num ;
   char addrstring[1025];
   char maskstring[1025];

   num = pset_count( addr_list );
   for ( u = 0 ; u < num ; u++ )
   {
      struct comp_addr *cap = CAP( pset_pointer( addr_list, u ) ) ;
      const char *type ;

      if ( cap->addr_type == NUMERIC_ADDR )
         type = "NUMERIC" ;
      else if ( cap->addr_type == NET_ADDR )
         type = "NET" ;
      else if ( cap->addr_type == HOST_ADDR )
         type = "HOST" ;
      else
         type = "BAD" ;
      
      memset(addrstring, 0, sizeof(addrstring));
      memset(maskstring, 0, sizeof(maskstring));
      if( cap->version == 4 ) {
         uint32_t addr = htonl(cap->a.addr);
         uint32_t mask = htonl(cap->m.mask);
         inet_ntop(AF_INET, &addr, addrstring, sizeof(addrstring));
         inet_ntop(AF_INET, &mask, maskstring, sizeof(maskstring));
      }
      else if( cap->version == 6 ) {
         inet_ntop(AF_INET6, &cap->a.addr6, addrstring, sizeof(addrstring));
         inet_ntop(AF_INET6, &cap->m.mask6, maskstring, sizeof(maskstring));
      }

      if ( cap->addr_type == NET_ADDR )
         Sprint(fd, " %s/%s(%s)", addrstring, maskstring, type);
      else if ( cap->addr_type == HOST_ADDR )
         Sprint( fd, " %s(%s)",  cap->name, type ) ;
      else
         Sprint( fd, " %s(%s)", addrstring, type ) ;
   }
}


void addrlist_free( pset_h addr_list )
{
   pset_apply( addr_list, free, NULL ) ;
}


/*
 * Verify's an address has more than numbers & dots. 
 * Returns 0 if numbers & dots, 1 otherwise.
 */
int check_hostname( const char *addr )
{
   int i;

   for (i = 0; addr[i]; ++i)
   {
      if ( !isdigit(addr[i]) && (addr[i] != '.') )
         return 1;
   }
   return 0;
}


/*
 * Add an address to the address list
 */
static status_e add( pset_h addr_list, const struct comp_addr *cap )
{
   struct comp_addr *new_cap = NULL;
   const char *func = "add" ;

   new_cap = NEW_CAP() ;
   if ( new_cap == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }

   *new_cap = *cap ;
   if ( pset_add( addr_list, new_cap ) == NULL )
   {
      out_of_memory( func ) ;
      FREE_CAP( new_cap ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}


/*
 * Find the address and remove it from the list
 * Since there is no check when we add entries that an
 * address is not entered twice, in this function we remove all
 * addresses that match.
 *
 * XXX: we need to work on the way two cap's are considered equal
 */
static status_e xremove( pset_h addr_list, const struct comp_addr *cap )
{
   unsigned u = 0 ;
   struct comp_addr *old_cap ;

   for ( u = 0 ; u < pset_count( addr_list ) ; u++ )
   {
      old_cap = CAP( pset_pointer( addr_list, u ) ) ;

      if ( (cap->addr_type == HOST_ADDR) && ( old_cap->addr_type == HOST_ADDR ))
      {
         if ( EQ(cap->name, old_cap->name) )
	 {
            pset_pointer( addr_list, u ) = NULL ;
            FREE_CAP( old_cap ) ;
	 }
      }
      /* If the versions are the same, and the v6 addresses are the same,
       * or the v4 addresses are the same, then one's a dup. 
       */
      else if ( (old_cap->version == cap->version) && 
	(((old_cap->version == 6) && 
         (IN6_ARE_ADDR_EQUAL( &(old_cap->a.addr6), &(cap->a.addr6))) && 
         (IN6_ARE_ADDR_EQUAL( &(old_cap->m.mask6), &(cap->m.mask6))) ) ||
         ((old_cap->version == cap->version) && (old_cap->version == 4) && 
         old_cap->a.addr == cap->a.addr && old_cap->m.mask == cap->m.mask)) )
      {
         pset_pointer( addr_list, u ) = NULL ;
         FREE_CAP( old_cap ) ;
      }
   }
   pset_compact( addr_list ) ;
   return( OK ) ;
}

/*
 * Function allows the use of 0.0.0.0/24 style address ranges for access cntl.
 *   --bbraun 10/26/98
 *
 * Updated to handle ::/46 style address ranges for access cntl.
 */
static result_e explicit_mask( const char *str_addr, statfunc op, 
                               pset_h addr_list)
{
   struct comp_addr         ca ;
   int val;
   unsigned mask;
   struct addrinfo hints, *res;
   char saddr[INET6_ADDRSTRLEN];

   memset(saddr, 0, INET6_ADDRSTRLEN);

   if (strchr(str_addr, OPEN_CURLY_BRACKET))  /* Don't even try factorized */
      return CANT_PARSE ;

   if (sizeof(saddr) < 46)
      return CANT_PARSE ;
   val = sscanf(str_addr, "%45[^/]/%u", saddr, &mask);
   if( val < 2 ) 
      return CANT_PARSE ;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = PF_UNSPEC;
   hints.ai_flags = AI_NUMERICHOST;
   if( getaddrinfo(saddr, NULL, &hints, &res) ) {
      return CANT_PARSE;
   }

   if( res->ai_family == AF_INET ) {
      ca.version = 4;
      ca.a.addr = ntohl( SAIN(res->ai_addr)->sin_addr.s_addr );
      if(mask == 32) {
         ca.addr_type = NUMERIC_ADDR;
         ca.m.mask      = 0xFFFFFFFF;
      } else {
         uint32_t i;
         unsigned n;
   
         ca.addr_type = NET_ADDR ;
      
         i = 0x80000000U;
         ca.m.mask = 0;
         /* Go through and set each bit to 1 */
         for( n=mask; n != 0 ; n--)
         {
            ca.m.mask |= i;
            i /= 2;
         }
      }
   } /* PF_INET */

   else if( res->ai_family == AF_INET6 ) {
      ca.version = 6;
      if( mask >= 128 ) {
         ca.addr_type = HOST_ADDR;
      } else {
         ca.addr_type = NET_ADDR;
      }
      memcpy( &ca.a.addr6, &(SAIN6(res->ai_addr)->sin6_addr), 
	    sizeof(struct in6_addr) );
      xsetmask((char *)&ca.m.mask6, mask, sizeof(ca.m.mask6));
   } /* PF_INET6 */

   freeaddrinfo(res);

   return( ( (*op)( addr_list, &ca ) == OK ) ? PARSED : ERROR ) ;
}

/*
 * Try to parse 'str_addr' as a symbolic net name
 * 
 * NOTE: This doesn't work with IPv6 addresses.
 */
static result_e net_addr( const char *str_addr, statfunc op, pset_h addr_list )
{
   /*
    *
    *  The following table demonstrates how the mask is determined
    *  given a net number N and following the relation:
    *     net #1 <= N <= #2
    *
    *     net #1      net #2      mask
    *        0           0        FFFFFFFF    (this should be rejected)
    *        1           FF       FF000000
    *        100         FFFF     FFFF0000
    *        10000       FFFFFF   FFFFFF00
    *        1000000     FFFFFFFF FFFFFFFF
    */
   static struct { uint32_t lim, mask, shift ; } net_to_mask[] =
   {
      { 0,            0xFF000000,   24  },
      { 0xFF,         0xFFFF0000,   16  },
      { 0xFFFF,       0xFFFFFF00,   8   },
      { 0xFFFFFF,     0xFFFFFFFF,   0   },
      { 0xFFFFFFFF,   0,            0   }
   } ;
   struct comp_addr         ca ;
   struct netent            *nep ;
   uint32_t                 net_num ;
   int                      i ;
   const char              *func = "net_addr" ;

   nep = getnetbyname( str_addr ) ;
   if ( nep == NULL || nep->n_addrtype != AF_INET || nep->n_net == 0 )
      return( CANT_PARSE ) ;

   for ( i = 0, net_num = (uint32_t) nep->n_net ;; i++ )
   {
      if ( net_to_mask[ i ].mask == 0 )
      {
         msg( LOG_CRIT, func,
            "INTERNAL ERROR: Cannot process net number %u", net_num ) ;
         return( ERROR ) ;
      }
      if ( net_to_mask[i].lim < net_num && net_num <= net_to_mask[i+1].lim )
      {
         ca.addr_type = NET_ADDR ;
         ca.a.addr = net_num << net_to_mask[ i ].shift ;
         ca.m.mask = net_to_mask[ i ].mask ;
         ca.version = 4;
         return( ( (*op)( addr_list, &ca ) == OK ) ? PARSED : ERROR ) ;
      }
   }
}


/*
 * Try to parse 'str_addr' as a numeric address
 */
static result_e numeric_addr( const char *str_addr, 
                               status_e (*op)(), 
                               pset_h addr_list )
{
   struct comp_addr ca ;
   struct addrinfo hints, *res = NULL;
   struct in6_addr zero;
   uint32_t mask, addr;

   if (strchr(str_addr, '/')) /* Don't even try explicit masks */
      return CANT_PARSE;
   if(strchr(str_addr, OPEN_CURLY_BRACKET))  /* Don't even try factorized */
      return CANT_PARSE;
   
   memset(&zero, 0, sizeof(zero));
   memset(&hints, 0, sizeof(hints));

   hints.ai_flags = AI_NUMERICHOST;
   if (strchr(str_addr, ':'))
      hints.ai_family = PF_INET6;
   else
      hints.ai_family = PF_INET;

   if( getaddrinfo(str_addr, NULL, &hints, &res) != 0 ) 
      return CANT_PARSE;

   if ( res == NULL )
      return CANT_PARSE;

   if ( res->ai_addr == NULL ) {
      freeaddrinfo(res);
      return CANT_PARSE;
   }

   if( res->ai_family == AF_INET6 ) {
      ca.version = 6;
      ca.addr_type = NUMERIC_ADDR;
      if( memcmp( &(SAIN6(res->ai_addr)->sin6_addr), &zero, 
	    sizeof(struct in6_addr) ) == 0 ) {
         memset( &ca.a.addr6, 0, sizeof(struct in6_addr) );
         memset( &ca.m.mask6, 0, sizeof(struct in6_addr) );
      } else {
         memcpy( &ca.a.addr6, &(SAIN6(res->ai_addr)->sin6_addr), 
	    sizeof(struct in6_addr) );
         memset(&ca.m.mask6, 0xFF, sizeof(struct in6_addr));
      }
   } else if( res->ai_family == AF_INET ) {
      ca.version = 4;
      ca.addr_type = NUMERIC_ADDR;
      if( SAIN(res->ai_addr)->sin_addr.s_addr == 0 ) {
         ca.a.addr = 0;
         ca.m.mask = 0;
      } else {
         addr = (uint32_t) ntohl( SAIN(res->ai_addr)->sin_addr.s_addr );
         for ( mask = 0xFF ;; )
         {
            if ( addr & mask )
               break ;
            mask <<= 8 ;
            mask |= 0xFF ;
         }
         mask = ~( mask >> 8 ) ;

         ca.a.addr = addr;
         ca.m.mask = mask;
      }
   
   }

   freeaddrinfo(res);
   return( ( (*op)( addr_list, &ca ) == OK ) ? PARSED : ERROR ) ;
}


/*
 * Try to parse 'str_addr' as a symbolic host name
 * Apply 'op' to the 'addrlist' for *all* IP addresses of the host
 */
static result_e host_addr( const char *str_addr, status_e (*op)(), pset_h addr_list )
{
   struct comp_addr ca;
   struct addrinfo hints, *res = NULL;
   char addr[46];

   if (strchr(str_addr, '/')) /* Don't even try explicit masks */
      return CANT_PARSE;
   if(strchr(str_addr, OPEN_CURLY_BRACKET))  /* Don't even try factorized */
      return CANT_PARSE;

   if( str_addr[0] == '.' ) 
   {
      if ( check_hostname(str_addr) )
      {
         ca.version = 0xFF;
         ca.addr_type = HOST_ADDR;
         /* XXX: does this really need to be NUL-padded? */
         strncpy(ca.name, str_addr, sizeof(ca.name)-1) ;
         ca.name[sizeof(ca.name)-1] = '\0';
         if ( (*op)( addr_list, &ca ) == FAILED )
            return ERROR ;
         return PARSED ;
      }
      else
	 return CANT_PARSE;
   }

   memset(&hints, 0, sizeof(hints));
   memset(addr, 0, sizeof(addr));

   hints.ai_flags = AI_CANONNAME;
   hints.ai_family = PF_UNSPEC;

   if ( getaddrinfo(str_addr, NULL, &hints, &res) != 0 ) 
      return CANT_PARSE;

   if ( res == NULL )
      return CANT_PARSE;

   strncpy(ca.name, str_addr, sizeof(ca.name)-1) ;
   ca.name[sizeof(ca.name)-1] = '\0';
   ca.addr_type = HOST_ADDR ;
   ca.version = 0xFF;
   freeaddrinfo(res);

   if ( (*op)( addr_list, &ca ) == FAILED ) 
      return ERROR ;
   else
      return PARSED ;
}


/*
 * Try to parse 'str_addr' as a factorized address
 * (for example, 128.138.{200,201})
 *
 * XXX: It is unclear whether this function should exist. It is really doing
 *      the job of a preprocessor.
 *
 * This does not work for IPv6 Addresses.
 */
static result_e factorized_addr( const char *str_addr, 
                                  status_e (*op)(), 
                                  pset_h addr_list )
{
   int                pass ;
   char               last   = DOT ;
   unsigned           num   = 0 ;
   int                shift = 24 ;   /* because we assume a 32-bit IP address */
   uint32_t           addr = 0 ;
   struct comp_addr   ca ;
   const char        *func = "factorized_addr" ;
   int                i, j;

   for ( i = 0 ; str_addr[i] != OPEN_CURLY_BRACKET ; last = str_addr[i++] )
   {
      if ( isdigit( str_addr[i] ) )
      {
         num = num * 10 + str_addr[i] - '0' ;
         continue ;
      }
      switch ( str_addr[i] )
      {
         case DOT:
            if ( last == DOT )
            {
               parsemsg( LOG_ERR, func,
                  "Bad address: %s. Consecutive dots", str_addr ) ;
               return( ERROR ) ;
            }
            addr = addr * 256 + num ;
            num = 0 ;
            shift -= 8 ;
            break ;
         
         default:
            return( CANT_PARSE ) ;
      }
   }

   ca.addr_type = NUMERIC_ADDR ;
   ca.version = 4;
   if ( addr != 0 )
      addr <<= ( shift+8 ) ;

   /*
    * First pass is for syntax checking
    */
   j = i;
   for ( pass = 0 ; pass < 2 ; pass++ )
   {
      i = j;
      num = 0 ;
      for ( i = i+1, last = COMMA ;; last = str_addr[i++] )
      {
         if ( isdigit( str_addr[i] ) )
         {
            num = num * 10 + str_addr[i] - '0' ;
            continue ;
         }
         switch ( str_addr[i] )
         {
            case COMMA:
            case CLOSED_CURLY_BRACKET:
               if ( last == COMMA )
               {
                  parsemsg( LOG_ERR, func,
                     "Bad address: %s. Consecutive commas", str_addr ) ;
                  return( ERROR ) ;
               }

               if ( pass == 1 )
               {
                  ca.a.addr = addr + ( num << shift ) ;
                  ca.m.mask = ~( ( 1 << shift ) - 1 ) ;
                  if ( (*op)( addr_list, &ca ) == FAILED )
                     return( ERROR ) ;
                  num = 0 ;
               }
               break ;
            
            default:
               parsemsg( LOG_ERR, func, "Bad address: %s", str_addr ) ;
               return( ERROR ) ;
         }
         if ( str_addr[i] == CLOSED_CURLY_BRACKET )
         {
            if ( str_addr[i+1] != NUL )
            {
               parsemsg( LOG_ERR, func, "Bad address: %s", str_addr ) ;
               return( ERROR ) ;
            }

            if ( pass == 0 )
               break ;
            else
               return( PARSED ) ;
         }
      }
   }
   /* NOTREACHED */

   return( ERROR );
}


/*
 * Try to parse 'str_addr' using all known methods.
 * Try until one of the methods succeeds.
 * A successful method will apply 'op' with the parsed address to the 
 * 'addr_list'. The 'op' can be either 'add' or 'remove'
 * This means that the parsed address will be either added or removed
 * from the addr_list.
 */
static status_e addrlist_op( pset_h addr_list, 
                              status_e (*op)(), 
                              const char *str_addr )
{
   int i ;
   static result_e (*addr_parser[])() =
      {
         numeric_addr,
         host_addr,
         explicit_mask,
         factorized_addr,
         net_addr,
         NULL
      } ;
   const char *func = "addrlist_op" ;

   if (str_addr == NULL)
      return FAILED;
   
   if (str_addr[0] == NUL )
      return OK;
   
   for ( i = 0 ; addr_parser[ i ] != NULL ; i++ )
      switch ( (*addr_parser[ i ])( str_addr, op, addr_list ) )
      {
         case PARSED:
            return OK;
         case ERROR:
            return FAILED;
         case CANT_PARSE:
            break;
      }

   parsemsg( LOG_ERR, func, "failed to parse %s", str_addr ) ;
   return OK;
}


status_e addrlist_add( pset_h addr_list, const char *str_addr ) 
{
   return( addrlist_op( addr_list, add, str_addr ) ) ;
}


status_e addrlist_remove( pset_h addr_list, const char *str_addr ) 
{
   return( addrlist_op( addr_list, xremove, str_addr ) ) ;
}


status_e addrlist_copy( const pset_h from, pset_h *to )
{
   return( copy_pset( from, to, sizeof( struct comp_addr ) ) ) ;
}

