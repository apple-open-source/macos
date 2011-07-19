/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MANAGESIEVE_PROXY_H
#define __MANAGESIEVE_PROXY_H

void managesieve_proxy_reset(struct client *client);
int managesieve_proxy_parse_line(struct client *client, const char *line);

#endif
