/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# include "jam.h"
# include "lists.h"
# include "parse.h"
# include "variable.h"
# include "expand.h"
# include "hash.h"
# include "filesys.h"
# include "newstr.h"

/*
 * variable.c - handle jam multi-element variables
 *
 * External routines:
 *
 *	var_defines() - load a bunch of variable=value settings
 *	var_list() - variable expand an input list, generating a new list
 *	var_string() - expand a string with variables in it
 *	var_get() - get value of a user defined symbol
 *	var_set() - set a variable in jam's user defined symbol table
 *	var_swap() - swap a variable's value with the given one
 *	var_done() - free variable tables
 *
 * Internal routines:
 *
 *	var_enter() - make new var symbol table entry, returning var ptr
 *	var_dump() - dump a variable to stdout
 *
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 08/23/94 (seiwald) - Support for '+=' (append to variable)
 * 01/22/95 (seiwald) - split environment variables at blanks or :'s
 * 05/10/95 (seiwald) - split path variables at SPLITPATH (not :)
 * 12/09/00 (andersb) - added support for deferred variables
 */

static struct hash *varhash = 0;
static LIST *exported_variables = L0;

#ifdef APPLE_EXTENSIONS

typedef struct _def_asg DEF_ASG ;
struct _def_asgs {
	unsigned        count;
	unsigned        capacity;
	struct _def_asg {
	    char	    *symbol;
	    char	    *value;
	    unsigned	    append;
	}		asgs[0];
} ;
static struct _def_asgs *def_asgs = NULL;

#endif


/*
 * VARIABLE - a user defined multi-value variable
 */

typedef struct _variable VARIABLE ;

struct _variable {
	char	*symbol;
	LIST	*value;
} ;

static VARIABLE	*var_enter();
static void	var_dump();



/*
 * var_export() - mark a symbol for later exporting to subshells that want it
 *
 */void
var_export( symbol )
char *symbol;
{
    LIST * l = exported_variables;

    while( l  &&  strcmp(symbol, l->string) != 0 )
	l = list_next( l );
    if( l == NULL )
    {
	if( DEBUG_COMPILE )
	    printf( "marking %s for export\n", symbol );
	exported_variables = list_new( exported_variables, newstr(symbol) );
    }
}


/*
 * var_defines() - load a bunch of variable=value settings
 *
 * If variable name ends in PATH, split value at :'s.  
 * Otherwise, split at blanks.
 */

void
var_defines( e, export )
char **e;
int export;
{
	for( ; *e; e++ )
	{
	    char *val;

	    if( val = strchr( *e, '=' ) )
	    {
		LIST *l = L0;
#if !defined(__APPLE__)
		int pp, p;
#endif
		char buf[ MAXSYM ];
#if defined(__APPLE__)
		l = list_new( l, newstr( val + 1 ) );
#else
		char split = ' ';

		/* Split *PATH at :'s, not spaces */

		if( val - 4 >= *e )
		{
		    if( !strncmp( val - 4, "PATH", 4 ) ||
		        !strncmp( val - 4, "Path", 4 ) ||
		        !strncmp( val - 4, "path", 4 ) )
			    split = SPLITPATH;
		}

		/* Do the split */

		for( pp = val + 1; p = strchr( pp, split ); pp = p + 1 )
		{
		    strncpy( buf, pp, p - pp );
		    buf[ p - pp ] = '\0';
		    l = list_new( l, newstr( buf ) );
		}

		l = list_new( l, newstr( pp ) );
#endif
		/* Get name */

		strncpy( buf, *e, val - *e );
		buf[ val - *e ] = '\0';

		var_set( buf, l, VAR_SET , export /* export-in-environment */ );
	    }
	}
}

/*
 * var_list() - variable expand an input list, generating a new list
 *
 * Returns a newly created list.
 */

LIST *
var_list( ilist, lol )
LIST	*ilist;
LOL	*lol;
{
	LIST *olist = 0;

	while( ilist )
	{
	    char *s = ilist->string;
	    olist = var_expand( olist, s, s + strlen(s), lol, 1 );
	    ilist = list_next( ilist );
	}

	return olist;
}


/*
 * var_string() - expand a string with variables in it
 *
 * Copies in to out; doesn't modify targets & sources.
 */

int
var_string( in, out, outsize, lol )
char	*in;
char	*out;
int	outsize;
LOL	*lol;
{
	char 	*out0 = out;
	char	*oute = out + outsize - 1;

	while( *in )
	{
	    char	*lastword;
	    int		dollar = 0;

	    /* Copy white space */

	    while( isspace( *in ) )
	    {
		if( out >= oute )
		    return -1;

		*out++ = *in++;
	    }

	    lastword = out;

	    /* Copy non-white space, watching for variables */

	    while( *in && !isspace( *in ) )
	    {
	        if( out >= oute )
		    return -1;

		if( in[0] == '$' && in[1] == '(' )
		    dollar++;

		*out++ = *in++;
	    }

	    /* If a variable encountered, expand it and and embed the */
	    /* space-separated members of the list in the output. */

	    if( dollar )
	    {
		LIST	*l;

		l = var_expand( L0, lastword, out, lol, 0 );

		out = lastword;

		for( ; l; l = list_next( l ) )
		{
		    int so = strlen( l->string );

		    if( out + so >= oute )
			return -1;

		    strcpy( out, l->string );
		    out += so;
		    *out++ = ' ';
		}

		list_free( l );
	    }
	}

	if( out >= oute )
	    return -1;

	*out++ = '\0';

	return out - out0;
}

/*
 * var_get() - get value of a user defined symbol
 *
 * Returns NULL if symbol unset.
 */

LIST *
var_get( symbol )
char	*symbol;
{
	VARIABLE var, *v = &var;

	v->symbol = symbol;

	if( varhash && hashcheck( varhash, (HASHDATA **)&v ) )
	{
	    if( DEBUG_VARGET )
		var_dump( v->symbol, v->value, "get" );
	    return v->value;
	}
    
	return 0;
}

/*
 * var_set() - set a variable in jam's user defined symbol table
 *
 * 'flag' controls the relationship between new and old values of
 * the variable: SET replaces the old with the new; APPEND appends
 * the new to the old; DEFAULT only uses the new if the variable
 * was previously unset.
 *
 * Copies symbol.  Takes ownership of value.
 */

void
var_set( symbol, value, flag, export )
char	*symbol;
LIST	*value;
int	flag;
int	export;
{
	VARIABLE *v = var_enter( symbol );

	if( DEBUG_VARSET )
	    var_dump( symbol, value, "set" );

	switch( flag )
	{
	case VAR_SET:
	    /* Replace value */
	    list_free( v->value );
	    v->value = value;
	    break;

	case VAR_APPEND:
	    /* Append value */
	    v->value = list_append( v->value, value );
	    break;

	case VAR_DEFAULT:
	    /* Set only if unset */
	    if( !v->value )
		v->value = value;
	    else
		list_free( value );
	    break;
	}
	if( export )
	    var_export( symbol );
}

/*
 * var_swap() - swap a variable's value with the given one
 */

LIST *
var_swap( symbol, value )
char	*symbol;
LIST	*value;
{
	VARIABLE *v = var_enter( symbol );
	LIST 	 *oldvalue = v->value;

	if( DEBUG_VARSET )
	    var_dump( symbol, value, "set" );

	v->value = value;

	return oldvalue;
}



/*
 * var_enter() - make new var symbol table entry, returning var ptr
 */

static VARIABLE *
var_enter( symbol )
char	*symbol;
{
	VARIABLE var, *v = &var;

	if( !varhash )
	    varhash = hashinit( sizeof( VARIABLE ), "variables" );

	v->symbol = symbol;
	v->value = 0;

	if( hashenter( varhash, (HASHDATA **)&v ) )
	    v->symbol = newstr( symbol );	/* never freed */

	return v;
}

/*
 * var_dump() - dump a variable to stdout
 */

static void
var_dump( symbol, value, what )
char	*symbol;
LIST	*value;
char	*what;
{
	printf( "%s %s = ", what, symbol );
	list_print( value );
	printf( "\n" );
}

/*
 * var_done() - free variable tables
 */

void
var_done()
{
	hashdone( varhash );
}


#ifdef MEASURE_SETENV
static unsigned  setenv_num = 0;
static unsigned  setenv_bytes = 0;
#endif

void
var_setenv( symbol, value )
const char *symbol;
LIST *value;
{
	int shouldset = 1;
    #ifdef EXPORT_ONLY_NONFUNKY_IDENTIFIERS
	register const char *sp;
	register char ch;

	for( sp = symbol; (ch = *sp) != '\0'; sp++ )
	{
	    if( !(ch >= 'A' && ch <= 'Z') && !(ch >= 'a' && ch <= 'z') && !(ch >= '0' && ch <= '9') && !(ch == '_') )
	    {
		shouldset = 0;
		break;
	    }
	}
    #endif
	if( shouldset )
	{
	    const int MAXVALUE_LEN = (1024 * 8) - 1;      // an arbitrary limit...
	    char buffer[MAXVALUE_LEN+1];
	    register char * bp = buffer;
	    LIST * l;

	    for( l = value ; l != NULL  &&  (bp - buffer) < MAXVALUE_LEN; l = list_next( l ) )
	    {
		register const char * sp;
		register unsigned needsquotes = 0;
		register int len = 0;
		register int extralen = 0;

		if( bp != buffer  &&  (bp - buffer) + 1 <= MAXVALUE_LEN )
		    *bp++ = ' ';
		for( sp = l->string; *sp; sp++, len++ )
		{
		#ifdef SHOULD_QUOTE_SETENV_VARS 
		    register char ch = *sp;
		    if( !(ch >= 'A' && ch <= 'Z')  &&  !(ch >= 'a' && ch <= 'z')  &&  !(ch == '_') )
		    {
			needsquotes = 1;
			if( ch == '\\'  ||  ch == '\"' )
			    extralen++;
		    }
		#endif
		}
		if( (bp - buffer) + len + extralen + (needsquotes ? 2 : 0) > MAXVALUE_LEN )
		{
		    fprintf(stderr, "warning: maximum value length exceeded when setting variable '%s' in environment; truncating value to %u characters\n", symbol, MAXVALUE_LEN);
		    len = MAXVALUE_LEN - (bp - buffer) - extralen - (needsquotes ? 2 : 0);
		}
		if( needsquotes )
		{
		    if ( (bp - buffer) + 1 <= MAXVALUE_LEN )
			*bp++ = '\"';
		    for( sp = l->string; *sp && len >= 0; sp++, len-- )
		    {
			register char ch = *sp;
			if( ch == '\\'  ||  ch == '\"' )
			    *bp++ = '\\';
			*bp++ = ch;
		    }
		    if ( (bp - buffer) + 1 <= MAXVALUE_LEN )
			*bp++ = '\"';
		}
		else
		{
		    memcpy(bp, l->string, len);
		    bp += len;
		}
	    }
	    *bp = '\0';
	#ifdef DEBUG_VARIABLE_EXPORTING
	    printf("[setenv '%s' '%s']\n", symbol, buffer);
	#endif
	    setenv(symbol, buffer, 0);
	#ifdef MEASURE_SETENV
	    setenv_num++;
	    setenv_bytes += strlen(symbol) + 1 + strlen(buffer) + 1;
	#endif
	}
}

void var_setenv_all_exported_variables()
{
	register LIST	*l;

    #ifdef MEASURE_SETENV
	setenv_num = 0;
	setenv_bytes = 0;
    #endif
	for( l = exported_variables; l; l = list_next( l ) )
	    var_setenv( l->string, var_get( l->string ) );
    #ifdef MEASURE_SETENV
  	printf("[did setenv %u variables (%u bytes)]\n", setenv_num, setenv_bytes);
    #endif
}



#ifdef APPLE_EXTENSIONS


//
//  Simple, growable string buffers (used by deferred-variable support only)
//

struct string_buffer {
    unsigned  length;
    unsigned  capacity;
    char *    characters;
} ;


static void strbuf_init (struct string_buffer * sbuf)
{
    sbuf->length = 0;
    sbuf->capacity = 16;
    sbuf->characters = malloc(sbuf->capacity * sizeof(char));
}


static char * strbuf_characters (struct string_buffer * sbuf)
{
    return sbuf->characters;
}


static unsigned strbuf_length (struct string_buffer * sbuf)
{
    return sbuf->length;
}


static inline void strbuf_append_characters (struct string_buffer * sbuf, const char * characters, unsigned length)
{
    if (sbuf->length + length > sbuf->capacity) {
	sbuf->capacity += length;
	sbuf->characters = realloc(sbuf->characters, sbuf->capacity);
    }
    memcpy(sbuf->characters + sbuf->length, characters, length);
    sbuf->length += length;
}


static inline void strbuf_append_character (struct string_buffer * sbuf, char ch)
{
    if (sbuf->length + 1 > sbuf->capacity) {
	sbuf->capacity += 32;
	sbuf->characters = realloc(sbuf->characters, sbuf->capacity);
    }
    *(sbuf->characters + sbuf->length++) = ch;
}


static void strbuf_append_quoted_characters (struct string_buffer * sbuf, const char * characters, unsigned length, unsigned quote_only_if_needed)
{
    register unsigned   needs_quotes = 0;    // Do we really need to quote the string?
    register unsigned   extra_length = 2;    // For the two quotes, plus any escape characters
    register unsigned   i;

    for (i = 0; i < length; i++) {
	register char ch = characters[i];
	if (isspace(ch) || ch == '\\' || ch == '\"' || ch == '\'') {
	    needs_quotes = 1;
	    if( ch == '\\'  ||  ch == '\"' )
		extra_length++;
	}
    }
    if (quote_only_if_needed  &&  !needs_quotes) {
	extra_length = 0;
	strbuf_append_characters(sbuf, characters, length);
    }
    else {
	register char *   dstp;

	if (sbuf->length + length + extra_length > sbuf->capacity) {
	    sbuf->capacity += length + extra_length;
	    sbuf->characters = realloc(sbuf->characters, sbuf->capacity);
	}
	dstp = sbuf->characters + sbuf->length;
	*dstp++ = '\"';
	for (i = 0; i < length; i++) {
	    register char ch = characters[i];
	    if (ch == '\\'  ||  ch == '\"') {
		*dstp++ = '\\';
	    }
	    *dstp++ = ch;
	}
	*dstp++ = '\"';
	sbuf->length += length + extra_length;
    }
}


static void strbuf_free (struct string_buffer * sbuf)
{
    free(sbuf->characters);
}



//
//  Conversion back and forth between lists and shell-style command lines in string buffers
//

static void strbuf_append_list_as_command_line_arguments (struct string_buffer * sbuf, LIST * list)
    /** Appends the list elements in 'list' to 'sbuf', quoting each list element if needed, and adding a space in between each. Any required quoting is done according to standard Bourne shell command-line quoting rules. */
{
    while (list != NULL) {
	strbuf_append_quoted_characters(sbuf, list->string, strlen(list->string), 1);
	if (list->next != NULL) {
	    strbuf_append_character(sbuf, ' ');
	}
	list = list->next;
    }
}

static LIST * list_by_parsing_as_commandline_arguments (const char * characters, unsigned length)
    /** Creates and returns a new list by parsing the given string according to Bourne shell command-line rules. */
{
    LIST *                 list = NULL;
    char                   dst_buffer[10240];
    register unsigned      in_escape_char = 0;
    register unsigned      in_single_quote = 0;
    register unsigned      in_double_quote = 0;
    register unsigned      had_quote = 0;
    register const char *  sp = characters;
    register const char *  ep = characters + length;
    register char *        dp = dst_buffer;

    //printf("list_by_parsing_as_commandline_arguments(|"); fwrite(characters, length, 1, stdout); printf("|) {\n");
    while (sp < ep) {
	register unsigned ch = *sp++;
	if (in_escape_char) {
	    //printf("(appending escaped char '%c', leaving escape mode)\n", ch);
	    *dp++ = ch;
	    in_escape_char = 0;
	}
	else if (ch == '\\') {
	    //printf("(entering escape mode)\n");
	    in_escape_char = 1;
	}
	else if (ch == '\''  &&  !in_double_quote) {
	    //printf("(toggling single-quote mode to %u)\n", !in_single_quote);
	    in_single_quote = !in_single_quote;
	    had_quote = 1;
	}
	else if (ch == '\"'  &&  !in_single_quote) {
	    //printf("(toggling double-quote mode to %u)\n", !in_double_quote);
	    in_double_quote = !in_double_quote;
	    had_quote = 1;
	}
	else if ((ch == ' '  ||  ch == '\t'  ||  ch == '\n'  ||  ch == '\r')  &&  !in_single_quote  &&  !in_double_quote) {
	    if (dp > dst_buffer  ||  had_quote) {
		*dp = '\0';
		//printf("arg is |%s|\n", dst_buffer);
		list = list_new(list, newstr(dst_buffer));
		had_quote = 0;
		dp = dst_buffer;
	    }
	    //printf("(skipping whitespace character)\n");
	}
	else {
	    //printf("(appending normal char '%c')\n", ch);
	    *dp++ = ch;
	}
    }
    if (dp > dst_buffer  ||  had_quote) {
	*dp = '\0';
	//printf("arg is |%s|\n", dst_buffer);
	list = list_new(list, newstr(dst_buffer));
    }
    //printf("}\n");
    return list;
}



//
//  Make-style deferred expansion of variables
//

static int find_next_variable_reference (const char * characters, unsigned length, char ** varRefStartPtr, char ** varNameStartPtr, char ** varNameEndPtr, char ** varRefEndPtr)
    /** Private function used by var_append_expansion_of_deferred_variable() to iterate through all non-single-quoted, non-escaped variable references in a string. This function doesn't modify the string -- rather, it returns up to four character pointers indicating the start and end of the variable reference as a whole, and the start and end of the variable name within it. Returns 1 if it finds a variable reference, 0 if not. */
{
    #define is_variable_char(_ch_) (((_ch_) >= 'A' && (_ch_) <= 'Z') || ((_ch_) >= 'a' && (_ch_) <= 'z') || ((_ch_) >= '0' && (_ch_) <= '9') || (_ch_ == '_'))
    register const char *   bufferCurrent;
    register const char *   bufferEnd;
    register const char *   varRefStart = NULL;
    register const char *   varNameStart = NULL;
    register const char *   varNameEnd = NULL;
    register const char *   varRefEnd = NULL;


    bufferCurrent = characters;
    bufferEnd = characters + length;
    while (bufferCurrent < bufferEnd  &&  varRefStart == NULL) {
        register char ch = *bufferCurrent++;
        switch (ch) {
            case '\\':
                if (bufferCurrent < bufferEnd) {
                    bufferCurrent++;
                }
                break;

            case '\'':
                while (bufferCurrent < bufferEnd  &&  (ch = *bufferCurrent++) != '\'') {
                    if (ch == '\\'  &&  bufferCurrent < bufferEnd) bufferCurrent++;
                }
                break;

            case '$':
                varRefStart = bufferCurrent-1;
                ch = *bufferCurrent++;
                if (ch == '('  ||  ch == '{'  ||  ch == '%'  ||  ch == '[') {
                    // Handle $(VAR), ${VAR}, $%VAR%, $[VAR selectorName] -- we respect backslashes as part of the variable name
                    const char closeDelimChar = (ch == '(') ? ')' : ((ch == '{') ? '}' : ((ch == '%') ? '%' : ']'));
                    varNameStart = bufferCurrent;
                    while (bufferCurrent < bufferEnd  &&  (ch = *bufferCurrent++) != closeDelimChar) {
                        if (ch == '\\'  &&  bufferCurrent < bufferEnd) bufferCurrent++;
                    }
                    if (ch == closeDelimChar) {
                        varNameEnd = bufferCurrent - 1;
                        varRefEnd = bufferCurrent;
                    }
                    else {
                        // We hit end-of-search-string without finding a delimiter... thus, no cigar
                        varRefStart = NULL;
                    }
                }
                else if (is_variable_char(ch)) {
                    // Handle $VAR -- we intentionally ignore backslashes in this case, since that's what most shells do
                    varNameStart = bufferCurrent-1;
                    while (bufferCurrent < bufferEnd  &&  (ch = *bufferCurrent++)  &&  is_variable_char(ch))
                        ;
                    if (bufferCurrent < bufferEnd) {
                        bufferCurrent--;
                    }
                    varNameEnd = bufferCurrent;
                    varRefEnd = bufferCurrent;
                    break;
                }
                else {
                    // This also handles the case of a trailing $ sign -- in this case, 'ch' is '\0'
                    varRefStart = NULL;
                    bufferCurrent--;
                }
                break;
        }
    }
    if (varRefStart != NULL) {
        if (varRefStartPtr != NULL) *varRefStartPtr = (char *)varRefStart;
        if (varNameStartPtr != NULL) *varNameStartPtr = (char *)varNameStart;
        if (varNameEndPtr != NULL) *varNameEndPtr = (char *)varNameEnd;
        if (varRefEndPtr != NULL) *varRefEndPtr = (char *)varRefEnd;
        return 1;
    }
    else {
        return 0;
    }
    #undef is_variable_char
}


static void var_append_expansion_of_deferred_variable (const char * symbol, unsigned length, int asgSearchStartIdx, struct string_buffer * expstrbuf)
    /** Private helper function that appends the expansion of 'symbol' to 'expstrbuf'. Recursive expansion is done as needed. The 'asgSearchStartIdx' is the index into the array of deferred assignments from which the search for a definition of 'symbol' should start. This is used when expanding concatenation assignments (X += Y). */
{
    const char *   value = NULL;
    char           symbolstr[length+1];
    int            shouldConcatenate = 0;
    int            i, asgIdx = -1;
    int            free_value = 0;

    // Find the definition of 'symbol'.
    if (DEBUG_VARSET) {
	printf("expanding ");fwrite(symbol, length, 1, stdout);printf(" {\n");
    }
    memcpy(symbolstr, symbol, length);
    symbolstr[length] = '\0';
    for (i = asgSearchStartIdx; i >= 0  &&  value == NULL; i--) {
	if (strcmp(def_asgs->asgs[i].symbol, symbolstr) == 0) {
	    value = def_asgs->asgs[i].value;
	    if (DEBUG_VARSET) {
		printf("[found deferred assignment of ");fwrite(symbol, length, 1, stdout);printf(" with value |%s| at index %u]\n", value, i);
	    }
	    shouldConcatenate = def_asgs->asgs[i].append;
	    asgIdx = i;
	}
    }
    // If we haven't found a value yet, we try to find one in the standard Jam variables.
    if (value == NULL) {
	LIST * jamvar_list = var_get(symbolstr);
	if (jamvar_list != NULL) {
	    struct string_buffer jamvar_sbuf;
	    strbuf_init(&jamvar_sbuf);
	    strbuf_append_list_as_command_line_arguments(&jamvar_sbuf, jamvar_list);
	    strbuf_append_character(&jamvar_sbuf, '\0');
	    value = (const char *)malloc(strbuf_length(&jamvar_sbuf) * sizeof(char));
	    memcpy((char *)value, strbuf_characters(&jamvar_sbuf), strbuf_length(&jamvar_sbuf) * sizeof(char));
	    strbuf_free(&jamvar_sbuf);
	    free_value = 1;
	    shouldConcatenate = 0;
	    if (DEBUG_VARSET) {
		printf("[found normal value of ");fwrite(symbol, length, 1, stdout);printf(" with value |%s|]\n", value);
	    }
	}
    }
    // If we found a value, we start expanding variable references in it. If we didn't find one, we're done (there's nothing to append).
    if (value != NULL) {
	const char *   subrange_start;
	const char *   subrange_end;
	const char *   var_ref_start;
	const char *   var_name_start;
	const char *   var_name_end;
	const char *   var_ref_end;

	// If it's a concatenation assignment, then we first append the expansion of the next definition of 'symbol' of lower precendence than the one we're currently expanding, i.e. we recurse with the search starting from 'asgSearchStartIdx'-1.
	if (shouldConcatenate) {
	    if (DEBUG_VARSET) {
		printf("[concatenation, so starting expansion of ");fwrite(symbol, length, 1, stdout);printf(" from search index %i]\n", asgIdx-1);
	    }
	    var_append_expansion_of_deferred_variable(symbol, length, asgIdx-1, expstrbuf);
	    strbuf_append_character(expstrbuf, ' ');
	}
	// Now we walk our string, expanding variable references as we go. The input string may contain escape characters and single quotes, both of which we need to honour as we do the variable expansion. We use find_next_variable_reference() to walk the string, honouring escaping and quoting.
	subrange_start = value;
	subrange_end = subrange_start + strlen(subrange_start);
        while (find_next_variable_reference(subrange_start, subrange_end-subrange_start, &var_ref_start, &var_name_start, &var_name_end, &var_ref_end)) {

            // First append any plain text prefix, up to the opening delimiter.
            if (subrange_start < var_ref_start) {
		strbuf_append_characters(expstrbuf, subrange_start, var_ref_start - subrange_start);
            }

            // Extract the name of the referenced build setting, and recursively expand it at the end of 'expstrbuf'.
            // !!!:anders:20001209  We should check for definition loops here (e.g. A=$(A) -- as in make, such loops aren't allowed.
	    var_append_expansion_of_deferred_variable(var_name_start, var_name_end - var_name_start, def_asgs->count - 1, expstrbuf);

            // Update the substring range. It starts immediately after the previous variable reference.
            subrange_start = var_ref_end;
        }

        // Finally, append remaining plain text, up to the end of the string. For strings that contain no variable references, this amounts to the entire string.
        if (subrange_start < subrange_end) {
	    strbuf_append_characters(expstrbuf, subrange_start, subrange_end - subrange_start);
        }
    }
    if (free_value) {
	free((char *)value);
    }
    if (DEBUG_VARSET) {
	printf("}\n");
    }
}



//
//  Dealing with deferred assignments
//

void var_set_deferred (const char * symbol, LIST * value, int flag, int export)
    /** Public function, similar to var_set(), but which adds a deferred assignment to the accumulated sequence of deferred assignments. Copies symbol, discards value (after turning it into a string). */
{
    struct string_buffer   sbuf;
    char *                 valstr;

    // Show a debug message, if appropriate.
    if (DEBUG_COMPILE) {
	printf("set-deferred %s %s ", symbol, (flag == VAR_APPEND) ? "+=" : "=");
	list_print(value);
	printf("\n");
    }
    // Grow the list.
    if (def_asgs == NULL) {
	if (DEBUG_VARSET) {
	    printf("[allocating deferred-assignment array with initial capacity %u]\n", 4);
	}
	def_asgs = malloc(sizeof(*def_asgs) + 4 * sizeof(*def_asgs->asgs) );
	def_asgs->count = 0;
	def_asgs->capacity = 4;
    }
    else if (def_asgs->count == def_asgs->capacity) {
	if (DEBUG_VARSET) {
	    printf("[growing deferred-assignment array capacity from %u to %u]\n", def_asgs->capacity, def_asgs->capacity * 2);
	}
	def_asgs->capacity *= 2;
	def_asgs = realloc(def_asgs, sizeof(*def_asgs) + def_asgs->capacity * sizeof(*def_asgs->asgs));
    }
    // Convert the value list into a string.
    strbuf_init(&sbuf);
    strbuf_append_list_as_command_line_arguments(&sbuf, value);
    strbuf_append_character(&sbuf, '\0');
    valstr = (char *)malloc(strbuf_length(&sbuf) * sizeof(char));
    memcpy((char *)valstr, strbuf_characters(&sbuf), strbuf_length(&sbuf) * sizeof(char));
    strbuf_free(&sbuf);
    list_free(value);
    // Record the assignment.
    def_asgs->asgs[def_asgs->count].symbol = newstr(symbol);
    def_asgs->asgs[def_asgs->count].value = valstr;
    def_asgs->asgs[def_asgs->count].append = (flag == VAR_APPEND) ? 1 : 0;
    def_asgs->count++;
    // Mark the variable as exported, if needed.
    if (export) {
	var_export(symbol);
    }
}


void var_commit_all_deferred_assignments ()
    /** Public function that commits all deferred assignments (by evaluating their expansion and executing var_set for each one), and clears the array of deferred assignments. */
{
    if (def_asgs != NULL) {
	struct hash *   commited_defvars_hash;
	int             i;

	// First append all command-line assignments to the list of accumulated deferred assignments.
	for (i = 0; globs.cmdline_defines[i] != NULL; i++) {
	    char *   equals_sign_p;

	    if ((equals_sign_p = strchr(globs.cmdline_defines[i], '=')) != NULL  &&  (equals_sign_p > globs.cmdline_defines[i])) {
		char    varname[MAXSYM];
		LIST *  list = NULL;
		int     flag = (equals_sign_p[-1] == '+') ? VAR_APPEND : VAR_SET;

		list = list_new(list, newstr(equals_sign_p + 1));
		strncpy(varname, globs.cmdline_defines[i], equals_sign_p - globs.cmdline_defines[i] - ((equals_sign_p[-1] == '+') ? 1 : 0));
		varname[equals_sign_p - globs.cmdline_defines[i]] = '\0';
		var_set_deferred(varname, list, flag, 1 /* export-in-environment */ );
	    }
	}

	// Now walk the deferred assignments from back to front, and apply the first assignment of each unique variable.
	commited_defvars_hash = hashinit(sizeof(char *), "committed defvars" );
	for (i = def_asgs->count-1; i >= 0; i--) {
	    char ** v = &def_asgs->asgs[i].symbol;
	    if (!hashcheck(commited_defvars_hash, (HASHDATA **)&v)) {
		struct string_buffer  sbuf;
		LIST *                lst;

		(void)hashenter(commited_defvars_hash, (HASHDATA **)&v);
		strbuf_init(&sbuf);
		var_append_expansion_of_deferred_variable(def_asgs->asgs[i].symbol, strlen(def_asgs->asgs[i].symbol), def_asgs->count - 1, &sbuf);
		strbuf_append_character(&sbuf, '\0');
		lst = list_by_parsing_as_commandline_arguments(strbuf_characters(&sbuf), strbuf_length(&sbuf)-1);
		if (DEBUG_COMPILE) {
		    printf("committing deferred assignment %s := %s\n", def_asgs->asgs[i].symbol, strbuf_characters(&sbuf));
		}
		var_set(def_asgs->asgs[i].symbol, lst, VAR_SET, 1);
		strbuf_free(&sbuf);
	    }
	    else {
		if (DEBUG_COMPILE) {
		    printf("skipping %s %s %s\n", def_asgs->asgs[i].symbol, def_asgs->asgs[i].append ? "+=" : "=", def_asgs->asgs[i].value);
		}
	    }
	}
	for (i = def_asgs->count-1; i >= 0; i--) {
	    free(def_asgs->asgs[i].value);
	}
	free(def_asgs);
	def_asgs = NULL;
	hashdone(commited_defvars_hash);
    }
}

#endif
