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

#ifndef URLAUTH_PLUGIN_H
#define URLAUTH_PLUGIN_H

#include "imap-url.h"
#include "mail-storage.h"

#define URLAUTH_KEY_BYTES		16

extern const char urlauth_plugin_binary_dependency[];

struct module;

void urlauth_plugin_init(struct module *module);
void urlauth_plugin_deinit(void);

void urlauth_keys_init(void);
void urlauth_keys_deinit(void);
bool urlauth_keys_get(struct mailbox *box, buffer_t *key);
bool urlauth_keys_set(struct mailbox *box);
bool urlauth_keys_delete(struct mailbox *box);
bool urlauth_keys_reset(struct mailbox_list *list);

bool urlauth_url_validate(const struct imap_url_parts *parts, bool full,
			  const char **error);

void urlauth_urlauth_generate_internal(const char *rump, const buffer_t *key,
				       string_t *urlauth);

#endif
