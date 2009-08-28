// regress.h -- Regression test program, header file
// $Id: regress.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "mk4.h"
#include "mk4io.h"
#include "mk4str.h"

#define TraceAll  false

// default for dos and unix is to assume they don't support exceptions
#if defined (_DOS) || defined (unix) || defined (__unix__) || \
defined(__GNUC__) || defined(_WIN32_WCE)
#if !defined (q4_NOTHROW)
#define q4_NOTHROW 1
#endif 
#endif 

#ifdef _WIN32_WCE
int remove(const char*);
#endif 

#if _MSC_VER == 800
#pragma warning (disable: 4703) // function too large for global optimizations

// also no exceptions in MSVC 1.52 when used with a QuickWin target
#if defined (_QWINVER) && !defined (q4_NOTHROW)
#define q4_NOTHROW 1    
#endif 
#endif 

#if q4_NOTHROW
#define try
#define catch(x)  if (0)

extern const char *msg;
#endif 

#if defined (macintosh)  
#define TESTDIR ":tests:"
#define TEXTOUT "wt"
#define LINESEP "\r"
#elif defined (__VMS)
#define TESTDIR "[.tests]"
#define TEXTOUT "w"
#define LINESEP "\r\n" // is this correct?
#elif defined (unix) || defined (__unix__) || defined (__GNUC__) || \
defined(__hpux)
#define TESTDIR "tests/"
#define TEXTOUT "w"
#define LINESEP "\n"
#else 
#define TESTDIR "tests\\"
#define TEXTOUT "wt"
#define LINESEP "\r\n"
#endif 

#include <stdio.h>

#if q4_MFC && defined(_DEBUG)
#define B(n_,d_,c_) \
if (StartTest(c_, #n_, #d_)) \
{ \
CMemoryState oldState, newState, diffState; \
oldState.Checkpoint(); \
afxTraceEnabled = TraceAll; \
try \
{ \
{
#define E \
} \
puts("<<< done."); \
} \
catch (const char* msg) { CatchMsg(msg); } \
catch (...) { CatchOther(); } \
afxTraceEnabled = true; \
fflush(stdout); \
newState.Checkpoint(); \
if (diffState.Difference(oldState, newState)) \
{ \
fputs("\tMemory leaked!\n", stderr); \
puts("*** Memory leaked ***"); \
TRACE("   *** Memory leaked, "); \
diffState.DumpAllObjectsSince(); \
} \
fflush(stdout); \
}
#else 
#define B(n_,d_,c_) \
if (StartTest(c_, #n_, #d_)) \
{ \
try \
{ \
{
#define E \
} \
puts("<<< done."); \
} \
catch (const char* msg) { CatchMsg(msg); } \
catch (...) { CatchOther(); } \
fflush(stdout); \
}
#endif 

#define A(e_) if (e_) ; else FailExpr(#e_)

#define W(f_) remove(#f_)
#define R(f_) A(remove(#f_) == 0)
#define D(f_) DumpFile(#f_, TESTDIR #f_ ".txt")

typedef c4_BytesProp c4_MemoProp;

extern void DumpFile(const char *in_, const char *out_);
extern void Fail(const char *msg);
extern void FailExpr(const char *expr);
extern int StartTest(int, const char *, const char*);
extern void CatchMsg(const char *msg);
extern void CatchOther();

extern void TestBasics1();
extern void TestBasics2();
extern void TestCustom1();
extern void TestCustom2();
extern void TestDiffer();
extern void TestExtend();
extern void TestFormat();
extern void TestLimits();
extern void TestMapped();
extern void TestNotify();
extern void TestResize();
extern void TestStores1();
extern void TestStores2();
extern void TestStores3();
extern void TestStores4();
extern void TestStores5();

//  The Borland C++ RTL does not want file handle objects to cross
//  DLL boundaries, so we use special fopen/fclose hooks in the DLL.

#if defined (__BORLANDC__) // this assumes Metakit is in a DLL!
extern FILE *f4_FileOpenInDLL(const char *, const char*);
extern int f4_FileCloseInDLL(FILE*);

#define fopen f4_FileOpenInDLL
#define fclose f4_FileCloseInDLL
#endif
