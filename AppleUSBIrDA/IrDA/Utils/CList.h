/*
    File:       CList.h

    Contains:   Interface to the CList class


*/

#ifndef __CLIST_H
#define __CLIST_H

#include "CDynamicArray.h"

class CItemComparer;

//-----------------------------------------------------------------------
//      CList
//-----------------------------------------------------------------------
class CList : public CDynamicArray
{
    OSDeclareDefaultStructors(CList);

public:
    static CList* cList(ArrayIndex size = kDefaultChunkSize);
    Boolean init(ArrayIndex size = kDefaultChunkSize);
    void    free(void);

    // get

    void*       At(ArrayIndex index);
    void*       First(void);
    void*       Last(void);

    // insertion

    IrDAErr     Insert(void* item);
    Boolean     InsertUnique(void* item);
    IrDAErr     InsertBefore(ArrayIndex index, void* item);
    IrDAErr     InsertAt(ArrayIndex index, void* item);
    IrDAErr     InsertFirst(void* item);
    IrDAErr     InsertLast(void* item);
    
    // removal

    IrDAErr     Remove(void* item);
    IrDAErr     RemoveAt(ArrayIndex index);
    IrDAErr     RemoveFirst(void);
    IrDAErr     RemoveLast(void);

    // replacement

    IrDAErr     Replace(void* oldItem, void* newItem);
    IrDAErr     ReplaceAt(ArrayIndex index, void* newItem);
    IrDAErr     ReplaceFirst(void* newItem);
    IrDAErr     ReplaceLast(void* newItem);

    // indexing

    ArrayIndex  GetIdentityIndex(void* item);
    ArrayIndex  GetEqualityIndex(void* item);

    // searching

    void*       Search(CItemComparer* test, ArrayIndex& index);
    Boolean     Contains(void* item) { return GetIdentityIndex(item) != kEmptyIndex;}

    // old names from TList (remove when no longer referenced)
    long Count()                { return fSize; };
    Boolean Empty()             { return (fSize == 0);};
//  Boolean AddUnique(void* add) { return InsertUnique(add);}
//  ArrayIndex Index(void* item) { return GetIdentityIndex(item);}
//  void* Ith(ArrayIndex index) { return At(index);}

}; // CList


//-----------------------------------------------------------------------
//      CList inlines
//-----------------------------------------------------------------------

inline void* CList::First(void)
    { return At(0); }

inline void* CList::Last(void)
    { return At(fSize - 1); }

inline IrDAErr CList::Insert(void* item)
    { return InsertAt(fSize, item); }

inline IrDAErr CList::InsertBefore(ArrayIndex index, void* item)
    { return InsertAt(index, item); }

inline IrDAErr CList::InsertFirst(void* item)
    { return InsertAt(0, item); }

inline IrDAErr CList::InsertLast(void* item)
    { return InsertAt(fSize, item); }

inline IrDAErr CList::RemoveAt(ArrayIndex index)
    { return RemoveElementsAt(index, 1); }

inline IrDAErr CList::RemoveFirst()
    { return RemoveElementsAt(0, 1); }

inline IrDAErr CList::RemoveLast()
    { return RemoveElementsAt(fSize - 1, 1); }

inline IrDAErr CList::ReplaceFirst(void* newItem)
    { return ReplaceAt(0, newItem); }

inline IrDAErr CList::ReplaceLast(void* newItem)
    { return ReplaceAt(fSize - 1, newItem); }

inline ArrayIndex CList::GetEqualityIndex(void* item)
    { return GetIdentityIndex(item); }


#endif  /*  __CLIST_H   */
