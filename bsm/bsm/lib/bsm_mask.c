/*
 * Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdlib.h>

#include <libbsm.h>

/* MT-Safe */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int firsttime = 1;

/*
 * XXX  ev_cache, once created, sticks around until the calling program
 * exits.  This may or may not be a problem as far as absolute memory usage
 * goes, but at least there don't appear to be any leaks in using the cache.  
 */
LIST_HEAD(, audit_event_map) ev_cache;

static int load_event_table() 
{
	struct au_event_ent *ev;
	struct audit_event_map *elem;
	
	pthread_mutex_lock(&mutex);		

	LIST_INIT(&ev_cache);

	setauevent(); /* rewind to beginning of entries */

	/* 
	 * loading of the cache happens only once; 
	 * dont check if cache is already loaded 
	 */   
	
	/* Enumerate the events */	
	while((ev = getauevent()) != NULL) {
		elem = (struct audit_event_map *) 
				malloc (sizeof (struct audit_event_map));
		if(elem == NULL) {
			free_au_event_ent(ev);
			pthread_mutex_unlock(&mutex);		
			return -1;
		}
		elem->ev = ev;
		LIST_INSERT_HEAD(&ev_cache, elem, ev_list);
	}
	pthread_mutex_unlock(&mutex);		
	return 1;
}

/* Add a new event to the cache */
static int add_to_cache(struct au_event_ent *ev) 
{
	struct au_event_ent *oldev;
	struct audit_event_map *elem;
		
	pthread_mutex_lock(&mutex);		

	LIST_FOREACH(elem, &ev_cache, ev_list) {
		if(elem->ev->ae_number == ev->ae_number) {
			/* Swap old with the new */
			oldev = elem->ev;
			elem->ev = ev;	
			free_au_event_ent(oldev);
			pthread_mutex_unlock(&mutex);		
			return 1;
		}
	}	

	/* Add this event as a new entry in the list */	
	elem = (struct audit_event_map *) 
			malloc (sizeof (struct audit_event_map));
	if(elem == NULL) {
		/* XXX Do we need to clean up ? */
		pthread_mutex_unlock(&mutex);		
		return -1;
	}
	elem->ev = ev;
	LIST_INSERT_HEAD(&ev_cache, elem, ev_list);

	pthread_mutex_unlock(&mutex);		
	return 1;
	
}

/* Read the event with the matching event number from the cache */
static struct au_event_ent *read_from_cache(au_event_t event) 
{
	struct audit_event_map *elem;

	pthread_mutex_lock(&mutex);		

	LIST_FOREACH(elem, &ev_cache, ev_list) {
		if(elem->ev->ae_number == event) {
			pthread_mutex_unlock(&mutex);		
			return elem->ev;		
		}
	}	

	pthread_mutex_unlock(&mutex);		
	return NULL;
}


/* 
 * Check if the audit event is preselected against the preselction mask 
 */ 
int au_preselect(au_event_t event, au_mask_t *mask_p, int sorf, int flag)
{
	struct au_event_ent *ev = NULL;
	au_class_t effmask = 0;
			
	if(mask_p == NULL) {
		return -1;
	}

	/* If we are here for the first time, load the event database */
	if(firsttime) {
		firsttime = 0;
		if( -1 == load_event_table()) {
			return -1;
		}
	}		

 	if(flag == AU_PRS_REREAD) {
		/* get the event structure from the event number */
		ev = getauevnum(event);
		if(ev != NULL) {
			add_to_cache(ev);
		}
	}
	else if(flag == AU_PRS_USECACHE) {
		ev = read_from_cache(event);
	}

	if(ev == NULL) {
		return -1;
	}

	if(sorf & AU_PRS_SUCCESS) {
		effmask |= (mask_p->am_success & ev->ae_class);
	}
	
	if(sorf & AU_PRS_FAILURE) {
		effmask |= (mask_p->am_failure & ev->ae_class);
	}
	
	if(effmask != 0) {
		return 1;
	}

	return 0;	
}

