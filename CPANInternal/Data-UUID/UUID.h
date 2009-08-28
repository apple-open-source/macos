#if !defined __UUID_H__
#    define  __UUID_H__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
// No unistd.h in MS VC
#include <unistd.h>
#endif
#include <time.h>
#include "md5.h"

#if !defined INT2PTR
#if (IVSIZE == PTRSIZE) && (UVSIZE == PTRSIZE)
#  define PTRV			UV
#  define INT2PTR(any,d)	(any)(d)
#else
#  if PTRSIZE == LONGSIZE 
#    define PTRV		unsigned long
#  else
#    define PTRV		unsigned
#  endif
#  define INT2PTR(any,d)	(any)(PTRV)(d)
#endif
#define NUM2PTR(any,d)	(any)(PTRV)(d)
#define PTR2IV(p)	INT2PTR(IV,p)
#define PTR2UV(p)	INT2PTR(UV,p)
#define PTR2NV(p)	NUM2PTR(NV,p)
#if PTRSIZE == LONGSIZE 
#  define PTR2ul(p)	(unsigned long)(p)
#else
#  define PTR2ul(p)	INT2PTR(unsigned long,p)	
#endif
#endif

#if defined __cygwin__ || defined __mingw32__ || defined _MSC_VER
#include <windows.h>
#endif
#if defined __darwin__
#include <sys/file.h>
#endif

#ifdef _MSC_VER
#include <process.h>
#endif

#if !defined _STDIR
#    define  _STDIR			"/var/tmp"
#endif
#if !defined _DEFAULT_UMASK
#    define  _DEFAULT_UMASK		0007
#endif

#define UUID_STATE			".UUID_STATE"
#define UUID_NODEID			".UUID_NODEID"
#if defined __mingw32__ || defined _WIN32 || defined _MSC_VER
#define UUID_STATE_NV_STORE		_STDIR"\\"UUID_STATE
#define UUID_NODEID_NV_STORE		_STDIR"\\"UUID_NODEID
#else
#define UUID_STATE_NV_STORE		_STDIR"/"UUID_STATE
#define UUID_NODEID_NV_STORE		_STDIR"/"UUID_NODEID
#endif

#define UUIDS_PER_TICK 1024
#ifdef _MSC_VER
#define I64(C) C##i64
#else
#define I64(C) C##LL
#endif

#define F_BIN 0
#define F_STR 1
#define F_HEX 2
#define F_B64 3

#define CHECK(f1, f2) if (f1 != f2) RETVAL = f1 < f2 ? -1 : 1;

typedef unsigned int       unsigned32;
typedef unsigned short     unsigned16;
typedef unsigned char      unsigned8;
typedef unsigned char      byte;
#ifndef _MSC_VER
typedef unsigned long long unsigned64_t;
# else
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;

typedef unsigned __int64 unsigned64_t;
// http://msdn2.microsoft.com/en-us/library/296az74e.aspx - Integer Limits

typedef int pid_t;
#endif /* _MSC_VER */
typedef unsigned64_t       perl_uuid_time_t;

#if   defined __solaris__ || defined __linux__
#     define LOCK(f)		lockf(fileno(f),F_LOCK,0);
#     define UNLOCK(f)		lockf(fileno(f),F_ULOCK,0);
#elif defined __darwin__
#     define LOCK(f)		flock(fileno(f),LOCK_EX);
#     define UNLOCK(f)		flock(fileno(f),LOCK_UN);
#else
#     define LOCK(f)
#     define UNLOCK(f)
#endif

#undef perl_uuid_t

typedef struct _uuid_node_t {
   char nodeID[6];
} uuid_node_t;

typedef struct _perl_uuid_t {
   unsigned32          time_low;
   unsigned16          time_mid;
   unsigned16          time_hi_and_version;
   unsigned8           clock_seq_hi_and_reserved;
   unsigned8           clock_seq_low;
   byte                node[6];
} perl_uuid_t;

typedef struct _uuid_state_t { 
   perl_uuid_time_t ts;
   uuid_node_t node;
   unsigned16  cs;  
} uuid_state_t;

typedef struct _uuid_context_t {
   uuid_state_t state;
   uuid_node_t  nodeid;
   perl_uuid_time_t  next_save;
} uuid_context_t;

static void format_uuid_v1(
   perl_uuid_t      *uuid, 
   unsigned16   clockseq,
   perl_uuid_time_t  timestamp, 
   uuid_node_t  node
);
static void format_uuid_v3(
   perl_uuid_t      *uuid, 
   unsigned     char hash[16]
);
static void       get_current_time(perl_uuid_time_t * timestamp);
static unsigned16 true_random(void);
static void       get_system_time(perl_uuid_time_t *perl_uuid_time);
static void       get_random_info(unsigned char seed[16]);

static char   *base64 = 
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char index64[256] = {
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,62, 255,255,255,63,
   52,53,54,55, 56,57,58,59, 60,61,255,255, 255,254,255,255,
   255, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
   15,16,17,18, 19,20,21,22, 23,24,25,255, 255,255,255,255,
   255,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
   41,42,43,44, 45,46,47,48, 49,50,51,255, 255,255,255,255,

   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
   255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
};
#endif
