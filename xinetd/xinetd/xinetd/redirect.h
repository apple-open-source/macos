#ifndef REDIRECT_H
#define REDIRECT_H

#include "server.h"

#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
void redir_handler(struct server *serp);

#endif
