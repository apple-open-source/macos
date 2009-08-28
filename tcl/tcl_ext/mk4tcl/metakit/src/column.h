// column.h --
// $Id: column.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Definition of the column classes
 */

#ifndef __COLUMN_H__
#define __COLUMN_H__

/////////////////////////////////////////////////////////////////////////////
// Declarations in this file

class c4_Column; // a column in a table
class c4_ColIter; // an iterator over column data
class c4_ColCache; // manages a cache for columns

class c4_Persist; // not defined here
class c4_Strategy; // not defined here

/////////////////////////////////////////////////////////////////////////////

class c4_Column {
    c4_PtrArray _segments;
    t4_i32 _position;
    t4_i32 _size;
    c4_Persist *_persist;
    t4_i32 _gap;
    int _slack;
    bool _dirty;

  public:
    c4_Column(c4_Persist *persist_);
    //: Constructs a column using the specified persistence manager.
    ~c4_Column();

    void SetBuffer(t4_i32);
    //: Allocate a new buffer of the specified size.

    c4_Persist *Persist()const;
    //: Returns persistence manager for this column, or zero.
    c4_Strategy &Strategy()const;
    //: Returns the associated strategy pointer.
    t4_i32 Position()const;
    //: Special access for the DUMP program.
    t4_i32 ColSize()const;
    //: Returns the number of bytes as stored on disk.
    bool IsDirty()const;
    //: Returns true if contents needs to be saved.

    void SetLocation(t4_i32, t4_i32);
    //: Sets the position and size of this column on file.
    void PullLocation(const t4_byte * &ptr_);
    //: Extract position and size of this column.

    int AvailAt(t4_i32 offset_)const;
    //: Returns number of bytes we can access at once.
    const t4_byte *LoadNow(t4_i32);
    //: Makes sure the data is loaded into memory.
    t4_byte *CopyNow(t4_i32);
    //: Makes sure a copy of the data is in memory.
    void Grow(t4_i32, t4_i32);
    //: Grows the buffer by inserting space.
    void Shrink(t4_i32, t4_i32);
    //: Shrinks the buffer by removing space.
    void SaveNow(c4_Strategy &, t4_i32 pos_);
    //: Save the buffer to file.

    const t4_byte *FetchBytes(t4_i32 pos_, int len_, c4_Bytes &buffer_, bool
      forceCopy_);
    //: Returns pointer to data, use buffer only if non-contiguous.
    void StoreBytes(t4_i32 pos_, const c4_Bytes &buffer_);
    //: Stores a copy of the buffer in the column.

    bool RequiresMap()const;
    void ReleaseAllSegments();

    static t4_i32 PullValue(const t4_byte * &ptr_);
    static void PushValue(t4_byte * &ptr_, t4_i32 v_);

    void InsertData(t4_i32 index_, t4_i32 count_, bool clear_);
    void RemoveData(t4_i32 index_, t4_i32 count_);
    void RemoveGap();

    enum {
        kSegBits = 12, kSegMax = 1 << kSegBits, kSegMask = kSegMax - 1
    };

  private:
    static int fSegIndex(t4_i32 offset_);
    static t4_i32 fSegOffset(int index_);
    static int fSegRest(t4_i32 offset_);

    bool UsesMap(const t4_byte*)const;
    bool IsMapped()const;

    void ReleaseSegment(int);
    void SetupSegments();
    void Validate()const;
    void FinishSlack();

    void MoveGapUp(t4_i32 pos_);
    void MoveGapDown(t4_i32 pos_);
    void MoveGapTo(t4_i32 pos_);

    t4_byte *CopyData(t4_i32, t4_i32, int);
};

/////////////////////////////////////////////////////////////////////////////

class c4_ColOfInts: public c4_Column {
  public:
    c4_ColOfInts(c4_Persist *persist_, int width_ = sizeof(t4_i32));

    int RowCount()const;
    void SetRowCount(int numRows_);

    void FlipBytes();

    int ItemSize(int index_);
    const void *Get(int index_, int &length_);
    void Set(int index_, const c4_Bytes &buf_);

    t4_i32 GetInt(int index_);
    void SetInt(int index_, t4_i32 value_);

    void Insert(int index_, const c4_Bytes &buf_, int count_);
    void Remove(int index_, int count_);

    static int CalcAccessWidth(int numRows_, t4_i32 colSize_);

    void SetAccessWidth(int bits_);
    void FixSize(bool fudge_);
    void ForceFlip();

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);

  private:
    typedef void(c4_ColOfInts:: *tGetter)(int);
    typedef bool(c4_ColOfInts:: *tSetter)(int, const t4_byte*);

    void Get_0b(int index_);
    void Get_1b(int index_);
    void Get_2b(int index_);
    void Get_4b(int index_);
    void Get_8i(int index_);
    void Get_16i(int index_);
    void Get_16r(int index_);
    void Get_32i(int index_);
    void Get_32r(int index_);
    void Get_64i(int index_);
    void Get_64r(int index_);

    bool Set_0b(int index_, const t4_byte *item_);
    bool Set_1b(int index_, const t4_byte *item_);
    bool Set_2b(int index_, const t4_byte *item_);
    bool Set_4b(int index_, const t4_byte *item_);
    bool Set_8i(int index_, const t4_byte *item_);
    bool Set_16i(int index_, const t4_byte *item_);
    bool Set_16r(int index_, const t4_byte *item_);
    bool Set_32i(int index_, const t4_byte *item_);
    bool Set_32r(int index_, const t4_byte *item_);
    bool Set_64i(int index_, const t4_byte *item_);
    bool Set_64r(int index_, const t4_byte *item_);

    void ResizeData(int index_, int count_, bool clear_ = false);

    tGetter _getter;
    tSetter _setter;

    union {
        t4_byte _item[8]; // holds temp result (careful with alignment!)
        double _aligner; // needed for SPARC
    };

    int _currWidth; // number of bits used for one entry (0..64)
    int _dataWidth; // number of bytes used for passing a value along
    int _numRows;
    bool _mustFlip;
};

/////////////////////////////////////////////////////////////////////////////

class c4_ColIter {
    c4_Column &_column;
    t4_i32 _limit;
    t4_i32 _pos;
    int _len;
    const t4_byte *_ptr;

  public:
    c4_ColIter(c4_Column &col_, t4_i32 offset_, t4_i32 limit_);
    //  ~c4_ColIter ();

    bool Next();
    bool Next(int max_);

    const t4_byte *BufLoad()const;
    t4_byte *BufSave();
    int BufLen()const;
};

/////////////////////////////////////////////////////////////////////////////

#if q4_INLINE
#include "column.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#endif
