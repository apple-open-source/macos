#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <libbsm.h>

/*
 * Parse the contents of the audit_event file to return
 * au_event_ent entries
 */   
static FILE *fp = NULL;
static char linestr[AU_LINE_MAX];
static char *delim = ":";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * XXX The reentrant versions of the following functions is TBD
 * XXX struct au_event_ent *getauevent_r(au_event_ent_t *e);
 * XXX struct au_event_ent *getauevnam_r(au_event_ent_t *e, char *name);
 * XXX struct au_event_ent *getauevnum_r(au_event_ent_t *e, au_event_t event_number);
 */


/* 
 * Allocate an au_event_ent structure
 */  
static struct au_event_ent *get_event_area()
{
	struct au_event_ent *e;
		
	e = (struct au_event_ent *) malloc (sizeof(struct au_event_ent));
	if(e == NULL) {
		return NULL;
	}
	e->ae_name = (char *)malloc(AU_EVENT_NAME_MAX * sizeof(char));
	if(e->ae_name == NULL) {
		free(e);
		return NULL;
	}
	e->ae_desc = (char *)malloc(AU_EVENT_DESC_MAX * sizeof(char));
	if(e->ae_desc == NULL) {
		free(e->ae_name);
		free(e);
		return NULL;
	}

	return e;
}

/*
 * Free the au_event_ent structure
 */  
void free_au_event_ent(struct au_event_ent *e)
{
    if (e)
    {
	if (e->ae_name) 
	    free(e->ae_name);
	if (e->ae_desc) 
	    free(e->ae_desc);
	free(e);
    }
}

/* 
 * Parse one line from the audit_event file into 
 * the au_event_ent structure
 */ 
static struct au_event_ent *eventfromstr(char *str, char *delim, struct au_event_ent *e) 
{
	char *evno, *evname, *evdesc, *evclass;
	struct au_mask evmask;
	char *last;
	
	evno = strtok_r(str, delim, &last);
	evname = strtok_r(NULL, delim, &last);
	evdesc = strtok_r(NULL, delim, &last);
	evclass = strtok_r(NULL, delim, &last);

	if((evno == NULL) 
		|| (evname == NULL)
		|| (evdesc == NULL)
		|| (evclass == NULL)) {

		return NULL;
	}		

	if(strlen(evname) >= AU_EVENT_NAME_MAX) {
			return NULL;
	}
	strcpy(e->ae_name, evname);

	if(strlen(evdesc) >= AU_EVENT_DESC_MAX) {
			return NULL;
	}
	strcpy(e->ae_desc, evdesc);
	
	e->ae_number = atoi(evno);

	/* 
	 * find out the mask that corresponds 
	 * to the given list of classes. 
	 */ 
	if(getauditflagsbin(evclass, &evmask) != 0)
		e->ae_class = AU_NULL;
	else 
		e->ae_class = evmask.am_success; 

	return e;
}

/*
 * Rewind the audit_event file
 */  
void setauevent()
{
	pthread_mutex_lock(&mutex);

	if(fp != NULL) {
		fseek(fp, 0, SEEK_SET);
	}

	pthread_mutex_unlock(&mutex);
}

/*
 * Close the open file pointers
 */  
void endauevent()
{
	pthread_mutex_lock(&mutex);
	
	if(fp != NULL) {
		fclose(fp);
		fp = NULL;
	}

	pthread_mutex_unlock(&mutex);
}

/*
 * Enumerate the au_event_ent entries
 */ 
struct au_event_ent *getauevent()
{
	struct au_event_ent *e;
	char *nl;

	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_EVENT_FILE, "r")) == NULL)) {
		
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	if(fgets(linestr, AU_LINE_MAX, fp) == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	/* Remove new lines */
	if((nl = strrchr(linestr, '\n')) != NULL) {
		*nl = '\0';
	}

	e = get_event_area();
	if(e == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	/* Get the next event structure */	
	if(eventfromstr(linestr, delim, e) == NULL) {

		free_au_event_ent(e);

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	pthread_mutex_unlock(&mutex);
	return e;
}

/*
 * Search for an audit event structure having the given event name
 */  
struct au_event_ent *getauevnam(char *name)
{
	struct au_event_ent *e;
	char *nl;

	if(name == NULL) {
		return NULL;
	}
	
	/* Rewind to beginning of the file */
	setauevent();
	
	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_EVENT_FILE, "r")) == NULL)) {
		
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	e = get_event_area(); 
	if(e == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	while(fgets(linestr, AU_LINE_MAX, fp) != NULL) {
		/* Remove new lines */
		if((nl = strrchr(linestr, '\n')) != NULL) {
			*nl = '\0';
		}
		
		if(eventfromstr(linestr, delim, e) != NULL) {
			if(!strcmp(name, e->ae_name)) {
					
				pthread_mutex_unlock(&mutex);
				return e;
			}
		}	
	}

	free_au_event_ent(e);

	pthread_mutex_unlock(&mutex);
	return NULL;

}


/*
 * Search for an audit event structure having the given event number
 */  
struct au_event_ent *getauevnum(au_event_t event_number)
{
	struct au_event_ent *e;
	char *nl;

	/* Rewind to beginning of the file */
	setauevent();

	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_EVENT_FILE, "r")) == NULL)) {
		
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	e = get_event_area(); 
	if(e == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	while(fgets(linestr, AU_LINE_MAX, fp) != NULL) {
		/* Remove new lines */
		if((nl = strrchr(linestr, '\n')) != NULL) {
			*nl = '\0';
		}
	
		if(eventfromstr(linestr, delim, e) != NULL) {
			if(event_number == e->ae_number) {
					
				pthread_mutex_unlock(&mutex);
				return e;
			}
		}	
	}

	free_au_event_ent(e);

	pthread_mutex_unlock(&mutex);
	return NULL;

}

/*
 * Search for an audit_event entry with a given event_name 
 * and returns the corresponding event number
 */ 
au_event_t *getauevnonam(char *event_name)
{
	struct au_event_ent *e;
	au_event_t *n = NULL;

	e = getauevnam(event_name);
	if(e != NULL) {
		n = (au_event_t *) malloc (sizeof(au_event_t));
		if(n != NULL) {
			*n = e->ae_number;		
		}
		free_au_event_ent(e);
	}
	 
	return n;
}

void free_au_event(au_event_t *e)
{
    if (e) 
	free(e);
}
