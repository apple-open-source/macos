/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

static char RCSid[] = "$Id: print.c,v 1.1.1.1 2002/01/31 07:09:04 zarzycki Exp $" ;

#include <stdio.h>
#include <math.h>
#include <values.h>
#include <string.h>

#include "sio.h"

#define FLUSH()			fflush( stdout ) ; Sflush( 1 )
#define COMPARE( printf_count, sprint_count )										\
							if ( printf_count != sprint_count )							\
								printf( "printf_count = %d, sprint_count = %d\n",	\
											printf_count, sprint_count )

enum bool { NO = 0, YES = 1 } ;

enum test_flag
{
	DECIMAL, HEX, CAP_HEX, OCTAL, UNSIGNED,
	F_FLOAT, G_FLOAT, E_FLOAT, CAP_E_FLOAT, CAP_G_FLOAT,
	CHAR, STRING,
	POINTER,
	BOUND,
	N_FLAGS
} ;

typedef enum test_flag FLAG ;

#define CHECK( f )				if ( ! flags[ f ] ) return

/*
 * Flags
 */
enum bool flags[ N_FLAGS ] ;

char *precision ;
char *width ;
char *print_flags ;

int i_begin = 123456 ;
int i_end = 123470 ;
int i_step = 1 ;

double f_begin = 1.234567654312 ;
double f_end = 2.0 ;
double f_step = 0.011 ;

#define LEN( s )					( s ? strlen( s ) : 0 )

char *format( f )
	char *f ;
{
	char *malloc() ;
	static char *newfmt ;

	if ( newfmt )
		free( newfmt ) ;

	newfmt = malloc( strlen( f )
				+ LEN( precision ) + LEN( width ) + LEN( print_flags ) + 2 ) ;
	(void) strcpy( newfmt, "%" ) ;
	if ( print_flags )
		(void) strcat( newfmt, print_flags ) ;
	if ( width )
		(void) strcat( newfmt, width ) ;
	if ( precision )
		(void) strcat( strcat( newfmt, "." ), precision ) ;
	(void) strcat( newfmt, &f[1] ) ;
	return( newfmt ) ;
}

#define decimal_test()			integer_test( "%d %d\n", DECIMAL )
#define hex_test()				integer_test( "%x %x\n", HEX )
#define cap_hex_test()			integer_test( "%X %X\n", CAP_HEX )
#define octal_test()				integer_test( "%o %o\n", OCTAL )
#define unsigned_test()			integer_test( "%u %u\n", UNSIGNED )

void integer_test( fmt, flag )
	char *fmt ;
	FLAG flag ;
{
	int i ;
	int ccs, ccp ;

	CHECK( flag ) ;
	fmt = format( fmt ) ;

	for ( i = i_begin ; i < i_end ; i += i_step )
	{
		ccp = printf( fmt, -i, i ) ;
		ccs = Sprint( 2, fmt, -i, i ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;
	}
}


#define f_float_test()			fp_test( "%f\n", F_FLOAT )
#define g_float_test()			fp_test( "%g\n", G_FLOAT )
#define e_float_test()			fp_test( "%e\n", E_FLOAT )
#define cap_e_float_test()		fp_test( "%E\n", CAP_E_FLOAT )			
#define cap_g_float_test()		fp_test( "%G\n", CAP_G_FLOAT )

void fp_test( fmt, flag )
	char *fmt ;
	FLAG flag ;
{
	double d ;
	double step ;
	int ccs, ccp ;

	CHECK( flag ) ;
	fmt = format( fmt ) ;
	
	for ( d = f_begin, step = f_step ; d < f_end ; d += step, step += step )
	{

		ccp = printf( fmt, d ) ;
		ccs = Sprint( 2, fmt, d ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;
	}
}


void char_test()
{
	char *s = "foobar" ;
	int len = strlen( s ) ;
	int i ;
	char *fmt = "%c\n" ;
	int ccs, ccp ;

	CHECK( CHAR ) ;
	fmt = format( fmt ) ;

	for ( i = 0 ; i < len ; i++ )
	{
		ccp = printf( fmt, s[ i ] ) ;
		ccs = Sprint( 2, fmt, s[ i ] ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;
	}
}


void string_test()
{
	static char *list[] = 
	{
		"foobar",
		"hello",
		"world",
		"this is a very long string, a really long string, really, true, honest",
		"i am getting tired of this",
		"SO THIS IS THE END",
		0
	} ;
	char *fmt = "%s\n" ;
	char **p ;
	int ccp, ccs ;

	CHECK( STRING ) ;
	fmt = format( fmt ) ;

	for ( p = &list[ 0 ] ; *p ; p++ )
	{
		ccp = printf( fmt, *p ) ;
		ccs = Sprint( 2, fmt, *p ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;
	}
}


void pointer_test()
{
	struct foo
	{
		char bar1 ;
		short bar2 ;
		int bar3 ;
		long bar4 ;
		char *bar5 ;
	} foo, *end = &foo, *p ;
	char *fmt = "%p\n" ;
	int ccp, ccs ;

	CHECK( POINTER ) ;
	fmt = format( fmt ) ;

	end += 10 ;
	for ( p = &foo ; p < end ; p++ )
	{
		ccp = printf( fmt, p ) ;
		ccs = Sprint( 2, fmt, p ) ;
		FLUSH() ;
	}
}


/* 
 * bound_test is only available on SunOS 4.x
 */
#if defined( sun )

void bound_test()
{
	char *fmt ;
	double bound_values[ 10 ] ;
	static char *bound_names[] =
	{
		"min_subnormal",
		"max_subnormal",
		"min_normal",
		"max_normal",
		"infinity",
		"quiet_nan",
		"signaling_nan"
	} ;
	int n_values ;
	int i ;
	int ccp, ccs ;

	bound_values[ 0 ] = min_subnormal() ;
	bound_values[ 1 ] = max_subnormal() ;
	bound_values[ 2 ] = min_normal() ;
	bound_values[ 3 ] = max_normal() ;
	bound_values[ 4 ] = infinity() ;
	bound_values[ 5 ] = quiet_nan( 7L ) ;
	bound_values[ 6 ] = signaling_nan( 7L ) ;
	n_values = 7 ;

	CHECK( BOUND ) ;

	for ( i = 0 ; i < n_values ; i++ )
	{
		double d = bound_values[ i ] ;
		char *name = bound_names[ i ] ;

		fmt = format( "%f (%s)\n" ) ;
		ccp = printf( fmt, d, name ) ;
		ccs = Sprint( 2, fmt, d, name ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;

		fmt = format( "%e (%s)\n" ) ;
		ccp = printf( fmt, d, name ) ;
		ccs = Sprint( 2, fmt, d, name ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;

		fmt = format( "%g (%s)\n" ) ;
		ccp = printf( fmt, d, name ) ;
		ccs = Sprint( 2, fmt, d, name ) ;
		FLUSH() ;
		COMPARE( ccp, ccs ) ;
	}

	fmt = format( "%d (MININT)\n" ) ;
	ccp = printf( fmt, -MAXINT-1 ) ;
	ccs = Sprint( 2, fmt, -MAXINT-1 ) ;
	COMPARE( ccp, ccs ) ;
}
#else
void bound_test()
{
}
#endif


int get_options( argc, argv )
	int argc ;
	char *argv[] ;
{
	int arg_index = 1 ;
	char *p ;
	double atof() ;

	for ( arg_index = 1 ;
			arg_index < argc && argv[ arg_index ][ 0 ] == '-' ; arg_index++ )
	{
		switch ( argv[ arg_index ][ 1 ] )
		{
			case 'd':
				flags[ DECIMAL ] = YES ;
				break ;
			
			case 'x':
				flags[ HEX ] = YES ;
				break ;
			
			case 'X':
				flags[ CAP_HEX ] = YES ;
				break ;
			
			case 'o':
				flags[ OCTAL ] = YES ;
				break ;
			
			case 'u':
				flags[ UNSIGNED ] = YES ;
				break ;

			case 'f':
				flags[ F_FLOAT ] = YES ;
				break ;
			
			case 'g':
				flags[ G_FLOAT ] = YES ;
				break ;
			
			case 'e':
				flags[ E_FLOAT ] = YES ;
				break ;
			
			case 'E':
				flags[ CAP_E_FLOAT ] = YES ;
				break ;
			
			case 'G':
				flags[ CAP_G_FLOAT ] = YES ;
				break ;

			case 'c':
				flags[ CHAR ] = YES ;
				break ;
			
			case 's':
				flags[ STRING ] = YES ;
				break ;
			
			case 'p':
				flags[ POINTER ] = YES ;
				break ;
			
			case 'b':		/* this is for checking bounds in fp formats */
				flags[ BOUND ] = YES ;
				break ;
				
			case 'P':	/* precision, must be followed by a number, e.g. -P10 */
				precision = &argv[ arg_index ][ 2 ] ;
				break ;
			
			case 'W':	/* width, must be followed by a number, e.g. -w10 */
				width = &argv[ arg_index ][ 2 ] ;
				break ;
			
			case 'F':	/* flags, whatever is after the F */
				print_flags = &argv[ arg_index ][ 2 ] ;
				break ;
			
			/*
			 * Options recognized in this case:	-Vf, -Vi
			 * Usage: -V[if] start end step
			 */
			case 'V':
				/*
				 * Check if we have enough extra arguments
				 */
				if ( argc - ( arg_index + 1 ) < 3 )
				{
					fprintf( stderr, "Insufficient # of args after V option\n" ) ;
					exit( 1 ) ;
				}
				switch ( argv[ arg_index ][ 2 ] )
				{
					case 'f':
						f_begin = atof( argv[ arg_index+1 ] ) ;
						f_end   = atof( argv[ arg_index+2 ] ) ;
						f_step  = atof( argv[ arg_index+3 ] ) ;
						break ;
					
					case 'i':
						i_begin = atoi( argv[ arg_index+1 ] ) ;
						i_end   = atoi( argv[ arg_index+2 ] ) ;
						i_step  = atoi( argv[ arg_index+3 ] ) ;
						break ;
				}
				arg_index += 3 ;
				break ;

			case 'S':
				f_step = atof( &argv[ arg_index ][ 2 ] ) ;
				break ;
		}
	}
	return( arg_index ) ;
}


#define EQ( s1, s2 )				( strcmp( s1, s2 ) == 0 )


int main( argc, argv )
	int argc ;
	char *argv[] ;
{

	if ( Sbuftype( 2, SIO_LINEBUF ) == SIO_ERR )
	{
		char *msg = "Sbuftype failed\n" ;

		write( 2, msg, strlen( msg ) ) ;
		exit( 1 ) ;
	}

	if ( argc == 1 || argc == 2 && EQ( argv[ 1 ], "ALL" ) )
	{
		/* perform all tests */
		int i ;

		for ( i = 0 ; i < N_FLAGS ; i++ )
			flags[ i ] = YES ;
	}
	else
		(void) get_options( argc, argv ) ;

	decimal_test() ;
	hex_test() ;
	cap_hex_test() ;
	octal_test() ;
	unsigned_test() ;

	f_float_test() ;
	g_float_test() ;
	e_float_test() ;
	cap_g_float_test() ;
	cap_e_float_test() ;

	string_test() ;
	char_test() ;
	pointer_test() ;
	bound_test() ;
	exit( 0 ) ;
}
