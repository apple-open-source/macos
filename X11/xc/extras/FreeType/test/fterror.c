/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  E. Dieterich                                                            */
/*                                                                          */
/*  fterror: test errstr functionality.                                     */
/*                                                                          */
/****************************************************************************/


#include <stdio.h>
#include <stdlib.h>

#include "freetype.h"
#include "ftxerr18.h"

/*
 *  Basically, an external program using FreeType shouldn't depend on an
 *  internal file of the FreeType library, especially not on ft_conf.h -- but
 *  to avoid another configure script which tests for the existence of the
 *  i18n stuff we include ft_conf.h here since we can be sure that our test
 *  programs use the same configuration options as the library itself.
 */

#include "ft_conf.h"


#ifdef HAVE_LIBINTL_H

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libintl.h>

#else /* HAVE_LIBINTL_H */

#define gettext( x )  ( x )

#endif /* HAVE_LIBINTL_H */


  int
  main( void )
  {
    int    i;
#ifdef HAVE_LIBINTL_H
    char*  domain;


    setlocale( LC_ALL, "" );
    bindtextdomain( "freetype", LOCALEDIR );
    domain = textdomain( "freetype" );
#endif

#if 0
    printf( "domain: %s\n", domain = textdomain( "" ) );
#endif
    printf( gettext( "Start of fterror.\n" ) );

    for ( i = 0; i < 10; i++ )
      printf( "Code: %i, %s\n", i, TT_ErrToString18( i ) );

#if 0
    printf( "domain: %s\n", domain = textdomain( "" ) );
#endif
    printf( gettext( "End of fterror.\n" ) );

    exit( EXIT_SUCCESS );      /* for safety reasons */

    return 0;       /* never reached */
  }


/* End */
