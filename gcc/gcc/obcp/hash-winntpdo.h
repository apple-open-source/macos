/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -g -G -o -t -N is_reserved_word -k1,4,$,7 obcp-winnt.gperf  */
/* Command-line: gperf -p -j1 -g -G -o -t -N is_reserved_word '-k1,4,$,7' obcp-winnt.gperf  */
struct resword { char *name; short token; enum rid rid; enum languages lang; char *save; };

#define TOTAL_KEYWORDS 116
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 15
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 224
/* maximum key range = 221, duplicates = 0 */

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
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225,  42, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
     225, 225, 225, 225, 225,   0, 225,  68,  37,  45,
      41,   0, 108,  11,   4,  54, 225,   0,  43,  20,
      32,   2,  40,  70,  35,  10,   1,  83,  10,   6,
      22,  18, 225, 225, 225, 225, 225, 225,
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
      {"",}, {"",}, {"",}, {"",}, 
      {"__const",  TYPE_QUAL, RID_CONST, lang_c,},
      {"__const__",  TYPE_QUAL, RID_CONST, lang_c,},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE, lang_c,},
      {"throw",  THROW, NORID, lang_cplusplus,		/* Extension */},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE, lang_c,},
      {"",}, 
      {"__asm__",  GCC_ASM_KEYWORD, NORID, lang_c,},
      {"",}, 
      {"goto",  GOTO, NORID, lang_c,},
      {"",}, 
      {"switch",  SWITCH, NORID, lang_c, },
      {"try",  TRY, NORID, lang_cplusplus,		/* Extension */},
      {"static_cast",  STATIC_CAST, NORID, lang_cplusplus,},
      {"",}, 
      {"this",  THIS, NORID, lang_cplusplus, },
      {"",}, {"",}, {"",}, 
      {"typename",  TYPENAME_KEYWORD, NORID, lang_cplusplus,},
      {"__typeof__",  TYPEOF, NORID, lang_c,},
      {"",}, 
      {"oneway",  TYPE_QUAL, RID_ONEWAY, lang_objc,},
      {"",}, {"",}, 
      {"__asm",  GCC_ASM_KEYWORD, NORID, lang_c,},
      {"not",  '!', NORID, lang_c,},
      {"",}, 
      {"extern",  SCSPEC, RID_EXTERN, lang_c,},
      {"or",  OROR, NORID, lang_c,},
      {"",}, 
      {"new",  NEW, NORID, lang_cplusplus,},
      {"",}, {"",}, 
      {"enum",  ENUM, NORID, lang_c,},
      {"do",  DO, NORID, lang_c,},
      {"",}, 
      {"delete",  DELETE, NORID, lang_cplusplus,},
      {"typeid",  TYPEID, NORID, lang_cplusplus,},
      {"case",  CASE, NORID, lang_c,},
      {"template",  TEMPLATE, NORID, lang_cplusplus, },
      {"short",  TYPESPEC, RID_SHORT, lang_c, },
      {"",}, 
      {"dllexport",  DLL_EXPORT, RID_DLLEXPORT,},
      {"while",  WHILE, NORID, lang_c, },
      {"",}, {"",}, 
      {"private",  VISSPEC, RID_PRIVATE, lang_cplusplus,},
      {"int",  TYPESPEC, RID_INT, lang_c,},
      {"",}, 
      {"xor",  '^', NORID, lang_c,},
      {"const",  TYPE_QUAL, RID_CONST, lang_c,},
      {"static",  SCSPEC, RID_STATIC, lang_c, },
      {"bycopy",  TYPE_QUAL, RID_BYCOPY, lang_objc,},
      {"__signed__",  TYPESPEC, RID_SIGNED, lang_c,},
      {"__declspec",  DECLSPEC, RID_DECLSPEC,},
      {"__attribute",  ATTRIBUTE, NORID, lang_c,},
      {"__extension__",  EXTENSION, NORID, lang_c,},
      {"__attribute__",  ATTRIBUTE, NORID, lang_c,},
      {"long",  TYPESPEC, RID_LONG, lang_c,},
      {"class",  AGGR, RID_CLASS, lang_cplusplus,},
      {"",}, 
      {"__inline",  SCSPEC, RID_INLINE, lang_c,},
      {"",}, 
      {"__inline__",  SCSPEC, RID_INLINE, lang_c,},
      {"",}, 
      {"auto",  SCSPEC, RID_AUTO, lang_c,},
      {"or_eq",  ASSIGN, NORID, lang_c,},
      {"",}, 
      {"bitor",  '|', NORID, lang_c,},
      {"__stdcall__",   TYPE_QUAL, RID_STDCALL,},
      {"",}, 
      {"operator",  OPERATOR, NORID, lang_cplusplus,},
      {"",}, 
      {"double",  TYPESPEC, RID_DOUBLE, lang_c,},
      {"",}, 
      {"__alignof__",  ALIGNOF, NORID, lang_c,},
      {"",}, 
      {"in",  TYPE_QUAL, RID_IN, lang_objc,},
      {"signed",  TYPESPEC, RID_SIGNED, lang_c, },
      {"__wchar_t",  TYPESPEC, RID_WCHAR, lang_cplusplus, /* Unique to ANSI C++ */},
      {"asm",  ASM_KEYWORD, NORID, lang_c,},
      {"protected",  VISSPEC, RID_PROTECTED, lang_cplusplus,},
      {"",}, 
      {"@encode",  ENCODE, NORID, lang_objc,},
      {"mutable",  SCSPEC, RID_MUTABLE, lang_cplusplus,},
      {"void",  TYPESPEC, RID_VOID, lang_c, },
      {"id",  OBJECTNAME, RID_ID, lang_objc,},
      {"xor_eq",  ASSIGN, NORID, lang_c,},
      {"catch",  CATCH, NORID, lang_cplusplus,},
      {"struct",  AGGR, RID_RECORD, lang_c, },
      {"",}, {"",}, 
      {"__signed",  TYPESPEC, RID_SIGNED, lang_c,},
      {"virtual",  SCSPEC, RID_VIRTUAL, lang_cplusplus, },
      {"@private",  PRIVATE, NORID, lang_objc,},
      {"",}, 
      {"dllimport",  DLL_IMPORT, RID_DLLIMPORT,},
      {"not_eq",  EQCOMPARE, NORID, lang_c,},
      {"namespace",  NAMESPACE, NORID, lang_cplusplus,},
      {"break",  BREAK, NORID, lang_c,},
      {"const_cast",  CONST_CAST, NORID, lang_cplusplus,},
      {"and",  ANDAND, NORID, lang_c,},
      {"",}, 
      {"inline",  SCSPEC, RID_INLINE, lang_c,},
      {"typeof",  TYPEOF, NORID, lang_c, },
      {"",}, {"",}, 
      {"default",  DEFAULT, NORID, lang_c,},
      {"char",  TYPESPEC, RID_CHAR, lang_c,},
      {"__label__",  LABEL, NORID, lang_c,},
      {"__stdcall",  TYPE_QUAL, RID_STDCALL,},
      {"union",  AGGR, RID_UNION, lang_c, },
      {"false",  CXX_FALSE, NORID, lang_c,},
      {"sizeof",  SIZEOF, NORID, lang_c, },
      {"sigof",  SIGOF, NORID, lang_cplusplus,		/* Extension */},
      {"@class",  CLASS, NORID, lang_objc,},
      {"bool",  TYPESPEC, RID_BOOL, lang_c,},
      {"@end",  END, NORID, lang_objc,},
      {"volatile",  TYPE_QUAL, RID_VOLATILE, lang_c, },
      {"@selector",  SELECTOR, NORID, lang_objc, },
      {"",}, 
      {"register",  SCSPEC, RID_REGISTER, lang_c, },
      {"compl",  '~', NORID, lang_c,},
      {"public",  VISSPEC, RID_PUBLIC, lang_cplusplus, },
      {"__signature__",  AGGR, RID_SIGNATURE, lang_cplusplus	/* Extension */,},
      {"__typeof",  TYPEOF, NORID, lang_c,},
      {"continue",  CONTINUE, NORID, lang_c,},
      {"",}, {"",}, 
      {"@protected",  PROTECTED, NORID, lang_objc,},
      {"@protocol",  PROTOCOL, NORID, lang_objc,},
      {"",}, 
      {"inout",  TYPE_QUAL, RID_INOUT, lang_objc,},
      {"and_eq",  ASSIGN, NORID, lang_c,},
      {"",}, 
      {"for",  FOR, NORID, lang_c,},
      {"",}, 
      {"stdcall",  TYPE_QUAL, RID_STDCALL,},
      {"@implementation",  IMPLEMENTATION, NORID, lang_objc,},
      {"byref",  TYPE_QUAL, RID_BYREF, lang_objc,},
      {"",}, 
      {"bitand",  '&', NORID, lang_c,},
      {"",}, 
      {"overload",  OVERLOAD, NORID, lang_cplusplus,},
      {"friend",  SCSPEC, RID_FRIEND, lang_cplusplus,},
      {"return",  RETURN, NORID, lang_c, },
      {"",}, {"",}, {"",}, {"",}, 
      {"@interface",  INTERFACE, NORID, lang_objc,},
      {"",}, {"",}, 
      {"if",  IF, NORID, lang_c,},
      {"@defs",  DEFS, NORID, lang_objc,},
      {"",}, 
      {"dynamic_cast",  DYNAMIC_CAST, NORID, lang_cplusplus,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"@public",  PUBLIC, NORID, lang_objc,},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"float",  TYPESPEC, RID_FLOAT, lang_c,},
      {"",}, {"",}, {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED, lang_c, },
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"__alignof",  ALIGNOF, NORID, lang_c,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, 
      {"typedef",  SCSPEC, RID_TYPEDEF, lang_c, },
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
