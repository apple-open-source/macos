// handler.h --
// $Id: handler.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Definition of the main handler classes
 */

#ifndef __HANDLER_H__
#define __HANDLER_H__

/////////////////////////////////////////////////////////////////////////////
// Declarations in this file

class c4_Handler; // data representation handler

//  class c4_Sequence;
class c4_HandlerSeq; // a sequence built from handlers

class c4_Column; // not defined here
class c4_Field; // not defined here
class c4_Persist; // not defined here
class c4_SaveContext; // not defined here

/////////////////////////////////////////////////////////////////////////////

class c4_Handler {
    c4_Property _property;

  public:
    c4_Handler(const c4_Property &_prop);
    //: Constructor (this is an abstract base class).
    virtual ~c4_Handler();

    virtual void Define(int, const t4_byte **);
    //: Called when the corresponding table has been fully defined.
    virtual void FlipBytes();
    //: Called to reverse the internal byte order of foreign data.
    virtual void Commit(c4_SaveContext &ar_);
    //: Commit the associated column(s) to file.
    virtual void OldDefine(char, c4_Persist &);

    const c4_Property &Property()const;
    //: Returns the property associated with this handler.
    int PropId()const;
    //: Returns the id of the property associated with this handler.

    void ClearBytes(c4_Bytes &buf_)const;
    //: Returns the default value for items of this type.

    virtual int ItemSize(int index_) = 0;
    //: Return width of specified data item.
    void GetBytes(int index_, c4_Bytes &buf_, bool copySmall_ = false);
    //: Used for backward compatibility, should probably be replaced.
    virtual const void *Get(int index_, int &length_) = 0;
    //: Retrieves the data item at the specified index.
    virtual void Set(int index_, const c4_Bytes &buf_) = 0;
    //: Stores a new data item at the specified index.

    int Compare(int index_, const c4_Bytes &buf_);
    //: Compares an entry with a specified data item.

    virtual void Insert(int index_, const c4_Bytes &buf_, int count_) = 0;
    //: Inserts 1 or more data items at the specified index.
    virtual void Remove(int index_, int count_) = 0;
    //: Removes 1 or more data items at the specified index.
    void Move(int from_, int to_);
    //: Move a data item to another position.

    virtual c4_Column *GetNthMemoCol(int index_, bool alloc_ = false);
    //: Special access to underlying data of memo entries

    virtual bool IsPersistent()const;
    //: True if this handler might do I/O to satisfy fetches

    virtual void Unmapped();
    //: Make sure this handler stops using file mappings

    virtual bool HasSubview(int index_);
    //: True if this subview has materialized into an object
};

/////////////////////////////////////////////////////////////////////////////

class c4_HandlerSeq: public c4_Sequence {
    c4_PtrArray _handlers;
    c4_Persist *_persist;
    c4_Field *_field;
    c4_HandlerSeq *_parent;
    int _numRows;

  public:
    c4_HandlerSeq(c4_Persist*);
    c4_HandlerSeq(c4_HandlerSeq &owner_, c4_Handler *handler_);

    virtual int NumRows()const;
    virtual void SetNumRows(int);

    virtual int NumHandlers()const;
    virtual c4_Handler &NthHandler(int)const;
    virtual const c4_Sequence *HandlerContext(int)const;
    virtual int AddHandler(c4_Handler*);

    void DefineRoot();
    void Restructure(c4_Field &, bool remove_);
    void DetachFromParent();
    void DetachFromStorage(bool full_);
    void DetermineSpaceUsage();

    c4_Field &Definition()const;
    const char *Description();
    c4_HandlerSeq &Parent()const;
    virtual c4_Persist *Persist()const;

    c4_Field &Field(int)const;
    int NumFields()const;
    char ColumnType(int index_)const;
    bool IsNested(int)const;

    void Prepare(const t4_byte **ptr_, bool selfDesc_);
    void OldPrepare();

    void FlipAllBytes();
    void ExchangeEntries(int srcPos_, c4_HandlerSeq &dst_, int dstPos_);

    c4_HandlerSeq &SubEntry(int, int)const;

    c4_Field *FindField(const c4_Handler *handler_);

    void UnmappedAll();

    static void BuildMeta(int, int, c4_View &, const c4_Field &);

  protected:
    virtual c4_Handler *CreateHandler(const c4_Property &);

    virtual ~c4_HandlerSeq();
};

/////////////////////////////////////////////////////////////////////////////

#if q4_INLINE
#include "handler.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#endif
