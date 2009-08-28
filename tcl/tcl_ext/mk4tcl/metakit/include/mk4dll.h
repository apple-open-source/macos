// mk4dll.h --
// $Id: mk4dll.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html
//
//  Import declarations for DLLs

#ifndef __MK4_H__
#error This file is included by "mk4.h", it cannot be used standalone
#endif 

#ifndef d4_DLL
#ifdef _WIN32
#ifdef _USRDLL
#define d4_DLL      __declspec(dllexport)
#else 
#define d4_DLL      __declspec(dllimport)
#endif 
#else 
#define d4_DLL          
#endif 
#endif 

#ifndef d4_DLLSPEC
#ifdef _MSC_VER
#define d4_DLLSPEC(t)   d4_DLL t
#else 
#define d4_DLLSPEC(t)   t d4_DLL
#endif 
#endif 

/////////////////////////////////////////////////////////////////////////////
// Declarations in this file

class d4_DLL c4_Bytes;
class d4_DLL c4_BytesProp;
class d4_DLL c4_BytesRef;
class d4_DLL c4_Cursor;
class d4_DLL c4_CustomViewer;
class d4_DLL c4_DoubleProp;
class d4_DLL c4_DoubleRef;
class d4_DLL c4_FileStrategy;
class d4_DLL c4_FileStream;
class d4_DLL c4_FloatProp;
class d4_DLL c4_FloatRef;
class d4_DLL c4_IntProp;
class d4_DLL c4_IntRef;
class d4_DLL c4_LongRef;
class d4_DLL c4_Property;
class d4_DLL c4_Reference;
class d4_DLL c4_Row;
class d4_DLL c4_RowRef;
class d4_DLL c4_Sequence;
class d4_DLL c4_Storage;
class d4_DLL c4_Strategy;
class d4_DLL c4_Stream;
class d4_DLL c4_StringProp;
class d4_DLL c4_StringRef;
class d4_DLL c4_View;
class d4_DLL c4_ViewProp;
class d4_DLL c4_ViewRef;

#if !q4_MFC
class d4_DLL c4_String;
#endif 

/////////////////////////////////////////////////////////////////////////////

d4_DLLSPEC(bool)operator == (const c4_View &a_, const c4_View &b_);
d4_DLLSPEC(bool)operator != (const c4_View &a_, const c4_View &b_);
d4_DLLSPEC(bool)operator < (const c4_View &a_, const c4_View &b_);
d4_DLLSPEC(bool)operator > (const c4_View &a_, const c4_View &b_);
d4_DLLSPEC(bool)operator <= (const c4_View &a_, const c4_View &b_);
d4_DLLSPEC(bool)operator >= (const c4_View &a_, const c4_View &b_);

d4_DLLSPEC(bool)operator == (c4_Cursor a_, c4_Cursor b_);
d4_DLLSPEC(bool)operator != (c4_Cursor a_, c4_Cursor b_);
d4_DLLSPEC(bool)operator < (c4_Cursor a_, c4_Cursor b_);
d4_DLLSPEC(bool)operator > (c4_Cursor a_, c4_Cursor b_);
d4_DLLSPEC(bool)operator <= (c4_Cursor a_, c4_Cursor b_);
d4_DLLSPEC(bool)operator >= (c4_Cursor a_, c4_Cursor b_);
d4_DLLSPEC(c4_Cursor)operator + (c4_Cursor cursor_, int offset_);
d4_DLLSPEC(c4_Cursor)operator + (int offset_, c4_Cursor cursor_);

d4_DLLSPEC(bool)operator == (const c4_RowRef &a_, const c4_RowRef &b_);
d4_DLLSPEC(bool)operator != (const c4_RowRef &a_, const c4_RowRef &b_);
d4_DLLSPEC(bool)operator < (const c4_RowRef &a_, const c4_RowRef &b_);
d4_DLLSPEC(bool)operator > (const c4_RowRef &a_, const c4_RowRef &b_);
d4_DLLSPEC(bool)operator <= (const c4_RowRef &a_, const c4_RowRef &b_);
d4_DLLSPEC(bool)operator >= (const c4_RowRef &a_, const c4_RowRef &b_);
d4_DLLSPEC(c4_Row)operator + (const c4_RowRef &a_, const c4_RowRef &b_);

d4_DLLSPEC(bool)operator == (const c4_Bytes &a_, const c4_Bytes &b_);
d4_DLLSPEC(bool)operator != (const c4_Bytes &a_, const c4_Bytes &b_);

d4_DLLSPEC(bool)operator == (const c4_Reference &, const c4_Reference &);
d4_DLLSPEC(bool)operator != (const c4_Reference &, const c4_Reference &);

#if !q4_MFC
d4_DLLSPEC(c4_String)operator + (const c4_String &, const c4_String &);
d4_DLLSPEC(c4_String)operator + (const c4_String &, const char*);
d4_DLLSPEC(c4_String)operator + (const char *, const c4_String &);

d4_DLLSPEC(bool)operator == (const c4_String &, const c4_String &);
d4_DLLSPEC(bool)operator != (const c4_String &, const c4_String &);
d4_DLLSPEC(bool)operator == (const c4_String &s1, const char *s2);
d4_DLLSPEC(bool)operator == (const char *s1, const c4_String &s2);
d4_DLLSPEC(bool)operator != (const c4_String &s1, const char *s2);
d4_DLLSPEC(bool)operator != (const char *s1, const c4_String &s2);
#endif 

/////////////////////////////////////////////////////////////////////////////
