/*
 * Copyright 1994 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * command.h - the CMD structure and routines to manipulate them
 *
 * Both ACTION and CMD contain a rule, targets, and sources.  An
 * ACTION describes a rule to be applied to the given targets and
 * sources; a CMD is what actually gets executed by the shell.  The
 * differences are due to:
 *
 *	ACTIONS must be combined if 'actions together' is given.
 *	ACTIONS must be split if 'actions piecemeal' is given.
 *	ACTIONS must have current sources omitted for 'actions updated'.
 */

/*
 * CMD - an action, ready to be formatted into a buffer and executed
 */

typedef struct _cmd CMD;

struct _cmd
{
	CMD	*next;
	CMD	*tail;		/* valid on in head */
	RULE	*rule;		/* rule->actions contains shell script */
	LIST	*shell;		/* $(SHELL) value */
	int	exportvars;	/* nonzero to export variables using setenv */
	LOL	args;		/* LISTs for $(<), $(>) */
	char	buf[ CMDBUF ];	/* actual commands */
} ;

CMD 	*cmd_new();
void	cmd_free();

# define cmd_next( c ) ((c)->next)
