/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,4,$ c-parse.gperf  */ 
struct resword { char *name; short token; enum rid rid; };

#define TOTAL_KEYWORDS 86
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 7
#define MAX_HASH_VALUE 172
/* maximum key range = 166, duplicates = 0 */

#ifdef __GNUC__
inline
#endif
static unsigned int
hash (str, len)
     register char *str;
     register int unsigned len;
{
  static unsigned char asso_values[] =
    {
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173,   1, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
     173, 173, 173, 173, 173,   1, 173,  74,  18,  54,
       1,  30,  26,   1,   1,  42, 173,   3,  61,  28,
      57,  15,   2, 173,   5,  50,   1,  56,   3,   7,
      32,   9, 173, 173, 173, 173, 173, 173,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 4:
        hval += asso_values[str[3]];
      case 3:
      case 2:
      case 1:
        hval += asso_values[str[0]];
        break;
    }
  return hval + asso_values[str[len - 1]];
}

static struct resword wordlist[] =
{
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"@end",  END, NORID},
      {"",}, 
      {"void",  TYPESPEC, RID_VOID},
      {"",}, {"",}, {"",}, {"",}, 
      {"__stdcall__",   TYPE_QUAL, RID_STDCALL},
      {"__iterator__",  SCSPEC, RID_ITERATOR},
      {"__attribute__",  ATTRIBUTE, NORID},
      {"__iterator",  SCSPEC, RID_ITERATOR},
      {"do",  DO, NORID},
      {"out",  TYPE_QUAL, RID_OUT},
      {"",}, 
      {"__typeof__",  TYPEOF, NORID},
      {"",}, {"",}, 
      {"__const",  TYPE_QUAL, RID_CONST},
      {"",}, 
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"@protected",  PROTECTED, NORID},
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"",}, {"",}, {"",}, {"",}, 
      {"for",  FOR, NORID},
      {"goto",  GOTO, NORID},
      {"__imag",  IMAGPART, NORID},
      {"oneway",  TYPE_QUAL, RID_ONEWAY},
      {"__imag__",  IMAGPART, NORID},
      {"",}, 
      {"__real__",  REALPART, NORID},
      {"dllexport",  DLL_EXPORT, RID_DLLEXPORT},
      {"@interface",  INTERFACE, NORID},
      {"__attribute",  ATTRIBUTE, NORID},
      {"__typeof",  TYPEOF, NORID},
      {"id",  OBJECTNAME, RID_ID},
      {"int",  TYPESPEC, RID_INT},
      {"__extension__",  EXTENSION, NORID},
      {"bycopy",  TYPE_QUAL, RID_BYCOPY},
      {"",}, {"",}, {"",}, 
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"dllimport",  DLL_IMPORT, RID_DLLIMPORT},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"double",  TYPESPEC, RID_DOUBLE},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"switch",  SWITCH, NORID},
      {"__asm__",  ASM_KEYWORD, NORID},
      {"register",  SCSPEC, RID_REGISTER},
      {"short",  TYPESPEC, RID_SHORT},
      {"",}, 
      {"typeof",  TYPEOF, NORID},
      {"typedef",  SCSPEC, RID_TYPEDEF},
      {"",}, {"",}, 
      {"long",  TYPESPEC, RID_LONG},
      {"char",  TYPESPEC, RID_CHAR},
      {"__inline__",  SCSPEC, RID_INLINE},
      {"if",  IF, NORID},
      {"",}, 
      {"__stdcall",  TYPE_QUAL, RID_STDCALL},
      {"",}, 
      {"__alignof__",  ALIGNOF, NORID},
      {"@implementation",  IMPLEMENTATION, NORID},
      {"@selector",  SELECTOR, NORID},
      {"",}, {"",}, 
      {"byref",  TYPE_QUAL, RID_BYREF},
      {"@public",  PUBLIC, NORID},
      {"@private",  PRIVATE, NORID},
      {"@defs",  DEFS, NORID},
      {"default",  DEFAULT, NORID},
      {"__asm",  ASM_KEYWORD, NORID},
      {"__label__",  LABEL, NORID},
      {"@protocol",  PROTOCOL, NORID},
      {"",}, {"",}, {"",}, 
      {"enum",  ENUM, NORID},
      {"",}, 
      {"@encode",  ENCODE, NORID},
      {"continue",  CONTINUE, NORID},
      {"else",  ELSE, NORID},
      {"__declspec",  DECLSPEC, RID_DECLSPEC},
      {"__inline",  SCSPEC, RID_INLINE},
      {"__alignof",  ALIGNOF, NORID},
      {"__real",  REALPART, NORID},
      {"@compatibility_alias",  ALIAS, NORID},
      {"break",  BREAK, NORID},
      {"in",  TYPE_QUAL, RID_IN},
      {"",}, 
      {"while",  WHILE, NORID},
      {"inout",  TYPE_QUAL, RID_INOUT},
      {"asm",  ASM_KEYWORD, NORID},
      {"float",  TYPESPEC, RID_FLOAT},
      {"unsigned",  TYPESPEC, RID_UNSIGNED},
      {"auto",  SCSPEC, RID_AUTO},
      {"",}, 
      {"const",  TYPE_QUAL, RID_CONST},
      {"static",  SCSPEC, RID_STATIC},
      {"sizeof",  SIZEOF, NORID},
      {"struct",  STRUCT, NORID},
      {"signed",  TYPESPEC, RID_SIGNED},
      {"volatile",  TYPE_QUAL, RID_VOLATILE},
      {"",}, {"",}, 
      {"case",  CASE, NORID},
      {"",}, 
      {"inline",  SCSPEC, RID_INLINE},
      {"",}, {"",}, 
      {"extern",  SCSPEC, RID_EXTERN},
      {"return",  RETURN, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"@class",  CLASS, NORID},
      {"",}, 
      {"union",  UNION, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, 
      {"stdcall",  TYPE_QUAL, RID_STDCALL},
};

#ifdef __GNUC__
inline
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
