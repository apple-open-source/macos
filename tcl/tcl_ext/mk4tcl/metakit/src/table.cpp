// table.cpp --
// $Id: table.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Loose ends, these should be moved
 */

#include "header.h"
#include "handler.h"
#include "store.h"
#include "field.h"
#include "format.h"
#include "persist.h"

/////////////////////////////////////////////////////////////////////////////
// Implemented in this file

class c4_Bytes;
class c4_HandlerSeq;

/////////////////////////////////////////////////////////////////////////////

/** @class c4_Bytes
 *
 *  Generic data buffer, with optional automatic clean up.
 *
 *  These objects are used to pass around untyped data without concern about
 *  clean-up.  They know whether the bytes need to be de-allocated when these
 *  objects go out of scope.  Small amounts of data are stored in the object.
 *
 *  Objects of this class are used a lot within Metakit to manipulate its own
 *  data items generically.  The c4_BytesProp class allows storing binary
 *  data explicitly in a file.  If such data files must be portable, then the 
 *  application itself must define a generic format to deal with byte order.
 *
 *  How to store an object in binary form in a row (this is not portable):
 * @code
 *    struct MyStruct { ... };
 *    MyStruct something;
 *  
 *    c4_BytesProp pData ("Data");
 *    c4_Row row;
 *  
 *    pData (row) = c4_Bytes (&something, sizeof something);
 * @endcode
 */

/// Construct an object with contents, optionally as a copy
c4_Bytes::c4_Bytes(const void *buf_, int len_, bool copy_): _size(len_), _copy
  (copy_) {
    _contents = (t4_byte*)buf_; // moved out of intializers for DEC CXX 5.7
    if (_copy)
      _MakeCopy();
}

/// Copy constructor   
c4_Bytes::c4_Bytes(const c4_Bytes &src_): _size(src_._size), _copy(src_._copy) {
  _contents = src_._contents; // moved out of intializers for DEC CXX 5.7
  if (_copy || _contents == src_._buffer)
    _MakeCopy();
}

/// Assignment, this may make a private copy of contents
c4_Bytes &c4_Bytes::operator = (const c4_Bytes &src_) {
  if (&src_ != this) {
    _LoseCopy();

    _contents = src_._contents;
    _size = src_._size;
    _copy = src_._copy;

    if (_copy || _contents == src_._buffer)
      _MakeCopy();
  }

  return  *this;
}

/// Swap the contents and ownership of two byte objects
void c4_Bytes::Swap(c4_Bytes &bytes_) {
  t4_byte *p = _contents;
  int s = _size;
  bool c = _copy;

  _contents = bytes_._contents;
  _size = bytes_._size;
  _copy = bytes_._copy;

  bytes_._contents = p;
  bytes_._size = s;
  bytes_._copy = c;

  // if either one is using its local buffer, swap those too
  if (_contents == bytes_._buffer || p == _buffer) {
    t4_byte t[sizeof _buffer];

    memcpy(t, _buffer, sizeof _buffer);
    memcpy(_buffer, bytes_._buffer, sizeof _buffer);
    memcpy(bytes_._buffer, t, sizeof _buffer);

    if (_contents == bytes_._buffer)
      _contents = _buffer;

    if (bytes_._contents == _buffer)
      bytes_._contents = bytes_._buffer;
  }
}

/// Define contents as a freshly allocated buffer of given size
t4_byte *c4_Bytes::SetBuffer(int length_) {
  /* No substantial improvement measured:
  Perhaps keep a correctly sized c4_Bytes object in each property?
  It means c4_...Ref objects would need to store a pointer, not an id.

  if (length_ == _size)
  return _contents; // no work needed, get out fast
   */
  _LoseCopy();

  _size = length_;
  _copy = _size > (int)sizeof _buffer;

  return _contents = _copy ? d4_new t4_byte[_size]: _buffer;
}

/// Allocate a buffer and fills its contents with zero bytes
t4_byte *c4_Bytes::SetBufferClear(int length_) {
  return (t4_byte*)memset(SetBuffer(length_), 0, length_);
}

void c4_Bytes::_MakeCopy() {
  d4_assert(_contents != 0);

  _copy = _size > (int)sizeof _buffer;

  if (_size > 0)
    _contents = (t4_byte*)memcpy(_copy ? d4_new t4_byte[_size]: _buffer,
      _contents, _size);
}

/// Return true if the contents of both objects are equal
bool operator == (const c4_Bytes &a_, const c4_Bytes &b_) {
  return a_._contents == b_._contents || (a_._size == b_._size && memcmp
    (a_._contents, b_._contents, a_._size) == 0);
}

/////////////////////////////////////////////////////////////////////////////
