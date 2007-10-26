/* fileio.c -- does standard I/O

  (c) 1998-2006 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author: iccir $ 
    $Date: 2007/01/30 23:46:51 $ 
    $Revision: 1.3 $ 

  Default implementations of Tidy input sources
  and output sinks based on standard C FILE*.

*/

#include <stdio.h>

#include "forward.h"
#include "fileio.h"
#include "tidy.h"

#ifndef SUPPORT_POSIX_MAPPED_FILES
typedef struct _fp_input_source
{
    FILE*        fp;
    TidyBuffer   unget;
} FileSource;

static int TIDY_CALL filesrc_getByte( void* sourceData )
{
  FileSource* fin = (FileSource*) sourceData;
  int bv;
  if ( fin->unget.size > 0 )
    bv = tidyBufPopByte( &fin->unget );
  else
    bv = fgetc( fin->fp );
  return bv;
}

static Bool TIDY_CALL filesrc_eof( void* sourceData )
{
  FileSource* fin = (FileSource*) sourceData;
  Bool isEOF = ( fin->unget.size == 0 );
  if ( isEOF )
    isEOF = feof( fin->fp ) != 0;
  return isEOF;
}

static void TIDY_CALL filesrc_ungetByte( void* sourceData, byte bv )
{
  FileSource* fin = (FileSource*) sourceData;
  tidyBufPutByte( &fin->unget, bv );
}

int TY_(initFileSource)( TidyInputSource* inp, FILE* fp )
{
  FileSource* fin = NULL;

  inp->getByte    = filesrc_getByte;
  inp->eof        = filesrc_eof;
  inp->ungetByte  = filesrc_ungetByte;

  fin = (FileSource*) MemAlloc( sizeof(FileSource) );
  ClearMemory( fin, sizeof(FileSource) );
  fin->fp = fp;
  inp->sourceData = fin;
  return 0;
}

void TY_(freeFileSource)( TidyInputSource* inp, Bool closeIt )
{
    FileSource* fin = (FileSource*) inp->sourceData;
    if ( closeIt && fin && fin->fp )
      fclose( fin->fp );
    tidyBufFree( &fin->unget );
    MemFree( fin );
}
#endif

void TIDY_CALL TY_(filesink_putByte)( void* sinkData, byte bv )
{
  FILE* fout = (FILE*) sinkData;
  fputc( bv, fout );
}

void TY_(initFileSink)( TidyOutputSink* outp, FILE* fp )
{
  outp->putByte  = TY_(filesink_putByte);
  outp->sinkData = fp;
}

/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
