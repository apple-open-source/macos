/*
 * (c) Copyright 2001-2002 by Steve Grubb
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#include "config.h"
#include "pset.h"
#include "str.h"
#include "addr.h"
#include "msg.h"
#include "sconf.h"
#include "sensor.h"
#include "xconfig.h"
#include "xtimer.h"

/*
 * This is the globals for the Sensor. The Sensor will add the incoming IP
 * address to the global_no_access table for whatever the configured time is.
 */
static pset_h global_no_access = NULL;        /* global no_access list   */
static pset_h global_no_access_time = NULL;   /* time of the infraction   */
static int timer_id = 0;		      /* Timer ID */

/* This function is called via a timer callback every 60 seconds */
static void scrub_global_access_list( void );


void init_sensor( void )
{
   if ( global_no_access == NULL )
      global_no_access = pset_create(10, 10);
   if ( global_no_access_time == NULL )
      global_no_access_time = pset_create(10, 10);
}

/*
 * This function runs in the parent context and updates the global_no_access
 * list. 
 */
void process_sensor( const struct service *sp, const union xsockaddr *addr)
{
   const char *func = "process_sensor";

   if (sp->svc_conf->sc_deny_time != 0)   /* 0 simply logs it   */
   {
      if ( pset_count( global_no_access ) < MAX_GLOBAL_NO_ACCESS)
      {
         int item_matched = addrlist_match( global_no_access, SA(addr) );

	 if ( item_matched == 0)
	 {   /* no match...adding to the list   */
            char *dup_addr = new_string(xaddrname( addr ) );

	    if (dup_addr == NULL )
               return ;

	    if (addrlist_add(global_no_access, dup_addr) == FAILED)
               msg(LOG_ERR, func,
                  "Failed adding %s to the global_no_access list", dup_addr);
            else
            {
               time_t nowtime;
               char time_buf[40], *tmp;

	       nowtime = time(NULL);
	       msg(LOG_CRIT, func,
	           "Adding %s to the global_no_access list for %d minutes",
	            dup_addr, sp->svc_conf->sc_deny_time);

	       if (sp->svc_conf->sc_deny_time == -1)
                    strcpy(time_buf, "-1");
               else
                    strx_nprint(time_buf, 38, "%ld",
                       (time_t)nowtime+(60*sp->svc_conf->sc_deny_time));

	       tmp = new_string(time_buf);
               if (tmp != NULL)
               {
                  if (pset_add(global_no_access_time, tmp) == NULL)
                  {
                     msg(LOG_ERR, func,
                         "Failed adding %s to the global_no_access_time list. "
                         "global_no_access list is broken, xinetd needs "
			 "restarting.", dup_addr);
                 /* ideally, we should rollback the previous addr addition.   */
                  }
               }
	       if (pset_count(global_no_access) && (timer_id == 0) )
                  timer_id = xtimer_add( scrub_global_access_list, 60 );
            }
            free(dup_addr);
         }
         else
	 {
	    /* Here again, eh?...update time stamp. */
            char *exp_time;
	    time_t stored_time;

	    item_matched--; /* Is # plus 1, to even get here must be >= 1 */
            exp_time = pset_pointer( global_no_access_time, item_matched ) ;
            if (exp_time == NULL)
               return ;

            if ( parse_base10(exp_time, (int *)&stored_time) )
            {  /* if never let them off, bypass */
               if (stored_time != -1)
               {
                  time_t nowtime, new_time;

                  nowtime = time(NULL);
                  new_time = (time_t)nowtime+(60*sp->svc_conf->sc_deny_time);                     if (difftime(new_time, (time_t)stored_time) > 0.0)
	          {   /* new_time is longer save it   */
		     char time_buf[40], *new_exp_time;

		     strx_nprint(time_buf, 38, "%ld", (long)new_time);
		     new_exp_time = new_string(time_buf);
		     if ( new_exp_time )
		     {
		        free(exp_time);
			global_no_access_time->ptrs[ 
                        (unsigned)item_matched ] = new_exp_time;
                     }
                  }
               }
            }
         }
      }
      else
         msg(LOG_ERR, func, "Maximum global_no_access count reached.");
   }
}

/* They hit a real server...note, this is likely to be a child process. */
status_e check_sensor( const union xsockaddr *addr)
{

   if ( (global_no_access) && pset_count( global_no_access ) )
   {
      if (addrlist_match( global_no_access, SA(addr)))
         return FAILED;
   }
   return OK;
}
   

static void scrub_global_access_list( void )
{
   unsigned count;
   const char *func = "scrub_global_no_access_list";

   if ( global_no_access == NULL )
      count = 0;
   else
      count = pset_count( global_no_access );

   if ( count )
   {
      int found_one = 0;
      unsigned u;
      time_t nowtime = time(NULL);

      for (u=0; u < count; u++)
      {
         char *exp_time;
         time_t stored_time;
	 
	 exp_time = pset_pointer( global_no_access_time, u ) ;
         stored_time = atol(exp_time);

	 if (stored_time == -1)   /* never let them off   */
	    continue;

	 if (difftime(nowtime, (time_t)stored_time) >= 0.0)
	 {
	    __pset_pointer ptr;

            pset_pointer(global_no_access, u) = NULL;
            ptr = global_no_access_time->ptrs[ u ];
            free(ptr);
            pset_pointer(global_no_access_time, u ) = NULL;
            found_one = 1;
         }
      }
      if (found_one)
      {
         pset_compact( global_no_access );
         pset_compact( global_no_access_time );
	 msg(LOG_INFO, func, 
	    "At least 1 DENY_TIME has expired, global_no_access list updated");
      }

      /* If there's still more on the list, start another callback. */
      count = pset_count( global_no_access );
      if ( count )
         timer_id = xtimer_add( scrub_global_access_list, 60 );
      else
      {
         timer_id = 0;
	 msg(LOG_INFO, func, 
	          "global_no_access list is empty.");
      }
   }
}

