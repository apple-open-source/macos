// mk4.h --
// $Id: mk4.h 1669 2007-06-16 00:23:25Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Main Metakit library include file
 */

#ifndef __MK4_H__
#define __MK4_H__

//---------------------------------------------------------------------------
//
//  TITLE
//                                
//      The Metakit Library, by Jean-Claude Wippler, Equi4 Software, NL.
//      
//  DESCRIPTION
//                                
//      Structured data storage with commit / rollback and on-demand loading.
//  
//  ACKNOWLEDGEMENTS
//                                                                        
//      To Liesbeth and Myra, for making this possible.
//
//---------------------------------------------------------------------------
//
//  NAMING CONVENTIONS        PREFIX    REMARKS
//                              
//      Compile time options    q4_     Always defined as 1 or 0, capitalized
//      Preprocessor defines    d4_     Use with "#ifdef" or "#if defined()"
//      Classes                 c4_     Classes, listed at start of headers
//      Typedefs                t4_     Type definitions, if outside classes
//      Global functions        f4_     Internal, these are rarely defined
//
//      Member functions                Start in uppercase
//      Instance variables      _       And start in lowercase
//      Static members          _       And start in uppercase
//
//      Local variable names            Start in lowercase
//      Formal parameter names          Start lowercase, end with underscore
//
//---------------------------------------------------------------------------

/// Current release = 100 * major + 10 * minor + maintenance
#define d4_MetakitLibraryVersion 249    // 2.4.9.7 release, Jun 16, 2007
#define d4_MetaKitLibraryVersion d4_MetakitLibraryVersion // compat, yuck

//---------------------------------------------------------------------------
// Declarations in this file

class c4_View; // a view on underlying data
class c4_Cursor; // an index into a view
class c4_RowRef; // a reference to a row
class c4_Row; // one row in a view
class c4_Bytes; // used to pass around generic data
class c4_Storage; // manages view persistence
class c4_CustomViewer; // used for customizable views
class c4_Stream; // abstract stream class
class c4_Strategy; // system and file interface

class c4_Property; // for access inside rows
class c4_IntProp;
class c4_LongProp;
class c4_FloatProp;
class c4_DoubleProp;
class c4_StringProp;
class c4_BytesProp;
class c4_ViewProp;

// Everything below is part of the implementation, not for public use

class c4_Sequence; // a collection of rows

class c4_Reference; // refers to the actual data values
class c4_IntRef;
class c4_LongRef;
class c4_FloatRef;
class c4_DoubleRef;
class c4_BytesRef;
class c4_StringRef;
class c4_ViewRef;

class c4_Dependencies; // not defined here
class c4_Handler; // not defined here
class c4_Notifier; // not defined here
class c4_Persist; // not defined here

//---------------------------------------------------------------------------

// determine whether we need to include "mk4dll.h" to link as DLL
#if defined (MKDLL_EXPORTS) && !defined (q4_KITDLL)
#define q4_KITDLL 1
#endif 

// omit floats and doubles in small model 16-bit Intel builds
#if defined (_DOS) && defined (_M_I86SM) && !defined (q4_TINY)
#define q4_TINY 1
#endif 

// and here's the other end of the scale...
#if !defined (_WIN32) && !defined (q4_LONG64)
#if defined (_PA_RISC2_0) || defined (__powerpc64__) || defined(__sparcv9) || \
defined(__x86_64__) || defined(__s390x__) || defined(__alpha) ||  \
  (defined(__ia64) && (!defined(__HP_aCC) || defined(__LP64__))) || \
  (defined(__APPLE__) && defined(__LP64__))
#define q4_LONG64 1
#endif 
#endif 

// default to inlining for maximum performance
#if !defined (q4_INLINE)
#define q4_INLINE 1
#endif 

//---------------------------------------------------------------------------

// Borland C++ and C++ Builder
#if defined (__BORLANDC__)
// by default, if runtime is linked as a DLL, then so is Metakit
#if defined (_RTLDLL) && !defined (q4_KITDLL)
#define q4_KITDLL 1
#endif 

// Borland 5.0 supports the bool datatype
#if __BORLANDC__ >= 0x500
#define q4_BOOL 1
#endif 
#endif // __BORLANDC__

// IRIX supports the bool datatype
// define before gcc to cover both the gcc and MipsPRO compiler
#if defined (sgi)
#define q4_BOOL 1
#undef bool
#undef true
#undef false
#endif 

// GNU gcc/egcs
#if defined (__GNUC__)
#ifndef q4_BOOL
#define q4_BOOL 1
#endif 
#ifndef HAVE_LONG_LONG
#define HAVE_LONG_LONG 1
#endif 
#endif 

// HP aCC
#if defined (__HP_aCC)
#ifndef HAVE_LONG_LONG
#define HAVE_LONG_LONG 1
#endif 
#endif 

// Metrowerks CodeWarrior
#if defined (__MWERKS__)
#if __option(bool)
#define q4_BOOL 1       // bool datatype is optionally supported
// undef, these conflict with c4_Storage::c4_Storage overloading
#undef bool
#undef true
#undef false
#endif 
#endif 

// Microsoft Visual C++
#if defined (_MSC_VER)
// MSVC 5.0 supports the bool datatype, MSVC 4.x has no namespaces
#if _MSC_VER >= 1100
#define q4_BOOL 1
#define LONG_LONG __int64
#else 
#define q4_NO_NS 1
#endif 

// a kludge to avoid having to use ugly DLL exprt defs in this header
#pragma warning(disable: 4273) // inconsistent dll linkage
#endif // _MSC_VER

//---------------------------------------------------------------------------
// Other definitions needed by the public Metakit library header files

#if !q4_BOOL && !q4_STD         // define a bool datatype
#define false 0
#define true 1
#define bool int
#endif 

#if q4_KITDLL                   // add declaration specifiers
#include "mk4dll.h"
#endif 

#if q4_INLINE                   // enable inline expansion
#define d4_inline inline
#else 
#define d4_inline
#endif 

typedef unsigned char t4_byte; // create typedefs for t4_byte, etc.

#if q4_LONG64
typedef int t4_i32; // if longs are 64b, then int must be 32b
#else 
typedef long t4_i32; // if longs aren't 64b, then they are 32b
#endif 

#if q4_LONG64           // choose a way to represent 64b integers
typedef long t4_i64;
#elif defined (LONG_LONG)
typedef LONG_LONG t4_i64;
#elif HAVE_LONG_LONG
typedef long long t4_i64;
#else 
struct t4_i64 {
    long l1;
    long l2;
};
bool operator == (const t4_i64 a_, const t4_i64 b_);
bool operator < (const t4_i64 a_, const t4_i64 b_);
#endif 

//---------------------------------------------------------------------------

class c4_View {
  protected:
    c4_Sequence *_seq;

  public:
    /* Construction / destruction / assignment */
    c4_View(c4_Sequence * = 0);
    c4_View(c4_CustomViewer*);
    c4_View(c4_Stream*);
    c4_View(const c4_Property &property_);
    c4_View(const c4_View &);
    ~c4_View();

    c4_View &operator = (const c4_View &);
    c4_Persist *Persist()const; // added 16-11-2000 to simplify c4_Storage

    /* Getting / setting the number of rows */
    int GetSize()const;
    void SetSize(int, int =  - 1);

    void RemoveAll();

    /*: Getting / setting individual elements */
    c4_RowRef GetAt(int)const;
    c4_RowRef operator[](int)const;

    void SetAt(int, const c4_RowRef &);
    c4_RowRef ElementAt(int);

    bool GetItem(int, int, c4_Bytes &)const;
    void SetItem(int, int, const c4_Bytes &)const;

    /* These can increase the number of rows */
    void SetAtGrow(int, const c4_RowRef &);
    int Add(const c4_RowRef &);

    /* Insertion / deletion of rows */
    void InsertAt(int, const c4_RowRef &, int = 1);
    void RemoveAt(int, int = 1);
    void InsertAt(int, const c4_View &);

    bool IsCompatibleWith(const c4_View &)const;
    void RelocateRows(int, int, c4_View &, int);

    /* Dealing with the properties of this view */
    int NumProperties()const;
    const c4_Property &NthProperty(int)const;
    int FindProperty(int);
    int FindPropIndexByName(const char*)const;
    c4_View Duplicate()const;
    c4_View Clone()const;
    int AddProperty(const c4_Property &);
    c4_View operator, (const c4_Property &)const;

    const char *Description()const;

    /* Derived views */
    c4_View Sort()const;
    c4_View SortOn(const c4_View &)const;
    c4_View SortOnReverse(const c4_View &, const c4_View &)const;

    c4_View Select(const c4_RowRef &)const;
    c4_View SelectRange(const c4_RowRef &, const c4_RowRef &)const;

    c4_View Project(const c4_View &)const;
    c4_View ProjectWithout(const c4_View &)const;

    int GetIndexOf(const c4_RowRef &)const;
    int RestrictSearch(const c4_RowRef &, int &, int &);

    /* Custom views */
    c4_View Slice(int, int =  - 1, int = 1)const;
    c4_View Product(const c4_View &)const;
    c4_View RemapWith(const c4_View &)const;
    c4_View Pair(const c4_View &)const;
    c4_View Concat(const c4_View &)const;
    c4_View Rename(const c4_Property &, const c4_Property &)const;

    c4_View GroupBy(const c4_View &, const c4_ViewProp &)const;
    c4_View Counts(const c4_View &, const c4_IntProp &)const;
    c4_View Unique()const;

    c4_View Union(const c4_View &)const;
    c4_View Intersect(const c4_View &)const;
    c4_View Different(const c4_View &)const;
    c4_View Minus(const c4_View &)const;

    c4_View JoinProp(const c4_ViewProp &, bool = false)const;
    c4_View Join(const c4_View &, const c4_View &, bool = false)const;

    c4_View ReadOnly()const;
    c4_View Hash(const c4_View &, int = 1)const;
    c4_View Blocked()const;
    c4_View Ordered(int = 1)const;
    c4_View Indexed(const c4_View &, const c4_View &, bool = false)const;

    /* Searching */
    int Find(const c4_RowRef &, int = 0)const;
    int Search(const c4_RowRef &)const;
    int Locate(const c4_RowRef &, int * = 0)const;

    /* Comparing view contents */
    int Compare(const c4_View &)const;

    friend bool operator == (const c4_View &, const c4_View &);
    friend bool operator != (const c4_View &, const c4_View &);
    friend bool operator < (const c4_View &, const c4_View &);
    friend bool operator > (const c4_View &, const c4_View &);
    friend bool operator <= (const c4_View &, const c4_View &);
    friend bool operator >= (const c4_View &, const c4_View &);

  protected:
    void _IncSeqRef();
    void _DecSeqRef();

    /// View references are allowed to peek inside view objects
    friend class c4_ViewRef;

    // DROPPED: Structure() const;
    // DROPPED: Description(const c4_View& view_);
};

//---------------------------------------------------------------------------

#if defined(os_aix) && defined(compiler_ibmcxx) && (compiler_ibmcxx > 500)
bool operator == (const c4_RowRef &a_, const c4_RowRef &b_);
bool operator != (const c4_RowRef &a_, const c4_RowRef &b_);
bool operator <= (const c4_RowRef &a_, const c4_RowRef &b_);
bool operator >= (const c4_RowRef &a_, const c4_RowRef &b_);
bool operator > (const c4_RowRef &a_, const c4_RowRef &b_);
bool operator < (const c4_RowRef &a_, const c4_RowRef &b_);
#endif 

class c4_Cursor {
  public:
    /// Pointer to the sequence
    c4_Sequence *_seq;
    /// Current index into the sequence
    int _index;

    /* Construction / destruction / dereferencing */
    /// Construct a new cursor
    c4_Cursor(c4_Sequence &, int);

    /// Dereference this cursor to "almost" a row
    c4_RowRef operator *()const;

    /// This is the same as *(cursor + offset)
    c4_RowRef operator[](int)const;

    /* Stepping the iterator forwards / backwards */
    /// Pre-increment the cursor
    c4_Cursor &operator++();
    /// Post-increment the cursor
    c4_Cursor operator++(int);
    /// Pre-decrement the cursor
    c4_Cursor &operator--();
    /// Post-decrement the cursor
    c4_Cursor operator--(int);

    /// Advance by a given offset
    c4_Cursor &operator += (int);
    /// Back up by a given offset
    c4_Cursor &operator -= (int);

    /// Subtract a specified offset
    c4_Cursor operator - (int)const;
    /// Return the distance between two cursors
    int operator - (c4_Cursor)const;

    /// Add specified offset
    friend c4_Cursor operator + (c4_Cursor, int);
    /// Add specified offset to cursor
    friend c4_Cursor operator + (int, c4_Cursor);

    /* Comparing row positions */
    /// Return true if both cursors are equal
    friend bool operator == (c4_Cursor, c4_Cursor);
    /// Return true if both cursors are not equal
    friend bool operator != (c4_Cursor, c4_Cursor);
    /// True if first cursor is less than second cursor
    friend bool operator < (c4_Cursor, c4_Cursor);
    /// True if first cursor is greater than second cursor
    friend bool operator > (c4_Cursor, c4_Cursor);
    /// True if first cursor is less or equal to second cursor
    friend bool operator <= (c4_Cursor, c4_Cursor);
    /// True if first cursor is greater or equal to second cursor
    friend bool operator >= (c4_Cursor, c4_Cursor);

    /* Comparing row contents */
    /// Return true if the contents of both rows are equal
    friend bool operator == (const c4_RowRef &, const c4_RowRef &);
    /// Return true if the contents of both rows are not equal
    friend bool operator != (const c4_RowRef &, const c4_RowRef &);
    /// True if first row is less than second row
    friend bool operator < (const c4_RowRef &, const c4_RowRef &);
    /// True if first row is greater than second row
    friend bool operator > (const c4_RowRef &, const c4_RowRef &);
    /// True if first row is less or equal to second row
    friend bool operator <= (const c4_RowRef &, const c4_RowRef &);
    /// True if first row is greater or equal to second row
    friend bool operator >= (const c4_RowRef &, const c4_RowRef &);
};

//---------------------------------------------------------------------------

class c4_RowRef {
    /// A row reference is a cursor in disguise
    c4_Cursor _cursor;

  public:
    /* General operations */
    /// Assign the value of another row to this one
    c4_RowRef operator = (const c4_RowRef &);
    /// Return the cursor associated to this row
    c4_Cursor operator &()const;
    /// Return the underlying container view
    c4_View Container()const;

  protected:
    /// Constructor, not for general use
    c4_RowRef(c4_Cursor);

    friend class c4_Cursor;
    friend class c4_Row;
};

//---------------------------------------------------------------------------
/// An entry in a collection with copy semantics.
//
//  Rows can exist by themselves and as contents of views.  Row assignment
//  implies that a copy of the contents of the originating row is made.
//
//  A row is implemented as an unattached view with exactly one element.

class c4_Row: public c4_RowRef {
  public:
    /// Construct a row with no properties
    c4_Row();
    /// Construct a row from another one
    c4_Row(const c4_Row &);
    /// Construct a row copy from a row reference
    c4_Row(const c4_RowRef &);
    /// Destructor
    ~c4_Row();

    /// Assign a copy of another row to this one
    c4_Row &operator = (const c4_Row &);
    /// Copy another row to this one
    c4_Row &operator = (const c4_RowRef &);

    /// Add all properties and values into this row
    void ConcatRow(const c4_RowRef &);
    /// Return a new row which is the concatenation of two others
    friend c4_Row operator + (const c4_RowRef &, const c4_RowRef &);

  private:
    static c4_Cursor Allocate();
    static void Release(c4_Cursor);
};

//---------------------------------------------------------------------------

class c4_Bytes {
    union {
        t4_byte _buffer[16];
        double _aligner; // on a Sparc, the int below wasn't enough...
    };

    t4_byte *_contents;
    int _size;
    bool _copy;

  public:
    c4_Bytes();
    c4_Bytes(const void *, int);
    c4_Bytes(const void *, int, bool);
    c4_Bytes(const c4_Bytes &);
    ~c4_Bytes();

    c4_Bytes &operator = (const c4_Bytes &);
    void Swap(c4_Bytes &);

    int Size()const;
    const t4_byte *Contents()const;

    t4_byte *SetBuffer(int);
    t4_byte *SetBufferClear(int);

    friend bool operator == (const c4_Bytes &, const c4_Bytes &);
    friend bool operator != (const c4_Bytes &, const c4_Bytes &);

  private:
    void _MakeCopy();
    void _LoseCopy();
};

//---------------------------------------------------------------------------

class c4_Storage: public c4_View {
  public:
    /// Construct streaming-only storage object
    c4_Storage();
    /// Construct a storage using the specified strategy handler
    c4_Storage(c4_Strategy &, bool = false, int = 1);
    /// Construct a storage object, keeping the current structure
    c4_Storage(const char *, int);
    /// Reconstruct a storage object from a suitable view
    c4_Storage(const c4_View &);
    /// Destructor, usually closes file, but does not commit by default
    ~c4_Storage();

    void SetStructure(const char*);
    bool AutoCommit(bool = true);
    c4_Strategy &Strategy()const;
    const char *Description(const char * = 0);

    bool SetAside(c4_Storage &);
    c4_Storage *GetAside()const;

    bool Commit(bool = false);
    bool Rollback(bool = false);

    c4_ViewRef View(const char*);
    c4_View GetAs(const char*);

    bool LoadFrom(c4_Stream &);
    void SaveTo(c4_Stream &);

    t4_i32 FreeSpace(t4_i32 *bytes_ = 0);

    //DROPPED: c4_Storage (const char* filename_, const char* description_);
    //DROPPED: c4_View Store(const char* name_, const c4_View& view_);
    //DROPPED: c4_HandlerSeq& RootTable() const;
    //DROPPED: c4_RowRef xContents() const;

  private:
    void Initialize(c4_Strategy &, bool, int);
};

//---------------------------------------------------------------------------

class c4_Property {
    short _id;
    char _type;

  public:
    /// Construct a new property with the give type and id
    c4_Property(char, int);
    /// Construct a new property with the give type and name
    c4_Property(char, const char*);
    ~c4_Property();

    c4_Property(const c4_Property &);
    void operator = (const c4_Property &);

    const char *Name()const;
    char Type()const;

    int GetId()const;

    c4_Reference operator()(const c4_RowRef &)const;

    void Refs(int)const;

    c4_View operator, (const c4_Property &)const;

    static void CleanupInternalData();
};

/// Integer properties.
class c4_IntProp: public c4_Property {
  public:
    /// Construct a new property
    c4_IntProp(const char*);
    /// Destructor
    ~c4_IntProp();

    /// Get or set an integer property in a row
    c4_IntRef operator()(const c4_RowRef &)const;
    /// Get an integer property in a row
    t4_i32 Get(const c4_RowRef &)const;
    /// Set an integer property in a row
    void Set(const c4_RowRef &, t4_i32)const;

    /// Creates a row with one integer, shorthand for AsRow.
    c4_Row operator[](t4_i32)const;
    /// Creates a row with one integer.
    c4_Row AsRow(t4_i32)const;
};

#if !q4_TINY

/// Long int properties.
class c4_LongProp: public c4_Property {
  public:
    /// Construct a new property
    c4_LongProp(const char*);
    /// Destructor
    ~c4_LongProp();

    /// Get or set a long int property in a row
    c4_LongRef operator()(const c4_RowRef &)const;
    /// Get a long int property in a row
    t4_i64 Get(const c4_RowRef &)const;
    /// Set a long int property in a row
    void Set(const c4_RowRef &, t4_i64)const;

    /// Creates a row with one long int, shorthand for AsRow.
    c4_Row operator[](t4_i64)const;
    /// Creates a row with one long int.
    c4_Row AsRow(t4_i64)const;
};

/// Floating point properties.
class c4_FloatProp: public c4_Property {
  public:
    /// Construct a new property
    c4_FloatProp(const char*);
    /// Destructor
    ~c4_FloatProp();

    /// Get or set a floating point property in a row
    c4_FloatRef operator()(const c4_RowRef &)const;
    /// Get a floating point property in a row
    double Get(const c4_RowRef &)const;
    /// Set a floating point property in a row
    void Set(const c4_RowRef &, double)const;

    /// Create a row with one floating point value, shorthand for AsRow
    c4_Row operator[](double)const;
    /// Create a row with one floating point value
    c4_Row AsRow(double)const;
};

/// Double precision properties.
class c4_DoubleProp: public c4_Property {
  public:
    /// Construct a new property.
    c4_DoubleProp(const char*);
    /// Destructor
    ~c4_DoubleProp();

    /// Get or set a double precision property in a row
    c4_DoubleRef operator()(const c4_RowRef &)const;
    /// Get a double precision property in a row
    double Get(const c4_RowRef &)const;
    /// Set a double precision property in a row
    void Set(const c4_RowRef &, double)const;

    /// Create a row with one double precision value, shorthand for AsRow
    c4_Row operator[](double)const;
    /// Create a row with one double precision value
    c4_Row AsRow(double)const;
};
#endif // !q4_TINY

/// String properties.
class c4_StringProp: public c4_Property {
  public:
    /// Construct a new property
    c4_StringProp(const char*);
    /// Destructor
    ~c4_StringProp();

    /// Get or set a string property in a row
    c4_StringRef operator()(const c4_RowRef &)const;
    /// Get a string property in a row
    const char *Get(const c4_RowRef &)const;
    /// Set a string property in a row
    void Set(const c4_RowRef &, const char*)const;

    /// Create a row with one string, shorthand for AsRow
    c4_Row operator[](const char*)const;
    /// Create a row with one string
    c4_Row AsRow(const char*)const;
};

/// Binary properties.
class c4_BytesProp: public c4_Property {
  public:
    /// Construct a new property
    c4_BytesProp(const char*);
    /// Destructor
    ~c4_BytesProp();

    /// Get or set a bytes property in a row
    c4_BytesRef operator()(const c4_RowRef &)const;
    /// Get a bytes property in a row
    c4_Bytes Get(const c4_RowRef &)const;
    /// Set a bytes property in a row
    void Set(const c4_RowRef &, const c4_Bytes &)const;

    /// Create a row with one bytes object, shorthand for AsRow
    c4_Row operator[](const c4_Bytes &)const;
    /// Create a row with one bytes object
    c4_Row AsRow(const c4_Bytes &)const;
};

/// View properties.
class c4_ViewProp: public c4_Property {
  public:
    /// Construct a new property
    c4_ViewProp(const char*);
    /// Destructor
    ~c4_ViewProp();

    /// Get or set a view property in a row
    c4_ViewRef operator()(const c4_RowRef &)const;
    /// Get a view property in a row
    c4_View Get(const c4_RowRef &)const;
    /// Set a view property in a row
    void Set(const c4_RowRef &, const c4_View &)const;

    /// Create a row with one view, shorthand for AsRow
    c4_Row operator[](const c4_View &)const;
    /// Create a row with one view
    c4_Row AsRow(const c4_View &)const;
};

//---------------------------------------------------------------------------

class c4_CustomViewer {
  protected:
    /// Constructor, must be overriden in derived class
    c4_CustomViewer();
  public:
    /// Destructor
    virtual ~c4_CustomViewer();

    /// Return the structure of this view (initialization, called once)
    virtual c4_View GetTemplate() = 0;
    /// Return the number of rows in this view
    virtual int GetSize() = 0;
    int Lookup(const c4_RowRef &, int &);
    virtual int Lookup(c4_Cursor, int &);
    /// Fetch one data item, return it as a generic data value
    virtual bool GetItem(int, int, c4_Bytes &) = 0;
    virtual bool SetItem(int, int, const c4_Bytes &);
    bool InsertRows(int, const c4_RowRef &, int = 1);
    virtual bool InsertRows(int, c4_Cursor, int = 1);
    virtual bool RemoveRows(int, int = 1);
};

//---------------------------------------------------------------------------
/// A stream is a virtual helper class to serialize in binary form.

class c4_Stream {
  public:
    virtual ~c4_Stream();

    /// Fetch some bytes sequentially
    virtual int Read(void *, int) = 0;
    /// Store some bytes sequentially
    virtual bool Write(const void *, int) = 0;
};

//---------------------------------------------------------------------------
/// A strategy encapsulates code dealing with the I/O system interface.

class c4_Strategy {
  public:
    c4_Strategy();
    virtual ~c4_Strategy();

    virtual bool IsValid()const;
    virtual int DataRead(t4_i32, void *, int);
    virtual void DataWrite(t4_i32, const void *, int);
    virtual void DataCommit(t4_i32);
    virtual void ResetFileMapping();
    virtual t4_i32 FileSize();
    virtual t4_i32 FreshGeneration();

    void SetBase(t4_i32);
    t4_i32 EndOfData(t4_i32 =  - 1);

    /// True if the storage format is not native (default is false)
    bool _bytesFlipped;
    /// Error code of last failed I/O operation, zero if I/O was ok
    int _failure;
    /// First byte in file mapping, zero if not active
    const t4_byte *_mapStart;
    /// Number of bytes filled with active data
    t4_i32 _dataSize;
    /// All file positions are relative to this offset
    t4_i32 _baseOffset;
    /// The root position of the shallow tree walks
    t4_i32 _rootPos;
    /// The size of the root column
    t4_i32 _rootLen;
};

//---------------------------------------------------------------------------
/// A sequence is an abstract base class for views on ranges of records.
//
//  Sequences represent arrays of rows (or indexed collections / tables).
//  Insertion and removal of entries is allowed, but could take linear time.
//  A reference count is maintained to decide when the object should go away.

class c4_Sequence {
    /// Reference count
    int _refCount;
    /// Pointer to dependency list, or null if nothing depends on this
    c4_Dependencies *_dependencies;

  protected:
    /// Optimization: cached property index
    int _propertyLimit;
    /// Optimization: property map for faster access
    short *_propertyMap; // see c4_HandlerSeq::Reset()
    /// allocated on first use by c4_Sequence::Buffer()
    c4_Bytes *_tempBuf;

  public:
    /* General */
    /// Abstract constructor
    c4_Sequence();

    virtual int Compare(int, c4_Cursor)const;
    virtual bool RestrictSearch(c4_Cursor, int &, int &);
    void SetAt(int, c4_Cursor);
    virtual int RemapIndex(int, const c4_Sequence*)const;

    /* Reference counting */
    void IncRef();
    void DecRef();
    int NumRefs()const;

    /* Adding / removing rows */
    /// Return the current number of rows
    virtual int NumRows()const = 0;
    void Resize(int, int =  - 1);

    virtual void InsertAt(int, c4_Cursor, int = 1);
    virtual void RemoveAt(int, int = 1);
    virtual void Move(int, int);

    /* Properties */
    int NthPropId(int)const;
    int PropIndex(int);
    int PropIndex(const c4_Property &);

    /// Return the number of data handlers in this sequence
    virtual int NumHandlers()const = 0;
    /// Return a reference to the N-th handler in this sequence
    virtual c4_Handler &NthHandler(int)const = 0;
    /// Return the context of the N-th handler in this sequence
    virtual const c4_Sequence *HandlerContext(int)const = 0;
    /// Add the specified data handler to this sequence
    virtual int AddHandler(c4_Handler*) = 0;
    /// Create a handler of the appropriate type
    virtual c4_Handler *CreateHandler(const c4_Property &) = 0;

    virtual const char *Description();

    /* Element access */
    /// Return width of specified data item
    virtual int ItemSize(int, int);
    /// Retrieve one data item from this sequence
    virtual bool Get(int, int, c4_Bytes &);
    /// Store a data item into this sequence
    virtual void Set(int, const c4_Property &, const c4_Bytes &);

    /* Dependency notification */
    void Attach(c4_Sequence*);
    void Detach(c4_Sequence*);
    /// Return a pointer to the dependencies, or null
    c4_Dependencies *GetDependencies()const;

    virtual c4_Notifier *PreChange(c4_Notifier &);
    virtual void PostChange(c4_Notifier &);

    const char *UseTempBuffer(const char*);

  protected:
    virtual ~c4_Sequence();

    void ClearCache();

  public:
    //! for c4_Table::Sequence setup
    virtual void SetNumRows(int) = 0;
    virtual c4_Persist *Persist()const;

    c4_Bytes &Buffer();

  private:
    c4_Sequence(const c4_Sequence &); // not implemented
    void operator = (const c4_Sequence &); // not implemented
};

//---------------------------------------------------------------------------
/// A reference is used to get or set typed data, using derived classes.
//
//  Objects of this class are only intended to be used as a temporary handle
//  while getting and setting properties in a row.  They are normally only
//  constructed as result of function overload operators: "property (row)".

class c4_Reference {
  protected:
    /// The cursor which points to the data
    c4_Cursor _cursor;
    /// The property associated to this reference
    const c4_Property &_property;

  public:
    /// Constructor
    c4_Reference(const c4_RowRef &, const c4_Property &);

    /// Assignment of one data item
    c4_Reference &operator = (const c4_Reference &);

    /// Return width of the referenced data item
    int GetSize()const;
    /// Retrieve the value of the referenced data item
    bool GetData(c4_Bytes &)const;
    /// Store a value into the referenced data item
    void SetData(const c4_Bytes &)const;

    /// Return true if the contents of both references is equal
    friend bool operator == (const c4_Reference &, const c4_Reference &);
    /// Return true if the contents of both references is not equal
    friend bool operator != (const c4_Reference &, const c4_Reference &);

  private:
    void operator &()const; // not implemented
};

//---------------------------------------------------------------------------

/// Used to get or set integer values.
class c4_IntRef: public c4_Reference {
  public:
    /// Constructor
    c4_IntRef(const c4_Reference &);
    /// Get the value as integer
    operator t4_i32()const;
    /// Set the value to the specified integer
    c4_IntRef &operator = (t4_i32);
};

#if !q4_TINY

/// Used to get or set long int values.
class c4_LongRef: public c4_Reference {
  public:
    /// Constructor
    c4_LongRef(const c4_Reference &);
    /// Get the value as long int
    operator t4_i64()const;
    /// Set the value to the specified long int
    c4_LongRef &operator = (t4_i64);
};

/// Used to get or set floating point values.
class c4_FloatRef: public c4_Reference {
  public:
    /// Constructor
    c4_FloatRef(const c4_Reference &);
    /// Get the value as floating point
    operator double()const;
    /// Set the value to the specified floating point
    c4_FloatRef &operator = (double);
};

/// Used to get or set double precision values.
class c4_DoubleRef: public c4_Reference {
  public:
    /// Constructor
    c4_DoubleRef(const c4_Reference &);
    /// Get the value as floating point
    operator double()const;
    /// Set the value to the specified floating point
    c4_DoubleRef &operator = (double);
};

#endif // !q4_TINY

/// Used to get or set binary object values.
class c4_BytesRef: public c4_Reference {
  public:
    /// Constructor
    c4_BytesRef(const c4_Reference &);
    /// Get the value as binary object
    operator c4_Bytes()const;
    /// Set the value to the specified binary object
    c4_BytesRef &operator = (const c4_Bytes &);

    /// Fetch data from the memo field, up to end if length is zero
    c4_Bytes Access(t4_i32, int = 0, bool = false)const;
    /// Store data, resize by diff_ bytes, return true if successful
    bool Modify(const c4_Bytes &, t4_i32, int = 0)const;
};

/// Used to get or set string values.
class c4_StringRef: public c4_Reference {
  public:
    /// Constructor
    c4_StringRef(const c4_Reference &);
    /// Get the value as string
    operator const char *()const;
    /// Set the value to the specified string
    c4_StringRef &operator = (const char*);
};

/// Used to get or set view values.
class c4_ViewRef: public c4_Reference {
  public:
    /// Constructor
    c4_ViewRef(const c4_Reference &);
    /// Get the value as view
    operator c4_View()const;
    /// Set the value to the specified view
    c4_ViewRef &operator = (const c4_View &);
};

//---------------------------------------------------------------------------
// Debug logging option, can generate log of changes for one/all properties

#if q4_LOGPROPMODS
FILE *f4_LogPropMods(FILE *fp_, int propId_);
#else 
#define f4_LogPropMods(a,b) 0
#endif 

//---------------------------------------------------------------------------

#if q4_INLINE
#include "mk4.inl"
#endif 

//---------------------------------------------------------------------------

#endif // __MK4_H__
