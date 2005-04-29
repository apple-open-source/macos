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

/* private heartbeat ring struct */
typedef struct beat_struct
{
    beathandler f;
    void *arg;
    int freq;
    int last;
    pool p;
    struct beat_struct *prev;
    struct beat_struct *next;
} *beat, _beat;

/* master hook for the ring */
beat heartbeat__ring;

void *heartbeat(void *arg)
{
    beat b, b2;
    result r;

    while(1)
    {
        pth_sleep(1);
        if(jabberd__signalflag) jabberd_signal();
        if(heartbeat__ring==NULL) break;

	/* run through the ring */
	for(b = heartbeat__ring->next; b != heartbeat__ring; b = b->next)
	{
	    /* beats can fire on a frequency, just keep a counter */
	    if(b->last++ == b->freq)
	    {
	        b->last = 0;
	        r = (b->f)(b->arg);

	        if(r == r_UNREG)
	        { /* this beat doesn't want to be fired anymore, unlink and free */
	            b2 = b->prev;
		    b->prev->next = b->next;
		    b->next->prev = b->prev;
		    pool_free(b->p);
		    b = b2; /* reset b to accomodate the for loop */
	        }
	    }
	}
    }
    return NULL;
}

/* register a function to receive heartbeats */
beat new_beat(void)
{
    beat newb;
    pool p;

    p = pool_new();
    newb = pmalloc_x(p, sizeof(_beat), 0);
    newb->p = p;

    return newb;
}

/* register a function to receive heartbeats */
void register_beat(int freq, beathandler f, void *arg)
{
    beat newb;

    if(freq <= 0 || f == NULL || heartbeat__ring == NULL) return; /* uhh, probbably don't want to allow negative heartbeats, since the counter will count infinitly to a core */

    /* setup the new beat */
    newb = new_beat();
    newb->f = f;
    newb->arg = arg;
    newb->freq = freq;
    newb->last = 0;

    /* insert into global ring */
    newb->next = heartbeat__ring->next;
    heartbeat__ring->next = newb;
    newb->prev = heartbeat__ring;
    newb->next->prev = newb;
}

/* start up the heartbeat */
void heartbeat_birth(void)
{
    /* init the ring */
    heartbeat__ring = new_beat();
    heartbeat__ring->next = heartbeat__ring->prev = heartbeat__ring;

    /* start the thread */
    pth_spawn(PTH_ATTR_DEFAULT, heartbeat, NULL);
}

void heartbeat_death(void)
{
    beat cur;
    while(heartbeat__ring!=NULL)
    {
       cur=heartbeat__ring;
       if(heartbeat__ring->next==heartbeat__ring) 
       {
           heartbeat__ring=NULL;
       }
       else
       {
           if(heartbeat__ring->next!=NULL)
               heartbeat__ring->next->prev=heartbeat__ring->prev;
           if(heartbeat__ring->prev!=NULL) 
               heartbeat__ring->prev->next=heartbeat__ring->next;
           heartbeat__ring=heartbeat__ring->next;
       }
       pool_free(cur->p);
    }
}
