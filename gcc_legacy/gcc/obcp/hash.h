/* C code produced by gperf version 2.7.2 */
/* Command-line: gperf -L C -p -j1 -g -G -o -t -N is_reserved_word -k'1,4,7,$' /Volumes/DATA-2/Dev/WC/apple/MacOSX-new_obcp/compiler/gcc/obcp/gxx.gperf  */
/* Command-line: gperf -L C -p -j1 -g -G -o -t -N is_reserved_word -k'1,4,7,$' gxx.gperf  */
struct resword { const char *name; short token; enum rid rid;};
;

#define TOTAL_KEYWORDS 130
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 246
/* maximum key range = 243, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (str, len)
     register const char *str;
     register unsigned int len;
{
  static unsigned char asso_values[] =
    {
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247,  84, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247,   0, 247,  82,  83,   3,
       18,   0,  61,  20,  15,  39, 247,   1,   0,  54,
       34,   2,  27,  27,  15,  74,   6, 105, 117, 113,
        5,  45, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
      247, 247, 247, 247, 247, 247
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      case 6:
      case 5:
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static struct resword wordlist[] =
  {
    {""}, {""}, {""}, {""},
    {"else", ELSE, NORID,},
    {""},
    {"__real", REALPART, NORID},
    {"case", CASE, NORID,},
    {"__real__", REALPART, NORID},
    {""},
    {"true", CXX_TRUE, NORID,},
    {"out", CV_QUALIFIER, RID_OUT},
    {""},
    {"__complex__", TYPESPEC, RID_COMPLEX},
    {"export", SCSPEC, RID_EXPORT,},
    {""},
    {"__complex", TYPESPEC, RID_COMPLEX},
    {"__const__", CV_QUALIFIER, RID_CONST},
    {"__volatile", CV_QUALIFIER, RID_VOLATILE},
    {"or", OROR, NORID,},
    {"__volatile__", CV_QUALIFIER, RID_VOLATILE},
    {"__const", CV_QUALIFIER, RID_CONST},
    {"do", DO, NORID,},
    {"xor", '^', NORID,},
    {"delete", DELETE, NORID,},
    {"__vector", TYPESPEC, RID_VECTOR},
    {"catch", CATCH, NORID,},
    {"__restrict__", CV_QUALIFIER, RID_RESTRICT},
    {"goto", GOTO, NORID,},
    {""},
    {"typeid", TYPEID, NORID,},
    {"__restrict", CV_QUALIFIER, RID_RESTRICT},
    {"pixel", TYPESPEC, RID_PIXEL,},
    {"__wchar_t", TYPESPEC, RID_WCHAR  /* Unique to ANSI C++ */,},
    {"or_eq", ASSIGN, NORID,},
    {"compl", '~', NORID,},
    {"public", VISSPEC, RID_PUBLIC,},
    {"char", TYPESPEC, RID_CHAR,},
    {"xor_eq", ASSIGN, NORID,},
    {""},
    {"extern", SCSPEC, RID_EXTERN,},
    {""},
    {"operator", OPERATOR, NORID,},
    {"not", '!', NORID,},
    {"long", TYPESPEC, RID_LONG,},
    {"__alignof__", ALIGNOF, NORID},
    {"__pixel", TYPESPEC, RID_PIXEL},
    {"template", TEMPLATE, RID_TEMPLATE,},
    {"int", TYPESPEC, RID_INT,},
    {"__signed__", TYPESPEC, RID_SIGNED},
    {""}, {""},
    {"__extension__", EXTENSION, NORID},
    {"explicit", SCSPEC, RID_EXPLICIT,},
    {"try", TRY, NORID,},
    {""},
    {"__attribute", ATTRIBUTE, NORID},
    {"__typeof__", TYPEOF, NORID},
    {"__attribute__", ATTRIBUTE, NORID},
    {"id", OBJECTNAME, RID_ID},
    {""}, {""},
    {"__imag__", IMAGPART, NORID},
    {""}, {""},
    {"__signed", TYPESPEC, RID_SIGNED},
    {"protected", VISSPEC, RID_PROTECTED,},
    {"not_eq", EQCOMPARE, NORID,},
    {"typename", TYPENAME_KEYWORD, NORID,},
    {""}, {""}, {""}, {""},
    {"typeof", TYPEOF, NORID,},
    {""},
    {"in", CV_QUALIFIER, RID_IN},
    {"__inline", SCSPEC, RID_INLINE},
    {"register", SCSPEC, RID_REGISTER,},
    {"__inline__", SCSPEC, RID_INLINE},
    {"for", FOR, NORID,},
    {"__imag", IMAGPART, NORID},
    {"__asm__", ASM_KEYWORD, NORID},
    {""}, {""},
    {"inline", SCSPEC, RID_INLINE,},
    {"friend", SCSPEC, RID_FRIEND,},
    {"reinterpret_cast", REINTERPRET_CAST, NORID,},
    {"bool", TYPESPEC, RID_BOOL,},
    {"const", CV_QUALIFIER, RID_CONST,},
    {"static", SCSPEC, RID_STATIC,},
    {"auto", SCSPEC, RID_AUTO,},
    {"__label__", LABEL, NORID},
    {""}, {""},
    {"@encode", ENCODE, NORID},
    {""},
    {"const_cast", CONST_CAST, NORID,},
    {"static_cast", STATIC_CAST, NORID,},
    {"@protocol", PROTOCOL, NORID},
    {""},
    {"short", TYPESPEC, RID_SHORT,},
    {"switch", SWITCH, NORID,},
    {"if", IF, NORID,},
    {"and", ANDAND, NORID,},
    {"__alignof", ALIGNOF, NORID},
    {"bitor", '|', NORID,},
    {""},
    {"double", TYPESPEC, RID_DOUBLE,},
    {""},
    {"__sigof__", SIGOF, NORID		/* Extension */,},
    {""},
    {"__null", CONSTANT, RID_NULL},
    {"enum", ENUM, NORID,},
    {""},
    {"@selector", SELECTOR, NORID},
    {"and_eq", ASSIGN, NORID,},
    {"__typeof", TYPEOF, NORID},
    {"@protected", PROTECTED, NORID},
    {"while", WHILE, NORID,},
    {"default", DEFAULT, NORID,},
    {""},
    {"dynamic_cast", DYNAMIC_CAST, NORID,},
    {"continue", CONTINUE, NORID,},
    {""},
    {"@end", END, NORID},
    {"namespace", NAMESPACE, NORID,},
    {"throw", THROW, NORID,},
    {""}, {""}, {""},
    {"virtual", SCSPEC, RID_VIRTUAL,},
    {""},
    {"signed", TYPESPEC, RID_SIGNED,},
    {"__asm", ASM_KEYWORD, NORID},
    {"__signature__", AGGR, RID_SIGNATURE	/* Extension */,},
    {"typedef", SCSPEC, RID_TYPEDEF,},
    {"bycopy", CV_QUALIFIER, RID_BYCOPY},
    {"@private", PRIVATE, NORID},
    {""},
    {"asm", ASM_KEYWORD, NORID,},
    {"false", CXX_FALSE, NORID,},
    {"sizeof", SIZEOF, NORID,},
    {"sigof", SIGOF, NORID		/* Extension */,},
    {"mutable", SCSPEC, RID_MUTABLE,},
    {"vector", TYPESPEC, RID_VECTOR,},
    {""},
    {"union", AGGR, RID_UNION,},
    {""}, {""},
    {"byref", CV_QUALIFIER, RID_BYREF},
    {"new", NEW, NORID,},
    {"private", VISSPEC, RID_PRIVATE,},
    {"vec_step", VEC_STEP, NORID},
    {""},
    {"float", TYPESPEC, RID_FLOAT,},
    {"inout", CV_QUALIFIER, RID_INOUT},
    {"class", AGGR, RID_CLASS,},
    {"void", TYPESPEC, RID_VOID,},
    {"this", THIS, NORID,},
    {""},
    {"return", RETURN_KEYWORD, NORID,},
    {"@interface", INTERFACE, NORID},
    {""}, {""},
    {"using", USING, NORID,},
    {""},
    {"oneway", CV_QUALIFIER, RID_ONEWAY},
    {""}, {""}, {""},
    {"unsigned", TYPESPEC, RID_UNSIGNED,},
    {"break", BREAK, NORID,},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {"@public", PUBLIC, NORID},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {"bitand", '&', NORID,},
    {""},
    {"struct", AGGR, RID_RECORD,},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {""}, {""}, {""}, {""}, {""}, {""},
    {"volatile", CV_QUALIFIER, RID_VOLATILE,},
    {""}, {""}, {""}, {""}, {""}, {""},
    {"@implementation", IMPLEMENTATION, NORID},
    {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {"signature", AGGR, RID_SIGNATURE	/* Extension */,},
    {""},
    {"@defs", DEFS, NORID},
    {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {""}, {""}, {""}, {""},
    {"@compatibility_alias", ALIAS, NORID},
    {""}, {""}, {""}, {""}, {""}, {""}, {""},
    {"@class", CLASS, NORID}
  };

#ifdef __GNUC__
__inline
#endif
struct resword *
is_reserved_word (str, len)
     register const char *str;
     register unsigned int len;
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
