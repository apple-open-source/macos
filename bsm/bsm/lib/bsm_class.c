#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <libbsm.h>

/*
 * Parse the contents of the audit_class file to return 
 * struct au_class_ent entries
 */   
static FILE *fp = NULL;
static char linestr[AU_LINE_MAX];
static char *delim = ":";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * XXX The reentrant versions of the following functions is TBD
 * XXX struct au_class_ent *getclassent_r(au_class_ent_t *class_int); 
 * XXX struct au_class_ent *getclassnam_r(au_class_ent_t *class_int, const char *name); 
 */



/*
 * Allocate a au_class_ent structure
 */  
static struct au_class_ent *get_class_area()
{
	struct au_class_ent *c;
		
	c = (struct au_class_ent *) malloc (sizeof(struct au_class_ent));
	if(c == NULL) {
		return NULL;
	}
	c->ac_name = (char *)malloc(AU_CLASS_NAME_MAX * sizeof(char));
	if(c->ac_name == NULL) {
		free(c);
		return NULL;
	}
	c->ac_desc = (char *)malloc(AU_CLASS_DESC_MAX * sizeof(char));
	if(c->ac_desc == NULL) {
		free(c->ac_name);
		free(c);
		return NULL;
	}

	return c;
}


/*
 * Free the au_class_ent structure
 */   
void free_au_class_ent(struct au_class_ent *c)
{
    if (c)
    {
	if (c->ac_name)
	    free(c->ac_name);
	if (c->ac_desc)
	    free(c->ac_desc);
	free(c);
    }
}

/*
 * Parse a single line from the audit_class file passed in str
 * to the struct au_class_ent elements; store the result in c
 */   
static struct au_class_ent *classfromstr(char *str, char *delim, struct au_class_ent *c) 
{
	char *classname, *classdesc, *classflag;
	char *last;

	/* each line contains flag:name:desc */		
	classflag = strtok_r(str, delim, &last);
	classname = strtok_r(NULL, delim, &last);
	classdesc = strtok_r(NULL, delim, &last);

	if((classflag == NULL) 
		|| (classname == NULL)
		|| (classdesc == NULL)) {

		return NULL;
	}		

	/*
	 * Check for very large classnames
	 */  
	if(strlen(classname) >= AU_CLASS_NAME_MAX) {
		return NULL;
	}

	strcpy(c->ac_name, classname);

	/*
	 * Check for very large class description
	 */  
	if(strlen(classdesc) >= AU_CLASS_DESC_MAX) {
		return NULL;
	}
	strcpy(c->ac_desc, classdesc);

	c->ac_class = strtoul(classflag, (char **) NULL, 0);

	return c;
}

/*
 * Return the next au_class_ent structure from the file
 * setauclass should be called before invoking this function
 * for the first time  
 */  
struct au_class_ent *getauclassent()
{
	struct au_class_ent *c;
	char *tokptr, *nl;	
	
	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_CLASS_FILE, "r")) == NULL)) {
		
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	if(fgets(linestr, AU_LINE_MAX, fp) == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	/* Remove trailing new line character */
	if((nl = strrchr(linestr, '\n')) != NULL) {
		*nl = '\0';
	}
	
	tokptr = linestr;
	
	c = get_class_area(); /* allocate */
	if(c == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	/* parse tokptr to au_class_ent components */	
	if(classfromstr(tokptr, delim, c) == NULL) {

		free_au_class_ent(c);

		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	pthread_mutex_unlock(&mutex);
	return c;
}

/*
 * Return the next au_class_entry having the given class name
 */  
struct au_class_ent *getauclassnam(const char *name)
{
	struct au_class_ent *c;
	char *nl;

	if(name == NULL) {
		return NULL;
	}

	/* Rewind to beginning of file */
	setauclass();
		
	pthread_mutex_lock(&mutex);

	if((fp == NULL) 
		&& ((fp = fopen(AUDIT_CLASS_FILE, "r")) == NULL)) {
		
		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	
	c = get_class_area(); /* allocate */ 
	if(c == NULL) {

		pthread_mutex_unlock(&mutex);
		return NULL;
	}
	while(fgets(linestr, AU_LINE_MAX, fp) != NULL) {
		/* Remove trailing new line character */
		if((nl = strrchr(linestr, '\n')) != NULL) {
			*nl = '\0';
		}

		/* parse tokptr to au_class_ent components */	
		if(classfromstr(linestr, delim, c) != NULL) {
			if(!strcmp(name, c->ac_name)) {
					
				pthread_mutex_unlock(&mutex);
				return c;
			}
		}	
	}

	free_au_class_ent(c);

	pthread_mutex_unlock(&mutex);
	return NULL;

}

/*
 * Rewind to the beginning of the enumeration
 */  
void setauclass()
{
	pthread_mutex_lock(&mutex);

	if(fp != NULL) {
		fseek(fp, 0, SEEK_SET);
	}

	pthread_mutex_unlock(&mutex);
}

/*
 * audit_class processing is complete; close any open files 
 */  
void endauclass()
{
	pthread_mutex_lock(&mutex);
	
	if(fp != NULL) {
		fclose(fp);
		fp = NULL;
	}

	pthread_mutex_unlock(&mutex);
}
