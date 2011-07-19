/* Dtrace providers for IMAP and POP3 */

/*
 * Copyright (c) 2008-2011 Apple Inc. All rights reserved.
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

/*
 * Dtrace sort of but not really understands struct types.  To avoid
 * warnings the arguments use only simple C types.  The actual types
 * are:
 *	struct db_od *od_info
 *	struct od_user *user_rec
 *	struct client_command_context *cmd
 *	struct client *client
 * Dtrace doesn't understand const either.  Phooey.
 */

provider dovecot {
	probe od__lookup__cached(void *od_info, char *user_name,
				 void *user_rec);
	probe od__lookup__start(void *od_info, char *user_name);
	probe od__lookup__finish(void *od_info, char *user_name,
				 void *user_rec);

	probe od__sacl__start(void *user_rec, char *group);
	probe od__sacl__finish(void *user_rec, char *group, int ret);

	probe imap__login__command__start(void *client, char *cmd_name,
					  void *args);
	probe imap__login__command__finish(void *client, char *cmd_name,
					   void *args, int ret);

	probe imap__command__start(void *cmd);
	probe imap__command__finish(void *cmd);

	probe pop3__login__command__start(void *client, char *line,
					  char *args);
	probe pop3__login__command__finish(void *client, char *line,
					   char *args, int result);

	probe pop3__command__start(void *client, char *line, char *args);
	probe pop3__command__finish(void *client, char *line, char *args,
				    int ret);
};
