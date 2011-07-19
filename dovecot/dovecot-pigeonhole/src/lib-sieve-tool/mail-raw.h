/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MAIL_RAW_H
#define __MAIL_RAW_H

#include "lib.h"
#include "master-service.h"

struct mail_raw {
	pool_t pool;
	struct mail *mail;

	struct mailbox *box;
	struct mailbox_transaction_context *trans;
};

struct mail_raw_user *mail_raw_user_init
(struct master_service *service, const char *username,
	struct mail_user *mail_user);
void mail_raw_user_deinit(struct mail_raw_user **_ruser);

struct mail_raw *mail_raw_open_file
	(struct mail_raw_user *ruser, const char *path);
struct mail_raw *mail_raw_open_data
	(struct mail_raw_user *ruser, string_t *mail_data);
void mail_raw_close(struct mail_raw **mailr);


#endif /* __MAIL_RAW_H */
