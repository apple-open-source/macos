/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef CONF_H
#define CONF_H

/*
 * $Id: conf.h,v 1.1.1.1 2002/01/31 07:09:05 zarzycki Exp $
 */


#include "pset.h"
#include "sconf.h"


struct configuration
{
   pset_h cnf_service_confs ;
   struct service_config *cnf_defaults ;
} ;

#define CNF_DEFAULTS( confp )        (confp)->cnf_defaults
#define CNF_SERVICE_CONFS( confp )   (confp)->cnf_service_confs

void cnf_free(struct configuration *confp);
struct service_config *cnf_extract(struct configuration *confp,struct service_config *scp);
void cnf_dump(struct configuration *confp,int fd);
status_e cnf_init(struct configuration *confp,int *fdp,psi_h *iterp);
unsigned cnf_start_services(struct configuration *confp);


#endif    /* CONF_H */

