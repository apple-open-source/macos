#ifndef __SERVERLIST__
#define __SERVERLIST__

#include "nbputilities.h"

extern "C" {
/* Name Binding Protocol (NBP) Functions */

int nbp_parse_entity(at_entity_t *entity,
		     char *str);
int nbp_make_entity(at_entity_t *entity, 
		    char *obj, 
		    char *type, 
		    char *zone);
int nbp_confirm(at_entity_t *entity,
		at_inet_t *dest,
		at_retry_t *retry);
int nbp_lookup(at_entity_t *entity,
	       at_nbptuple_t *buf,
	       int max,
	       at_retry_t *retry);
int nbp_register(at_entity_t *entity, 
		 int fd, 
		 at_retry_t *retry);
int nbp_remove(at_entity_t *entity, 
	       int fd);	     

};

#endif


