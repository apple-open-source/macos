/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

/*
 * $Id: sio.h,v 1.1.1.3 2002/10/02 21:07:08 bbraun Exp $
 */

#ifndef __SIO_H
#define __SIO_H

#include <errno.h>
#include <stdarg.h>

/*
 * Naming conventions:
 *		1) SIO functions and macros have names starting with a capital S
 *		2) SIO constants meant to be used by user programs have 
 *			names starting with SIO_
 *		3) Internal functions, struct identifiers, enum identifiers 
 *			etc. have names starting with __sio
 *		4) Internal constants and macros have names starting with __SIO
 */


/*
 * external constants
 *
 * SIO_FLUSH_ALL: flush all output streams
 * SIO_ERR: operation failed
 * SIO_EOF:	eof on stream
 * #define SIO_EOF				(-2)
 */
#define SIO_FLUSH_ALL				(-1)
#define SIO_ERR					(-1)

/*
 * Buffering types
 */
#define SIO_FULLBUF			0
#define SIO_LINEBUF			1
#define SIO_NOBUF			2

/*
 * Descriptor for an input stream
 */
struct __sio_input_descriptor
{
	/*
	 * buf:		points to the buffer area.
	 *		When doing memory mapping, it is equal to the unit 
	 *		from which we are reading. When doing buffered I/O
	 *		it points to the primary buffer. The auxiliary
	 *		buffer is right below buf and is of the same size.
	 */
	char *buf ;
	unsigned buffer_size ;

	char *start ;                 /* start of valid buffer contents   	*/
	char *end ;                   /* end of valid buffer contents + 1 	*/
	char *nextb ;                 /* pointer to next byte to read/write 	*/
											/* Always:  start <= nextb < end			*/

	unsigned line_length ;
	int max_line_length ;
	int tied_fd ;

	int memory_mapped ;				/* flag to denote if we use				*/
											/* memory mapping								*/
} ;

typedef struct __sio_input_descriptor __sio_id_t ;


/*
 * Descriptor for an output stream
 */
struct __sio_output_descriptor
{
	/*
	 * buf:		points to the buffer area.
	 * buf_end: is equal to buf + buffer_size
	 */
	char *buf ;
	char *buf_end ;

	unsigned buffer_size ;

	char *start ;                 /* start of valid buffer contents   	*/
											/* (used by the R and W functions) 		*/
	char *nextb ;                 /* pointer to next byte to read/write  */
											/* Always:  start <= nextb < buf_end	*/
	int buftype ;						/* type of buffering 						*/
} ;

typedef struct __sio_output_descriptor __sio_od_t ;



/*
 * Stream types
 */
enum __sio_stream { __SIO_INPUT_STREAM, __SIO_OUTPUT_STREAM } ;


/*
 * General descriptor
 */
struct __sio_descriptor
{
	union
	{
		__sio_id_t input_descriptor ;
		__sio_od_t output_descriptor ;
	} descriptor ;
	enum __sio_stream stream_type ;
	int initialized ;
} ;

typedef struct __sio_descriptor __sio_descriptor_t ;


/*
 * The array of descriptors (as many as available file descriptors)
 */
extern int __sio_n_descriptors ;
extern __sio_descriptor_t *__sio_descriptors ;


/*
 * Internally used macros
 */
#define __SIO_FD_INITIALIZED( fd ) \
	(fd < __sio_n_descriptors && __sio_descriptors[ fd ].initialized)
#define __SIO_ID( fd )	(__sio_descriptors[ fd ].descriptor.input_descriptor)
#define __SIO_OD( fd )	(__sio_descriptors[ fd ].descriptor.output_descriptor)
#define __SIO_MUST_FLUSH( od, ch )				\
			( (od).buftype != SIO_FULLBUF &&	\
			( (od).buftype == SIO_NOBUF || ch == '\n' ) )


/*
 * SIO Macros:
 *
 *		SIOLINELEN( fd )
 *
 * NOTE: The maximum line size depends on whether the descriptor
 *	was originally memory mapped. If it was, then the maximum
 *	line size will be the map_unit_size (a function of the system
 *	page size and PAGES_MAPPED). Otherwise, it will be either the
 *	optimal block size as reported by stat(2) or SIO_BUFFER_SIZE.
 */

#define SIOLINELEN( fd )      __SIO_ID( fd ).line_length 

/*
 * The Read functions
 */
char *Srdline ( int fd ) ;

/*
 * The Write functions
 */
int Swrite ( int fd, const char *buf, int nbytes );
int Sprint ( int fd, const char *format, ... )
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 3)));
#else
	;
#endif
int Sputchar( int fd, char c );
int Sprintv ( int fd, const char *format, va_list ap )
#ifdef __GNUC__
	__attribute__ ((format (printf, 2, 0)));
#else
	;
#endif

/*
 * other functions
 */
int Sdone ( int fd ) ;
int Sflush ( int fd ) ;
int Sclose ( int fd ) ;
int Sbuftype ( int fd, int type ) ;
int Smorefds ( int ) ;
int __sio_converter( __sio_od_t *, int , const char *, va_list );
int sio_setup(int fd, __sio_descriptor_t **dp, int type );
void sio_init( void );

#endif /* __SIO_H */

