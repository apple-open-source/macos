/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -L C -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$  */
/* Command-line: gperf -L KR-C -F ', 0, 0' -p -j1 -i 1 -g -o -t -N is_reserved_word -k1,3,$ c-parse.gperf  */ 
struct resword { const char *name; short token; enum rid rid; };

#define TOTAL_KEYWORDS 91
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 13
#define MAX_HASH_VALUE 211
/* maximum key range = 199, duplicates = 0 */

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
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212,  34, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
     212, 212, 212, 212, 212,   1, 212, 115,   7,  37,
      20,   6,  75,  46,  12,  13, 212,   1,  15,   8,
      61,  91,  58, 212,  19,   8,   1,  20,  65,   6,
      23,   4,   2, 212, 212, 212, 212, 212,
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
        break;
    }
  return hval + asso_values[str[len - 1]];
}

static struct resword wordlist[] =
{
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, 
      {"__typeof__",  TYPEOF, NORID},
      {"",}, {"",}, {"",}, {"",}, 
      {"int",  TYPESPEC, RID_INT},
      {"break",  BREAK, NORID},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"__extension__",  EXTENSION, NORID},
      {"",}, 
      {"__imag__",  IMAGPART, NORID},
      {"else",  ELSE, NORID},
      {"__inline__",  SCSPEC, RID_INLINE},
      {"__label__",  LABEL, NORID},
      {"__iterator__",  SCSPEC, RID_ITERATOR},
      {"__inline",  SCSPEC, RID_INLINE},
      {"__real__",  REALPART, NORID},
      {"while",  WHILE, NORID},
      {"__restrict",  TYPE_QUAL, RID_RESTRICT},
      {"__direct__",  SCSPEC, RID_DIRECT},
      {"__restrict__",  TYPE_QUAL, RID_RESTRICT},
      {"struct",  STRUCT, NORID},
      {"id",  OBJECTNAME, RID_ID},
      {"restrict",  TYPE_QUAL, RID_RESTRICT},
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"enum",  ENUM, NORID},
      {"switch",  SWITCH, NORID},
      {"inline",  SCSPEC, RID_INLINE},
      {"__real",  REALPART, NORID},
      {"",}, 
      {"__iterator",  SCSPEC, RID_ITERATOR},
      {"",}, {"",}, 
      {"__const",  TYPE_QUAL, RID_CONST},
      {"",}, 
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"",}, 
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"",}, 
      {"double",  TYPESPEC, RID_DOUBLE},
      {"@defs",  DEFS, NORID},
      {"bycopy",  TYPE_QUAL, RID_BYCOPY},
      {"case",  CASE, NORID},
      {"unsigned",  TYPESPEC, RID_UNSIGNED},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"@class",  CLASS, NORID},
      {"",}, {"",}, 
      {"__imag",  IMAGPART, NORID},
      {"@private",  PRIVATE, NORID},
      {"@selector",  SELECTOR, NORID},
      {"",}, 
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"",}, {"",}, {"",}, 
      {"extern",  SCSPEC, RID_EXTERN},
      {"",}, 
      {"in",  TYPE_QUAL, RID_IN},
      {"@protocol",  PROTOCOL, NORID},
      {"__private_extern__",  TYPE_QUAL, RID_PRIVATE_EXTERN},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"signed",  TYPESPEC, RID_SIGNED},
      {"__pixel",  TYPESPEC, RID_PIXEL},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"@protected",  PROTECTED, NORID},
      {"",}, 
      {"__typeof",  TYPEOF, NORID},
      {"",}, 
      {"return",  RETURN, NORID},
      {"",}, {"",}, 
      {"if",  IF, NORID},
      {"sizeof",  SIZEOF, NORID},
      {"register",  SCSPEC, RID_REGISTER},
      {"__vector",  TYPESPEC, RID_VECTOR},
      {"volatile",  TYPE_QUAL, RID_VOLATILE},
      {"",}, 
      {"out",  TYPE_QUAL, RID_OUT},
      {"",}, 
      {"@public",  PUBLIC, NORID},
      {"union",  UNION, NORID},
      {"",}, 
      {"pixel",  TYPESPEC, RID_PIXEL},
      {"void",  TYPESPEC, RID_VOID},
      {"default",  DEFAULT, NORID},
      {"const",  TYPE_QUAL, RID_CONST},
      {"short",  TYPESPEC, RID_SHORT},
      {"byref",  TYPE_QUAL, RID_BYREF},
      {"oneway",  TYPE_QUAL, RID_ONEWAY},
      {"@encode",  ENCODE, NORID},
      {"",}, 
      {"inout",  TYPE_QUAL, RID_INOUT},
      {"@interface",  INTERFACE, NORID},
      {"continue",  CONTINUE, NORID},
      {"do",  DO, NORID},
      {"",}, {"",}, 
      {"for",  FOR, NORID},
      {"bool",  TYPESPEC, RID_BOOL},
      {"@implementation",  IMPLEMENTATION, NORID},
      {"@end",  END, NORID},
      {"",}, {"",}, {"",}, {"",}, 
      {"__asm__",  ASM_KEYWORD, NORID},
      {"",}, 
      {"long",  TYPESPEC, RID_LONG},
      {"vector",  TYPESPEC, RID_VECTOR},
      {"__alignof__",  ALIGNOF, NORID},
      {"__asm",  ASM_KEYWORD, NORID},
      {"__attribute__",  ATTRIBUTE, NORID},
      {"",}, {"",}, 
      {"__attribute",  ATTRIBUTE, NORID},
      {"asm",  ASM_KEYWORD, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"typeof",  TYPEOF, NORID},
      {"typedef",  SCSPEC, RID_TYPEDEF},
      {"goto",  GOTO, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"@compatibility_alias",  ALIAS, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, 
      {"static",  SCSPEC, RID_STATIC},
      {"",}, 
      {"vec_step",  VEC_STEP, NORID},
      {"",}, {"",}, {"",}, 
      {"float",  TYPESPEC, RID_FLOAT},
      {"",}, {"",}, 
      {"char",  TYPESPEC, RID_CHAR},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"__alignof",  ALIGNOF, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"auto",  SCSPEC, RID_AUTO},
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
