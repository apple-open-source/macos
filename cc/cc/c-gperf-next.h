/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$  c-parse.gperf  */ 
struct resword { char *name; short token; enum rid rid; };

#define TOTAL_KEYWORDS 82
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 8
#define MAX_HASH_VALUE 126
/* maximum key range = 119, duplicates = 0 */

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
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127,   3, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
     127, 127, 127, 127, 127,   1, 127,  57,   1,  17,
      40,   6,   2,  24,   9,   5, 127,   4,  26,  33,
      69,   3,  81, 127,  41,  13,   1,  28,  46,   2,
       4,  20,   2, 127, 127, 127, 127, 127,
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
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"out",  TYPE_QUAL, RID_OUT},
      {"if",  IF, NORID},
      {"int",  TYPESPEC, RID_INT},
      {"float",  TYPESPEC, RID_FLOAT},
      {"__typeof",  TYPEOF, NORID},
      {"__typeof__",  TYPEOF, NORID},
      {"inout",  TYPE_QUAL, RID_INOUT},
      {"__imag__",  IMAGPART, NORID},
      {"break",  BREAK, NORID},
      {"__inline__",  SCSPEC, RID_INLINE},
      {"while",  WHILE, NORID},
      {"__iterator__",  SCSPEC, RID_ITERATOR},
      {"__inline",  SCSPEC, RID_INLINE},
      {"__extension__",  EXTENSION, NORID},
      {"short",  TYPESPEC, RID_SHORT},
      {"sizeof",  SIZEOF, NORID},
      {"",}, 
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"__const",  TYPE_QUAL, RID_CONST},
      {"@defs",  DEFS, NORID},
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"else",  ELSE, NORID},
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"goto",  GOTO, NORID},
      {"switch",  SWITCH, NORID},
      {"",}, 
      {"oneway",  TYPE_QUAL, RID_ONEWAY},
      {"__imag",  IMAGPART, NORID},
      {"__label__",  LABEL, NORID},
      {"",}, 
      {"@compatibility_alias",  ALIAS, NORID},
      {"case",  CASE, NORID},
      {"",}, {"",}, 
      {"inline",  SCSPEC, RID_INLINE},
      {"bycopy",  TYPE_QUAL, RID_BYCOPY},
      {"do",  DO, NORID},
      {"",}, 
      {"id",  OBJECTNAME, RID_ID},
      {"@class",  CLASS, NORID},
      {"byref",  TYPE_QUAL, RID_BYREF},
      {"default",  DEFAULT, NORID},
      {"__real__",  REALPART, NORID},
      {"__direct__",  SCSPEC, RID_DIRECT},
      {"",}, {"",}, 
      {"@public",  PUBLIC, NORID},
      {"",}, 
      {"__iterator",  SCSPEC, RID_ITERATOR},
      {"@private",  PRIVATE, NORID},
      {"@selector",  SELECTOR, NORID},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"struct",  STRUCT, NORID},
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"",}, 
      {"auto",  SCSPEC, RID_AUTO},
      {"__asm__",  ASM_KEYWORD, NORID},
      {"",}, {"",}, 
      {"__alignof",  ALIGNOF, NORID},
      {"__alignof__",  ALIGNOF, NORID},
      {"enum",  ENUM, NORID},
      {"__attribute__",  ATTRIBUTE, NORID},
      {"",}, 
      {"__real",  REALPART, NORID},
      {"__attribute",  ATTRIBUTE, NORID},
      {"in",  TYPE_QUAL, RID_IN},
      {"",}, {"",}, 
      {"@protocol",  PROTOCOL, NORID},
      {"double",  TYPESPEC, RID_DOUBLE},
      {"",}, 
      {"extern",  SCSPEC, RID_EXTERN},
      {"signed",  TYPESPEC, RID_SIGNED},
      {"",}, 
      {"@encode",  ENCODE, NORID},
      {"volatile",  TYPE_QUAL, RID_VOLATILE},
      {"for",  FOR, NORID},
      {"@interface",  INTERFACE, NORID},
      {"unsigned",  TYPESPEC, RID_UNSIGNED},
      {"typeof",  TYPEOF, NORID},
      {"typedef",  SCSPEC, RID_TYPEDEF},
      {"const",  TYPE_QUAL, RID_CONST},
      {"static",  SCSPEC, RID_STATIC},
      {"@protected",  PROTECTED, NORID},
      {"void",  TYPESPEC, RID_VOID},
      {"__asm",  ASM_KEYWORD, NORID},
      {"",}, {"",}, {"",}, 
      {"continue",  CONTINUE, NORID},
      {"__private_extern__",  TYPE_QUAL, RID_PRIVATE_EXTERN},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"union",  UNION, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"register",  SCSPEC, RID_REGISTER},
      {"",}, 
      {"@end",  END, NORID},
      {"return",  RETURN, NORID},
      {"",}, 
      {"char",  TYPESPEC, RID_CHAR},
      {"@implementation",  IMPLEMENTATION, NORID},
      {"",}, {"",}, 
      {"long",  TYPESPEC, RID_LONG},
      {"",}, {"",}, 
      {"asm",  ASM_KEYWORD, NORID},
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
