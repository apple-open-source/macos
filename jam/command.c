/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * command.c - maintain lists of commands
 *
 * Each target has a list of actions that are to be applied to it, but due
 * to modifications on the actions they do not map one-to-one to the commands
 * that are the be executed against the target.  The CMD datatype holds
 * a single command that is to be executed against a target, and they can
 * chain together to represent the full collection of commands used to
 * update a target.
 *
 * External routines:
 *	cmd_new() - make a new CMD and chain it
 *	cmd_free() - free a CMD
 *
 * Macros:
 *	cmd_next() - follow chain of CMDs	
 */

# include "jam.h"

# include "lists.h"
# include "parse.h"
# include "variable.h"
# include "rules.h"

# include "command.h"

/*
 * cmd_new() - make a new CMD and chain it
 */

CMD *
cmd_new( chain, rule, targets, sources, shell, exportvars )
CMD	*chain;
RULE	*rule;
LIST	*targets;
LIST	*sources;
LIST	*shell;
int	exportvars;
{
	int     len;

	CMD *cmd = (CMD *)malloc( sizeof( CMD ) );

	cmd->rule = rule;
	cmd->shell = shell;
	cmd->exportvars = exportvars;

	lol_init( &cmd->args );
	lol_add( &cmd->args, targets );
	lol_add( &cmd->args, sources );

	len = var_string( rule->actions, cmd->buf, CMDBUF, &cmd->args );
	
	if( len < 0 )
	{
	    printf( "fatal error: %s command block too long (max %d)\n", 
		rule->name, CMDBUF );
	    exit( EXITBAD );
	}

	if( !chain ) chain = cmd;
	else chain->tail->next = cmd;
	chain->tail = cmd;
	cmd->next = 0;

	return chain;
}

/*
 * cmd_free() - free a CMD
 */

void
cmd_free( cmd )
CMD	*cmd;
{
	lol_free( &cmd->args );
	list_free( cmd->shell );
	free( (char *)cmd );
}
