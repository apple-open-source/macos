/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -g -o -t -N is_reserved_word -k1,4,$,7 gxx-winnt.gperf  */
/* Command-line: gperf -p -j1 -g -o -t -N is_reserved_word -k1,4,$,7 gplus.gperf  */
struct resword { char *name; short token; enum rid rid;};

#define TOTAL_KEYWORDS 103
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 16
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 206
/* maximum key range = 203, duplicates = 0 */

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
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
     207, 207, 207, 207, 207,   0, 207,   8,   2,  63,
      43,   0,  80,   6,   4,  52, 207,   1,  13,  67,
      19,  59,  51,   1,  40,  11,   1,  59,   5,  20,
       1,   2, 207, 207, 207, 207, 207, 207,
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

#ifdef __GNUC__
inline
#endif
struct resword *
is_reserved_word (str, len)
     register char *str;
     register unsigned int len;
{
  static struct resword wordlist[] =
    {
      {"",}, {"",}, {"",}, {"",}, 
      {"else",  ELSE, NORID,},
      {"true",  CXX_TRUE, NORID,},
      {"try",  TRY, NORID,},
      {"",}, 
      {"xor_eq",  ASSIGN, NORID,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"and_eq",  ASSIGN, NORID,},
      {"break",  BREAK, NORID,},
      {"",}, 
      {"__asm__",  GCC_ASM_KEYWORD, NORID},
      {"",}, 
      {"__stdcall__",   TYPE_QUAL, RID_STDCALL,},
      {"",}, 
      {"switch",  SWITCH, NORID,},
      {"not",  '!', NORID,},
      {"static_cast",  STATIC_CAST, NORID,},
      {"extern",  SCSPEC, RID_EXTERN,},
      {"not_eq",  EQCOMPARE, NORID,},
      {"this",  THIS, NORID,},
      {"",}, 
      {"long",  TYPESPEC, RID_LONG,},
      {"__label__",  LABEL, NORID},
      {"__stdcall",  TYPE_QUAL, RID_STDCALL,},
      {"bool",  TYPESPEC, RID_BOOL,},
      {"__extension__",  EXTENSION, NORID},
      {"volatile",  TYPE_QUAL, RID_VOLATILE,},
      {"",}, 
      {"namespace",  NAMESPACE, NORID,},
      {"",}, 
      {"while",  WHILE, NORID,},
      {"virtual",  SCSPEC, RID_VIRTUAL,},
      {"",}, {"",}, 
      {"new",  NEW, NORID,},
      {"__alignof__",  ALIGNOF, NORID},
      {"xor",  '^', NORID,},
      {"",}, 
      {"__inline",  SCSPEC, RID_INLINE},
      {"",}, 
      {"__inline__",  SCSPEC, RID_INLINE},
      {"delete",  DELETE, NORID,},
      {"typeid",  TYPEID, NORID,},
      {"double",  TYPESPEC, RID_DOUBLE,},
      {"",}, {"",}, 
      {"and",  ANDAND, NORID,},
      {"",}, 
      {"int",  TYPESPEC, RID_INT,},
      {"short",  TYPESPEC, RID_SHORT,},
      {"",}, 
      {"bitand",  '&', NORID,},
      {"default",  DEFAULT, NORID,},
      {"template",  TEMPLATE, RID_TEMPLATE,},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"private",  VISSPEC, RID_PRIVATE,},
      {"__attribute",  ATTRIBUTE, NORID},
      {"or_eq",  ASSIGN, NORID,},
      {"__attribute__",  ATTRIBUTE, NORID},
      {"case",  CASE, NORID,},
      {"__const",  TYPE_QUAL, RID_CONST},
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"__typeof__",  TYPEOF, NORID},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"__signature__",  AGGR, RID_SIGNATURE	/* Extension */,},
      {"explicit",  SCSPEC, RID_EXPLICIT,},
      {"",}, 
      {"typename",  TYPENAME_KEYWORD, NORID,},
      {"struct",  AGGR, RID_RECORD,},
      {"asm",  ASM_KEYWORD, NORID,},
      {"signed",  TYPESPEC, RID_SIGNED,},
      {"const",  TYPE_QUAL, RID_CONST,},
      {"static",  SCSPEC, RID_STATIC,},
      {"mutable",  SCSPEC, RID_MUTABLE,},
      {"__asm",  GCC_ASM_KEYWORD, NORID},
      {"__declspec",  DECLSPEC, RID_DECLSPEC,},
      {"throw",  THROW, NORID,},
      {"",}, 
      {"typeof",  TYPEOF, NORID,},
      {"",}, 
      {"using",  USING, NORID,},
      {"class",  AGGR, RID_CLASS,},
      {"",}, {"",}, {"",}, 
      {"float",  TYPESPEC, RID_FLOAT,},
      {"void",  TYPESPEC, RID_VOID,},
      {"false",  CXX_FALSE, NORID,},
      {"sizeof",  SIZEOF, NORID,},
      {"signature",  AGGR, RID_SIGNATURE	/* Extension */,},
      {"",}, {"",}, 
      {"or",  OROR, NORID,},
      {"",}, 
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"do",  DO, NORID,},
      {"protected",  VISSPEC, RID_PROTECTED,},
      {"bitor",  '|', NORID,},
      {"stdcall",  TYPE_QUAL, RID_STDCALL,},
      {"",}, {"",}, 
      {"inline",  SCSPEC, RID_INLINE,},
      {"",}, 
      {"dllexport",  DLL_EXPORT, RID_DLLEXPORT,},
      {"__wchar_t",  TYPESPEC, RID_WCHAR  /* Unique to ANSI C++ */,},
      {"",}, {"",}, 
      {"reinterpret_cast",  REINTERPRET_CAST, NORID,},
      {"",}, {"",}, {"",}, {"",}, 
      {"__alignof",  ALIGNOF, NORID},
      {"",}, 
      {"for",  FOR, NORID,},
      {"return",  RETURN, NORID,},
      {"",}, {"",}, 
      {"dynamic_cast",  DYNAMIC_CAST, NORID,},
      {"goto",  GOTO, NORID,},
      {"friend",  SCSPEC, RID_FRIEND,},
      {"auto",  SCSPEC, RID_AUTO,},
      {"continue",  CONTINUE, NORID,},
      {"compl",  '~', NORID,},
      {"public",  VISSPEC, RID_PUBLIC,},
      {"if",  IF, NORID,},
      {"catch",  CATCH, NORID,},
      {"",}, {"",}, 
      {"enum",  ENUM, NORID,},
      {"",}, 
      {"register",  SCSPEC, RID_REGISTER,},
      {"__sigof__",  SIGOF, NORID		/* Extension */,},
      {"union",  AGGR, RID_UNION,},
      {"",}, {"",}, {"",}, {"",}, 
      {"char",  TYPESPEC, RID_CHAR,},
      {"const_cast",  CONST_CAST, NORID,},
      {"__typeof",  TYPEOF, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"sigof",  SIGOF, NORID		/* Extension */,},
      {"",}, {"",}, 
      {"overload",  OVERLOAD, NORID,},
      {"",}, {"",}, {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED,},
      {"",}, 
      {"dllimport",  DLL_IMPORT, RID_DLLIMPORT,},
      {"",}, {"",}, {"",}, 
      {"typedef",  SCSPEC, RID_TYPEDEF,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"operator",  OPERATOR, NORID,},
    };

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
