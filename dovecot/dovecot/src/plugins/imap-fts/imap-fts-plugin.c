/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

#include "imap-common.h"
#include "imap-commands.h"
#include "str.h"
#include "fts-plugin.h"
#include "imap-fts-plugin.h"

const char *imap_fts_plugin_version = DOVECOT_VERSION;

static struct module *imap_fts_module;
static void (*next_hook_client_created)(struct client **client);

static bool cmd_fts_compact(struct client_command_context *cmd)
{
	if (!client_verify_open_mailbox(cmd))
		return TRUE;

	fts_compact(cmd->client->mailbox);
	client_send_tagline(cmd, "OK Compacted.");
	return TRUE;
}

static void imap_fts_client_created(struct client **client)
{
	if (mail_user_is_plugin_loaded((*client)->user, imap_fts_module))
		str_append((*client)->capability_string, " X-FTS-COMPACT");

	if (next_hook_client_created != NULL)
		next_hook_client_created(client);
}

void imap_fts_plugin_init(struct module *module)
{
	command_register("X-FTS-COMPACT", cmd_fts_compact, 0);

	imap_fts_module = module;
	next_hook_client_created =
		imap_client_created_hook_set(imap_fts_client_created);
}

void imap_fts_plugin_deinit(void)
{
	command_unregister("X-FTS-COMPACT");

	imap_client_created_hook_set(next_hook_client_created);
}

const char *imap_fts_plugin_dependencies[] = { "fts", NULL };
const char imap_fts_plugin_binary_dependency[] = "imap";
