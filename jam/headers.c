/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# include "jam.h"
# include "lists.h"
# include "parse.h"
# include "compile.h"
# include "rules.h"
# include "variable.h"
#if defined(__APPLE__)
#include <sys/param.h>
#include <regex.h>
#else
# include "regexp.h"
#endif
# include "headers.h"
# include "newstr.h"

/*
 * headers.c - handle #includes in source files
 *
 * Using regular expressions provided as the variable $(HDRSCAN), 
 * headers() searches a file for #include files and phonies up a
 * rule invocation:
 * 
 *	$(HDRRULE) <target> : <include files> ;
 *
 * External routines:
 *    headers() - scan a target for include files and call HDRRULE
 *
 * Internal routines:
 *    headers1() - using regexp, scan a file and build include LIST
 *
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 */

static LIST *headers1();

/*
 * headers() - scan a target for include files and call HDRRULE
 */

# define MAXINC 10

void
headers( t )
TARGET *t;
{
	LIST	*hdrscan;
	LIST	*hdrrule;
	LIST	*headlist = 0;
	PARSE	p[3];
#if defined(__APPLE__)
	int	ret;
	regex_t	*re[ MAXINC ];
#else
	regexp	*re[ MAXINC ];
#endif
	int	rec = 0;

	if( !( hdrscan = var_get( "HDRSCAN" ) ) || 
	    !( hdrrule = var_get( "HDRRULE" ) ) )
	        return;

	if( DEBUG_HEADER )
	    printf( "header scan %s\n", t->name );

	/* Compile all regular expressions in HDRSCAN */

	while( rec < MAXINC && hdrscan )
	{
#if defined(__APPLE__)
	  re[rec] = (regex_t *)malloc(sizeof(regex_t));
	  ret = regcomp( re[rec++], hdrscan->string, REG_EXTENDED );
	  if (ret != 0) {
	      char *errbuf = NULL;
	      size_t bufsize;
	      bufsize = regerror(ret, re[--rec], NULL, 0);
	      errbuf = (char *)malloc(bufsize);
	      (void)regerror(ret, re[rec], errbuf, bufsize);
	      fprintf(stderr, "regcomp(%s) failed: %s\n", hdrscan->string, errbuf);
	      regfree(re[rec]);
	      free(re[rec]);
	      free(errbuf);
	      return;
	  }
#else
	    re[rec++] = regcomp( hdrscan->string );
#endif
	    hdrscan = list_next( hdrscan );
	}

	/* Doctor up call to HDRRULE rule */
	/* Call headers1() to get LIST of included files. */

	p[0].string = hdrrule->string;
	p[0].left = &p[1];
	p[1].llist = list_new( L0, t->name );
	p[1].left = &p[2];
	p[2].llist = headers1( headlist, t->boundname, rec, re );
	p[2].left = 0;

	if( p[2].llist )
	{
	    LOL lol0;
	    lol_init( &lol0 );
	    compile_rule( p, &lol0 );
	}

	/* Clean up */

	list_free( p[1].llist );
	list_free( p[2].llist );

	while( rec ) {
#if defined(__APPLE__)
	  regfree( re[--rec] );
	  free( re[rec] );
#else
	    free( (char *)re[--rec] );
#endif
	}
}


#if defined(__APPLE__)

char * eol_agnostic_fgets (char * buffer, size_t buffer_size, FILE * file)
{
    int      ch;
    int      n = 0;

    while ((ch = getc(file)) != EOF)
    {
        if (ch == '\n')
            break;
        else if (ch == '\r')
        {
            if ((ch = getc(file)) != '\n'  &&  ch != EOF)
                ungetc(ch, file);
            break;
        }
        if (n < buffer_size-1)
            buffer[n++] = ch;
    }
    buffer[n] = '\0';
    return (ch == EOF && n == 0) ? NULL : buffer;
}

#endif


/*
 * headers1() - using regexp, scan a file and build include LIST
 */

static LIST *
headers1( l, file, rec, re )
LIST	*l;
char	*file;
int	rec;
#if defined(__APPLE__)
regex_t	*re[];
#else
regexp	*re[];
#endif
{
    FILE	*f;
    char	buf[ 1024 ];
    int		i;
#if defined(__APPLE__)
    size_t	nmatch = 2;
    regmatch_t	pmatch[2];
#endif

    if( !( f = fopen( file, "r" ) ) )
	return l;

#if defined(__APPLE__)
    while( eol_agnostic_fgets( buf, sizeof( buf ), f ) )
#else
    while( fgets( buf, sizeof( buf ), f ) )
#endif
    {
	for( i = 0; i < rec; i++ )
#if defined(__APPLE__)
            if( regexec( re[i], buf, nmatch, pmatch, 0 ) == 0)
	      {
                    char *filename = buf + pmatch[1].rm_so;
                    size_t len = pmatch[1].rm_eo - pmatch[1].rm_so;
                    filename[len] = '\0';

                    if( DEBUG_HEADER )
                        printf( "header found: %s\n", filename );

                    l = list_new( l, newstr( filename ) );
	      }
#else
	    if( regexec( re[i], buf ) && re[i]->startp[1] )
	{
	    re[i]->endp[1][0] = '\0';

	    if( DEBUG_HEADER )
		printf( "header found: %s\n", re[i]->startp[1] );

	    l = list_new( l, newstr( re[i]->startp[1] ) );
	}
#endif
    }

    fclose( f );

    return l;
}

#if !defined(__APPLE__)
void
regerror( s )
char *s;
{
	printf( "re error %s\n", s );
}
#endif
