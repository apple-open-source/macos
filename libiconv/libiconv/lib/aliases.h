/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf -m 10 lib/aliases.gperf  */
/* Computed positions: -k'1,3-11,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "lib/aliases.gperf"
struct alias { int name; unsigned int encoding_index; };

#define TOTAL_KEYWORDS 327
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 45
#define MIN_HASH_VALUE 8
#define MAX_HASH_VALUE 874
/* maximum key range = 867, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
aliases_hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      875, 875, 875, 875, 875, 875, 875, 875, 875, 875,
      875, 875, 875, 875, 875, 875, 875, 875, 875, 875,
      875, 875, 875, 875, 875, 875, 875, 875, 875, 875,
      875, 875, 875, 875, 875, 875, 875, 875, 875, 875,
      875, 875, 875, 875, 875,  14, 160, 875,  48,   2,
        3,  17,  62,   7,   5,  52,   9,  15, 266, 875,
      875, 875, 875, 875, 875, 124, 118,   3,  20,  90,
      138,  10, 113,   2, 356, 137,   4, 176,  19,   2,
       14, 875,  46,  14,  25,  94, 159, 113, 167,   4,
        3, 875, 875, 875, 875,  50, 875, 875, 875, 875,
      875, 875, 875, 875, 875, 875, 875, 875, 875, 875,
      875, 875, 875, 875, 875, 875, 875, 875, 875, 875,
      875, 875, 875, 875, 875, 875, 875, 875
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str8[sizeof("L1")];
    char stringpool_str9[sizeof("L2")];
    char stringpool_str11[sizeof("L6")];
    char stringpool_str13[sizeof("L5")];
    char stringpool_str15[sizeof("L8")];
    char stringpool_str18[sizeof("862")];
    char stringpool_str22[sizeof("866")];
    char stringpool_str23[sizeof("L3")];
    char stringpool_str24[sizeof("CN")];
    char stringpool_str25[sizeof("CP1251")];
    char stringpool_str27[sizeof("CP1252")];
    char stringpool_str28[sizeof("CP862")];
    char stringpool_str31[sizeof("CP1256")];
    char stringpool_str32[sizeof("CP866")];
    char stringpool_str35[sizeof("CP1255")];
    char stringpool_str36[sizeof("C99")];
    char stringpool_str37[sizeof("CP1361")];
    char stringpool_str39[sizeof("CP1258")];
    char stringpool_str44[sizeof("GB2312")];
    char stringpool_str46[sizeof("CP932")];
    char stringpool_str48[sizeof("SJIS")];
    char stringpool_str49[sizeof("CP819")];
    char stringpool_str50[sizeof("CP936")];
    char stringpool_str55[sizeof("CP1253")];
    char stringpool_str57[sizeof("R8")];
    char stringpool_str58[sizeof("L7")];
    char stringpool_str60[sizeof("LATIN1")];
    char stringpool_str62[sizeof("LATIN2")];
    char stringpool_str64[sizeof("CP1133")];
    char stringpool_str66[sizeof("LATIN6")];
    char stringpool_str68[sizeof("L4")];
    char stringpool_str70[sizeof("LATIN5")];
    char stringpool_str71[sizeof("ISO8859-1")];
    char stringpool_str73[sizeof("ISO8859-2")];
    char stringpool_str74[sizeof("LATIN8")];
    char stringpool_str75[sizeof("CYRILLIC")];
    char stringpool_str77[sizeof("ISO8859-6")];
    char stringpool_str80[sizeof("ISO8859-16")];
    char stringpool_str81[sizeof("ISO8859-5")];
    char stringpool_str84[sizeof("ISO8859-15")];
    char stringpool_str85[sizeof("ISO8859-8")];
    char stringpool_str86[sizeof("ISO-8859-1")];
    char stringpool_str88[sizeof("ISO-8859-2")];
    char stringpool_str90[sizeof("LATIN3")];
    char stringpool_str92[sizeof("ISO-8859-6")];
    char stringpool_str95[sizeof("ISO-8859-16")];
    char stringpool_str96[sizeof("ISO-8859-5")];
    char stringpool_str97[sizeof("ISO8859-9")];
    char stringpool_str98[sizeof("ISO-IR-6")];
    char stringpool_str99[sizeof("ISO-8859-15")];
    char stringpool_str100[sizeof("ISO-8859-8")];
    char stringpool_str101[sizeof("ISO8859-3")];
    char stringpool_str103[sizeof("UHC")];
    char stringpool_str104[sizeof("ISO8859-13")];
    char stringpool_str105[sizeof("ISO-IR-126")];
    char stringpool_str106[sizeof("ISO-IR-226")];
    char stringpool_str107[sizeof("ISO-IR-166")];
    char stringpool_str108[sizeof("850")];
    char stringpool_str110[sizeof("US")];
    char stringpool_str111[sizeof("ISO-IR-165")];
    char stringpool_str112[sizeof("ISO-8859-9")];
    char stringpool_str114[sizeof("ISO-IR-58")];
    char stringpool_str115[sizeof("CP949")];
    char stringpool_str116[sizeof("ISO-8859-3")];
    char stringpool_str117[sizeof("CP1250")];
    char stringpool_str118[sizeof("HZ")];
    char stringpool_str119[sizeof("ISO-8859-13")];
    char stringpool_str120[sizeof("CP850")];
    char stringpool_str122[sizeof("ISO_8859-1")];
    char stringpool_str124[sizeof("ISO_8859-2")];
    char stringpool_str125[sizeof("CP1257")];
    char stringpool_str126[sizeof("CP950")];
    char stringpool_str127[sizeof("ISO-IR-138")];
    char stringpool_str128[sizeof("ISO_8859-6")];
    char stringpool_str129[sizeof("ISO-IR-159")];
    char stringpool_str130[sizeof("CSISO2022CN")];
    char stringpool_str131[sizeof("ISO_8859-16")];
    char stringpool_str132[sizeof("ISO_8859-5")];
    char stringpool_str133[sizeof("UCS-2")];
    char stringpool_str134[sizeof("CP367")];
    char stringpool_str135[sizeof("ISO_8859-15")];
    char stringpool_str136[sizeof("ISO_8859-8")];
    char stringpool_str137[sizeof("ISO-IR-199")];
    char stringpool_str138[sizeof("ASCII")];
    char stringpool_str139[sizeof("EUCCN")];
    char stringpool_str140[sizeof("ISO646-CN")];
    char stringpool_str141[sizeof("ISO-2022-CN")];
    char stringpool_str142[sizeof("ISO_8859-15:1998")];
    char stringpool_str144[sizeof("ISO-IR-101")];
    char stringpool_str145[sizeof("CP1254")];
    char stringpool_str146[sizeof("BIG5")];
    char stringpool_str148[sizeof("ISO_8859-9")];
    char stringpool_str149[sizeof("TIS620")];
    char stringpool_str151[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str152[sizeof("ISO_8859-3")];
    char stringpool_str153[sizeof("CSBIG5")];
    char stringpool_str154[sizeof("EUC-CN")];
    char stringpool_str155[sizeof("ISO_8859-13")];
    char stringpool_str157[sizeof("CSASCII")];
    char stringpool_str158[sizeof("ISO-CELTIC")];
    char stringpool_str160[sizeof("LATIN7")];
    char stringpool_str161[sizeof("BIG-5")];
    char stringpool_str164[sizeof("TIS-620")];
    char stringpool_str166[sizeof("ISO8859-10")];
    char stringpool_str167[sizeof("CSGB2312")];
    char stringpool_str168[sizeof("CN-BIG5")];
    char stringpool_str170[sizeof("ISO-IR-109")];
    char stringpool_str171[sizeof("ISO8859-7")];
    char stringpool_str172[sizeof("ISO-IR-148")];
    char stringpool_str174[sizeof("ISO-IR-179")];
    char stringpool_str175[sizeof("ISO-IR-203")];
    char stringpool_str177[sizeof("ISO_8859-10:1992")];
    char stringpool_str179[sizeof("ISO_8859-16:2000")];
    char stringpool_str180[sizeof("LATIN4")];
    char stringpool_str181[sizeof("ISO-8859-10")];
    char stringpool_str183[sizeof("X0212")];
    char stringpool_str184[sizeof("ISO-IR-149")];
    char stringpool_str185[sizeof("MAC")];
    char stringpool_str186[sizeof("ISO-8859-7")];
    char stringpool_str188[sizeof("VISCII")];
    char stringpool_str189[sizeof("GB18030")];
    char stringpool_str190[sizeof("ISO-IR-110")];
    char stringpool_str191[sizeof("ISO8859-4")];
    char stringpool_str193[sizeof("CP874")];
    char stringpool_str194[sizeof("ISO8859-14")];
    char stringpool_str195[sizeof("CSVISCII")];
    char stringpool_str197[sizeof("ISO_8859-14:1998")];
    char stringpool_str199[sizeof("ISO-IR-127")];
    char stringpool_str200[sizeof("ISO-IR-57")];
    char stringpool_str202[sizeof("ISO-IR-87")];
    char stringpool_str203[sizeof("ISO-IR-157")];
    char stringpool_str204[sizeof("IBM862")];
    char stringpool_str206[sizeof("ISO-8859-4")];
    char stringpool_str208[sizeof("IBM866")];
    char stringpool_str209[sizeof("ISO-8859-14")];
    char stringpool_str210[sizeof("CSISOLATIN1")];
    char stringpool_str211[sizeof("ELOT_928")];
    char stringpool_str212[sizeof("CSISOLATIN2")];
    char stringpool_str213[sizeof("TIS620-0")];
    char stringpool_str214[sizeof("GB_2312-80")];
    char stringpool_str215[sizeof("ISO-IR-14")];
    char stringpool_str216[sizeof("CSISOLATIN6")];
    char stringpool_str217[sizeof("ISO_8859-10")];
    char stringpool_str218[sizeof("KOI8-T")];
    char stringpool_str219[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str220[sizeof("CSISOLATIN5")];
    char stringpool_str221[sizeof("ISO646-US")];
    char stringpool_str222[sizeof("ISO_8859-7")];
    char stringpool_str223[sizeof("CHAR")];
    char stringpool_str224[sizeof("GB_1988-80")];
    char stringpool_str225[sizeof("IBM819")];
    char stringpool_str226[sizeof("TCVN")];
    char stringpool_str227[sizeof("X0201")];
    char stringpool_str236[sizeof("ISO-IR-100")];
    char stringpool_str240[sizeof("CSISOLATIN3")];
    char stringpool_str241[sizeof("X0208")];
    char stringpool_str242[sizeof("ISO_8859-4")];
    char stringpool_str244[sizeof("CSUCS4")];
    char stringpool_str245[sizeof("ISO_8859-14")];
    char stringpool_str246[sizeof("CN-GB-ISOIR165")];
    char stringpool_str247[sizeof("CSISO57GB1988")];
    char stringpool_str248[sizeof("CSISO58GB231280")];
    char stringpool_str250[sizeof("CSUNICODE11")];
    char stringpool_str251[sizeof("UCS-4")];
    char stringpool_str252[sizeof("CSKOI8R")];
    char stringpool_str254[sizeof("UTF8")];
    char stringpool_str256[sizeof("UNICODE-1-1")];
    char stringpool_str258[sizeof("MS-CYRL")];
    char stringpool_str260[sizeof("KOI8-R")];
    char stringpool_str261[sizeof("MACCYRILLIC")];
    char stringpool_str262[sizeof("KSC_5601")];
    char stringpool_str263[sizeof("US-ASCII")];
    char stringpool_str264[sizeof("UTF-16")];
    char stringpool_str266[sizeof("ISO-10646-UCS-2")];
    char stringpool_str268[sizeof("CN-GB")];
    char stringpool_str269[sizeof("UTF-8")];
    char stringpool_str274[sizeof("IBM-CP1133")];
    char stringpool_str275[sizeof("UTF-32")];
    char stringpool_str278[sizeof("ISO-IR-144")];
    char stringpool_str280[sizeof("GEORGIAN-PS")];
    char stringpool_str287[sizeof("GBK")];
    char stringpool_str293[sizeof("TCVN-5712")];
    char stringpool_str295[sizeof("TCVN5712-1")];
    char stringpool_str296[sizeof("IBM850")];
    char stringpool_str298[sizeof("TIS620.2529-1")];
    char stringpool_str301[sizeof("CSKSC56011987")];
    char stringpool_str304[sizeof("CSUNICODE11UTF7")];
    char stringpool_str307[sizeof("HZ-GB-2312")];
    char stringpool_str310[sizeof("IBM367")];
    char stringpool_str312[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str314[sizeof("TIS620.2533-1")];
    char stringpool_str315[sizeof("CHINESE")];
    char stringpool_str316[sizeof("UCS-2LE")];
    char stringpool_str318[sizeof("CSISO2022KR")];
    char stringpool_str321[sizeof("WINDOWS-1251")];
    char stringpool_str322[sizeof("WINDOWS-1252")];
    char stringpool_str323[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str324[sizeof("WINDOWS-1256")];
    char stringpool_str325[sizeof("ISO-10646-UCS-4")];
    char stringpool_str326[sizeof("WINDOWS-1255")];
    char stringpool_str327[sizeof("EUCKR")];
    char stringpool_str328[sizeof("WINDOWS-1258")];
    char stringpool_str329[sizeof("ISO-2022-KR")];
    char stringpool_str330[sizeof("CSISOLATIN4")];
    char stringpool_str331[sizeof("CSIBM866")];
    char stringpool_str332[sizeof("CSUNICODE")];
    char stringpool_str336[sizeof("WINDOWS-1253")];
    char stringpool_str338[sizeof("CSISOLATINARABIC")];
    char stringpool_str339[sizeof("UCS-2-INTERNAL")];
    char stringpool_str342[sizeof("EUC-KR")];
    char stringpool_str347[sizeof("KS_C_5601-1989")];
    char stringpool_str349[sizeof("EUCTW")];
    char stringpool_str351[sizeof("GREEK8")];
    char stringpool_str355[sizeof("UTF-7")];
    char stringpool_str356[sizeof("KOI8-U")];
    char stringpool_str357[sizeof("CSISOLATINGREEK")];
    char stringpool_str358[sizeof("MS-ANSI")];
    char stringpool_str360[sizeof("TIS620.2533-0")];
    char stringpool_str361[sizeof("UNICODEBIG")];
    char stringpool_str362[sizeof("ARMSCII-8")];
    char stringpool_str364[sizeof("EUC-TW")];
    char stringpool_str367[sizeof("WINDOWS-1250")];
    char stringpool_str370[sizeof("UNICODELITTLE")];
    char stringpool_str371[sizeof("WINDOWS-1257")];
    char stringpool_str372[sizeof("JP")];
    char stringpool_str373[sizeof("VISCII1.1-1")];
    char stringpool_str374[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str375[sizeof("UCS-4LE")];
    char stringpool_str376[sizeof("NEXTSTEP")];
    char stringpool_str380[sizeof("ARABIC")];
    char stringpool_str381[sizeof("WINDOWS-1254")];
    char stringpool_str384[sizeof("KS_C_5601-1987")];
    char stringpool_str389[sizeof("ROMAN8")];
    char stringpool_str398[sizeof("UCS-4-INTERNAL")];
    char stringpool_str403[sizeof("KOI8-RU")];
    char stringpool_str405[sizeof("ISO_8859-5:1988")];
    char stringpool_str406[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str407[sizeof("ISO_8859-8:1988")];
    char stringpool_str415[sizeof("ISO_8859-3:1988")];
    char stringpool_str419[sizeof("ISO_8859-9:1989")];
    char stringpool_str426[sizeof("CSEUCKR")];
    char stringpool_str427[sizeof("MULELAO-1")];
    char stringpool_str430[sizeof("UCS-2BE")];
    char stringpool_str434[sizeof("ECMA-118")];
    char stringpool_str437[sizeof("CSISOLATINHEBREW")];
    char stringpool_str439[sizeof("BIG5HKSCS")];
    char stringpool_str441[sizeof("KOREAN")];
    char stringpool_str442[sizeof("ASMO-708")];
    char stringpool_str443[sizeof("ISO_8859-1:1987")];
    char stringpool_str444[sizeof("ISO_8859-2:1987")];
    char stringpool_str445[sizeof("UTF-16LE")];
    char stringpool_str446[sizeof("ISO_8859-6:1987")];
    char stringpool_str448[sizeof("CSEUCTW")];
    char stringpool_str451[sizeof("UCS-2-SWAPPED")];
    char stringpool_str452[sizeof("MACTHAI")];
    char stringpool_str454[sizeof("BIG5-HKSCS")];
    char stringpool_str458[sizeof("UTF-32LE")];
    char stringpool_str460[sizeof("ISO_8859-4:1988")];
    char stringpool_str463[sizeof("CSISO2022JP2")];
    char stringpool_str465[sizeof("MS-EE")];
    char stringpool_str469[sizeof("GREEK")];
    char stringpool_str471[sizeof("MACICELAND")];
    char stringpool_str473[sizeof("CSISO2022JP")];
    char stringpool_str474[sizeof("ISO-2022-JP-1")];
    char stringpool_str475[sizeof("ISO-2022-JP-2")];
    char stringpool_str476[sizeof("MACINTOSH")];
    char stringpool_str479[sizeof("CSISO14JISC6220RO")];
    char stringpool_str482[sizeof("EUCJP")];
    char stringpool_str483[sizeof("ISO646-JP")];
    char stringpool_str484[sizeof("ISO-2022-JP")];
    char stringpool_str485[sizeof("CSISO159JISX02121990")];
    char stringpool_str486[sizeof("JIS_C6226-1983")];
    char stringpool_str489[sizeof("UCS-4BE")];
    char stringpool_str491[sizeof("WINDOWS-874")];
    char stringpool_str493[sizeof("ISO_8859-7:1987")];
    char stringpool_str494[sizeof("JIS0208")];
    char stringpool_str497[sizeof("EUC-JP")];
    char stringpool_str503[sizeof("WCHAR_T")];
    char stringpool_str510[sizeof("UCS-4-SWAPPED")];
    char stringpool_str511[sizeof("ISO_646.IRV:1991")];
    char stringpool_str517[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str521[sizeof("HP-ROMAN8")];
    char stringpool_str525[sizeof("CSHPROMAN8")];
    char stringpool_str540[sizeof("ECMA-114")];
    char stringpool_str554[sizeof("MACCROATIAN")];
    char stringpool_str559[sizeof("UTF-16BE")];
    char stringpool_str569[sizeof("UTF8-MAC")];
    char stringpool_str572[sizeof("UTF-32BE")];
    char stringpool_str573[sizeof("MACROMAN")];
    char stringpool_str581[sizeof("TCVN5712-1:1993")];
    char stringpool_str584[sizeof("UTF-8-MAC")];
    char stringpool_str588[sizeof("SHIFT-JIS")];
    char stringpool_str599[sizeof("HEBREW")];
    char stringpool_str605[sizeof("CSMACINTOSH")];
    char stringpool_str608[sizeof("MACARABIC")];
    char stringpool_str610[sizeof("MS-HEBR")];
    char stringpool_str614[sizeof("BIGFIVE")];
    char stringpool_str624[sizeof("SHIFT_JIS")];
    char stringpool_str629[sizeof("BIG-FIVE")];
    char stringpool_str631[sizeof("ANSI_X3.4-1986")];
    char stringpool_str635[sizeof("ANSI_X3.4-1968")];
    char stringpool_str636[sizeof("MS-TURK")];
    char stringpool_str645[sizeof("CSISO87JISX0208")];
    char stringpool_str652[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str655[sizeof("JIS_X0212")];
    char stringpool_str686[sizeof("MACCENTRALEUROPE")];
    char stringpool_str687[sizeof("JISX0201-1976")];
    char stringpool_str691[sizeof("CSSHIFTJIS")];
    char stringpool_str697[sizeof("MACGREEK")];
    char stringpool_str699[sizeof("JIS_X0201")];
    char stringpool_str708[sizeof("MS-GREEK")];
    char stringpool_str713[sizeof("JIS_X0208")];
    char stringpool_str721[sizeof("JIS_X0212-1990")];
    char stringpool_str727[sizeof("MS-ARAB")];
    char stringpool_str733[sizeof("MACTURKISH")];
    char stringpool_str742[sizeof("JIS_X0208-1983")];
    char stringpool_str767[sizeof("JAVA")];
    char stringpool_str773[sizeof("JIS_X0208-1990")];
    char stringpool_str791[sizeof("MACUKRAINE")];
    char stringpool_str798[sizeof("CSHALFWIDTHKATAKANA")];
    char stringpool_str806[sizeof("MACROMANIA")];
    char stringpool_str809[sizeof("CSEUCPKDFMTJAPANESE")];
    char stringpool_str813[sizeof("WINBALTRIM")];
    char stringpool_str834[sizeof("JOHAB")];
    char stringpool_str869[sizeof("JIS_X0212.1990-0")];
    char stringpool_str871[sizeof("MACHEBREW")];
    char stringpool_str874[sizeof("MS_KANJI")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "L1",
    "L2",
    "L6",
    "L5",
    "L8",
    "862",
    "866",
    "L3",
    "CN",
    "CP1251",
    "CP1252",
    "CP862",
    "CP1256",
    "CP866",
    "CP1255",
    "C99",
    "CP1361",
    "CP1258",
    "GB2312",
    "CP932",
    "SJIS",
    "CP819",
    "CP936",
    "CP1253",
    "R8",
    "L7",
    "LATIN1",
    "LATIN2",
    "CP1133",
    "LATIN6",
    "L4",
    "LATIN5",
    "ISO8859-1",
    "ISO8859-2",
    "LATIN8",
    "CYRILLIC",
    "ISO8859-6",
    "ISO8859-16",
    "ISO8859-5",
    "ISO8859-15",
    "ISO8859-8",
    "ISO-8859-1",
    "ISO-8859-2",
    "LATIN3",
    "ISO-8859-6",
    "ISO-8859-16",
    "ISO-8859-5",
    "ISO8859-9",
    "ISO-IR-6",
    "ISO-8859-15",
    "ISO-8859-8",
    "ISO8859-3",
    "UHC",
    "ISO8859-13",
    "ISO-IR-126",
    "ISO-IR-226",
    "ISO-IR-166",
    "850",
    "US",
    "ISO-IR-165",
    "ISO-8859-9",
    "ISO-IR-58",
    "CP949",
    "ISO-8859-3",
    "CP1250",
    "HZ",
    "ISO-8859-13",
    "CP850",
    "ISO_8859-1",
    "ISO_8859-2",
    "CP1257",
    "CP950",
    "ISO-IR-138",
    "ISO_8859-6",
    "ISO-IR-159",
    "CSISO2022CN",
    "ISO_8859-16",
    "ISO_8859-5",
    "UCS-2",
    "CP367",
    "ISO_8859-15",
    "ISO_8859-8",
    "ISO-IR-199",
    "ASCII",
    "EUCCN",
    "ISO646-CN",
    "ISO-2022-CN",
    "ISO_8859-15:1998",
    "ISO-IR-101",
    "CP1254",
    "BIG5",
    "ISO_8859-9",
    "TIS620",
    "ISO-2022-CN-EXT",
    "ISO_8859-3",
    "CSBIG5",
    "EUC-CN",
    "ISO_8859-13",
    "CSASCII",
    "ISO-CELTIC",
    "LATIN7",
    "BIG-5",
    "TIS-620",
    "ISO8859-10",
    "CSGB2312",
    "CN-BIG5",
    "ISO-IR-109",
    "ISO8859-7",
    "ISO-IR-148",
    "ISO-IR-179",
    "ISO-IR-203",
    "ISO_8859-10:1992",
    "ISO_8859-16:2000",
    "LATIN4",
    "ISO-8859-10",
    "X0212",
    "ISO-IR-149",
    "MAC",
    "ISO-8859-7",
    "VISCII",
    "GB18030",
    "ISO-IR-110",
    "ISO8859-4",
    "CP874",
    "ISO8859-14",
    "CSVISCII",
    "ISO_8859-14:1998",
    "ISO-IR-127",
    "ISO-IR-57",
    "ISO-IR-87",
    "ISO-IR-157",
    "IBM862",
    "ISO-8859-4",
    "IBM866",
    "ISO-8859-14",
    "CSISOLATIN1",
    "ELOT_928",
    "CSISOLATIN2",
    "TIS620-0",
    "GB_2312-80",
    "ISO-IR-14",
    "CSISOLATIN6",
    "ISO_8859-10",
    "KOI8-T",
    "CSISOLATINCYRILLIC",
    "CSISOLATIN5",
    "ISO646-US",
    "ISO_8859-7",
    "CHAR",
    "GB_1988-80",
    "IBM819",
    "TCVN",
    "X0201",
    "ISO-IR-100",
    "CSISOLATIN3",
    "X0208",
    "ISO_8859-4",
    "CSUCS4",
    "ISO_8859-14",
    "CN-GB-ISOIR165",
    "CSISO57GB1988",
    "CSISO58GB231280",
    "CSUNICODE11",
    "UCS-4",
    "CSKOI8R",
    "UTF8",
    "UNICODE-1-1",
    "MS-CYRL",
    "KOI8-R",
    "MACCYRILLIC",
    "KSC_5601",
    "US-ASCII",
    "UTF-16",
    "ISO-10646-UCS-2",
    "CN-GB",
    "UTF-8",
    "IBM-CP1133",
    "UTF-32",
    "ISO-IR-144",
    "GEORGIAN-PS",
    "GBK",
    "TCVN-5712",
    "TCVN5712-1",
    "IBM850",
    "TIS620.2529-1",
    "CSKSC56011987",
    "CSUNICODE11UTF7",
    "HZ-GB-2312",
    "IBM367",
    "UNICODE-1-1-UTF-7",
    "TIS620.2533-1",
    "CHINESE",
    "UCS-2LE",
    "CSISO2022KR",
    "WINDOWS-1251",
    "WINDOWS-1252",
    "CSPC862LATINHEBREW",
    "WINDOWS-1256",
    "ISO-10646-UCS-4",
    "WINDOWS-1255",
    "EUCKR",
    "WINDOWS-1258",
    "ISO-2022-KR",
    "CSISOLATIN4",
    "CSIBM866",
    "CSUNICODE",
    "WINDOWS-1253",
    "CSISOLATINARABIC",
    "UCS-2-INTERNAL",
    "EUC-KR",
    "KS_C_5601-1989",
    "EUCTW",
    "GREEK8",
    "UTF-7",
    "KOI8-U",
    "CSISOLATINGREEK",
    "MS-ANSI",
    "TIS620.2533-0",
    "UNICODEBIG",
    "ARMSCII-8",
    "EUC-TW",
    "WINDOWS-1250",
    "UNICODELITTLE",
    "WINDOWS-1257",
    "JP",
    "VISCII1.1-1",
    "GEORGIAN-ACADEMY",
    "UCS-4LE",
    "NEXTSTEP",
    "ARABIC",
    "WINDOWS-1254",
    "KS_C_5601-1987",
    "ROMAN8",
    "UCS-4-INTERNAL",
    "KOI8-RU",
    "ISO_8859-5:1988",
    "CSPC850MULTILINGUAL",
    "ISO_8859-8:1988",
    "ISO_8859-3:1988",
    "ISO_8859-9:1989",
    "CSEUCKR",
    "MULELAO-1",
    "UCS-2BE",
    "ECMA-118",
    "CSISOLATINHEBREW",
    "BIG5HKSCS",
    "KOREAN",
    "ASMO-708",
    "ISO_8859-1:1987",
    "ISO_8859-2:1987",
    "UTF-16LE",
    "ISO_8859-6:1987",
    "CSEUCTW",
    "UCS-2-SWAPPED",
    "MACTHAI",
    "BIG5-HKSCS",
    "UTF-32LE",
    "ISO_8859-4:1988",
    "CSISO2022JP2",
    "MS-EE",
    "GREEK",
    "MACICELAND",
    "CSISO2022JP",
    "ISO-2022-JP-1",
    "ISO-2022-JP-2",
    "MACINTOSH",
    "CSISO14JISC6220RO",
    "EUCJP",
    "ISO646-JP",
    "ISO-2022-JP",
    "CSISO159JISX02121990",
    "JIS_C6226-1983",
    "UCS-4BE",
    "WINDOWS-874",
    "ISO_8859-7:1987",
    "JIS0208",
    "EUC-JP",
    "WCHAR_T",
    "UCS-4-SWAPPED",
    "ISO_646.IRV:1991",
    "JIS_C6220-1969-RO",
    "HP-ROMAN8",
    "CSHPROMAN8",
    "ECMA-114",
    "MACCROATIAN",
    "UTF-16BE",
    "UTF8-MAC",
    "UTF-32BE",
    "MACROMAN",
    "TCVN5712-1:1993",
    "UTF-8-MAC",
    "SHIFT-JIS",
    "HEBREW",
    "CSMACINTOSH",
    "MACARABIC",
    "MS-HEBR",
    "BIGFIVE",
    "SHIFT_JIS",
    "BIG-FIVE",
    "ANSI_X3.4-1986",
    "ANSI_X3.4-1968",
    "MS-TURK",
    "CSISO87JISX0208",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "JIS_X0212",
    "MACCENTRALEUROPE",
    "JISX0201-1976",
    "CSSHIFTJIS",
    "MACGREEK",
    "JIS_X0201",
    "MS-GREEK",
    "JIS_X0208",
    "JIS_X0212-1990",
    "MS-ARAB",
    "MACTURKISH",
    "JIS_X0208-1983",
    "JAVA",
    "JIS_X0208-1990",
    "MACUKRAINE",
    "CSHALFWIDTHKATAKANA",
    "MACROMANIA",
    "CSEUCPKDFMTJAPANESE",
    "WINBALTRIM",
    "JOHAB",
    "JIS_X0212.1990-0",
    "MACHEBREW",
    "MS_KANJI"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct alias aliases[] =
  {
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 63 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str8, ei_iso8859_1},
#line 71 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str9, ei_iso8859_2},
    {-1},
#line 136 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str11, ei_iso8859_10},
    {-1},
#line 128 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str13, ei_iso8859_9},
    {-1},
#line 150 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str15, ei_iso8859_14},
    {-1}, {-1},
#line 199 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str18, ei_cp862},
    {-1}, {-1}, {-1},
#line 203 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str22, ei_cp866},
#line 79 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str23, ei_iso8859_3},
#line 274 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str24, ei_iso646_cn},
#line 170 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str25, ei_cp1251},
    {-1},
#line 173 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str27, ei_cp1252},
#line 197 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str28, ei_cp862},
    {-1}, {-1},
#line 185 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str31, ei_cp1256},
#line 201 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str32, ei_cp866},
    {-1}, {-1},
#line 182 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str35, ei_cp1255},
#line 54 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str36, ei_c99},
#line 334 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str37, ei_johab},
    {-1},
#line 191 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str39, ei_cp1258},
    {-1}, {-1}, {-1}, {-1},
#line 305 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str44, ei_euc_cn},
    {-1},
#line 297 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str46, ei_cp932},
    {-1},
#line 294 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str48, ei_sjis},
#line 60 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str49, ei_iso8859_1},
#line 309 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str50, ei_ces_gbk},
    {-1}, {-1}, {-1}, {-1},
#line 176 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str55, ei_cp1253},
    {-1},
#line 222 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str57, ei_hp_roman8},
#line 143 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str58, ei_iso8859_13},
    {-1},
#line 62 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str60, ei_iso8859_1},
    {-1},
#line 70 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str62, ei_iso8859_2},
    {-1},
#line 230 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str64, ei_cp1133},
    {-1},
#line 135 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str66, ei_iso8859_10},
    {-1},
#line 87 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str68, ei_iso8859_4},
    {-1},
#line 127 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str70, ei_iso8859_9},
#line 65 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str71, ei_iso8859_1},
    {-1},
#line 73 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str73, ei_iso8859_2},
#line 149 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str74, ei_iso8859_14},
#line 94 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str75, ei_iso8859_5},
    {-1},
#line 105 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str77, ei_iso8859_6},
    {-1}, {-1},
#line 162 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str80, ei_iso8859_16},
#line 96 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str81, ei_iso8859_5},
    {-1}, {-1},
#line 157 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str84, ei_iso8859_15},
#line 122 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str85, ei_iso8859_8},
#line 56 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str86, ei_iso8859_1},
    {-1},
#line 66 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str88, ei_iso8859_2},
    {-1},
#line 78 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str90, ei_iso8859_3},
    {-1},
#line 97 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str92, ei_iso8859_6},
    {-1}, {-1},
#line 158 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str95, ei_iso8859_16},
#line 90 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str96, ei_iso8859_5},
#line 130 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str97, ei_iso8859_9},
#line 16 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str98, ei_ascii},
#line 153 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str99, ei_iso8859_15},
#line 116 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str100, ei_iso8859_8},
#line 81 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str101, ei_iso8859_3},
    {-1},
#line 332 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str103, ei_cp949},
#line 144 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str104, ei_iso8859_13},
#line 109 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str105, ei_iso8859_7},
#line 161 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str106, ei_iso8859_16},
#line 238 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str107, ei_tis620},
#line 195 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str108, ei_cp850},
    {-1},
#line 21 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str110, ei_ascii},
#line 280 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str111, ei_isoir165},
#line 123 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str112, ei_iso8859_9},
    {-1},
#line 277 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str114, ei_gb2312},
#line 331 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str115, ei_cp949},
#line 74 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str116, ei_iso8859_3},
#line 167 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str117, ei_cp1250},
#line 314 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str118, ei_hz},
#line 139 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str119, ei_iso8859_13},
#line 193 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str120, ei_cp850},
    {-1},
#line 57 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str122, ei_iso8859_1},
    {-1},
#line 67 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str124, ei_iso8859_2},
#line 188 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str125, ei_cp1257},
#line 325 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str126, ei_cp950},
#line 119 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str127, ei_iso8859_8},
#line 98 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str128, ei_iso8859_6},
#line 269 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str129, ei_jisx0212},
#line 312 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str130, ei_iso2022_cn},
#line 159 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str131, ei_iso8859_16},
#line 91 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str132, ei_iso8859_5},
#line 27 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str133, ei_ucs2},
#line 19 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str134, ei_ascii},
#line 154 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str135, ei_iso8859_15},
#line 117 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str136, ei_iso8859_8},
#line 148 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str137, ei_iso8859_14},
#line 13 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str138, ei_ascii},
#line 304 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str139, ei_euc_cn},
#line 272 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str140, ei_iso646_cn},
#line 311 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str141, ei_iso2022_cn},
#line 155 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str142, ei_iso8859_15},
    {-1},
#line 69 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str144, ei_iso8859_2},
#line 179 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str145, ei_cp1254},
#line 319 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str146, ei_ces_big5},
    {-1},
#line 124 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str148, ei_iso8859_9},
#line 233 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str149, ei_tis620},
    {-1},
#line 313 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str151, ei_iso2022_cn_ext},
#line 75 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str152, ei_iso8859_3},
#line 324 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str153, ei_ces_big5},
#line 303 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str154, ei_euc_cn},
#line 140 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str155, ei_iso8859_13},
    {-1},
#line 22 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str157, ei_ascii},
#line 151 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str158, ei_iso8859_14},
    {-1},
#line 142 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str160, ei_iso8859_13},
#line 320 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str161, ei_ces_big5},
    {-1}, {-1},
#line 232 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str164, ei_tis620},
    {-1},
#line 138 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str166, ei_iso8859_10},
#line 307 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str167, ei_euc_cn},
#line 323 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str168, ei_ces_big5},
    {-1},
#line 77 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str170, ei_iso8859_3},
#line 115 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str171, ei_iso8859_7},
#line 126 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str172, ei_iso8859_9},
    {-1},
#line 141 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str174, ei_iso8859_13},
#line 156 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str175, ei_iso8859_15},
    {-1},
#line 133 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str177, ei_iso8859_10},
    {-1},
#line 160 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str179, ei_iso8859_16},
#line 86 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str180, ei_iso8859_4},
#line 131 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str181, ei_iso8859_10},
    {-1},
#line 268 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str183, ei_jisx0212},
#line 285 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str184, ei_ksc5601},
#line 207 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str185, ei_mac_roman},
#line 106 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str186, ei_iso8859_7},
    {-1},
#line 241 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str188, ei_viscii},
#line 310 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str189, ei_gb18030},
#line 85 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str190, ei_iso8859_4},
#line 89 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str191, ei_iso8859_4},
    {-1},
#line 239 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str193, ei_cp874},
#line 152 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str194, ei_iso8859_14},
#line 243 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str195, ei_viscii},
    {-1},
#line 147 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str197, ei_iso8859_14},
    {-1},
#line 100 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str199, ei_iso8859_6},
#line 273 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str200, ei_iso646_cn},
    {-1},
#line 262 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str202, ei_jisx0208},
#line 134 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str203, ei_iso8859_10},
#line 198 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str204, ei_cp862},
    {-1},
#line 82 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str206, ei_iso8859_4},
    {-1},
#line 202 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str208, ei_cp866},
#line 145 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str209, ei_iso8859_14},
#line 64 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str210, ei_iso8859_1},
#line 111 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str211, ei_iso8859_7},
#line 72 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str212, ei_iso8859_2},
#line 234 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str213, ei_tis620},
#line 276 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str214, ei_gb2312},
#line 250 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str215, ei_iso646_jp},
#line 137 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str216, ei_iso8859_10},
#line 132 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str217, ei_iso8859_10},
#line 228 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str218, ei_koi8_t},
#line 95 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str219, ei_iso8859_5},
#line 129 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str220, ei_iso8859_9},
#line 14 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str221, ei_ascii},
#line 107 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str222, ei_iso8859_7},
#line 337 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str223, ei_local_char},
#line 271 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str224, ei_iso646_cn},
#line 61 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str225, ei_iso8859_1},
#line 244 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str226, ei_tcvn},
#line 255 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str227, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 59 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str236, ei_iso8859_1},
    {-1}, {-1}, {-1},
#line 80 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str240, ei_iso8859_3},
#line 261 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str241, ei_jisx0208},
#line 83 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str242, ei_iso8859_4},
    {-1},
#line 38 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str244, ei_ucs4},
#line 146 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str245, ei_iso8859_14},
#line 281 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str246, ei_isoir165},
#line 275 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str247, ei_iso646_cn},
#line 278 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str248, ei_gb2312},
    {-1},
#line 33 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str250, ei_ucs2be},
#line 36 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str251, ei_ucs4},
#line 164 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str252, ei_koi8_r},
    {-1},
#line 24 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str254, ei_utf8},
    {-1},
#line 32 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str256, ei_ucs2be},
    {-1},
#line 172 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str258, ei_cp1251},
    {-1},
#line 163 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str260, ei_koi8_r},
#line 213 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str261, ei_mac_cyrillic},
#line 282 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str262, ei_ksc5601},
#line 12 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str263, ei_ascii},
#line 41 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str264, ei_utf16},
    {-1},
#line 28 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str266, ei_ucs2},
    {-1},
#line 306 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str268, ei_euc_cn},
#line 23 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str269, ei_utf8},
    {-1}, {-1}, {-1}, {-1},
#line 231 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str274, ei_cp1133},
#line 44 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str275, ei_utf32},
    {-1}, {-1},
#line 93 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str278, ei_iso8859_5},
    {-1},
#line 227 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str280, ei_georgian_ps},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 308 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str287, ei_ces_gbk},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 245 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str293, ei_tcvn},
    {-1},
#line 246 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str295, ei_tcvn},
#line 194 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str296, ei_cp850},
    {-1},
#line 235 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str298, ei_tis620},
    {-1}, {-1},
#line 286 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str301, ei_ksc5601},
    {-1}, {-1},
#line 49 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str304, ei_utf7},
    {-1}, {-1},
#line 315 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str307, ei_hz},
    {-1}, {-1},
#line 20 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str310, ei_ascii},
    {-1},
#line 48 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str312, ei_utf7},
    {-1},
#line 237 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str314, ei_tis620},
#line 279 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str315, ei_gb2312},
#line 34 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str316, ei_ucs2le},
    {-1},
#line 336 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str318, ei_iso2022_kr},
    {-1}, {-1},
#line 171 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str321, ei_cp1251},
#line 174 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str322, ei_cp1252},
#line 200 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str323, ei_cp862},
#line 186 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str324, ei_cp1256},
#line 37 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str325, ei_ucs4},
#line 183 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str326, ei_cp1255},
#line 329 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str327, ei_euc_kr},
#line 192 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str328, ei_cp1258},
#line 335 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str329, ei_iso2022_kr},
#line 88 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str330, ei_iso8859_4},
#line 204 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str331, ei_cp866},
#line 29 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str332, ei_ucs2},
    {-1}, {-1}, {-1},
#line 177 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str336, ei_cp1253},
    {-1},
#line 104 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str338, ei_iso8859_6},
#line 50 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str339, ei_ucs2internal},
    {-1}, {-1},
#line 328 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str342, ei_euc_kr},
    {-1}, {-1}, {-1}, {-1},
#line 284 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str347, ei_ksc5601},
    {-1},
#line 317 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str349, ei_euc_tw},
    {-1},
#line 112 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str351, ei_iso8859_7},
    {-1}, {-1}, {-1},
#line 47 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str355, ei_utf7},
#line 165 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str356, ei_koi8_u},
#line 114 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str357, ei_iso8859_7},
#line 175 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str358, ei_cp1252},
    {-1},
#line 236 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str360, ei_tis620},
#line 31 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str361, ei_ucs2be},
#line 225 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str362, ei_armscii_8},
    {-1},
#line 316 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str364, ei_euc_tw},
    {-1}, {-1},
#line 168 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str367, ei_cp1250},
    {-1}, {-1},
#line 35 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str370, ei_ucs2le},
#line 189 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str371, ei_cp1257},
#line 251 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str372, ei_iso646_jp},
#line 242 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str373, ei_viscii},
#line 226 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str374, ei_georgian_academy},
#line 40 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str375, ei_ucs4le},
#line 224 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str376, ei_nextstep},
    {-1}, {-1}, {-1},
#line 103 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str380, ei_iso8859_6},
#line 180 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str381, ei_cp1254},
    {-1}, {-1},
#line 283 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str384, ei_ksc5601},
    {-1}, {-1}, {-1}, {-1},
#line 221 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str389, ei_hp_roman8},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 52 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str398, ei_ucs4internal},
    {-1}, {-1}, {-1}, {-1},
#line 166 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str403, ei_koi8_ru},
    {-1},
#line 92 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str405, ei_iso8859_5},
#line 196 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str406, ei_cp850},
#line 118 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str407, ei_iso8859_8},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 76 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str415, ei_iso8859_3},
    {-1}, {-1}, {-1},
#line 125 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str419, ei_iso8859_9},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 330 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str426, ei_euc_kr},
#line 229 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str427, ei_mulelao},
    {-1}, {-1},
#line 30 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str430, ei_ucs2be},
    {-1}, {-1}, {-1},
#line 110 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str434, ei_iso8859_7},
    {-1}, {-1},
#line 121 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str437, ei_iso8859_8},
    {-1},
#line 327 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str439, ei_big5hkscs},
    {-1},
#line 287 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str441, ei_ksc5601},
#line 102 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str442, ei_iso8859_6},
#line 58 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str443, ei_iso8859_1},
#line 68 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str444, ei_iso8859_2},
#line 43 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str445, ei_utf16le},
#line 99 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str446, ei_iso8859_6},
    {-1},
#line 318 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str448, ei_euc_tw},
    {-1}, {-1},
#line 51 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str451, ei_ucs2swapped},
#line 219 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str452, ei_mac_thai},
    {-1},
#line 326 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str454, ei_big5hkscs},
    {-1}, {-1}, {-1},
#line 46 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str458, ei_utf32le},
    {-1},
#line 84 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str460, ei_iso8859_4},
    {-1}, {-1},
#line 302 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str463, ei_iso2022_jp2},
    {-1},
#line 169 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str465, ei_cp1250},
    {-1}, {-1}, {-1},
#line 113 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str469, ei_iso8859_7},
    {-1},
#line 210 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str471, ei_mac_iceland},
    {-1},
#line 299 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str473, ei_iso2022_jp},
#line 300 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str474, ei_iso2022_jp1},
#line 301 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str475, ei_iso2022_jp2},
#line 206 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str476, ei_mac_roman},
    {-1}, {-1},
#line 252 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str479, ei_iso646_jp},
    {-1}, {-1},
#line 289 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str482, ei_euc_jp},
#line 249 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str483, ei_iso646_jp},
#line 298 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str484, ei_iso2022_jp},
#line 270 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str485, ei_jisx0212},
#line 263 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str486, ei_jisx0208},
    {-1}, {-1},
#line 39 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str489, ei_ucs4be},
    {-1},
#line 240 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str491, ei_cp874},
    {-1},
#line 108 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str493, ei_iso8859_7},
#line 260 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str494, ei_jisx0208},
    {-1}, {-1},
#line 288 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str497, ei_euc_jp},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 338 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str503, ei_local_wchar_t},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 53 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str510, ei_ucs4swapped},
#line 15 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str511, ei_ascii},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 248 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str517, ei_iso646_jp},
    {-1}, {-1}, {-1},
#line 220 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str521, ei_hp_roman8},
    {-1}, {-1}, {-1},
#line 223 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str525, ei_hp_roman8},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 101 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str540, ei_iso8859_6},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1},
#line 211 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str554, ei_mac_croatian},
    {-1}, {-1}, {-1}, {-1},
#line 42 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str559, ei_utf16be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 26 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str569, ei_utf8mac},
    {-1}, {-1},
#line 45 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str572, ei_utf32be},
#line 205 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str573, ei_mac_roman},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 247 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str581, ei_tcvn},
    {-1}, {-1},
#line 25 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str584, ei_utf8mac},
    {-1}, {-1}, {-1},
#line 293 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str588, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 120 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str599, ei_iso8859_8},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 208 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str605, ei_mac_roman},
    {-1}, {-1},
#line 218 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str608, ei_mac_arabic},
    {-1},
#line 184 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str610, ei_cp1255},
    {-1}, {-1}, {-1},
#line 322 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str614, ei_ces_big5},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 292 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str624, ei_sjis},
    {-1}, {-1}, {-1}, {-1},
#line 321 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str629, ei_ces_big5},
    {-1},
#line 18 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str631, ei_ascii},
    {-1}, {-1}, {-1},
#line 17 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str635, ei_ascii},
#line 181 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str636, ei_cp1254},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 264 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str645, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 290 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str652, ei_euc_jp},
    {-1}, {-1},
#line 265 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str655, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 209 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str686, ei_mac_centraleurope},
#line 254 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str687, ei_jisx0201},
    {-1}, {-1}, {-1},
#line 296 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str691, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 215 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str697, ei_mac_greek},
    {-1},
#line 253 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str699, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 178 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str708, ei_cp1253},
    {-1}, {-1}, {-1}, {-1},
#line 257 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str713, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 267 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str721, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 187 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str727, ei_cp1256},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 216 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str733, ei_mac_turkish},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 258 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str742, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 55 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str767, ei_java},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 259 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str773, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 214 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str791, ei_mac_ukraine},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 256 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str798, ei_jisx0201},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 212 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str806, ei_mac_romania},
    {-1}, {-1},
#line 291 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str809, ei_euc_jp},
    {-1}, {-1}, {-1},
#line 190 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str813, ei_cp1257},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1},
#line 333 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str834, ei_johab},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 266 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str869, ei_jisx0212},
    {-1},
#line 217 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str871, ei_mac_hebrew},
    {-1}, {-1},
#line 295 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str874, ei_sjis}
  };

#ifdef __GNUC__
__inline
#endif
const struct alias *
aliases_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = aliases_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int o = aliases[key].name;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &aliases[key];
            }
        }
    }
  return 0;
}
