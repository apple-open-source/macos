/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef _APPLE_
#undef HAVE_MMAP
#endif

#include "impl.h"
#include "sio.h"

static __sio_descriptor_t static_descriptor_array[ N_SIO_DESCRIPTORS ] ;
int __sio_n_descriptors = N_SIO_DESCRIPTORS ;
__sio_descriptor_t *__sio_descriptors = static_descriptor_array ;

static void terminate(const char *s);
static status_e setup_read_buffer( __sio_id_t *idp, unsigned buf_size );

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

/*
 * Code for finalization
 */
#ifdef HAVE_FINALIZATION_FUNCTION
static int finalizer_installed ;

SIO_DEFINE_FIN( sio_cleanup )
{
   (void) Sflush( SIO_FLUSH_ALL ) ;
}
#endif /* HAVE_FINALIZATION_FUNCTION */



#ifdef HAVE_MMAP

#define CHAR_NULL            ((char *)0)

/*
 * PAGES_MAPPED gives the size of each map unit in pages
 */
#define PAGES_MAPPED            2

static size_t map_unit_size = 0 ;         /* bytes */
static size_t page_size = 0 ;               /* bytes */

static mapd_s static_mapd_array[ N_SIO_DESCRIPTORS ] ;
static mapd_s *mmap_descriptors = static_mapd_array ;

#define MDP( fd )            ( mmap_descriptors + (fd) )


/*
 * NOTES ON MEMORY MAPPING:
 *
 *    1. Memory mapping works only for file descriptors opened for input
 *   2. Mapping an object to a part of the address space where another
 *   object is mapped will cause the old mapping to disappear (i.e. mmap
 *   will not fail)
 *
 * Memory mapping interface:
 *      SIO_MMAP : maps a file into a portion of the address space.
 *      SIO_MUNMAP: unmap a portion of the address space
 *      SIO_MNEED: indicate to the OS that we will need a portion of
 *                   our address space.
 *
 * The map_unit_size variable defines how much of the file is mapped at
 * a time. It is a multiple of the operating system page size. It is
 * not less than SIO_BUFFER_SIZE unless SIO_BUFFER_SIZE is not a
 * multiple of the page size (so the SIO_BUFFER_SIZE overrides
 * PAGES_MAPPED).
 *
 * NOTE: All memory mapping code is in this file only
 */


/*
 * Macros used by the memory mapping code
 */
#define FIRST_TIME( dp )      ( dp->buf == NULL )

/*
 * Functions to support memory mapping:
 *
 *         try_memory_mapping
 *         buffer_setup
 *         __sio_switch
 *         initial_map
 *         map_unit
 */

/*
 * try_memory_mapping attempts to setup the specified descriptor
 * for memory mapping. 
 * It returns FAILURE if it fails and SUCCESS if it is successful.
 * If HAVE_MMAP is not defined, the function is defined to be FAILURE.
 *
 * Sets fields:
 *      memory_mapped:          TRUE or FALSE
 *      
 * Also sets the following fields if memory_mapped is TRUE:
 *    file_offset, file_size, buffer_size
 *
 */
static status_e try_memory_mapping( int fd, __sio_id_t *idp, const struct stat *stp )
{
   int access_f ;

   /*
    * Do not try memory mapping if:
    *      1) The file is not a regular file
    *      2) The file is a regular file but has zero-length
    *      3) The file pointer is not positioned at the beginning of the file
    *      4) The fcntl to obtain the file descriptor flags fails
    *      5) The access mode is not O_RDONLY or O_RDWR
    *
    * The operations are done in this order to avoid the system calls
    * if possible.
    */
   if ( ( ( stp->st_mode & S_IFMT ) != S_IFREG ) ||
        ( stp->st_size == 0 ) ||
        ( lseek( fd, (long)0, 1 ) != 0 ) ||
        ( ( access_f = fcntl( fd, F_GETFL, 0 ) ) == -1 ) ||
        ( ( access_f &= 0x3 ) != O_RDONLY && access_f != O_RDWR ) )
   {
      idp->memory_mapped = FALSE ;
      return( FAILURE ) ;
   }

   /*
    * Determine page_size and map_unit_size.
    * Note that the code works even if PAGES_MAPPED is 0.
    */
   if ( page_size == 0 )
   {
      page_size = getpagesize() ;
      map_unit_size = page_size * PAGES_MAPPED ;
      if ( map_unit_size < SIO_BUFFER_SIZE )
      {
         if ( map_unit_size != 0 && SIO_BUFFER_SIZE % map_unit_size == 0 )
            map_unit_size = SIO_BUFFER_SIZE ;
         else
            map_unit_size = page_size ;
      }
   }
   
   MDP(fd)->file_offset = 0 ;
   MDP(fd)->file_size = stp->st_size ;
   idp->buffer_size = map_unit_size ;
   idp->buf = CHAR_NULL ;
   idp->memory_mapped = TRUE ;

   return( SUCCESS ) ;
}


/*
 * Copy the current_unit to the primary buffer
 *
 * Sets fields: start, end, nextb
 * Also sets the file pointer
 */
static void buffer_setup( __sio_id_t *idp, int fd, const struct map_unit *mu_cur, struct map_unit *mu_next )
{
   off_t new_offset ;

   sio_memcopy( mu_cur->addr, idp->buf, mu_cur->valid_bytes ) ;
   idp->start = idp->buf ;
   idp->end = idp->buf + mu_cur->valid_bytes ;
   idp->nextb = idp->buf + ( idp->nextb - mu_cur->addr ) ;

   if ( mu_next->addr != CHAR_NULL )
      new_offset = MDP(fd)->file_offset - mu_next->valid_bytes ;
   else
      new_offset = MDP(fd)->file_offset ;
   (void) lseek( fd, new_offset, 0 ) ;
}

/*
 * Switch from memory mapping to buffered I/O
 * If any mapping has occured, then the current unit is
 * copied into the buffer that is allocated.
 * Any data in the next unit is ignored.
 * We rely on idp->buf to identify the current unit (so it
 * better be equal to the address of one of the units).
 *
 * Sets fields:
 *         start, end, nextb
 */
status_e __sio_switch( __sio_id_t *idp, int fd )
{
   mapd_s *mdp = MDP( fd ) ;
   struct map_unit *mu_cur, *mu_next ;
   unsigned buffer_size = idp->buffer_size ;
   char *buf_addr = idp->buf ;
   int first_time = FIRST_TIME( idp ) ;

   /*
    * Initialize stream for buffering
    */
   if ( setup_read_buffer( idp, buffer_size ) == FAILURE )
      return( FAILURE ) ;

   if ( ! first_time )
   {
      /*
       * Find current, next unit
       */
      if ( buf_addr == mdp->first_unit.addr )
      {
         mu_cur = &mdp->first_unit ;
         mu_next = &mdp->second_unit ;
      }
      else
      {
         mu_cur = &mdp->second_unit ;
         mu_next = &mdp->first_unit ;
      }

      buffer_setup( idp, fd, mu_cur, mu_next ) ;
      /*
       * Destroy all mappings
       */
      (void) SIO_MUNMAP( mu_cur->addr, mu_cur->mapped_bytes ) ;
      if ( mu_next->addr != NULL )
         (void) SIO_MUNMAP( mu_next->addr, mu_next->mapped_bytes ) ;
   }
   else
      idp->start = idp->end = idp->nextb = idp->buf ;

   idp->memory_mapped = FALSE ;
   return( SUCCESS ) ;
}


/*
 * initial_map does the first memory map on the file descriptor.
 * It attempts to map both units.
 * The mapping always starts at file offset 0.
 *
 * SETS FIELDS:
 *         first_unit.*, second_unit.*
 *         file_offset
 *
 * Returns: 
 *         number of bytes mapped in first_unit
 *    or
 *         0 to indicate that mmap failed.
 */
static int initial_map( mapd_s *mdp, int fd )
{
   caddr_t addr ;
   size_t requested_length = 2 * map_unit_size ;
   size_t mapped_length = MIN( mdp->file_size, requested_length ) ;
   size_t bytes_left ;
   size_t bytes_in_unit ;

   addr = SIO_MMAP( CHAR_NULL, mapped_length, fd, 0 ) ;
   if ( addr == MAP_FAILED )
      return( 0 ) ;

   SIO_MNEED( addr, mapped_length ) ;

   /*
    * Map as much as possible in the first unit
    */
   bytes_in_unit = MIN( mapped_length, map_unit_size ) ;
   mdp->first_unit.addr          = addr ;
   mdp->first_unit.mapped_bytes    = bytes_in_unit ;
   mdp->first_unit.valid_bytes    = bytes_in_unit ;

   /*
    * If there is more, map it in the second unit.
    */
   bytes_left = mapped_length - bytes_in_unit ;
   if ( bytes_left != 0 )
   {
      mdp->second_unit.addr          = addr + bytes_in_unit ;
      mdp->second_unit.mapped_bytes = bytes_left ;
      mdp->second_unit.valid_bytes    = bytes_left ;
   }
   else
      mdp->second_unit.addr          = CHAR_NULL ;

   mdp->file_offset = mapped_length ;

   return( mdp->first_unit.valid_bytes ) ;
}


/*
 * ALGORITHM:
 *
 *      if ( there are more bytes in the file )
 *      {
 *         map them at the given unit
 *         update offset
 *         issue SIO_MNEED()
 *      }
 *      else
 *         unmap the unit
 */
static status_e map_unit( mapd_s *mdp, int fd, struct map_unit *mup )
{
   size_t bytes_left = mdp->file_size - mdp->file_offset ;
   size_t bytes_to_map = MIN( bytes_left, map_unit_size ) ;

   if ( bytes_to_map != 0 )
   {
      if ( SIO_MMAP( mup->addr, bytes_to_map,
                                             fd, mdp->file_offset ) == MAP_FAILED )
         return( FAILURE ) ;         /* XXX: need to do more ? */
      
      mup->valid_bytes = bytes_to_map ;
      ASSERT( mup->valid_bytes <= mup->mapped_bytes ) ;
      mdp->file_offset += bytes_to_map ;
      SIO_MNEED( mup->addr, mup->valid_bytes ) ;
   }
   else
   {
      (void) SIO_MUNMAP( mup->addr, mup->mapped_bytes ) ;
      mup->addr = CHAR_NULL ;
   }
   return( SUCCESS ) ;
}

#else

#define try_memory_mapping( x, y, z )            FAILURE

#endif /* HAVE_MMAP */


static status_e setup_read_buffer( __sio_id_t *idp, unsigned buf_size )
{
   char *buf ;

   /*
    * First allocate space for 2 buffers: primary and auxiliary
    */
   buf = malloc( buf_size * 2 ) ;
   if ( buf == NULL )
      return( FAILURE ) ;

   /*
    * The descriptor buf field should point to the start of the main buffer
    */
   idp->buf = buf + buf_size ;
   idp->buffer_size = buf_size ;
   return( SUCCESS ) ;
}


static status_e init_input_stream( __sio_id_t *idp, int fd, const struct stat *stp )
{
   /*
    * First initialize the fields relevant to buffering: buf, buffer_size
    */
   if ( try_memory_mapping( fd, idp, stp ) == FAILURE )
   {
      /*
       * Try to use normal buffering
       */
      unsigned buf_size = (unsigned)
                     ( stp->st_blksize ? stp->st_blksize : SIO_BUFFER_SIZE ) ;
      
      if ( setup_read_buffer( idp, buf_size ) == FAILURE )
         return( FAILURE ) ;
   }

    /*
    * Initialize remaining descriptor fields
    */
   idp->max_line_length = 2 * idp->buffer_size - 1 ;
   idp->start = idp->end = idp->nextb = idp->buf ;
   idp->tied_fd = SIO_NO_TIED_FD ;

   return( SUCCESS ) ;
}


static status_e init_output_stream( __sio_od_t *odp, int fd, struct stat *stp )
{
   unsigned buf_size ;
   char *buf ;

   buf_size = (unsigned)
                  ( stp->st_blksize ? stp->st_blksize : SIO_BUFFER_SIZE ) ;
   buf = malloc( buf_size ) ;
   if ( buf == NULL )
      return( FAILURE ) ;
   
   /*
    * Initialize buffering fields
    */
   odp->buf = buf ;
   odp->buffer_size = buf_size ;
   odp->buf_end = odp->buf + buf_size ;

   /*
    * Initialize remaining fields
    */
   odp->start = odp->nextb = odp->buf ;
   if ( isatty( fd ) )
      odp->buftype = SIO_LINEBUF ;

   if ( fd == 2 )
      odp->buftype = SIO_NOBUF ;

   return( SUCCESS ) ;
}


#ifndef HAVE_ISATTY

#ifdef HAVE_SYSVTTY

#include <termio.h>

static int isatty( int fd )
{
   struct termio t ;

   if ( ioctl( fd, TCGETA, &t ) == -1 && errno == ENOTTY )
      return( FALSE ) ;
   else
      return( TRUE ) ;
}
#endif   /* HAVE_SYSVTTY */

#ifdef HAVE_BSDTTY

#include <sgtty.h>

static int isatty( int fd )
{
   struct sgttyb s ;

   if ( ioctl( fd, TIOCGETP, &s ) == -1 && errno == ENOTTY )
      return( FALSE ) ;
   else
      return( TRUE ) ;
}
#endif   /* HAVE_BSDTTY */

#endif   /* ! HAVE_ISATTY */


/*
 * Initialize stream I/O for a file descriptor.
 *
 * Arguments:
 *      fd:            file descriptor
 *      dp:            descriptor pointer
 *      stream_type:    either __SIO_INPUT_STREAM or __SIO_OUTPUT_STREAM
 *
 * Returns
 *      0          if successful
 *     SIO_ERR   if the file descriptor is not valid (sets errno)
 *   exits      if stream_type is not __SIO_INPUT_STREAM or __SIO_OUTPUT_STREAM
 */
int __sio_init( __sio_descriptor_t *dp, int fd, enum __sio_stream stream_type )
{
   struct stat st ;

   memset(dp, 0, sizeof(__sio_descriptor_t));
   if ( fd >= __sio_n_descriptors )
   {
      errno = EBADF ;
      return( SIO_ERR ) ;
   }

   if ( fstat( fd, &st ) == -1 )
      return( SIO_ERR ) ;

   switch ( stream_type )
   {
      case __SIO_INPUT_STREAM:
         if ( init_input_stream( IDP( dp ), fd, &st ) == FAILURE )
            return( SIO_ERR ) ;
         break ;

      case __SIO_OUTPUT_STREAM:
         if ( init_output_stream( ODP( dp ), fd, &st ) == FAILURE )
            return( SIO_ERR ) ;
         break ;
         
      default:
         terminate( "SIO __sio_init: bad stream type (internal error).\n" ) ;
         /* NOTREACHED */
   }
   dp->stream_type = stream_type ;
   dp->initialized = TRUE ;

#ifdef HAVE_FINALIZATION_FUNCTION
   if ( ! finalizer_installed )
   {
      if ( ! SIO_FINALIZE( sio_cleanup ) )
      {
         char *s = "SIO __sio_init: finalizer installation failed\n" ;

         (void) write( 2, s, strlen( s ) ) ;
      }
      else
         finalizer_installed = TRUE ;
   }
#endif /* HAVE_FINALIZATION_FUNCTION */

   return( 0 ) ;
}


/*
 * __sio_writef writes the data in the buffer to the file descriptor.
 *
 * It tries to write as much data as possible until either all data
 * are written or an error occurs. EINTR is the only error that is
 * ignored.
 * In case an error occurs but some data were written, that number
 * is returned instead of SIO_ERR.
 *
 * Fields modified:
 *      When successful: start, nextb
 *      When not successful: start
 *
 * Return value:
 *      Number of bytes written
 *      SIO_ERR, if write(2) fails and no data were written
 */ 
int __sio_writef( __sio_od_t *odp, int fd )
{
   int b_in_buffer ;
   int cc_total = 0 ;

   /*
    * Make sure we don't exceed the buffer limits
    *   Maybe we should log this ?         XXX
    */
   if ( odp->nextb > odp->buf_end )
      odp->nextb = odp->buf_end ;

   b_in_buffer = odp->nextb - odp->start ;

   if ( b_in_buffer == 0 )
      return( 0 ) ;
   
   for ( ;; )
   {
      int cc ;

      cc = write( fd, odp->start, b_in_buffer ) ;
      if ( cc == b_in_buffer )
      {
         odp->start = odp->nextb = odp->buf ;
         cc_total += cc ;
         break ;
      }
      else if ( cc == -1 )
      {
         if ( errno == EINTR )
            continue ;
         else
            /*
             * If some bytes were written, return that number, otherwise
             * return SIO_ERR
             */
            return( ( cc_total != 0 ) ? cc_total : SIO_ERR ) ;
      }
      else         /* some bytes were written */
      {
         odp->start += cc ;         /* advance start of buffer */
         b_in_buffer -= cc ;         /* decrease number bytes left in buffer */
         cc_total += cc ;            /* count the bytes that were written */
      }
   }
   return( cc_total ) ;
}


/*
 * __sio_readf reads data from the file descriptor into the buffer.
 * Unlike __sio_writef it does NOT try to read as much data as will fit
 * in the buffer. It ignores EINTR.
 *
 * Returns: # of bytes read or SIO_ERR
 *
 * Fields set:
 *       If it does not return SIO_ERR, it sets start, nextb, end
 *         If it returns SIO_ERR, it does not change anything
 */
static int __sio_readf( __sio_id_t *idp, int fd )
{
   int cc ;

   /*
    * First check for a tied fd and flush the stream if necessary
    *
    *       XXX   the return value of __sio_writef is not checked.
    *               Is that right ?
    */
   if ( idp->tied_fd != SIO_NO_TIED_FD )
      (void) __sio_writef( &__SIO_OD( idp->tied_fd ), idp->tied_fd ) ;

#ifdef HAVE_MMAP
   if ( idp->memory_mapped )
   {
      mapd_s *mdp = MDP( fd ) ;

      /*
       * The functions initial_map and map_unit may fail.
       * In either case, we switch to buffered I/O.
       * If initial_map fails, we have read no data, so we
       * should perform a read(2).
       * If map_unit fails (for the next unit), we still have
       * the data in the current unit, so we can return.
       */
      if ( FIRST_TIME( idp ) )
      {
         cc = initial_map( mdp, fd ) ;
         if ( cc > 0 )
            idp->buf = mdp->first_unit.addr ;
         else
         {
            if ( __sio_switch( idp, fd ) == FAILURE )
               return( SIO_ERR ) ;
            cc = -1 ;
         }
      }
      else
      {
         struct map_unit *mu_cur, *mu_next ;

         if ( idp->buf == mdp->first_unit.addr )
         {
            mu_cur = &mdp->first_unit ;
            mu_next = &mdp->second_unit ;
         }
         else
         {
            mu_cur = &mdp->second_unit ;
            mu_next = &mdp->first_unit ;
         }

         if ( mu_next->addr != NULL )
         {
            idp->buf = mu_next->addr ;
            cc = mu_next->valid_bytes ;
            /*
             * XXX:  Here we may return SIO_ERR even though there
             *         are data in the current unit because the switch
             *         fails (possibly because malloc failed).
             */
            if ( map_unit( mdp, fd, mu_cur ) == FAILURE &&
                              __sio_switch( idp, fd ) == FAILURE )
               return( SIO_ERR ) ;
         }
         else
            cc = 0 ;
      }
      if ( cc >= 0 )
      {
         idp->end = idp->buf + cc ;
         idp->start = idp->nextb = idp->buf ;
         return( cc ) ;
      }
   }
#endif /* HAVE_MMAP */

   for ( ;; )
   {
      cc = read( fd, idp->buf, (int) idp->buffer_size ) ;
      if ( cc == -1 )
         if ( errno == EINTR )
            continue ;
         else
            return( SIO_ERR ) ;
      else
         break ;
   }

   idp->end = idp->buf + cc ;
   idp->start = idp->nextb = idp->buf ;
   return( cc ) ;
}


/*
 * __sio_extend_buffer is used by Srdline to extend the buffer
 * If successful, it returns the number of bytes that have been read.
 * If it fails (because of end-of-file or I/O error), it returns 0 or -1.
 *
 * Fields modified:
 *    idp->start points to the start of the buffer area (which is in the
 *    auxiliary buffer)
 *      Also, if successful, idp->nextb is set to idp->buf, idp->end is modified.
 */
int __sio_extend_buffer( __sio_id_t *idp, int fd, int b_left )
{
   int b_read ;

   /*
    * copy to auxiliary buffer
    */
   if ( b_left )
      sio_memcopy( idp->nextb, idp->buf - b_left, b_left ) ;
   b_read = __sio_readf( idp, fd ) ;
   idp->start = idp->buf - b_left ;
   return( b_read ) ;
}


/*
 * __sio_more tries to read more data from the given file descriptor iff
 * there is free space in the buffer.
 * __sio_more is used only by Srdline and only AFTER __sio_extend_buffer
 * has been called. This implies that 
 *      a) this is not a memory mapped file
 *      b) __sio_readf has been called (so we don't need to check for tied fd's
 *
 * Fields modified (only if successful):
 *         idp->end
 *
 * Return value: the number of bytes read.
 */
int __sio_more( __sio_id_t *idp, int fd )
{
   int b_left = &idp->buf[ idp->buffer_size ] - idp->end ;
   int cc ;

   if ( b_left <= 0 )
      return( 0 ) ;
   
   for ( ;; )
   {
      cc = read( fd, idp->end, b_left ) ;
      if ( cc >= 0 )
      {
         idp->end += cc ;
         return( cc ) ;
      }
      else
         if ( errno == EINTR )
            continue ;
         else
            return( SIO_ERR ) ;
   }
}


/*
 * Finalize a buffer by unmapping the file or freeing the malloc'ed memory
 */
int Sdone( int fd )
{
   __sio_descriptor_t *dp = &__sio_descriptors[ fd ] ;

   if ( fd >= __sio_n_descriptors || fd < __sio_n_descriptors || ! DESCRIPTOR_INITIALIZED( dp ) )
   {
      errno = EBADF ;
      return( SIO_ERR ) ;
   }

   switch ( dp->stream_type )
   {
      case __SIO_INPUT_STREAM:
         {
            __sio_id_t *idp = IDP( dp ) ;

#ifdef HAVE_MMAP
            if ( idp->memory_mapped )
            {
               mapd_s *mdp = MDP( fd ) ;

               if ( mdp->first_unit.addr != CHAR_NULL )
                  (void) SIO_MUNMAP( mdp->first_unit.addr,
                                          mdp->first_unit.mapped_bytes ) ;
               if ( mdp->second_unit.addr != CHAR_NULL )
                  (void) SIO_MUNMAP( mdp->second_unit.addr,
                                          mdp->second_unit.mapped_bytes ) ;
               idp->memory_mapped = FALSE ;
            }
            else
#endif   /* HAVE_MMAP */
               free( idp->buf - idp->buffer_size ) ;
               idp->nextb = idp->end = NULL ;
         }
         break ;
      
      case __SIO_OUTPUT_STREAM:
         {
            __sio_od_t *odp = ODP( dp ) ;

            if ( Sflush( fd ) == SIO_ERR )
               return( SIO_ERR ) ;
            free( odp->buf ) ;
            odp->nextb = odp->buf_end = NULL ;
         }
         break ;
      
      default:
         terminate( "SIO Sdone: bad stream type\n" ) ;
   }

   memset( dp, 0, sizeof(__sio_descriptor_t) );
   dp->initialized = FALSE ;
   return( 0 ) ;
}


static char *sioexpand( char *area, unsigned old_size, unsigned new_size, int is_static )
{
   char *new_area ;

   if ( is_static )
   {
      if ( ( new_area = malloc( new_size ) ) == NULL )
         return( NULL ) ;
      sio_memcopy( area, new_area, old_size ) ;
   }
   else
      if ( ( new_area = realloc( area, new_size ) ) == NULL )
         return( NULL ) ;
      
   return( new_area ) ;
}


/*
 * Expand the descriptor array (and if we use memory mapping the
 * memory mapping descriptors). We first expand the memory mapping
 * descriptors.
 * There is no problem if the expansion of the SIO descriptors fails
 * (i.e. there is no need to undo anything).
 */
int Smorefds(int fd)
{
   char *p ;
   int is_static ;
   unsigned new_size, old_size ;
   int n_fds = 5;

   if ( (__sio_n_descriptors+n_fds) < fd )
      n_fds = (fd - __sio_n_descriptors)*2;

#ifdef HAVE_MMAP
   old_size = __sio_n_descriptors * sizeof( mapd_s ) ;
   new_size = n_fds * sizeof( mapd_s ) ;
   new_size += old_size;
   is_static = ( mmap_descriptors == static_mapd_array ) ;
   p = sioexpand( (char *)mmap_descriptors, old_size, new_size, is_static ) ;
   if ( p == NULL )
      return( SIO_ERR ) ;
   memset(p+old_size, 0, new_size-old_size);
   mmap_descriptors = (mapd_s *) p ;
#endif   /* HAVE_MMAP */
   
   old_size = __sio_n_descriptors * sizeof( __sio_descriptor_t ) ;
   new_size = n_fds * sizeof( __sio_descriptor_t ) ;
   new_size += old_size;
   is_static =  ( __sio_descriptors == static_descriptor_array ) ;
   p = sioexpand( (char *)__sio_descriptors, old_size, new_size, is_static ) ;
   if ( p == NULL )
      return( SIO_ERR ) ;
   memset(p+old_size, 0, new_size-old_size);
   __sio_descriptors = (__sio_descriptor_t *) p ;

   __sio_n_descriptors += n_fds ;
   return( 0 ) ;
}


/*
 * Simple function that prints the string s at stderr and then calls
 * exit
 */
static void terminate( const char *s )
{
   (void) write( 2, s, strlen( s ) ) ;
   (void) abort() ;
   exit( 1 ) ;            /* in case abort fails */
}

void sio_init( void )
{
   memset( __sio_descriptors, 0, sizeof(__sio_descriptor_t) * N_SIO_DESCRIPTORS );
#ifdef HAVE_MMAP
   memset( mmap_descriptors, 0, sizeof(mapd_s) * N_SIO_DESCRIPTORS );
#endif
}

