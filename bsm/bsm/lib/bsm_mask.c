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

