/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -g -G -o -t -N is_reserved_word -k1,4,7,$  */
/* Command-line: gperf -p -j1 -g -G -o -t -N is_reserved_word '-k1,4,$,7' obcp.gperf  */
struct resword { char *name; short token; enum rid rid; enum languages lang; char *save; };

#define TOTAL_KEYWORDS 110
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 15
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 235
/* maximum key range = 232, duplicates = 0 */

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
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236,  50, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
     236, 236, 236, 236, 236,   0, 236,  32, 118,  30,
      22,   0,  84,  28,  11,  13, 236,   0,   9,  45,
      29,   2,  57,   3,  49,  38,   1,  98,  13,  51,
       0,  12, 236, 236, 236, 236, 236, 236,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 7:
        hval += asso_values[str[6]];
      case 6:
      case 5:
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
      {"",}, {"",}, {"",}, {"",}, 
      {"else",  ELSE, NORID, lang_c,},
      {"true",  CXX_TRUE, NORID, lang_c,},
      {"out",  TYPE_QUAL, RID_OUT, lang_objc,},
      {"",}, {"",}, 
      {"xor_eq",  ASSIGN, NORID, lang_c,},
      {"or_eq",  ASSIGN, NORID, lang_c,},
      {"__const",  TYPE_QUAL, RID_CONST, lang_c,},
      {"__const__",  TYPE_QUAL, RID_CONST, lang_c,},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE, lang_c,},
      {"",}, 
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE, lang_c,},
      {"try",  TRY, NORID, lang_cplusplus,		/* Extension */},
      {"int",  TYPESPEC, RID_INT, lang_c,},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"__signed__",  TYPESPEC, RID_SIGNED, lang_c,},
      {"__typeof__",  TYPEOF, NORID, lang_c,},
      {"__attribute",  ATTRIBUTE, NORID, lang_c,},
      {"do",  DO, NORID, lang_c,},
      {"__attribute__",  ATTRIBUTE, NORID, lang_c,},
      {"delete",  DELETE, NORID, lang_cplusplus,},
      {"typeid",  TYPEID, NORID, lang_cplusplus,},
      {"",}, {"",}, 
      {"inline",  SCSPEC, RID_INLINE, lang_c,},
      {"not",  '!', NORID, lang_c,},
      {"case",  CASE, NORID, lang_c,},
      {"extern",  SCSPEC, RID_EXTERN, lang_c,},
      {"goto",  GOTO, NORID, lang_c,},
      {"id",  OBJECTNAME, RID_ID, lang_objc,},
      {"not_eq",  EQCOMPARE, NORID, lang_c,},
      {"virtual",  SCSPEC, RID_VIRTUAL, lang_cplusplus, },
      {"auto",  SCSPEC, RID_AUTO, lang_c,},
      {"and_eq",  ASSIGN, NORID, lang_c,},
      {"__extension__",  EXTENSION, NORID, lang_c,},
      {"__signed",  TYPESPEC, RID_SIGNED, lang_c,},
      {"in",  TYPE_QUAL, RID_IN, lang_objc,},
      {"__asm__",  GCC_ASM_KEYWORD, NORID, lang_c,},
      {"",}, {"",}, {"",}, 
      {"__alignof__",  ALIGNOF, NORID, lang_c,},
      {"__label__",  LABEL, NORID, lang_c,},
      {"static_cast",  STATIC_CAST, NORID, lang_cplusplus,},
      {"xor",  '^', NORID, lang_c,},
      {"or",  OROR, NORID, lang_c,},
      {"typename",  TYPENAME_KEYWORD, NORID, lang_cplusplus,},
      {"",}, 
      {"switch",  SWITCH, NORID, lang_c, },
      {"and",  ANDAND, NORID, lang_c,},
      {"__signature__",  AGGR, RID_SIGNATURE, lang_cplusplus	/* Extension */,},
      {"throw",  THROW, NORID, lang_cplusplus,		/* Extension */},
      {"",}, 
      {"void",  TYPESPEC, RID_VOID, lang_c, },
      {"volatile",  TYPE_QUAL, RID_VOLATILE, lang_c, },
      {"default",  DEFAULT, NORID, lang_c,},
      {"",}, 
      {"while",  WHILE, NORID, lang_c, },
      {"__inline",  SCSPEC, RID_INLINE, lang_c,},
      {"template",  TEMPLATE, NORID, lang_cplusplus, },
      {"__inline__",  SCSPEC, RID_INLINE, lang_c,},
      {"long",  TYPESPEC, RID_LONG, lang_c,},
      {"namespace",  NAMESPACE, NORID, lang_cplusplus,},
      {"oneway",  TYPE_QUAL, RID_ONEWAY, lang_objc,},
      {"@private",  PRIVATE, NORID, lang_objc,},
      {"",}, 
      {"const",  TYPE_QUAL, RID_CONST, lang_c,},
      {"static",  SCSPEC, RID_STATIC, lang_c, },
      {"catch",  CATCH, NORID, lang_cplusplus,},
      {"private",  VISSPEC, RID_PRIVATE, lang_cplusplus,},
      {"",}, {"",}, 
      {"asm",  ASM_KEYWORD, NORID, lang_c,},
      {"this",  THIS, NORID, lang_cplusplus, },
      {"",}, 
      {"new",  NEW, NORID, lang_cplusplus,},
      {"mutable",  SCSPEC, RID_MUTABLE, lang_cplusplus,},
      {"",}, {"",}, 
      {"@encode",  ENCODE, NORID, lang_objc,},
      {"__asm",  GCC_ASM_KEYWORD, NORID, lang_c,},
      {"__wchar_t",  TYPESPEC, RID_WCHAR, lang_cplusplus, /* Unique to ANSI C++ */},
      {"protected",  VISSPEC, RID_PROTECTED, lang_cplusplus,},
      {"typeof",  TYPEOF, NORID, lang_c, },
      {"",}, 
      {"short",  TYPESPEC, RID_SHORT, lang_c, },
      {"enum",  ENUM, NORID, lang_c,},
      {"signed",  TYPESPEC, RID_SIGNED, lang_c, },
      {"",}, 
      {"dynamic_cast",  DYNAMIC_CAST, NORID, lang_cplusplus,},
      {"@end",  END, NORID, lang_objc,},
      {"if",  IF, NORID, lang_c,},
      {"@protocol",  PROTOCOL, NORID, lang_objc,},
      {"compl",  '~', NORID, lang_c,},
      {"public",  VISSPEC, RID_PUBLIC, lang_cplusplus, },
      {"",}, {"",}, {"",}, 
      {"__typeof",  TYPEOF, NORID, lang_c,},
      {"",}, {"",}, 
      {"const_cast",  CONST_CAST, NORID, lang_cplusplus,},
      {"operator",  OPERATOR, NORID, lang_cplusplus,},
      {"class",  AGGR, RID_CLASS, lang_cplusplus,},
      {"friend",  SCSPEC, RID_FRIEND, lang_cplusplus,},
      {"overload",  OVERLOAD, NORID, lang_cplusplus,},
      {"@protected",  PROTECTED, NORID, lang_objc,},
      {"",}, {"",}, 
      {"inout",  TYPE_QUAL, RID_INOUT, lang_objc,},
      {"@selector",  SELECTOR, NORID, lang_objc, },
      {"register",  SCSPEC, RID_REGISTER, lang_c, },
      {"",}, {"",}, 
      {"float",  TYPESPEC, RID_FLOAT, lang_c,},
      {"",}, {"",}, {"",}, 
      {"@class",  CLASS, NORID, lang_objc,},
      {"false",  CXX_FALSE, NORID, lang_c,},
      {"sizeof",  SIZEOF, NORID, lang_c, },
      {"sigof",  SIGOF, NORID, lang_cplusplus,		/* Extension */},
      {"",}, 
      {"__alignof",  ALIGNOF, NORID, lang_c,},
      {"char",  TYPESPEC, RID_CHAR, lang_c,},
      {"",}, 
      {"union",  AGGR, RID_UNION, lang_c, },
      {"",}, 
      {"for",  FOR, NORID, lang_c,},
      {"continue",  CONTINUE, NORID, lang_c,},
      {"bycopy",  TYPE_QUAL, RID_BYCOPY, lang_objc,},
      {"",}, 
      {"bool",  TYPESPEC, RID_BOOL, lang_c,},
      {"unsigned",  TYPESPEC, RID_UNSIGNED, lang_c, },
      {"",}, 
      {"struct",  AGGR, RID_RECORD, lang_c, },
      {"",}, 
      {"@interface",  INTERFACE, NORID, lang_objc,},
      {"double",  TYPESPEC, RID_DOUBLE, lang_c,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"break",  BREAK, NORID, lang_c,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      
      {"bitor",  '|', NORID, lang_c,},
      {"",}, 
      {"typedef",  SCSPEC, RID_TYPEDEF, lang_c, },
      {"@defs",  DEFS, NORID, lang_objc,},
      {"bitand",  '&', NORID, lang_c,},
      {"",}, {"",}, {"",}, 
      {"return",  RETURN, NORID, lang_c, },
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, 
      {"@implementation",  IMPLEMENTATION, NORID, lang_objc,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"byref",  TYPE_QUAL, RID_BYREF, lang_objc,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      
      {"@public",  PUBLIC, NORID, lang_objc,},
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
