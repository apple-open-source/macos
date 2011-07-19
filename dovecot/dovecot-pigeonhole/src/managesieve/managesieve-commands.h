/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MANAGESIEVE_COMMANDS_H
#define __MANAGESIEVE_COMMANDS_H

struct client_command_context;

#include "managesieve-parser.h"

typedef bool command_func_t(struct client_command_context *cmd);

struct command {
	const char *name;
	command_func_t *func;
};

/* Register command. Given name parameter must be permanently stored until
   command is unregistered. */
void command_register(const char *name, command_func_t *func);
void command_unregister(const char *name);

/* Register array of commands. */
void command_register_array(const struct command *cmdarr, unsigned int count);
void command_unregister_array(const struct command *cmdarr, unsigned int count);

struct command *command_find(const char *name);

void commands_init(void);
void commands_deinit(void);

/* MANAGESIEVE commands: */

/* Non-Authenticated State */
extern bool cmd_logout(struct client_command_context *cmd);
extern bool cmd_capability(struct client_command_context *cmd);
extern bool cmd_noop(struct client_command_context *cmd);

/* APPLE - <rdar://problem/9119321> */
extern bool cmd_forbidden(struct client_command_context *cmd);

/* Authenticated State */
extern bool cmd_putscript(struct client_command_context *cmd);
extern bool cmd_checkscript(struct client_command_context *cmd);
extern bool cmd_getscript(struct client_command_context *cmd);
extern bool cmd_setactive(struct client_command_context *cmd);
extern bool cmd_deletescript(struct client_command_context *cmd);
extern bool cmd_listscripts(struct client_command_context *cmd);
extern bool cmd_havespace(struct client_command_context *cmd);
extern bool cmd_renamescript(struct client_command_context *cmd);

#endif /* __MANAGESIEVE_COMMANDS_H */
