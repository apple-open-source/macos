/******************************************************************
*
*    PDO Windows NT compatibility header file 
*    
*    You should include this in every file that gets compiled 
*    by gcc.  
*
******************************************************************/

#ifndef _WINNT_PDO_H_
#define _WINNT_PDO_H_

// These things are only of interest to gcc, so this will make
// it safe to include this file regardless of if youre using 
// a different compiler or not.
#ifdef __GNUC__

// The version of the MS compiler.  The headers do a lot
// of conditional stuff with this flag that gcc can handle
// so we define it here.
#define _MSC_VER 0x0900

#endif /* __GNUC__ */

// This is the default calling convention supported by gcc so 
// this doesnt have any special meaning.  The only other
// calling convention supported by gcc so far is __stdcall
#define __cdecl

// NT doesnt support the _export keyword
#define _export

// These have no meaning under NT 
#define far
#define near
#define _huge

// This is used in WINDOWS.H to include a whole lot of stuff
// that gcc choked on.  If you need anything in the following
// list, then either we need to clean the headers up or just
// include it manually and fix up the specific header if there
// is a conflick.  The following headers are NOT include if 
// WIN32_LEAN_AND_MEAN is defined:
//	 cderr.h
//	 dde.h
//	 ddeml.h
//	 dlgs.h
//	 lzexpand.h
//	 mmsystem.h
//	 nb30.h
//	 rpc.h
//	 shellapi.h
//	 winperf.h
//	 winsock.h
//	 commdlg.h
//	 drivinit.h
//	 winspool.h
//	 ole2.h
#define WIN32_LEAN_AND_MEAN

// Again, some headers use this to indicate that youre on 
// an NT platform...I needed this to include process.h in specific
#define _NTSDK

// NT header files use this to indicate that unions cant be nameless.
// Unfortuately, only SOME of the header files use this!
#define NONAMELESSUNION

// Make sure were not using bcopy since it doesnt handle overlapping
// memory regions properly.  memmove does.
#ifndef HOOD
#undef bcopy
#define bcopy(src,dst,cnt) memmove(dst,src,cnt)
#endif /* HOOD */

// To support bzero
#undef bzero
#define bzero(dst,cnt) memset(dst,0,cnt)

// Microsoft defines all of their C type filenames with "_".  I didnt 
// immediately find a header that had these mappings so I just built
// them manually.  If these are in a header somewhere or are no longer
// needed, then they should be removed. - pmarcos
//#define O_TRUNC _O_TRUNC		// include <fcntl.h> instead
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#define stat _stat
#define environ _environ
#define strdup _strdup
#define strncasecmp _strnicmp
#define putenv _putenv
#define access _access
#define chmod _chmod
#define getcwd _getcwd
#define fdopen _fdopen
#define fstat _fstat
#define fileno _fileno
#define umask _umask
#define getpid _getpid
#define lseek _lseek
#define mktemp _mktemp
#define SIGHUP SIGBREAK
#define chdir _chdir
#define dup _dup
#define close(h)	   _close(h)
#define open(f,o,m)	   _open(f,o,m)
#define read(h,b,c)	   _read(h,b,c)
#define unlink(f)	   _unlink(f)
#define write(h,b,c)	   _write(h,b,c)

// NT doesnt support these functions, so just map them to 0
#define getuid() 0
#define getgid() 0

// sbrk is not supported under NT
#define sbrk(x) ""

// NT doesnt have a MAXPATHLEN variable.  I needed this when compiling 
// bfd for gas.  
#ifndef MAXPATHLEN
#define MAXPATHLEN 255
#endif /* MAXPATHLEN */

// I needed these for compiling the gnu binutils when getting gas to work
#define R_OK 04
#define W_OK 02
#define X_OK 00

// Needed for calls to access()
#ifndef F_OK
#define F_OK 00
#endif /* F_OK */

// NT doesnt support symbolic links so we just let stat stand in for 
// lstat.  If the file exists then thats fine, stat will do the right 
// thing.  If it doesnt exist, then it will return -1 which is what 
// lstat would have returned in the event of an error.  Additionally, 
// we set S_IFLNK to be 0xFFFF so that if somebody does something like
// 		if ((statb.st_mode & S_IFLNK) == S_IFLNK)
// that it will fail meaning that the file isnt a sym link.  
#define lstat _stat
#define S_IFLNK 0xFFFF

// Windows defines the macros max and min so be sure we pick them up
#ifndef MAX
#define MAX max
#endif /* MAX */
#ifndef MIN
#define MIN min
#endif /* MIN */

// NT doesnt have fsync and ftruncate so well define them to return an error
#define fsync(a) -1
#define ftruncate(a,b) -1

// NT doesnt have readlink so we define it to return an error
#define readlink(a,b,c) -1

// These would normally be in limits.h but NT doesnt provide them.
#define ULONG_LONG_MAX 18446744073709551615ULL
#define LONG_LONG_MAX 9223372036854775807LL
#define LONG_LONG_MIN (-LONG_LONG_MAX-1)

#define MINFLOAT ((float)1.17549435e-038)
#define MAXFLOAT ((float)3.40282347e+038)
#define MINDOUBLE 2.2250738585072014e-308
#define MAXDOUBLE 1.7976931348623157e+308

#endif /* _WINNT_PDO_H_ */
