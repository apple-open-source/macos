#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <libbsm.h>

/*
 * Parse the contents of the audit_user file into au_user_ent structures
 */  

static FILE *fp = NULL;
static char linestr[AU_LINE_MAX];
static char *delim = ":";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * XXX The reentrant versions of the following functions is TBD
 * XXX struct au_user_ent *getauusernam_r(au_user_ent_t *u, const char *name);
 * XXX struct au_user_ent *getauuserent_r(au_user_ent_t *u);             
 */

/*
 * Allocate a user area structure 
 */
static struct au_user_ent *get_user_area()
{
	struct au_user_ent *u;
		
	u = (struct au_user_ent *) malloc (sizeof(struct au_user_ent));
	if(u == NULL) {
		return NULL;
	}
	u->au_name = (char *)malloc(AU_USER_NAME_MAX * sizeof(char));
	if(u->au_name == NULL) {
		free(u);
		return NULL;
	}

	return u;
}

/*
 * Destroy a user area structure 
 */
static void destroy_user_area(struct au_user_ent *u)
{
	free(u->au_name);
	free(u);
}


/*
 * Parse one line from the audit_user file into the au_user_ent structure
 */  
static struct au_user_ent *userfromstr(char *str, char *delim, struct au_user_ent *u) 
{
	char *username, *always, *never;
	char *last;

	username = strtok_r(str, delim, &last);
	always = strtok_r(NULL, delim, &last);
	never = strtok_r(NULL, delim, &last);

	if((username == NULL) 
		|| (always == NULL)
		|| (never == NULL)) {

		return NULL;
	}		

	if(strlen(username) >= AU_USER_NAME_MAX) {
		return NULL;
	}

	strcpy(u->au_name, username);
	if(getauditflagsbin(always, &(u->au_always)) == -1) {
		return NULL;
	}

	if(getauditflagsbin(never, &(u->au_never)) == -1) {
		return NULL;
	}

	return u;
}

/*
 * Rewind to beginning of the file 
 */
void setauuser()
{
	pthread_mutex_lock(&mutex);

	if(fp != NULL) {
		fseek(fp, 0, SEEK_SET);
	}

	pthread_mutex_unlock(&mutex);
}

/*
 * Close the file descriptor
 */  
void endauuser()
{
	pthread_mutex_lock(&mutex);
	
	if(fp != NULL) {
		fclose(fp);
		fp = NULL;
	}

	pthread_mutex_unlock(&mutex);
}

/*
 * Enumerate the au_user_ent structures from the file
 */  
struct au_user_ent *getauuserent()
{
	struct au_user_ent *u;
	char *nl;

	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_USER_FILE, "r")) == NULL)) {
		
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

	u = get_user_area();
	if(u == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	/* Get the next structure */	
	if(userfromstr(linestr, delim, u) == NULL) {

		destroy_user_area(u);

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	pthread_mutex_unlock(&mutex);
	return u;
}

/*
 * Find a au_user_ent structure matching the given user name
 */  
struct au_user_ent *getauusernam(const char *name)
{
	struct au_user_ent *u;
	char *nl;

	if(name == NULL) {
		return NULL;
	}
	
	setauuser();
	
	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_USER_FILE, "r")) == NULL)) {
		
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	u = get_user_area(); 
	if(u == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	while(fgets(linestr, AU_LINE_MAX, fp) != NULL) {
	
		/* Remove new lines */
		if((nl = strrchr(linestr, '\n')) != NULL) {
			*nl = '\0';
		}
	
		if(userfromstr(linestr, delim, u) != NULL) {
			if(!strcmp(name, u->au_name)) {
					
				pthread_mutex_unlock(&mutex);
				return u;
			}
		}	
	}

	destroy_user_area(u);

	pthread_mutex_unlock(&mutex);
	return NULL;

}

/*
 * Read the default system wide audit classes from audit_control, 
 * combine with the per-user audit class and update the 
 * binary preselection mask  
 */ 
int au_user_mask(char *username, au_mask_t *mask_p)
{
	struct au_user_ent *u;
	char auditstring[MAX_AUDITSTRING_LEN + 1];

	/* get user mask */
	if((u = getauusernam(username)) != NULL) {

		if(-1 == getfauditflags(&u->au_always, &u->au_never, mask_p)) {
			return -1;
		}

		return 0;
	}

	/* read the default system mask */
	if(getacflg(auditstring, MAX_AUDITSTRING_LEN) == 0) {
		if(-1 == getauditflagsbin(auditstring, mask_p)) {
			return -1;
		}
		return 0;
	}   

	/* No masks defined */
	return -1;
}

/*
 * Generate the process audit state by combining the audit maks 
 * passed as parameters with the sustem audit masks 
 */ 
int getfauditflags(au_mask_t *usremask, au_mask_t *usrdmask, 
				au_mask_t *lastmask)
{
	char auditstring[MAX_AUDITSTRING_LEN + 1];
	
	if((usremask == NULL) 
		|| (usrdmask == NULL) 
		|| (lastmask == NULL)) {

			return -1;
	}
		
	lastmask->am_success = 0;
	lastmask->am_failure = 0;

	/* get the system mask */
	if(getacflg(auditstring, MAX_AUDITSTRING_LEN) == 0) {
		getauditflagsbin(auditstring, lastmask);
	}   
	
	ADDMASK(lastmask, usremask);
	SUBMASK(lastmask, usrdmask);

	return 0;
}
