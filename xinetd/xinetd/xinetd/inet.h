#ifndef X_INET_H
#define X_INET_H

#include "pset.h"
#include "sconf.h"
#include "conf.h"

int get_next_inet_entry(int fd,pset_h sconfs,struct service_config *defaults);
void parse_inet_conf_file(int fd,struct configuration *confp);

#endif
