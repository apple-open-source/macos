// fileio.cpp --
// $Id: fileio.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Implementation of c4_FileStrategy and c4_FileStream
 */

#include "header.h"
#include "mk4io.h"

#if q4_WIN32
#if q4_MSVC && !q4_STRICT
#pragma warning(disable: 4201) // nonstandard extension used : ...
#endif 
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif 

#if q4_UNIX && HAVE_MMAP
#include <sys/types.h>
#include <sys/mman.h>
#endif 

#if q4_UNIX
#include <unistd.h>
#include <fcntl.h>
#endif 

#if q4_WINCE
#define _get_osfhandle(x) x
#endif 

#ifndef _O_NOINHERIT
#define _O_NOINHERIT 0
#endif 

/////////////////////////////////////////////////////////////////////////////
//
//  The "Carbon" version of a build on Macintosh supports running under
//  either MacOS 7..9 (which has no mmap), or MacOS X (which has mmap).
//  The logic below was adapted from a contribution by Paul Snively, it
//  decides at run time which case it is, and switches I/O calls to match.

#if defined (q4_CARBON) && q4_CARBON
//#if q4_MAC && !defined (__MACH__) && (!q4_MWCW || __MWERKS__ >= 0x3000)
#undef HAVE_MMAP
#define HAVE_MMAP 1

#include <CFBundle.h>
#include <Folders.h>

#define PROT_NONE        0x00
#define PROT_READ        0x01
#define PROT_WRITE       0x02
#define PROT_EXEC        0x04

#define MAP_SHARED       0x0001
#define MAP_PRIVATE      0x0002

#define MAP_FIXED        0x0010
#define MAP_RENAME       0x0020
#define MAP_NORESERVE    0x0040
#define MAP_INHERIT      0x0080
#define MAP_NOEXTEND     0x0100
#define MAP_HASSEMAPHORE 0x0200

typedef unsigned long t4_u32;

static t4_u32 sfwRefCount = 0;
static CFBundleRef systemFramework = NULL;

static char *fake_mmap(char *, t4_u32, int, int, int, long long) {
  return (char*) - 1L;
}

static int fake_munmap(char *, t4_u32) {
  return 0;
}

static FILE *(*my_fopen)(const char *, const char*) = fopen;
static int(*my_fclose)(FILE*) = fclose;
static long(*my_ftell)(FILE*) = ftell;
static int(*my_fseek)(FILE *, long, int) = fseek;
static t4_u32(*my_fread)(void *ptr, t4_u32, t4_u32, FILE*) = fread;
static t4_u32(*my_fwrite)(const void *ptr, t4_u32, t4_u32, FILE*) = fwrite;
static int(*my_ferror)(FILE*) = ferror;
static int(*my_fflush)(FILE*) = fflush;
static int(*my_fileno)(FILE*) = fileno;
static char *(*my_mmap)(char *, t4_u32, int, int, int, long long) = fake_mmap;
static int(*my_munmap)(char *, t4_u32) = fake_munmap;

static void InitializeIO() {
  if (sfwRefCount++)
    return ;
  // race condition, infinitesimal risk

  FSRef theRef;
  if (FSFindFolder(kOnAppropriateDisk, kFrameworksFolderType, false, &theRef) 
    == noErr) {
    CFURLRef fw = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &theRef);
    if (fw) {
      CFURLRef bd = CFURLCreateCopyAppendingPathComponent
        (kCFAllocatorSystemDefault, fw, CFSTR("System.framework"), false);
      CFRelease(fw);
      if (bd) {
        systemFramework = CFBundleCreate(kCFAllocatorSystemDefault, bd);
        CFRelease(bd);
      }
    }
    if (!systemFramework || !CFBundleLoadExecutable(systemFramework))
      return ;
#define F(x) CFBundleGetFunctionPointerForName(systemFramework, CFSTR(#x))
    my_fopen = (FILE *(*)(const char *, const char*))F(fopen);
    my_fclose = (int(*)(FILE*))F(fclose);
    my_ftell = (long(*)(FILE*))F(ftell);
    my_fseek = (int(*)(FILE *, long, int))F(fseek);
    my_fread = (t4_u32(*)(void *ptr, t4_u32, t4_u32, FILE*))F(fread);
    my_fwrite = (t4_u32(*)(const void *ptr, t4_u32, t4_u32, FILE*))F(fwrite);
    my_ferror = (int(*)(FILE*))F(ferror);
    my_fflush = (int(*)(FILE*))F(fflush);
    my_fileno = (int(*)(FILE*))F(fileno);
    my_mmap = (char *(*)(char *, t4_u32, int, int, int, long long))F(mmap);
    my_munmap = (int(*)(char *, t4_u32))F(munmap);
#undef F
    d4_assert(my_fopen && my_fclose && my_ftell && my_fseek && my_fread &&
      my_fwrite && my_ferror && my_fflush && my_fileno && my_mmap && my_munmap);
  }
}

static void FinalizeIO() {
  if (--sfwRefCount)
    return ;
  // race condition, infinitesimal risk

  if (systemFramework) {
    CFBundleUnloadExecutable(systemFramework);
    CFRelease(systemFramework);
    systemFramework = 0;
  }
}

#define fopen my_fopen
#define fclose  my_fclose
#define ftell my_ftell
#define fseek my_fseek
#define fread my_fread
#define fwrite  my_fwrite
#define ferror  my_ferror
#define fflush  my_fflush
#define fileno  my_fileno
#define mmap  my_mmap
#define munmap  my_munmap

#else 

#define InitializeIO()
#define FinalizeIO()

#endif 

/////////////////////////////////////////////////////////////////////////////

#if q4_CHECK
#include <stdlib.h>

void f4_AssertionFailed(const char *cond_, const char *file_, int line_) {
  fprintf(stderr, "Assertion failed: %s (file %s, line %d)\n", cond_, file_,
    line_);
  abort();
}

#endif //q4_CHECK

/////////////////////////////////////////////////////////////////////////////
// c4_FileStream

c4_FileStream::c4_FileStream(FILE *stream_, bool owned_): _stream(stream_),
  _owned(owned_){}

c4_FileStream::~c4_FileStream() {
  if (_owned)
    fclose(_stream);
}

int c4_FileStream::Read(void *buffer_, int length_) {
  d4_assert(_stream != 0);

  return (int)fread(buffer_, 1, length_, _stream);
}

bool c4_FileStream::Write(const void *buffer_, int length_) {
  d4_assert(_stream != 0);

  return (int)fwrite(buffer_, 1, length_, _stream) == length_;
}

/////////////////////////////////////////////////////////////////////////////
// c4_FileStrategy

c4_FileStrategy::c4_FileStrategy(FILE *file_): _file(file_), _cleanup(0) {
  InitializeIO();
  ResetFileMapping();
}

c4_FileStrategy::~c4_FileStrategy() {
  _file = 0;
  ResetFileMapping();

  if (_cleanup)
    fclose(_cleanup);

  d4_assert(_mapStart == 0);
  FinalizeIO();
}

bool c4_FileStrategy::IsValid()const {
  return _file != 0;
}

t4_i32 c4_FileStrategy::FileSize() {
  d4_assert(_file != 0);

  long size =  - 1;

  long old = ftell(_file);
  if (old >= 0 && fseek(_file, 0, 2) == 0) {
    long pos = ftell(_file);
    if (fseek(_file, old, 0) == 0)
      size = pos;
  }

  if (size < 0)
    _failure = ferror(_file);

  return size;
}

t4_i32 c4_FileStrategy::FreshGeneration() {
  d4_assert(false);
  return 0;
}

void c4_FileStrategy::ResetFileMapping() {
#if q4_WIN32
  if (_mapStart != 0) {
    _mapStart -= _baseOffset;
    d4_dbgdef(BOOL g = )::UnmapViewOfFile((char*)_mapStart);
    d4_assert(g);
    _mapStart = 0;
    _dataSize = 0;
  }

  if (_file != 0) {
    t4_i32 len = FileSize();

    if (len > 0) {
      FlushFileBuffers((HANDLE)_get_osfhandle(_fileno(_file)));
      HANDLE h = ::CreateFileMapping((HANDLE)_get_osfhandle(_fileno(_file)), 0,
        PAGE_READONLY, 0, len, 0);

      if (h) {
        _mapStart = (t4_byte*)::MapViewOfFile(h, FILE_MAP_READ, 0, 0, len);

        if (_mapStart != 0) {
          _mapStart += _baseOffset;
          _dataSize = len - _baseOffset;
        }

        d4_dbgdef(BOOL f = )::CloseHandle(h);
        d4_assert(f);
      }
    }
  }
#elif HAVE_MMAP && !NO_MMAP
  if (_mapStart != 0) {
    _mapStart -= _baseOffset;
    munmap((char*)_mapStart, _baseOffset + _dataSize); // also loses const
    _mapStart = 0;
    _dataSize = 0;
  }

  if (_file != 0) {
    t4_i32 len = FileSize();

    if (len > 0) {
      _mapStart = (const t4_byte*)mmap(0, len, PROT_READ, MAP_SHARED, fileno
        (_file), 0);
      if (_mapStart != (void*) - 1L) {
        _mapStart += _baseOffset;
        _dataSize = len - _baseOffset;
      } else
        _mapStart = 0;
    }
  }
#endif 
}

#if q4_WIN32 && !q4_BORC && !q4_WINCE
static DWORD GetPlatformId() {
  static OSVERSIONINFO os;
  if (os.dwPlatformId == 0) {
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os);
  }
  return os.dwPlatformId;
}

#endif 

bool c4_FileStrategy::DataOpen(const char *fname_, int mode_) {
  d4_assert(!_file);

#if q4_WIN32 && !q4_BORC && !q4_WINCE
  int flags = _O_BINARY | _O_NOINHERIT | (mode_ > 0 ? _O_RDWR : _O_RDONLY);
  int fd =  - 1;

  if (GetPlatformId() != VER_PLATFORM_WIN32_NT)
    fd = _open(fname_, flags);
  if (fd ==  - 1) {
    WCHAR wName[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fname_,  - 1, wName, MAX_PATH);
    fd = _wopen(wName, flags);
  }
  if (fd !=  - 1)
    _cleanup = _file = _fdopen(fd, mode_ > 0 ? "r+b" : "rb");
#else 
  _cleanup = _file = fopen(fname_, mode_ > 0 ? "r+b" : "rb");
#if q4_UNIX
  if (_file != 0)
    fcntl(fileno(_file), F_SETFD, FD_CLOEXEC);
#endif //q4_UNIX
#endif //q4_WIN32 && !q4_BORC && !q4_WINCE

  if (_file != 0) {
    ResetFileMapping();
    return true;
  }

  if (mode_ > 0) {
#if q4_WIN32 && !q4_BORC && !q4_WINCE
    WCHAR wName[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fname_,  - 1, wName, MAX_PATH);
    fd = _wopen(wName, flags | _O_CREAT, _S_IREAD | _S_IWRITE);
    if (fd !=  - 1)
      _cleanup = _file = _fdopen(fd, "w+b");
#else 
    _cleanup = _file = fopen(fname_, "w+b");
#if q4_UNIX
    if (_file != 0)
      fcntl(fileno(_file), F_SETFD, FD_CLOEXEC);
#endif //q4_UNIX
#endif //q4_WIN32 && !q4_BORC && !q4_WINCE
  }

  //d4_assert(_file != 0);
  return false;
}

int c4_FileStrategy::DataRead(t4_i32 pos_, void *buf_, int len_) {
  d4_assert(_baseOffset + pos_ >= 0);
  d4_assert(_file != 0);

  //printf("DataRead at %d len %d\n", pos_, len_);
  return fseek(_file, _baseOffset + pos_, 0) != 0 ?  - 1: (int)fread(buf_, 1,
    len_, _file);
}

void c4_FileStrategy::DataWrite(t4_i32 pos_, const void *buf_, int len_) {
  d4_assert(_baseOffset + pos_ >= 0);
  d4_assert(_file != 0);
#if 0
  if (_mapStart <= buf_ && buf_ < _mapStart + _dataSize) {
    printf("DataWrite %08x at %d len %d (map %d)\n", buf_, pos_, len_, (const
      t4_byte*)buf_ - _mapStart + _baseOffset);
  } else {
    printf("DataWrite %08x at %d len %d\n", buf_, pos_, len_);
  }
  fprintf(stderr, 
    "  _mapStart %08x _dataSize %d buf_ %08x len_ %d _baseOffset %d\n",
    _mapStart, _dataSize, buf_, len_, _baseOffset);
  printf("  _mapStart %08x _dataSize %d buf_ %08x len_ %d _baseOffset %d\n",
    _mapStart, _dataSize, buf_, len_, _baseOffset);
  fflush(stdout);
#endif 

#if q4_WIN32 || __hpux || __MACH__ 
  // if (buf_ >= _mapStart && buf_ <= _mapLimit - len_)

  // a horrendous hack to allow file mapping for Win95 on network drive
  // must use a temp buf to avoid write from mapped file to same file
  // 
  //  6-Feb-1999  --  this workaround is not thread safe
  // 30-Nov-2001  --  changed to use the stack so now it is
  // 28-Oct-2002  --  added HP/UX to the mix, to avoid hard lockup
  char tempBuf[4096];
  d4_assert(len_ <= sizeof tempBuf);
  buf_ = memcpy(tempBuf, buf_, len_);
#endif 

  if (fseek(_file, _baseOffset + pos_, 0) != 0 || (int)fwrite(buf_, 1, len_,
    _file) != len_) {
    _failure = ferror(_file);
    d4_assert(_failure != 0);
    d4_assert(true); // always force an assertion failure in debug mode
  }
}

void c4_FileStrategy::DataCommit(t4_i32 limit_) {
  d4_assert(_file != 0);

  if (fflush(_file) < 0) {
    _failure = ferror(_file);
    d4_assert(_failure != 0);
    d4_assert(true); // always force an assertion failure in debug mode
    return ;
  }

  if (limit_ > 0) {
#if 0 // can't truncate file in a portable way!
    // unmap the file first, WinNT is more picky about this than Win95
    FILE *save = _file;

    _file = 0;
    ResetFileMapping();
    _file = save;

    _file->SetLength(limit_); // now we can resize the file
#endif 
    ResetFileMapping(); // remap, since file length may have changed
  }
}

/////////////////////////////////////////////////////////////////////////////
