#ifdef NEXT_SEMANTICS
#include "c-gperf-next.h"
#elif defined (_WIN32) && defined (NEXT_PDO)
#include "c-gperf-winntpdo.h"
#else
/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$ c-parse.gperf  */ 
struct resword { char *name; short token; enum rid rid; };

#define TOTAL_KEYWORDS 80
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 10
#define MAX_HASH_VALUE 162
/* maximum key range = 153, duplicates = 0 */

#ifdef __GNUC__
__inline
#endif
static unsigned int
hash (str, len)
     register char *str;
     register int unsigned len;
{
  static unsigned char asso_values[] =
    {
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163,   8, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
     163, 163, 163, 163, 163,   1, 163,  24,   8,  61,
      37,   6,  47,  49,   2,   5, 163,   3,  51,  30,
      58,  91,  35, 163,  33,  13,   1,  18,  49,   2,
       2,   5,   3, 163, 163, 163, 163, 163,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 3:
        hval += asso_values[str[2]];
      case 2:
      case 1:
        hval += asso_values[str[0]];
    }
  return hval + asso_values[str[len - 1]];
}

static struct resword wordlist[] =
{
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"int",  TYPESPEC, RID_INT},
      {"",}, {"",}, 
      {"__typeof__",  TYPEOF, NORID},
      {"",}, 
      {"__imag__",  IMAGPART, NORID},
      {"",}, 
      {"__inline__",  SCSPEC, RID_INLINE},
      {"while",  WHILE, NORID},
      {"__iterator__",  SCSPEC, RID_ITERATOR},
      {"__inline",  SCSPEC, RID_INLINE},
      {"__extension__",  EXTENSION, NORID},
      {"break",  BREAK, NORID},
      {"",}, {"",}, 
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"switch",  SWITCH, NORID},
      {"",}, {"",}, 
      {"else",  ELSE, NORID},
      {"",}, {"",}, 
      {"@defs",  DEFS, NORID},
      {"__asm__",  ASM_KEYWORD, NORID},
      {"",}, {"",}, {"",}, 
      {"__alignof__",  ALIGNOF, NORID},
      {"",}, 
      {"__attribute__",  ATTRIBUTE, NORID},
      {"",}, {"",}, 
      {"__attribute",  ATTRIBUTE, NORID},
      {"__real__",  REALPART, NORID},
      {"id",  OBJECTNAME, RID_ID},
      {"",}, {"",}, {"",}, {"",}, 
      {"__iterator",  SCSPEC, RID_ITERATOR},
      {"",}, {"",}, {"",}, 
      {"struct",  STRUCT, NORID},
      {"if",  IF, NORID},
      {"@private",  PRIVATE, NORID},
      {"@selector",  SELECTOR, NORID},
      {"__typeof",  TYPEOF, NORID},
      {"enum",  ENUM, NORID},
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"__asm",  ASM_KEYWORD, NORID},
      {"__imag",  IMAGPART, NORID},
      {"__label__",  LABEL, NORID},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"",}, 
      {"in",  TYPE_QUAL, RID_IN},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"double",  TYPESPEC, RID_DOUBLE},
      {"inline",  SCSPEC, RID_INLINE},
      {"sizeof",  SIZEOF, NORID},
      {"__const",  TYPE_QUAL, RID_CONST},
      {"extern",  SCSPEC, RID_EXTERN},
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED},
      {"",}, 
      {"@class",  CLASS, NORID},
      {"@encode",  ENCODE, NORID},
      {"bycopy",  TYPE_QUAL, RID_BYCOPY},
      {"__alignof",  ALIGNOF, NORID},
      {"@interface",  INTERFACE, NORID},
      {"",}, 
      {"case",  CASE, NORID},
      {"",}, 
      {"union",  UNION, NORID},
      {"asm",  ASM_KEYWORD, NORID},
      {"@protected",  PROTECTED, NORID},
      {"typeof",  TYPEOF, NORID},
      {"typedef",  SCSPEC, RID_TYPEDEF},
      {"__real",  REALPART, NORID},
      {"default",  DEFAULT, NORID},
      {"byref",  TYPE_QUAL, RID_BYREF},
      {"@public",  PUBLIC, NORID},
      {"void",  TYPESPEC, RID_VOID},
      {"out",  TYPE_QUAL, RID_OUT},
      {"",}, 
      {"return",  RETURN, NORID},
      {"",}, {"",}, 
      {"@protocol",  PROTOCOL, NORID},
      {"inout",  TYPE_QUAL, RID_INOUT},
      {"",}, 
      {"static",  SCSPEC, RID_STATIC},
      {"signed",  TYPESPEC, RID_SIGNED},
      {"",}, 
      {"@end",  END, NORID},
      {"oneway",  TYPE_QUAL, RID_ONEWAY},
      {"",}, 
      {"short",  TYPESPEC, RID_SHORT},
      {"@implementation",  IMPLEMENTATION, NORID},
      {"",}, {"",}, 
      {"volatile",  TYPE_QUAL, RID_VOLATILE},
      {"",}, 
      {"for",  FOR, NORID},
      {"",}, {"",}, {"",}, 
      {"auto",  SCSPEC, RID_AUTO},
      {"",}, 
      {"char",  TYPESPEC, RID_CHAR},
      {"register",  SCSPEC, RID_REGISTER},
      {"",}, 
      {"const",  TYPE_QUAL, RID_CONST},
      {"",}, {"",}, {"",}, {"",}, 
      {"do",  DO, NORID},
      {"",}, 
      {"@compatibility_alias",  ALIAS, NORID},
      {"continue",  CONTINUE, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"float",  TYPESPEC, RID_FLOAT},
      {"goto",  GOTO, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"long",  TYPESPEC, RID_LONG},
};

#ifdef __GNUC__
__inline
#endif
struct resword *
is_reserved_word (str, len)
     register char *str;
     register unsigned int len;
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register char *s = wordlist[key].name;

          if (*s == *str && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
#endif
