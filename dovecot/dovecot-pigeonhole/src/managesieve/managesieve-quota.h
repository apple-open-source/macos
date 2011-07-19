/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MANAGESIEVE_QUOTA_H
#define __MANAGESIEVE_QUOTA_H

bool managesieve_quota_check_validsize
	(struct client *client, size_t size);
bool managesieve_quota_check_all
	(struct client *client, const char *scriptname, size_t size);

#endif /* __MANAGESIEVE_QUOTA_H */
