// mk4io.h --
// $Id: mk4io.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Declaration of the file stream and strategy classes.
 */

#ifndef __MK4IO_H__
#define __MK4IO_H__

#include <stdio.h>

/////////////////////////////////////////////////////////////////////////////
/// A file stream can be used to serialize using the stdio library.

class c4_FileStream: public c4_Stream {
  public:
    c4_FileStream(FILE *stream_, bool owned_ = false);
    virtual ~c4_FileStream();

    virtual int Read(void *buffer_, int length_);
    virtual bool Write(const void *buffer_, int length_);

    FILE *_stream;
    bool _owned;
};

/////////////////////////////////////////////////////////////////////////////
/// A file strategy encapsulates code dealing with all file I/O.

class c4_FileStrategy: public c4_Strategy {
  public:
    /// Construct a new strategy object
    c4_FileStrategy(FILE *file_ = 0);
    virtual ~c4_FileStrategy();

    /// True if we can do I/O with this object
    virtual bool IsValid()const;
    /// Open a data file by name
    virtual bool DataOpen(const char *fileName_, int mode_);
    /// Read a number of bytes
    virtual int DataRead(t4_i32 pos_, void *buffer_, int length_);
    /// Write a number of bytes, return true if successful
    virtual void DataWrite(t4_i32 pos_, const void *buffer_, int length_);
    /// Flush and truncate file
    virtual void DataCommit(t4_i32 newSize_);
    /// Support for memory-mapped files
    virtual void ResetFileMapping();
    /// Report total size of the datafile
    virtual t4_i32 FileSize();
    /// Return a good value to use as fresh generation counter
    virtual t4_i32 FreshGeneration();

  protected:
    /// Pointer to file object
    FILE *_file;
    /// Pointer to same file object, if it must be deleted at end
    FILE *_cleanup;
};

/////////////////////////////////////////////////////////////////////////////

#endif // __MK4IO_H__
