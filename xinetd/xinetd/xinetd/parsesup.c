/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <ctype.h>
#include <syslog.h>

#include "sio.h"
#include "str.h"
#include "parsesup.h"
#include "msg.h"


/*
 * next_line returns the next line of the file or NULL if the end of file
 * is reached.
 * Comment lines and empty lines are skipped.
 */
char *next_line( int fd )
{
   for ( ;; )
   {
      char *p ;
      char *line = Srdline( fd ) ;

      if ( line == NULL )
         return( NULL ) ;

      line_count++ ;

      for ( p = line ;; p++ )
         if ( *p == NUL || *p == COMMENT_BEGIN )
            break ;                                /* skip this line */
         else if ( isspace( *p ) )
            continue ;                             /* skip white space */
         else
            return( line ) ;
   }
}


/*
 * Input:
 *      a line of the form
 *            name [SPACE] OP [SPACE] value [SPACE] value ...
 *
 * Recognize the attribute name and operator and place them in *attrp, *opp
 *
 * Currently, we allow any non-space character to be used in the
 * attribute name.
 *
 * Return value: a pointer to the character after OP.
 */
static char *get_attr_op( char *line, char **attrp, enum assign_op *opp )
{
   char            *p ;
   char            *attr ;
   enum assign_op   op ;
   const char      *func = "get_attr_op" ;

   /*
    * First get the attribute name
    */
   for ( p = line ; isspace( *p ) ; p++ ) ;      /* skip spaces */
   if ( *p == NUL )
   {
      parsemsg( LOG_ERR, func, "Empty line" ) ;
      return( NULL ) ;
   }

   attr = p ;
   for ( ; ! isspace( *p ) && (*p != '='); p++ ) ;            /* skip attribute name */
   if ( *p == NUL )
   {
      parsemsg( LOG_ERR, func, "Nothing after attribute: %s", attr ) ;
      return( NULL ) ;
   }
   if( *p == '=' ) {
      *p = NUL ;         /* now attribute name is NUL terminated */
      parsemsg( LOG_ERR, func, "Attribute %s needs a space before operator", attr);
      return( NULL ) ;
   }
   *p++ = NUL ;         /* now attribute name is NUL terminated */

   while ( isspace( *p ) ) p++ ;      /* skip spaces */

   switch ( *p )
   {
      case NUL:
         parsemsg( LOG_ERR, func, "Nothing after attribute: %s", attr ) ;
         return( NULL ) ;
   
      case '=':
         op = SET_EQ ;
         break ;
      
      case '+':
      case '-':
         op = ( *p++ == '+' ) ? PLUS_EQ : MINUS_EQ ;
         if ( *p == '=' )
            break ;
         
         /* FALL THROUGH if there is no '=' after the '+' or '-' */
      
      default:
         parsemsg( LOG_ERR, func, "Bad operator for attribute: %s", attr ) ;
         return( NULL ) ;
   }
   *attrp = attr ;
   *opp = op ;
   return( ++p ) ;      /* skip the '=' */
}


/*
 * Parse a line of the form:
 *         name OP value value value ...
 * where each value is a string and OP can be '=', '+=', '-='
 *
 * NOTE: We do not allocate space for the name and values. Instead we keep
 *         pointers to the line.
 */
status_e parse_line( char *line, 
                     char **namep, 
                     enum assign_op *opp, 
                     pset_h values )
{
   char      *value ;
   char      *values_string ;
   char      *attribute ;
   str_h    strp ;
   const char *func = "parse_line" ;

   if ( ( values_string = get_attr_op( line, &attribute, opp ) ) == NULL )
      return( FAILED ) ;

   /*
    * Now grab the values
    */
   strp = str_parse( values_string, " \t", STR_RETURN_ERROR, (int *)0 ) ;
   if ( strp == NULL )
   {
      parsemsg( LOG_CRIT, func, ES_NOMEM ) ;
      return( FAILED ) ;
   }

   while ( (value = str_component( strp )) )
   {
      if ( pset_add( values, value ) == NULL )
      {
         parsemsg( LOG_CRIT, func, ES_NOMEM ) ;
         str_endparse( strp ) ;
         return( FAILED ) ;
      }
   }

   str_endparse( strp ) ;
   *namep = attribute ;
   return( OK ) ;
}


void skip_entry( int fd )
{
   for ( ;; )
   {
      char *line = next_line( fd ) ;

      if ( line == NULL )         /* reached EOF ? */
      {
         parsemsg( LOG_WARNING, "skip_entry",
            "missing %c in last service entry", ENTRY_END ) ;
         break ;
      }

      if ( line_has_only_1_char( line, ENTRY_END ) )
         break ;
   }
}



/*
 * Returns TRUE if the given line contains a single instance of the
 * specified character and no other non-space characters
 */
int line_has_only_1_char( const char *line, char ch )
{
   const char *p ;
   char target_char = ch ;

   for ( p = line ; *p ; p++ )
      if ( *p == target_char )
         target_char = NUL ;
      else if ( ! isspace( *p ) )
         return( FALSE ) ;
   return( target_char != ch ) ;
}

