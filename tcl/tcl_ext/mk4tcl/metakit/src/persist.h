// persist.h --
// $Id: persist.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Definition of the core file management classes
 */

#ifndef __PERSIST_H__
#define __PERSIST_H__

/////////////////////////////////////////////////////////////////////////////
// Declarations in this file

class c4_SaveContext; // wraps file commits
class c4_Persist; // persistent table storage

class c4_Allocator; // not defined here
class c4_Column; // not defined here
class c4_Differ; // not defined here
class c4_FileMark; // not defined here
class c4_Strategy; // not defined here
class c4_HandlerSeq; // not defined here

/////////////////////////////////////////////////////////////////////////////

class c4_SaveContext {
    c4_Strategy &_strategy;
    c4_Column *_walk;
    c4_Differ *_differ;

    c4_Allocator *_space;
    c4_Allocator *_cleanup;
    c4_Allocator *_nextSpace;

    bool _preflight;
    bool _fullScan;
    int _mode;

    c4_DWordArray _newPositions;
    int _nextPosIndex;

    t4_byte *_bufPtr;
    t4_byte *_curr;
    t4_byte *_limit;
    t4_byte _buffer[512];

  public:
    c4_SaveContext(c4_Strategy &strategy_, bool fullScan_, int mode_, c4_Differ
      *differ_, c4_Allocator *space_);
    ~c4_SaveContext();

    void SaveIt(c4_HandlerSeq &root_, c4_Allocator **spacePtr_, c4_Bytes
      &rootWalk_);

    void StoreValue(t4_i32 v_);
    bool CommitColumn(c4_Column &col_);
    void CommitSequence(c4_HandlerSeq &seq_, bool selfDesc_);

    c4_Column *SetWalkBuffer(c4_Column *walk_);
    bool IsFlipped()const;

    bool Serializing()const;
    void AllocDump(const char *, bool = false);

  private:
    void FlushBuffer();
    void Write(const void *buf_, int len_);
};

/////////////////////////////////////////////////////////////////////////////

class c4_Persist {
    c4_Allocator *_space;
    c4_Strategy &_strategy;
    c4_HandlerSeq *_root;
    c4_Differ *_differ;
    c4_Bytes _rootWalk;
    bool(c4_Persist:: *_fCommit)(bool);
    int _mode;
    bool _owned;

    // used for on-the-fly conversion of old-format datafiles
    t4_byte *_oldBuf;
    const t4_byte *_oldCurr;
    const t4_byte *_oldLimit;
    t4_i32 _oldSeek;

    int OldRead(t4_byte *buf_, int len_);

  public:
    c4_Persist(c4_Strategy &, bool owned_, int mode_);
    ~c4_Persist();

    c4_HandlerSeq &Root()const;
    void SetRoot(c4_HandlerSeq *root_);
    c4_Strategy &Strategy()const;

    bool AutoCommit(bool = true);
    void DoAutoCommit();

    bool SetAside(c4_Storage &aside_);
    c4_Storage *GetAside()const;

    bool Commit(bool full_);
    bool Rollback(bool full_);

    bool LoadIt(c4_Column &walk_);
    void LoadAll();

    t4_i32 LookupAside(int id_);
    void ApplyAside(int id_, c4_Column &col_);

    void OccupySpace(t4_i32 pos_, t4_i32 len_);

    t4_i32 FetchOldValue();
    void FetchOldLocation(c4_Column &col_);

    t4_i32 FreeBytes(t4_i32 *bytes_ = 0);

    static c4_HandlerSeq *Load(c4_Stream*);
    static void Save(c4_Stream *, c4_HandlerSeq &root_);
};

/////////////////////////////////////////////////////////////////////////////

#endif
