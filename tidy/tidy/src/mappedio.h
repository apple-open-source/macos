#ifndef __TIDY_MAPPED_IO_H__
#define __TIDY_MAPPED_IO_H__

/* Interface to mmap style I/O

   (c) 2006 (W3C) MIT, ERCIM, Keio University
   See tidy.h for the copyright notice.

   $Id: mappedio.h,v 1.1 2007/01/31 02:44:03 iccir Exp $
*/

#if defined(_WIN32)
int TY_(DocParseFileWithMappedFile)( TidyDocImpl* doc, ctmbstr filnam );
#endif

#endif /* __TIDY_MAPPED_IO_H__ */
