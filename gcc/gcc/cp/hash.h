/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -L C -p -j1 -g -o -t -N is_reserved_word -k1,4,7,$ /Local/Public/turly/compiler/gcc/cp/gxx.gperf  */
/* Command-line: gperf -L KR-C -F ', 0, 0' -p -j1 -g -o -t -N is_reserved_word -k1,4,$,7 gplus.gperf  */
struct resword { const char *name; short token; enum rid rid;};

#define TOTAL_KEYWORDS 111
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 16
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 250
/* maximum key range = 247, duplicates = 0 */

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
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
     251, 251, 251, 251, 251,   0, 251,  74,   5,  22,
       1,   0, 101,  61,   0,  92, 251,   1,   0,  57,
      37,  69,  34,  24,  52,   7,  12,  50, 116,  10,
       3,   9, 251, 251, 251, 251, 251, 251,
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
      {"",}, 
      {"__real",  REALPART, NORID},
      {"delete",  DELETE, NORID,},
      {"__real__",  REALPART, NORID},
      {"bool",  TYPESPEC, RID_BOOL,},
      {"",}, {"",}, 
      {"double",  TYPESPEC, RID_DOUBLE,},
      {"",}, 
      {"__asm__",  ASM_KEYWORD, NORID},
      {"while",  WHILE, NORID,},
      {"true",  CXX_TRUE, NORID,},
      {"",}, {"",}, 
      {"typeid",  TYPEID, NORID,},
      {"",}, {"",}, {"",}, {"",}, 
      {"try",  TRY, NORID,},
      {"switch",  SWITCH, NORID,},
      {"case",  CASE, NORID,},
      {"",}, {"",}, {"",}, 
      {"this",  THIS, NORID,},
      {"",}, {"",}, 
      {"xor_eq",  ASSIGN, NORID,},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"pixel",  TYPESPEC, RID_PIXEL,},
      {"",}, 
      {"class",  AGGR, RID_CLASS,},
      {"static_cast",  STATIC_CAST, NORID,},
      {"extern",  SCSPEC, RID_EXTERN,},
      {"",}, {"",}, 
      {"const",  CV_QUALIFIER, RID_CONST,},
      {"static",  SCSPEC, RID_STATIC,},
      {"__alignof__",  ALIGNOF, NORID},
      {"catch",  CATCH, NORID,},
      {"new",  NEW, NORID,},
      {"signed",  TYPESPEC, RID_SIGNED,},
      {"not",  '!', NORID,},
      {"__extension__",  EXTENSION, NORID},
      {"",}, {"",}, 
      {"__null",  CONSTANT, RID_NULL},
      {"",}, 
      {"xor",  '^', NORID,},
      {"",}, {"",}, 
      {"compl",  '~', NORID,},
      {"public",  VISSPEC, RID_PUBLIC,},
      {"",}, 
      {"__restrict__",  CV_QUALIFIER, RID_RESTRICT},
      {"__imag__",  IMAGPART, NORID},
      {"template",  TEMPLATE, RID_TEMPLATE,},
      {"not_eq",  EQCOMPARE, NORID,},
      {"protected",  VISSPEC, RID_PROTECTED,},
      {"__asm",  ASM_KEYWORD, NORID},
      {"",}, {"",}, 
      {"do",  DO, NORID,},
      {"const_cast",  CONST_CAST, NORID,},
      {"__restrict",  CV_QUALIFIER, RID_RESTRICT},
      {"struct",  AGGR, RID_RECORD,},
      {"short",  TYPESPEC, RID_SHORT,},
      {"typename",  TYPENAME_KEYWORD, NORID,},
      {"and",  ANDAND, NORID,},
      {"",}, 
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"__inline",  SCSPEC, RID_INLINE},
      {"__label__",  LABEL, NORID},
      {"__inline__",  SCSPEC, RID_INLINE},
      {"break",  BREAK, NORID,},
      {"bitand",  '&', NORID,},
      {"export",  SCSPEC, RID_EXPORT,},
      {"__typeof__",  TYPEOF, NORID},
      {"",}, 
      {"__const__",  CV_QUALIFIER, RID_CONST},
      {"__volatile",  CV_QUALIFIER, RID_VOLATILE},
      {"continue",  CONTINUE, NORID,},
      {"__volatile__",  CV_QUALIFIER, RID_VOLATILE},
      {"",}, 
      {"__wchar_t",  TYPESPEC, RID_WCHAR  /* Unique to ANSI C++ */,},
      {"throw",  THROW, NORID,},
      {"",}, 
      {"or_eq",  ASSIGN, NORID,},
      {"__pixel",  TYPESPEC, RID_PIXEL},
      {"__const",  CV_QUALIFIER, RID_CONST},
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"signature",  AGGR, RID_SIGNATURE	/* Extension */,},
      {"and_eq",  ASSIGN, NORID,},
      {"",}, 
      {"default",  DEFAULT, NORID,},
      {"int",  TYPESPEC, RID_INT,},
      {"friend",  SCSPEC, RID_FRIEND,},
      {"",}, {"",}, {"",}, 
      {"explicit",  SCSPEC, RID_EXPLICIT,},
      {"false",  CXX_FALSE, NORID,},
      {"sizeof",  SIZEOF, NORID,},
      {"__attribute",  ATTRIBUTE, NORID},
      {"",}, 
      {"__attribute__",  ATTRIBUTE, NORID},
      {"enum",  ENUM, NORID,},
      {"typeof",  TYPEOF, NORID,},
      {"namespace",  NAMESPACE, NORID,},
      {"dynamic_cast",  DYNAMIC_CAST, NORID,},
      {"void",  TYPESPEC, RID_VOID,},
      {"or",  OROR, NORID,},
      {"__imag",  IMAGPART, NORID},
      {"",}, 
      {"long",  TYPESPEC, RID_LONG,},
      {"",}, {"",}, 
      {"__vector",  TYPESPEC, RID_VECTOR},
      {"char",  TYPESPEC, RID_CHAR,},
      {"bitor",  '|', NORID,},
      {"",}, {"",}, 
      {"asm",  ASM_KEYWORD, NORID,},
      {"virtual",  SCSPEC, RID_VIRTUAL,},
      {"",}, {"",}, 
      {"mutable",  SCSPEC, RID_MUTABLE,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"return",  RETURN_KEYWORD, NORID,},
      {"",}, 
      {"__alignof",  ALIGNOF, NORID},
      {"",}, {"",}, {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED,},
      {"",}, 
      {"using",  USING, NORID,},
      {"",}, {"",}, 
      {"for",  FOR, NORID,},
      {"private",  VISSPEC, RID_PRIVATE,},
      {"vec_step",  VEC_STEP, NORID},
      {"",}, {"",}, 
      {"union",  AGGR, RID_UNION,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"reinterpret_cast",  REINTERPRET_CAST, NORID,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"__signature__",  AGGR, RID_SIGNATURE	/* Extension */,},
      {"",}, {"",}, 
      {"sigof",  SIGOF, NORID		/* Extension */,},
      {"",}, {"",}, {"",}, 
      {"vector",  TYPESPEC, RID_VECTOR,},
      {"__typeof",  TYPEOF, NORID},
      {"",}, {"",}, 
      {"inline",  SCSPEC, RID_INLINE,},
      {"",}, 
      {"float",  TYPESPEC, RID_FLOAT,},
      {"",}, {"",}, 
      {"if",  IF, NORID,},
      {"",}, {"",}, 
      {"volatile",  CV_QUALIFIER, RID_VOLATILE,},
      {"",}, {"",}, {"",}, 
      {"__sigof__",  SIGOF, NORID		/* Extension */,},
      {"goto",  GOTO, NORID,},
      {"register",  SCSPEC, RID_REGISTER,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, 
      {"auto",  SCSPEC, RID_AUTO,},
      {"",}, {"",}, {"",}, {"",}, 
      {"typedef",  SCSPEC, RID_TYPEDEF,},
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
