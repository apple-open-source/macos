/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * make1.c - execute command to bring targets up to date
 *
 * This module contains make1(), the entry point called by make() to 
 * recursively decend the dependency graph executing update actions as
 * marked by make0().
 *
 * External routines:
 *
 *	make1() - execute commands to update a TARGET and all its dependents
 *
 * Internal routines, the recursive/asynchronous command executors:
 *
 *	make1a() - recursively traverse target tree, calling make1b()
 *	make1b() - dependents of target built, now build target with make1c()
 *	make1c() - launch target's next command, call make1b() when done
 *	make1d() - handle command execution completion and call back make1c()
 *
 * Internal support routines:
 *
 *	make1cmds() - turn ACTIONS into CMDs, grouping, splitting, etc
 *	make1chunk() - compute number of source that can fit on cmd line
 *	make1list() - turn a list of targets into a LIST, for $(<) and $(>)
 * 	make1settings() - for vars that get bound values, build up replacement lists
 * 	make1bind() - bind targets that weren't bound in dependency analysis
 *
 * 04/16/94 (seiwald) - Split from make.c.
 * 04/21/94 (seiwald) - Handle empty "updated" actions.
 * 05/04/94 (seiwald) - async multiprocess (-j) support
 * 06/01/94 (seiwald) - new 'actions existing' does existing sources
 * 12/20/94 (seiwald) - NOTIME renamed NOTFILE.
 * 01/19/95 (seiwald) - distinguish between CANTFIND/CANTMAKE targets.
 * 01/22/94 (seiwald) - pass per-target JAMSHELL down to execcmd().
 * 02/28/95 (seiwald) - Handle empty "existing" actions.
 * 03/10/95 (seiwald) - Fancy counts.
 */

# include "jam.h"

# include "lists.h"
# include "parse.h"
# include "variable.h"
# include "rules.h"

# include "search.h"
# include "newstr.h"
# include "make.h"
# include "command.h"
# include "execcmd.h"
# include <unistd.h>

static void make1a();
static void make1b();
static void make1c();
static void make1d();

static CMD *make1cmds();
static int make1chunk();
static LIST *make1list();
static SETTINGS *make1settings();
static void make1bind();

/* Ugly static - it's too hard to carry it through the callbacks. */

static struct {
	int	failed;
	int	skipped;
	int	total;
	int	made;
} counts[1] ;

/*
 * make1() - execute commands to update a TARGET and all its dependents
 */

static int intr = 0;

int
make1( t )
TARGET *t;
{
	memset( (char *)counts, 0, sizeof( *counts ) );

	/* Recursively make the target and its dependents */

	make1a( t, (TARGET *)0 );

	/* Wait for any outstanding commands to finish running. */

	while( execwait() )
	    ;

	/* Talk about it */

#ifdef APPLE_EXTENSIONS
	if( PARSABLE_OUTPUT )
	    pbx_printf( "JEND", "%i %i %i\n", counts->made, counts->failed, counts->skipped );
#endif

	if( DEBUG_MAKE && counts->failed )
	    printf( "...failed updating %d target(s)...\n", counts->failed );

	if( DEBUG_MAKE && counts->skipped )
	    printf( "...skipped %d target(s)...\n", counts->skipped );

	if( DEBUG_MAKE && counts->made )
	    printf( "...updated %d target(s)...\n", counts->made );

	return counts->total != counts->made;
}

/*
 * make1a() - recursively traverse target tree, calling make1b()
 */

static void
make1a( t, parent )
TARGET	*t;
TARGET	*parent;
{
	TARGETS	*c;
	int i;

	/* If the parent is the first to try to build this target */
	/* or this target is in the make1c() quagmire, arrange for the */
	/* parent to be notified when this target is built. */

	if( parent )
	    switch( t->progress )
	{
	case T_MAKE_INIT:
	case T_MAKE_ACTIVE:
	case T_MAKE_RUNNING:
	    t->parents = targetentry( t->parents, parent );
	    parent->asynccnt++;
	}

	if( t->progress != T_MAKE_INIT )
	    return;

	/* Asynccnt counts the dependents preventing this target from */
	/* proceeding to make1b() for actual building.  We start off with */
	/* a count of 1 to prevent anything from happening until we can */
	/* call all dependents.  This 1 is accounted for when we call */
	/* make1b() ourselves, below. */

	t->asynccnt = 1;

	/* Recurse on our dependents, manipulating progress to guard */
	/* against circular dependency. */

	t->progress = T_MAKE_ONSTACK;

	for( i = T_DEPS_DEPENDS; i <= T_DEPS_INCLUDES; i++ )
	    for( c = t->deps[i]; c && !intr; c = c->next )
		make1a( c->target, t );

	t->progress = T_MAKE_ACTIVE;

	/* Now that all dependents have bumped asynccnt, we now allow */
	/* decrement our reference to asynccnt. */ 

	make1b( t );
}

/*
 * make1b() - dependents of target built, now build target with make1c()
 */

static void
make1b( t )
TARGET	*t;
{
	TARGETS	*c;
	int 	i;
	char 	*failed = "dependents";

	/* If any dependents are still outstanding, wait until they */
	/* call make1b() to signal their completion. */

	if( --t->asynccnt )
	    return;

	/* Now ready to build target 't'... if dependents built ok. */

	/* Collect status from dependents */

	for( i = T_DEPS_DEPENDS; i <= T_DEPS_INCLUDES; i++ )
	    for( c = t->deps[i]; c; c = c->next )
		if( c->target->status > t->status )
	{
	    failed = c->target->name;
	    t->status = c->target->status;
	}

	/* If actions on deps have failed, bail. */
	/* Otherwise, execute all actions to make target */

	if( t->status == EXEC_CMD_FAIL && t->actions )
	{
	    ++counts->skipped;
	    printf( "...skipped %s for lack of %s...\n", t->name, failed );
	}

	if( t->status == EXEC_CMD_OK )
	    switch( t->fate )
	{
	case T_FATE_INIT:
	case T_FATE_MAKING:
	    /* shouldn't happen */

	case T_FATE_STABLE:
	case T_FATE_NEWER:
	    break;

	case T_FATE_CANTFIND:
	case T_FATE_CANTMAKE:
	    t->status = EXEC_CMD_FAIL;
	    break;

	case T_FATE_ISTMP:
	    if( DEBUG_MAKE )
		printf( "...using %s...\n", t->name );
	    break;

	case T_FATE_TOUCHED:
	case T_FATE_MISSING:
	case T_FATE_OUTDATED:
	case T_FATE_UPDATE:
	    /* Set "on target" vars, build actions, unset vars */
	    /* Set "progress" so that make1c() counts this target among */
	    /* the successes/failures. */

	    if( t->actions )
	    {
		counts->total++;

#ifdef APPLE_EXTENSIONS
		if( PARSABLE_OUTPUT )
		    pbx_printf( "JUPD", "%i\n", counts->total );
#else
		if( DEBUG_MAKE && !( counts->total % 100 ) )
		    printf( "...on %dth target...\n", counts->total );
#endif

		pushsettings( t->settings );
		t->cmds = (char *)make1cmds( t->actions );
		popsettings( t->settings );

		t->progress = T_MAKE_RUNNING;
	    }

	    break;
	}

	/* Call make1c() to begin the execution of the chain of commands */
	/* needed to build target.  If we're not going to build target */
	/* (because of dependency failures or because no commands need to */
	/* be run) the chain will be empty and make1c() will directly */
	/* signal the completion of target. */

	make1c( t );
}

/*
 * make1c() - launch target's next command, call make1b() when done
 */

static void
make1c( t )
TARGET	*t;
{
	CMD	*cmd = (CMD *)t->cmds;

	/* If there are (more) commands to run to build this target */
	/* (and we haven't hit an error running earlier comands) we */
	/* launch the command with execcmd(). */
	
	/* If there are no more commands to run, we collect the status */
	/* from all the actions then report our completion to all the */
	/* parents. */

	if( cmd && t->status == EXEC_CMD_OK )
	{
#ifdef APPLE_EXTENSIONS
	    if( PARSABLE_OUTPUT /*&& !( cmd->rule->flags & RULE_QUIETLY )*/ )
	    {
		unsigned	i = 0;
		LIST		*l;

		pbx_printf( ( cmd->rule->flags & RULE_QUIETLY ) ? "rl00" : "RL00", "%s|", cmd->rule->name );
		while ( ( l = lol_get( &cmd->args, i++ ) ) != NULL )
		    printf( "%s|", l->string );
		printf( "\n" );
	    }
#endif
	    if( DEBUG_MAKE )
		if( DEBUG_MAKEQ || ! ( cmd->rule->flags & RULE_QUIETLY ) )
		{
		    printf( "%s ", cmd->rule->name );
		    list_print( lol_get( &cmd->args, 0 ) );
		    printf( "\n" );
		}

	    if( DEBUG_EXEC )
		printf( "%s\n", cmd->buf );

#ifdef APPLE_EXTENSIONS
	    if( PARSABLE_OUTPUT )
	    {
            pbx_printf( "CB00", "%s\n", cmd->buf );
            pbx_printf( "CO00", "\n" );
	    }
#endif

	    if( globs.noexec )
	    {
		make1d( t, EXEC_CMD_OK );
	    } 
	    else 
	    {
		fflush( stdout );
		execcmd( cmd->buf, make1d, t, cmd->shell, cmd->exportvars );
	    }
	}
	else
	{
	    TARGETS	*c;
	    ACTIONS	*actions;

	    /* Collect status from actions, and distribute it as well */

	    for( actions = t->actions; actions; actions = actions->next )
		if( actions->action->status > t->status )
		    t->status = actions->action->status;

	    for( actions = t->actions; actions; actions = actions->next )
		if( t->status > actions->action->status )
		    actions->action->status = t->status;

	    /* Tally success/failure for those we tried to update. */

	    if( t->progress == T_MAKE_RUNNING )
		switch( t->status )
	    {
	    case EXEC_CMD_OK:
		++counts->made;
		break;
	    case EXEC_CMD_FAIL:
		++counts->failed;
		break;
	    }

	    /* Tell parents dependent has been built */

	    t->progress = T_MAKE_DONE;

	    for( c = t->parents; c; c = c->next )
		make1b( c->target );
	}
}

/*
 * make1d() - handle command execution completion and call back make1c()
 */

static void
make1d( t, status )
TARGET	*t;
int	status;
{
	CMD	*cmd = (CMD *)t->cmds;

#ifdef APPLE_EXTENSIONS
	if( PARSABLE_OUTPUT )
	    pbx_printf( "CE00", "\n" );
#endif

	/* Execcmd() has completed.  All we need to do is fiddle with the */
	/* status and signal our completion so make1c() can run the next */
	/* command.  On interrupts, we bail heavily. */

	if( status == EXEC_CMD_FAIL && ( cmd->rule->flags & RULE_IGNORE ) )
	    status = EXEC_CMD_OK;

	/* On interrupt, set intr so _everything_ fails */

	if( status == EXEC_CMD_INTR )
	    ++intr;

	if( status == EXEC_CMD_FAIL && DEBUG_MAKE )
	{
	    /* Print command text on failure */

	    if( !DEBUG_EXEC )
		printf( "%s\n", cmd->buf );

	    printf( "...failed %s ", cmd->rule->name );
	    list_print( lol_get( &cmd->args, 0 ) );
	    printf( "...\n" );
	}

	/* If the command was interrupted or failed and the target */
	/* is not "precious", remove the targets */

	if( status != EXEC_CMD_OK && !( cmd->rule->flags & RULE_TOGETHER ) )
	{
	    LIST *targets = lol_get( &cmd->args, 0 );

	    for( ; targets; targets = list_next( targets ) )
		if( !unlink( targets->string ) )
		    printf( "...removing %s\n", targets->string );
	}


    if( status != EXEC_CMD_OK && !globs.ignore )
        exit(EXEC_CMD_FAIL);
    
	/* Free this command and call make1c() to move onto next command. */
	t->status = status;
	t->cmds = (char *)cmd_next( cmd );

	cmd_free( cmd );

	make1c( t );
}

/*
 * make1cmds() - turn ACTIONS into CMDs, grouping, splitting, etc
 *
 * Essentially copies a chain of ACTIONs to a chain of CMDs, 
 * grouping RULE_TOGETHER actions, splitting RULE_PIECEMEAL actions,
 * and handling RULE_NEWSRCS actions.  The result is a chain of
 * CMDs which can be expanded by var_string() and executed with
 * execcmd().
 */

static CMD *
make1cmds( a0 )
ACTIONS	*a0;
{
	CMD *cmds = 0;
	LIST *shell = var_get( "JAMSHELL" );	/* shell is per-target */

	/* Step through actions */
	/* Actions may be shared with other targets or grouped with */
	/* RULE_TOGETHER, so actions already seen are skipped. */

	for( ; a0; a0 = a0->next )
	{
	    RULE    *rule = a0->action->rule;
	    SETTINGS *boundvars;
	    int	    chunk = 0;
	    LIST    *nt, *ns;
	    ACTIONS *a1;

	    /* Only do rules with commands to execute. */
	    /* If this action has already been executed, use saved status */

	    if( !rule->actions || a0->action->running )
		continue;

	    a0->action->running = 1;
	    
	    /* Make LISTS of targets and sources */
	    /* If `execute together` has been specified for this rule, tack */
	    /* on sources from each instance of this rule for this target. */

	    nt = make1list( L0, a0->action->targets, 0 );
	    ns = make1list( L0, a0->action->sources, rule->flags );

	    if( rule->flags & RULE_TOGETHER )
		for( a1 = a0->next; a1; a1 = a1->next )
		    if( a1->action->rule == rule && !a1->action->running )
	    {
		ns = make1list( ns, a1->action->sources, rule->flags );
		a1->action->running = 1;
	    }

	    /* If doing only updated (or existing) sources, but none have */
	    /* been updated (or exist), skip this action. */

	    if( !ns && ( rule->flags & ( RULE_NEWSRCS | RULE_EXISTING ) ) )
	    {
		list_free( nt );
		continue;
	    }

	    /* If we had 'actions xxx bind vars' we bind the vars now */

	    boundvars = make1settings( rule->bindlist );
	    pushsettings( boundvars );

	    /* If rule is to be cut into (at most) MAXLINE pieces, estimate */
	    /* bytes per $(>) element and aim for using MAXLINE minus a */
	    /* fudgefactor. */

	    if( rule->flags & RULE_PIECEMEAL )
		chunk = make1chunk( rule->actions, nt, ns );

	    /* Either cut the actions into pieces, or do it whole. */

	    if( chunk < 0 )
	    {
		printf( "fatal error: %s command line too long (max %d)\n", 
			rule->name, MAXLINE );
		exit( EXITBAD );
	    }

	    if( chunk )
	    {
		int  start;
		LIST *somes;

		if( DEBUG_EXECCMD )
		    printf( "%s: %d args per exec\n", rule->name, chunk );

		for( start = 0;
		     somes = list_sublist( ns, start, chunk );
		     start += chunk )
		{
		    cmds = cmd_new( cmds, rule, 
				list_copy( L0, nt ), somes, 
				list_copy( L0, shell ), (rule-> flags & RULE_EXPORTVARS) ? 1 : 0 );
		}

		list_free( nt );
		list_free( ns );
	    }
	    else
	    {
		cmds = cmd_new( cmds, rule, nt, ns, list_copy( L0, shell ), (rule-> flags & RULE_EXPORTVARS) ? 1 : 0 );
	    }

	    /* Free the variables whose values were bound by */
	    /* 'actions xxx bind vars' */

	    popsettings( boundvars );
	    freesettings( boundvars );
	}

	return cmds;
}

/*
 * make1chunk() - compute number of source that can fit on cmd line
 */

static int
make1chunk( cmd, targets, sources )
char	*cmd;
LIST	*targets;
LIST	*sources;
{
	int onesize;
	int twosize;
	int chunk = 0;
	char buf[ MAXLINE ];
	LOL lol;

	/* XXX -- egregious manipulation of lol */
	/* a) we set items directly, b) we don't free it */

	lol_init( &lol );
	lol.count = 2;
	lol.list[0] = targets;

	lol.list[1] = list_sublist( sources, 0, 1 );
	onesize = var_string( cmd, buf, MAXLINE, &lol );
	list_free( lol.list[1] );

	if( onesize < 0 )
	    return -1;

	lol.list[1] = list_sublist( sources, 0, 2 );
	twosize = var_string( cmd, buf, MAXLINE, &lol );
	list_free( lol.list[1] );

	if( twosize < 0 )
	    return -1;

	if( twosize > onesize )
	    chunk = 3 * ( MAXLINE - onesize ) / 5 / ( twosize - onesize ) + 1;

	return chunk;
}


/*
 * make1list() - turn a list of targets into a LIST, for $(<) and $(>)
 */

static LIST *
make1list( l, targets, flags )
LIST	*l;
TARGETS	*targets;
int	flags;
{
    for( ; targets; targets = targets->next )
    {
	TARGET *t = targets->target;

	/* Sources to 'actions existing' are never in the dependency */
	/* graph (if they were, they'd get built and 'existing' would */
	/* be superfluous, so throttle warning message about independent */
	/* targets. */

	if( t->binding == T_BIND_UNBOUND )
	    make1bind( t, !( flags & RULE_EXISTING ) );

	if( ( flags & RULE_EXISTING ) && t->binding != T_BIND_EXISTS )
	    continue;

	if( ( flags & RULE_NEWSRCS ) && t->fate <= T_FATE_STABLE )
	    continue;

	/* Prohibit duplicates for RULE_TOGETHER */

	if( flags & RULE_TOGETHER )
	{
	    LIST *m;

	    for( m = l; m; m = m->next )
		if( !strcmp( m->string, t->boundname ) )
		    break;

	    if( m )
		continue;
	}

	/* Build new list */

	l = list_new( l, copystr( t->boundname ) );
    }

    return l;
}

/*
 * make1settings() - for vars that get bound values, build up replacement lists
 */

static SETTINGS *
make1settings( vars )
LIST	*vars;
{
	SETTINGS *settings = 0;

	for( ; vars; vars = list_next( vars ) )
	{
	    LIST *l = var_get( vars->string );
	    LIST *nl = 0;

	    for( ; l; l = list_next( l ) ) 
	    {
		TARGET *t = bindtarget( l->string );

		/* Make sure the target is bound, warning if it is not in the */
		/* dependency graph. */

		if( t->binding == T_BIND_UNBOUND )
		    make1bind( t, 1 );

		/* Build new list */

		nl = list_new( nl, copystr( t->boundname ) );
	    }

	    /* Add to settings chain */

	    settings = addsettings( settings, 0, vars->string, nl );
	}

	return settings;
}

/*
 * make1bind() - bind targets that weren't bound in dependency analysis
 *
 * Spot the kludge!  If a target is not in the dependency tree, it didn't 
 * get bound by make0(), so we have to do it here.  Ugly.
 */

static void
make1bind( t, warn )
TARGET	*t;
int	warn;
{
	if( t->flags & T_FLAG_NOTFILE )
	    return;

	/* Sources to 'actions existing' are never in the dependency */
	/* graph (if they were, they'd get built and 'existing' would */
	/* be superfluous, so throttle warning message about independent */
	/* targets. */

	if( warn )
	    printf( "warning: using independent target %s\n", t->name );

	pushsettings( t->settings );
	t->boundname = search( t->name, &t->time );
	t->binding = t->time ? T_BIND_EXISTS : T_BIND_MISSING;
	popsettings( t->settings );
}
