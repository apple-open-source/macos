/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * (c) Portions Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#ifndef linux
#include <sys/types.h>
#endif

#ifdef _APPLE_
#undef HAVE_MMAP
#endif

#include <stdlib.h>
#include <unistd.h>

#include "sio.h"
#include "impl.h"

/*
 * SIO WRITE FUNCTIONS: Swrite, Sputc
 */

/*
 * Stream write call: arguments same as those of write(2)
 */
int Swrite( int fd, const char *addr, int nbytes )
{
	__sio_descriptor_t *dp ;
	__sio_od_t *odp ;
	int b_transferred ;
	int b_avail ;
	int total_b_transferred ;
	int b_written ;
	int b_in_buffer ;

	if( sio_setup( fd, &dp, __SIO_OUTPUT_STREAM ) == SIO_ERR )
		return( SIO_ERR );
	odp = ODP( dp ) ;
	ASSERT( odp->start <= odp->nextb && odp->nextb <= odp->buf_end ) ;

	b_avail = odp->buf_end - odp->nextb ;
	b_transferred = MIN( nbytes, b_avail ) ;
	sio_memcopy( addr, odp->nextb, b_transferred ) ;
	odp->nextb += b_transferred ;

	/*
	 * check if we are done
	 */
	if ( b_transferred == nbytes )
		return( b_transferred ) ;

	/*
	 * at this point we know that the buffer is full
	 */
	b_in_buffer = odp->buf_end - odp->start ;
	b_written = __sio_writef( odp, fd ) ;
	if ( b_written != b_in_buffer )
		return( (b_written >= nbytes) ? nbytes : b_written ) ;
	
	total_b_transferred = b_transferred ;
	addr += b_transferred ;
	nbytes -= b_transferred ;

	for ( ;; )
	{
		b_transferred = MIN( nbytes, odp->buffer_size ) ;
		sio_memcopy( addr, odp->nextb, b_transferred ) ;
		odp->nextb += b_transferred ;
		nbytes -= b_transferred ;
		if ( nbytes == 0 )
		{
			total_b_transferred += b_transferred ;
			break ;
		}
		/*
		 * the buffer is full
		 */
		b_written = __sio_writef( odp, fd ) ;
		if ( b_written != odp->buffer_size )
		{
			if ( b_written != SIO_ERR )
			{
				total_b_transferred += b_written ;
				odp->nextb += b_written ;
			}
			break ;
		}
		/*
		 * everything is ok
		 */
		total_b_transferred += b_transferred ;
		addr += b_transferred ;
	}
	return( total_b_transferred ) ;
}


/*
 * Add a character to a file
 */
static int Sputc( int fd, char c )
{
	__sio_descriptor_t *dp ;
	__sio_od_t *odp ;

	if( sio_setup( fd, &dp, __SIO_OUTPUT_STREAM) == SIO_ERR )
		return( SIO_ERR );
	odp = ODP( dp ) ;
	ASSERT( odp->start <= odp->nextb && odp->nextb <= odp->buf_end ) ;

	/*
	 * The following is a weak check since we should really be
	 * checking that nextb == buf_end (it would be an error for
	 * nextb to exceed buf_end; btw, the assertion above, when
	 * enabled makes sure this does not occur).
	 *
	 * NOTE: __sio_writef NEVER uses data beyond the end of buffer.
	 */
	if ( odp->nextb >= odp->buf_end )
	{
		int b_in_buffer = odp->buf_end - odp->start ;

		/*
		 * There is nothing we can do if __sio_writef does not manage
		 * to write the whole buffer
		 */
		if ( __sio_writef( odp, fd ) != b_in_buffer )
			return( SIO_ERR ) ;
	}
	*odp->nextb++ = c ;
	if ( __SIO_MUST_FLUSH( *odp, c ) && __sio_writef( odp, fd ) == SIO_ERR )
		return( SIO_ERR ) ;
	return ( c ) ;
}



/*
 * SIO READ FUNCTIONS
 */


/*
 * Read a line from a file
 * Returns a pointer to the beginning of the line or NULL
 */
char *Srdline( int fd )
{
	__sio_descriptor_t *dp ;
	__sio_id_t *idp ;
	char *cp ;
	char *line_start ;
	int b_left ;
	int extension ;

	if( sio_setup( fd, &dp, __SIO_INPUT_STREAM ) == SIO_ERR )
		return( NULL );
	idp = IDP( dp ) ;
	ASSERT( idp->start <= idp->nextb && idp->nextb <= idp->end ) ;

#ifdef HAVE_MMAP
	if ( idp->memory_mapped && __sio_switch( idp, fd ) == FAILURE )
		return( NULL ) ;
#endif

	b_left = idp->end - idp->nextb ;
	/*
	 * Look for a '\n'. If the search fails, extend the buffer
	 * and search again (the extension is performed by copying the
	 * bytes that were searched to the auxiliary buffer and reading 
	 * new input in the main buffer).
	 * If the new input still does not contain a '\n' and there is
	 * more space in the main buffer (this can happen with network
	 * connections), read more input until either the buffer is full
	 * or a '\n' is found.
	 * Finally, set cp to point to the '\n', and line_start to
	 * the beginning of the line
	 */
	if ( b_left && ( cp = sio_memscan( idp->nextb, b_left, '\n' ) ) != NULL )
	{
		line_start = idp->nextb ;
		idp->nextb = cp + 1 ;
	}
	else
	{
		extension = __sio_extend_buffer( idp, fd, b_left ) ;
		if ( extension > 0 )
		{
			ASSERT( idp->start <= idp->nextb && idp->nextb <= idp->end ) ;

			line_start = idp->start ;
			cp = sio_memscan( idp->nextb, extension, '\n' ) ;
			if ( cp != NULL )
				idp->nextb = cp + 1 ;
			else
				for ( ;; )
				{
					idp->nextb = idp->end ;
					extension = __sio_more( idp, fd ) ;
					if ( extension > 0 )
					{
						cp = sio_memscan( idp->nextb, extension, '\n' ) ;
						if ( cp == NULL )
							continue ;
						idp->nextb = cp + 1 ;
						break ;
					}
					else
					{
						/*
						 * If there is spare room in the buffer avoid trashing
						 * the last character
						 */
						if ( idp->end < &idp->buf[ idp->buffer_size ] )
							cp = idp->end ;
						else
							cp = &idp->buf[ idp->buffer_size - 1 ] ;
						break ;
					}
				}
		}
		else					/* buffer could not be extended */
			if ( b_left == 0 )
			{
				/*
				 * Set errno to 0 if EOF has been reached
				 */
				if ( extension == 0 )
					errno = 0 ;
				return( NULL ) ;
			}
			else
			{
				line_start = idp->start ;
				cp = idp->end ;
				/*
				 * By setting idp->nextb to be equal to idp->end,
				 * subsequent calls to Srdline will return NULL because
				 * __sio_extend_buffer will be invoked and it will return 0.
				 */
				idp->nextb = idp->end ;
			}
	}
	*cp = NUL ;
	idp->line_length = cp - line_start ;
	return( line_start ) ;
}



/*
 * SIO CONTROL FUNCTIONS
 */


/*
 * Flush the buffer associated with the given file descriptor
 * The special value, SIO_FLUSH_ALL flushes all buffers
 *
 * Return value:
 *			0 :  if fd is SIO_FLUSH_ALL or if the flush is successful
 *		SIO_ERR: if fd is not SIO_FLUSH_ALL and
 *								the flush is unsuccessful
 *							or the descriptor is not initialized or it is not 
 *								an output descriptor
 */
int Sflush( int fd )
{
   __sio_descriptor_t *dp ;
   int b_in_buffer ;

   if ( fd == SIO_FLUSH_ALL )
   {
      for ( fd = 0, dp = __sio_descriptors ;
				fd < __sio_n_descriptors ;
				dp++, fd++ )
         if ( DESCRIPTOR_INITIALIZED( dp ) &&
							dp->stream_type == __SIO_OUTPUT_STREAM )
            (void) __sio_writef( ODP( dp ), fd ) ;
      return( 0 ) ;
   }
   else
   {
      if ( fd >= __sio_n_descriptors ) {
         errno = EBADF ;
         return( SIO_ERR ) ;
      }

      dp = &__sio_descriptors[ fd ] ;

      if( (! DESCRIPTOR_INITIALIZED( dp )) || 
            (dp->stream_type != __SIO_OUTPUT_STREAM) )
         return( SIO_ERR );

      b_in_buffer = ODP( dp )->nextb - ODP( dp )->start ;
      if ( __sio_writef( ODP( dp ), fd ) != b_in_buffer )
         return( SIO_ERR ) ;
      else
         return( 0 ) ;
   }
}


/*
 * Close the file descriptor. This call is provided because
 * a file descriptor may be closed and then reopened. There is
 * no easy way for SIO to identify such a situation, so Sclose
 * must be used.
 *
 * Sclose invokes Sdone which finalizes the buffer.
 * There is no SIO_CLOSE_ALL value for fd because such a thing
 * would imply that the program will exit very soon, therefore
 * the closing of all file descriptors will be done in the kernel
 * (and the finalization will be done by the finalization function
 * NOTE: not true if the OS does not support a finalization function)
 *
 * There is no need to invoke SETUP; Sdone will do it.
 */
int Sclose( int fd )
{
	if ( __SIO_FD_INITIALIZED( fd ) )
		if ( Sdone( fd ) == SIO_ERR )
			return( SIO_ERR ) ;
	return( close( fd ) ) ;
}


/*
 * Changes the type of buffering on the specified descriptor.
 * As a side-effect, it initializes the descriptor as an output stream.
 */
int Sbuftype( int fd, int type )
{
	__sio_descriptor_t *dp ;

	/*
	 * Check for a valid type
	 */
	if ( type != SIO_LINEBUF && type != SIO_FULLBUF && type != SIO_NOBUF )
	{
		errno = EINVAL ;
		return( SIO_ERR ) ;
	}

	if( sio_setup( fd, &dp, __SIO_OUTPUT_STREAM ) == SIO_ERR )
		return( SIO_ERR );
	ODP( dp )->buftype = type ;
	return( 0 ) ;
}


#ifndef sio_memscan

static char *sio_memscan( const char *from, int how_many, char ch )
{
   char *p ;
   char *last = from + how_many ;

   for ( p = from ; p < last ; p++ )
      if ( *p == ch )
         return( p ) ;
      return( 0 ) ;
}

#endif	/* sio_memscan */


#ifdef NEED_MEMCOPY

void __sio_memcopy( const char *from, char *to, int nbytes )
{
   while ( nbytes-- )
      *to++ = *from++ ;
}

#endif /* NEED_MEMCOPY */

int sio_setup(int fd, __sio_descriptor_t **dp, int type)
{
   if ( fd >= __sio_n_descriptors ) {
      if( Smorefds(fd) != 0 ) {
         errno = EBADF;
         return(SIO_ERR);
      }
   }

   *dp = &__sio_descriptors[ fd ];

   if( DESCRIPTOR_INITIALIZED( *dp ) ) {
      if( (*dp)->stream_type != type ) {
         errno = EBADF;
         return( SIO_ERR );
      }
   } else if( __sio_init(*dp, fd, type) == SIO_ERR )
      return( SIO_ERR );

   return( 0 );
}

/*
 * The Sputchar function depends on the fact that the fields
 *                              nextb, buf_end, end
 * are 0 if a stream descriptor is not being used or has not yet been
 * initialized.
 * This is true initially because of the static allocation of the
 * descriptor array, and Sdone must make sure that it is true
 * after I/O on a descriptor is over.
 */
int Sputchar( int fd, char c )
{
   int ret = 0;

   if( __SIO_OD( fd ).nextb < __SIO_OD( fd ).buf_end ) {
      if( __SIO_OD(fd).buftype == SIO_FULLBUF ) {
         ret = *(__SIO_OD(fd).nextb)++ = (unsigned char) (c);
      } else if ( __SIO_OD(fd).buftype == SIO_LINEBUF ) {
         if( (*(__SIO_OD(fd).nextb) = (unsigned char)(c)) != '\n' ) {
            ret = *(__SIO_OD(fd).nextb)++;
         } else {
            ret = Sputc( fd, *(__SIO_OD(fd).nextb) );
         }
      } else {
         ret = Sputc( fd, c );
      }
   } else {
      ret = Sputc( fd, c );
   }

   return ret;
}


