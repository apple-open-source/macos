
/*  A Bison parser, made from core/parse-asn1.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	BSTRING_SYM	258
#define	HSTRING_SYM	259
#define	CSTRING_SYM	260
#define	UCASEFIRST_IDENT_SYM	261
#define	LCASEFIRST_IDENT_SYM	262
#define	NAMEDMACRO_SYM	263
#define	MACRODEFBODY_SYM	264
#define	BRACEBAL_SYM	265
#define	NUMBER_ERANGE	266
#define	NUMBER_SYM	267
#define	SNACC_ATTRIBUTES	268
#define	DOT_SYM	269
#define	COMMA_SYM	270
#define	LEFTBRACE_SYM	271
#define	RIGHTBRACE_SYM	272
#define	LEFTPAREN_SYM	273
#define	RIGHTPAREN_SYM	274
#define	LEFTBRACKET_SYM	275
#define	RIGHTBRACKET_SYM	276
#define	LESSTHAN_SYM	277
#define	MINUS_SYM	278
#define	GETS_SYM	279
#define	BAR_SYM	280
#define	TAGS_SYM	281
#define	BOOLEAN_SYM	282
#define	INTEGER_SYM	283
#define	BIT_SYM	284
#define	STRING_SYM	285
#define	OCTET_SYM	286
#define	NULL_SYM	287
#define	SEQUENCE_SYM	288
#define	OF_SYM	289
#define	SET_SYM	290
#define	IMPLICIT_SYM	291
#define	CHOICE_SYM	292
#define	ANY_SYM	293
#define	OBJECT_IDENTIFIER_SYM	294
#define	OPTIONAL_SYM	295
#define	DEFAULT_SYM	296
#define	COMPONENTS_SYM	297
#define	UNIVERSAL_SYM	298
#define	APPLICATION_SYM	299
#define	PRIVATE_SYM	300
#define	TRUE_SYM	301
#define	FALSE_SYM	302
#define	BEGIN_SYM	303
#define	END_SYM	304
#define	DEFINITIONS_SYM	305
#define	EXPLICIT_SYM	306
#define	ENUMERATED_SYM	307
#define	EXPORTS_SYM	308
#define	IMPORTS_SYM	309
#define	REAL_SYM	310
#define	INCLUDES_SYM	311
#define	MIN_SYM	312
#define	MAX_SYM	313
#define	SIZE_SYM	314
#define	FROM_SYM	315
#define	WITH_SYM	316
#define	COMPONENT_SYM	317
#define	PRESENT_SYM	318
#define	ABSENT_SYM	319
#define	DEFINED_SYM	320
#define	BY_SYM	321
#define	PLUS_INFINITY_SYM	322
#define	MINUS_INFINITY_SYM	323
#define	SEMI_COLON_SYM	324
#define	IA5STRING_SYM	325
#define	PRINTABLESTRING_SYM	326
#define	NUMERICSTRING_SYM	327
#define	TELETEXSTRING_SYM	328
#define	T61STRING_SYM	329
#define	VIDEOTEXSTRING_SYM	330
#define	VISIBLESTRING_SYM	331
#define	ISO646STRING_SYM	332
#define	GRAPHICSTRING_SYM	333
#define	GENERALSTRING_SYM	334
#define	GENERALIZEDTIME_SYM	335
#define	UTCTIME_SYM	336
#define	EXTERNAL_SYM	337
#define	OBJECTDESCRIPTOR_SYM	338
#define	OPERATION_SYM	339
#define	ARGUMENT_SYM	340
#define	RESULT_SYM	341
#define	ERRORS_SYM	342
#define	LINKED_SYM	343
#define	ERROR_SYM	344
#define	PARAMETER_SYM	345
#define	BIND_SYM	346
#define	BINDERROR_SYM	347
#define	UNBIND_SYM	348
#define	UNBINDERROR_SYM	349
#define	ASE_SYM	350
#define	OPERATIONS_SYM	351
#define	CONSUMERINVOKES_SYM	352
#define	SUPPLIERINVOKES_SYM	353
#define	AC_SYM	354
#define	ASES_SYM	355
#define	REMOTE_SYM	356
#define	INITIATOR_SYM	357
#define	RESPONDER_SYM	358
#define	ABSTRACTSYNTAXES_SYM	359
#define	CONSUMER_SYM	360
#define	EXTENSIONS_SYM	361
#define	CHOSEN_SYM	362
#define	EXTENSION_SYM	363
#define	CRITICAL_SYM	364
#define	FOR_SYM	365
#define	DELIVERY_SYM	366
#define	SUBMISSION_SYM	367
#define	TRANSFER_SYM	368
#define	EXTENSIONATTRIBUTE_SYM	369
#define	TOKEN_SYM	370
#define	TOKENDATA_SYM	371
#define	SECURITYCATEGORY_SYM	372
#define	OBJECT_SYM	373
#define	PORTS_SYM	374
#define	BOXC_SYM	375
#define	BOXS_SYM	376
#define	PORT_SYM	377
#define	ABSTRACTOPS_SYM	378
#define	REFINE_SYM	379
#define	AS_SYM	380
#define	RECURRING_SYM	381
#define	VISIBLE_SYM	382
#define	PAIRED_SYM	383
#define	ABSTRACTBIND_SYM	384
#define	ABSTRACTUNBIND_SYM	385
#define	TO_SYM	386
#define	ABSTRACTERROR_SYM	387
#define	ABSTRACTOPERATION_SYM	388
#define	ALGORITHM_SYM	389
#define	ENCRYPTED_SYM	390
#define	SIGNED_SYM	391
#define	SIGNATURE_SYM	392
#define	PROTECTED_SYM	393
#define	OBJECTTYPE_SYM	394
#define	SYNTAX_SYM	395
#define	ACCESS_SYM	396
#define	STATUS_SYM	397
#define	DESCRIPTION_SYM	398
#define	REFERENCE_SYM	399
#define	INDEX_SYM	400
#define	DEFVAL_SYM	401

#line 66 "core/parse-asn1.y"


#include "snacc.h"

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdio.h>

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "lib-types.h"
#include "snacc-util.h"
#include "exports.h"
#include "parser.h"
#include "lex-stuff.h"

/*
 * smallErrG
 *    used for small errors that should prevent code generation but not
 *    prevent the later error checking passes
 */
int smallErrG = FALSE;

/*
 * firstTimeThroughG
 *    used incase the asn1.lex was compiled with flex in which
 *    case the lexical analyzer must be reset for every ASN.1 file
 *    parsed, except the first
 */
static int firstTimeThroughG = TRUE;

/*
 *  modulePtrG
 *    used to hold the parsed value.  The root of the parse tree.
 */
Module *modulePtrG;


/*
 * oidElmtValDefsG
 *    used to hold integer values that are defined as arc numbers
 *    the modules object identifiers.
 * eg. FOO-MODULE { joint-iso-ccitt dod (2) foo (2) 3 2 } DEFINITIONS ::=
 *     would put dod/2 and foo/2 in the oidElmtValDefsG list
 * Note: only some oid's (modules name/import list module names)
 *       are parsed by the yacc code.  The rest are parsed later
 *       due to ambiguities that arise without type info.
 */
ValueDefList *oidElmtValDefsG = NULL;


/*
 * ApplTag
 *   used to hold APPLICATION tags that have been defined in
 *   a module.  This permits checking for the the error of
 *   using the same APPLICATION tag in 1 module.  The
 *   ApplTags list (appTagsG) is emptied for each module.
 */
typedef struct ApplTag
{
    unsigned long int lineNo;
    unsigned long int tagCode;
    struct ApplTag *next;
} ApplTag;

ApplTag *applTagsG  = NULL;

/*
 * Protos for ApplTag related stuff. These are defined at the
 * end  of this file
 */
void PushApplTag PROTO ((unsigned long int tagCode, unsigned long int lineNo));
void FreeApplTags();



/*
 * the following are globals to simplify disparity between
 * productions and produced data structure
 */

/*
 * these are used in the ValueRange subtype production
 */
static int      valueRangeUpperEndInclusiveG;
static int      valueRangeLowerEndInclusiveG;

/*
 * used to set exports flag in Type/value defs
 * exportListG holds the explicitly exported elements.
 * see SetExports routine in export.c
 */
ExportElmt *exportListG = NULL;
int         exportsParsedG;


/*
 * globals for the APPLICATION-CONTEXT macro productions
 */
static ValueList *rosAcSymmetricAsesG;
static ValueList *rosAcResponderConsumerOfG;
static ValueList *rosAcInitiatorConsumerOfG;

/*
 * used with MTSAS Extension macro
 * set to NULL for the initial parse.
 */
static AsnBool *mtsasCriticalForSubmissionG = NULL;
static AsnBool *mtsasCriticalForTransferG = NULL;
static AsnBool *mtsasCriticalForDeliveryG = NULL;

/*
 * Asn PORT macro globals
 */
static TypeOrValueList *asnConsumerG;
static TypeOrValueList *asnSupplierG;


/*
 * parseErrCountG
 *   used to prevent too many cascade errors
 */
int parseErrCountG = 0;
#define MAX_ERR 50
#define PARSE_ERROR()\
    parseErrCountG++;\
    modulePtrG->status = MOD_ERROR;\
    if (parseErrCountG > MAX_ERR)\
    {\
        fprintf (stderr, "Ackkkkk! too many errors - bye!\n");\
        exit (1);\
    }



#line 212 "core/parse-asn1.y"
typedef union
{
    int              intVal;
    unsigned int     uintVal;
    char            *charPtr;
    Type            *typePtr;
    NamedType       *namedTypePtr;
    NamedTypeList   *namedTypeListPtr;
    Value           *valuePtr;
    NamedValue      *namedValuePtr;
    SubtypeValue    *subtypeValuePtr;
    Subtype         *subtypePtr;
    ModuleId        *moduleId;
    OID             *oidPtr;
    OidList         *oidListPtr;
    TypeDef         *typeDefPtr;
    TypeDefList     *typeDefListPtr;
    ValueDef        *valueDefPtr;
    ValueDefList    *valueDefListPtr;
    ExportElmt      *exportList;
    ImportModule    *importModulePtr;
    ImportModuleList *importModuleListPtr;
    ImportElmt      *importElmtPtr;
    ImportElmtList  *importElmtListPtr;
    Tag             *tagPtr;
    TagList         *tagListPtr;
    Constraint      *constraintPtr;
    ConstraintList  *constraintListPtr;
    InnerSubtype    *innerSubtypePtr;
    ValueList       *valueListPtr;
    TypeOrValueList *typeOrValueListPtr;
    TypeOrValue     *typeOrValuePtr;
    AsnPort         *asnPortPtr;
    AsnPortList     *asnPortListPtr;
    AttributeList   *attrList;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		671
#define	YYFLAG		-32768
#define	YYNTBASE	147

#define YYTRANSLATE(x) ((unsigned)(x) <= 401 ? yytranslate[x] : 334)

static const short yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
    86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
    96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
   106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
   116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
   126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
   136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
   146
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     2,    11,    14,    17,    19,    22,    24,    26,
    30,    32,    36,    40,    42,    44,    46,    48,    53,    57,
    61,    63,    65,    67,    70,    72,    77,    81,    83,    85,
    87,    89,    92,    94,    96,    99,   102,   104,   106,   107,
   114,   118,   124,   126,   128,   135,   140,   142,   144,   146,
   148,   150,   152,   154,   156,   158,   160,   162,   164,   166,
   168,   170,   172,   174,   176,   178,   180,   182,   185,   188,
   190,   192,   194,   199,   201,   205,   210,   215,   217,   219,
   222,   225,   230,   232,   235,   241,   243,   245,   249,   253,
   256,   259,   261,   267,   269,   272,   276,   280,   285,   289,
   293,   297,   300,   304,   310,   313,   315,   320,   324,   327,
   331,   335,   340,   342,   344,   346,   348,   350,   352,   354,
   359,   361,   364,   369,   374,   378,   380,   384,   386,   388,
   390,   392,   394,   396,   398,   401,   406,   408,   411,   413,
   416,   418,   420,   422,   424,   427,   430,   434,   438,   440,
   442,   444,   448,   456,   458,   462,   465,   467,   470,   472,
   474,   476,   478,   480,   482,   488,   490,   492,   494,   496,
   501,   503,   505,   507,   509,   511,   513,   515,   516,   520,
   522,   524,   526,   528,   530,   532,   535,   539,   542,   544,
   546,   548,   550,   552,   554,   559,   564,   566,   568,   570,
   572,   574,   576,   578,   580,   581,   583,   585,   587,   590,
   592,   594,   596,   598,   600,   602,   604,   606,   608,   610,
   612,   614,   616,   618,   620,   622,   624,   626,   628,   630,
   632,   634,   636,   638,   640,   642,   644,   646,   648,   650,
   652,   654,   656,   658,   660,   662,   664,   666,   668,   670,
   672,   674,   676,   678,   680,   682,   684,   686,   688,   690,
   693,   698,   701,   703,   706,   708,   710,   712,   717,   719,
   724,   726,   729,   732,   734,   739,   742,   744,   747,   749,
   752,   754,   759,   762,   764,   767,   771,   776,   781,   783,
   788,   790,   792,   801,   806,   814,   816,   822,   824,   827,
   834,   836,   843,   845,   850,   852,   854,   858,   865,   867,
   869,   871,   875,   877,   879,   881,   885,   887,   889,   894,
   896,   899,   901,   905,   907,   909,   913,   915,   917,   919,
   921,   924,   926,   929,   931,   934,   936,   939,   942,   947,
   949,   951,   955,   958,   960,   962,   964,   967,   969,   974,
   976,   978,   981,   984,   989,   994,   999,  1001,  1005,  1008,
  1010,  1013,  1015,  1019,  1023,  1025,  1029,  1031,  1035,  1037,
  1040,  1044,  1049,  1051,  1054,  1058,  1063,  1065,  1068,  1071,
  1075,  1078,  1081,  1084,  1087,  1099,  1101,  1103,  1106,  1107,
  1110,  1111,  1116,  1117,  1122
};

static const short yyrhs[] = {    -1,
     0,   151,    50,   150,   149,    24,    48,   153,    49,     0,
    51,    26,     0,    36,    26,     0,   249,     0,   247,   152,
     0,   236,     0,   249,     0,   154,   157,   163,     0,   249,
     0,    53,   155,    69,     0,    53,     1,    69,     0,   249,
     0,   156,     0,   249,     0,   162,     0,   156,    15,   147,
   162,     0,    54,   158,    69,     0,    54,     1,    69,     0,
   249,     0,   159,     0,   249,     0,   159,   160,     0,   160,
     0,   161,    60,   147,   151,     0,   161,    15,   162,     0,
   162,     0,   248,     0,   246,     0,   253,     0,   163,   164,
     0,   164,     0,   165,     0,   165,    69,     0,     1,    69,
     0,   168,     0,   226,     0,     0,     8,    24,    48,   147,
   166,     9,     0,     8,    24,   167,     0,     8,    24,   247,
    14,   167,     0,   248,     0,   253,     0,   248,    24,   250,
   147,   171,   250,     0,   247,    14,   147,   248,     0,   169,
     0,   248,     0,   252,     0,   172,     0,   170,     0,   203,
     0,   174,     0,   175,     0,   181,     0,   183,     0,   185,
     0,   189,     0,   191,     0,   192,     0,   193,     0,   196,
     0,   197,     0,   201,     0,   202,     0,   179,     0,   180,
     0,    31,    30,     0,   246,   171,     0,   171,     0,    27,
     0,    28,     0,    28,    16,   176,    17,     0,   177,     0,
   176,    15,   177,     0,   246,    18,   178,    19,     0,   246,
    18,   228,    19,     0,    12,     0,    11,     0,    23,    12,
     0,    23,    11,     0,    52,    16,   176,    17,     0,    55,
     0,    29,    30,     0,    29,    30,    16,   182,    17,     0,
   176,     0,    32,     0,    33,   147,    16,     0,   184,   186,
    17,     0,   184,    17,     0,   187,   250,     0,   188,     0,
   187,    15,   250,   147,   188,     0,   173,     0,   173,    40,
     0,   173,    41,   235,     0,    42,    34,   171,     0,   246,
    42,    34,   171,     0,    33,    34,   171,     0,    35,   147,
    16,     0,   190,   186,    17,     0,   190,    17,     0,    35,
    34,   171,     0,    37,   147,    16,   194,    17,     0,   195,
   250,     0,   173,     0,   195,    15,   250,   173,     0,   246,
    22,   171,     0,   198,   171,     0,   198,    36,   171,     0,
   198,    51,   171,     0,    20,   200,   199,    21,     0,   245,
     0,   228,     0,    43,     0,    44,     0,    45,     0,   249,
     0,    38,     0,    38,    65,    66,   246,     0,    39,     0,
   171,   204,     0,    35,   214,    34,   171,     0,    33,   214,
    34,   171,     0,    18,   205,    19,     0,   206,     0,   205,
    25,   206,     0,   207,     0,   208,     0,   209,     0,   215,
     0,   214,     0,   216,     0,   227,     0,    56,   171,     0,
   210,    14,    14,   211,     0,   212,     0,   212,    22,     0,
   213,     0,    22,   213,     0,   227,     0,    57,     0,   227,
     0,    58,     0,    59,   204,     0,    60,   204,     0,    61,
    62,   217,     0,    61,    42,   218,     0,   204,     0,   219,
     0,   220,     0,    16,   221,    17,     0,    16,    14,    14,
    14,    15,   221,    17,     0,   222,     0,   221,    15,   222,
     0,   246,   223,     0,   223,     0,   224,   225,     0,   204,
     0,   249,     0,    63,     0,    64,     0,   249,     0,    40,
     0,   246,   171,    24,   147,   227,     0,   230,     0,   228,
     0,   229,     0,   246,     0,   247,    14,   147,   246,     0,
   232,     0,   234,     0,   233,     0,   178,     0,   243,     0,
   242,     0,   244,     0,     0,    16,   231,    10,     0,    46,
     0,    47,     0,    67,     0,    68,     0,    32,     0,   227,
     0,   246,   227,     0,    16,   237,    17,     0,   237,   238,
     0,   238,     0,   239,     0,   240,     0,   241,     0,   245,
     0,   246,     0,   246,    18,   239,    19,     0,   246,    18,
   228,    19,     0,     3,     0,     4,     0,     5,     0,    12,
     0,    11,     0,     7,     0,     6,     0,     6,     0,     0,
   251,     0,   249,     0,    13,     0,   251,    13,     0,   254,
     0,   261,     0,   263,     0,   267,     0,   269,     0,   274,
     0,   289,     0,   283,     0,   294,     0,   295,     0,   296,
     0,   297,     0,   298,     0,   303,     0,   307,     0,   316,
     0,   318,     0,   320,     0,   321,     0,   322,     0,   323,
     0,   326,     0,   325,     0,   324,     0,   327,     0,    84,
     0,    89,     0,    91,     0,    93,     0,    95,     0,    99,
     0,   108,     0,   106,     0,   114,     0,   115,     0,   116,
     0,   117,     0,   118,     0,   122,     0,   124,     0,   129,
     0,   130,     0,   133,     0,   132,     0,   134,     0,   135,
     0,   136,     0,   137,     0,   138,     0,   139,     0,    84,
   255,     0,   256,   257,   259,   260,     0,    85,   173,     0,
   249,     0,    86,   258,     0,   249,     0,   173,     0,   249,
     0,    87,    16,   286,    17,     0,   249,     0,    88,    16,
   286,    17,     0,   249,     0,    89,   262,     0,    90,   173,
     0,   249,     0,    91,   264,   265,   266,     0,    85,   173,
     0,   249,     0,    86,   173,     0,   249,     0,    92,   173,
     0,   249,     0,    93,   264,   265,   268,     0,    94,   173,
     0,   249,     0,    95,   270,     0,    95,   271,   272,     0,
    96,    16,   273,    17,     0,    97,    16,   273,    17,     0,
   249,     0,    98,    16,   273,    17,     0,   249,     0,   285,
     0,    99,   275,    91,   171,    93,   171,   276,   281,     0,
   100,    16,   285,    17,     0,   101,    96,    16,   227,    17,
   277,   278,     0,   249,     0,    96,    34,    16,   285,    17,
     0,   249,     0,   279,   280,     0,   102,   105,    34,    16,
   285,    17,     0,   249,     0,   103,   105,    34,    16,   285,
    17,     0,   249,     0,   104,    16,   282,    17,     0,   249,
     0,   236,     0,   282,    15,   236,     0,   106,   107,    60,
    16,   284,    17,     0,   285,     0,   249,     0,   227,     0,
   285,    15,   227,     0,   287,     0,   249,     0,   288,     0,
   287,    15,   288,     0,   171,     0,   227,     0,   108,   173,
   290,   291,     0,   108,     0,    41,   227,     0,   249,     0,
   109,   110,   292,     0,   249,     0,   293,     0,   292,    15,
   293,     0,   112,     0,   113,     0,   111,     0,   114,     0,
   114,   171,     0,   115,     0,   115,   171,     0,   116,     0,
   116,   171,     0,   117,     0,   117,   171,     0,   118,   299,
     0,   119,    16,   300,    17,     0,   249,     0,   301,     0,
   300,    15,   301,     0,   227,   302,     0,   120,     0,   121,
     0,   249,     0,   122,   304,     0,   122,     0,   123,    16,
   287,    17,     0,   305,     0,   306,     0,   305,   306,     0,
   306,   305,     0,    97,    16,   287,    17,     0,    98,    16,
   287,    17,     0,   124,   315,   125,   308,     0,   309,     0,
   308,    15,   309,     0,   310,   311,     0,   315,     0,   315,
   126,     0,   312,     0,   311,    15,   312,     0,   227,   302,
   313,     0,   127,     0,   128,    61,   314,     0,   315,     0,
   314,    15,   315,     0,   227,     0,   129,   317,     0,   129,
   317,   171,     0,   131,    16,   300,    17,     0,   249,     0,
   130,   319,     0,   130,   319,   171,     0,    60,    16,   300,
    17,     0,   249,     0,   133,   255,     0,   132,   262,     0,
   134,    90,   171,     0,   135,   171,     0,   136,   171,     0,
   137,   171,     0,   138,   171,     0,   139,   140,   171,   141,
   328,   142,   329,   330,   331,   332,   333,     0,   246,     0,
   246,     0,   143,   227,     0,     0,   144,   227,     0,     0,
   145,    16,   287,    17,     0,     0,   146,    16,   227,    17,
     0,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   461,   463,   468,   500,   502,   503,   510,   519,   521,   524,
   526,   529,   540,   547,   550,   552,   555,   563,   572,   574,
   579,   582,   584,   587,   592,   599,   610,   621,   634,   636,
   637,   650,   652,   655,   657,   658,   665,   680,   693,   695,
   734,   750,   775,   777,   784,   805,   820,   822,   832,   834,
   835,   836,   839,   841,   842,   843,   844,   845,   846,   847,
   848,   849,   850,   851,   852,   853,   854,   855,   861,   868,
   875,   882,   888,   896,   902,   909,   917,   925,   933,   939,
   950,   958,   967,   974,   980,   987,   993,  1000,  1005,  1021,
  1035,  1049,  1055,  1072,  1074,  1079,  1093,  1099,  1110,  1126,
  1130,  1145,  1160,  1174,  1191,  1204,  1210,  1225,  1240,  1266,
  1307,  1316,  1334,  1340,  1348,  1350,  1351,  1352,  1356,  1361,
  1370,  1378,  1388,  1401,  1417,  1424,  1440,  1452,  1454,  1455,
  1456,  1457,  1458,  1461,  1469,  1478,  1493,  1499,  1506,  1512,
  1519,  1521,  1528,  1530,  1537,  1547,  1556,  1563,  1571,  1586,
  1588,  1591,  1600,  1610,  1616,  1623,  1629,  1633,  1642,  1644,
  1647,  1649,  1650,  1651,  1663,  1675,  1677,  1680,  1682,  1695,
  1710,  1712,  1713,  1714,  1720,  1728,  1736,  1744,  1745,  1761,
  1768,  1777,  1784,  1794,  1804,  1810,  1819,  1846,  1856,  1861,
  1863,  1884,  1888,  1896,  1901,  1920,  1941,  1945,  1949,  1953,
  1961,  1969,  1973,  1977,  1981,  1988,  1990,  1993,  1999,  2010,
  2012,  2013,  2014,  2015,  2016,  2017,  2018,  2019,  2020,  2021,
  2022,  2023,  2024,  2025,  2026,  2027,  2028,  2029,  2030,  2031,
  2032,  2033,  2034,  2035,  2038,  2040,  2041,  2042,  2043,  2044,
  2045,  2046,  2047,  2048,  2049,  2050,  2051,  2052,  2053,  2054,
  2055,  2056,  2057,  2058,  2059,  2060,  2061,  2062,  2063,  2071,
  2075,  2091,  2093,  2096,  2098,  2102,  2104,  2108,  2113,  2118,
  2123,  2134,  2148,  2150,  2158,  2172,  2174,  2178,  2180,  2184,
  2186,  2194,  2209,  2211,  2219,  2228,  2240,  2248,  2253,  2257,
  2262,  2266,  2275,  2299,  2307,  2313,  2322,  2327,  2330,  2334,
  2339,  2342,  2347,  2350,  2355,  2359,  2365,  2377,  2391,  2393,
  2396,  2402,  2409,  2411,  2414,  2420,  2427,  2434,  2446,  2464,
  2476,  2478,  2481,  2483,  2487,  2489,  2492,  2498,  2503,  2516,
  2526,  2541,  2550,  2564,  2574,  2589,  2599,  2620,  2630,  2635,
  2638,  2644,  2651,  2660,  2666,  2671,  2683,  2694,  2702,  2707,
  2713,  2719,  2725,  2733,  2740,  2756,  2763,  2765,  2768,  2772,
  2774,  2777,  2779,  2782,  2789,  2791,  2795,  2797,  2800,  2813,
  2823,  2835,  2840,  2849,  2860,  2873,  2878,  2886,  2898,  2911,
  2922,  2934,  2945,  2959,  2969,  2995,  3017,  3038,  3040,  3043,
  3045,  3048,  3053,  3056,  3061
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","BSTRING_SYM",
"HSTRING_SYM","CSTRING_SYM","UCASEFIRST_IDENT_SYM","LCASEFIRST_IDENT_SYM","NAMEDMACRO_SYM",
"MACRODEFBODY_SYM","BRACEBAL_SYM","NUMBER_ERANGE","NUMBER_SYM","SNACC_ATTRIBUTES",
"DOT_SYM","COMMA_SYM","LEFTBRACE_SYM","RIGHTBRACE_SYM","LEFTPAREN_SYM","RIGHTPAREN_SYM",
"LEFTBRACKET_SYM","RIGHTBRACKET_SYM","LESSTHAN_SYM","MINUS_SYM","GETS_SYM","BAR_SYM",
"TAGS_SYM","BOOLEAN_SYM","INTEGER_SYM","BIT_SYM","STRING_SYM","OCTET_SYM","NULL_SYM",
"SEQUENCE_SYM","OF_SYM","SET_SYM","IMPLICIT_SYM","CHOICE_SYM","ANY_SYM","OBJECT_IDENTIFIER_SYM",
"OPTIONAL_SYM","DEFAULT_SYM","COMPONENTS_SYM","UNIVERSAL_SYM","APPLICATION_SYM",
"PRIVATE_SYM","TRUE_SYM","FALSE_SYM","BEGIN_SYM","END_SYM","DEFINITIONS_SYM",
"EXPLICIT_SYM","ENUMERATED_SYM","EXPORTS_SYM","IMPORTS_SYM","REAL_SYM","INCLUDES_SYM",
"MIN_SYM","MAX_SYM","SIZE_SYM","FROM_SYM","WITH_SYM","COMPONENT_SYM","PRESENT_SYM",
"ABSENT_SYM","DEFINED_SYM","BY_SYM","PLUS_INFINITY_SYM","MINUS_INFINITY_SYM",
"SEMI_COLON_SYM","IA5STRING_SYM","PRINTABLESTRING_SYM","NUMERICSTRING_SYM","TELETEXSTRING_SYM",
"T61STRING_SYM","VIDEOTEXSTRING_SYM","VISIBLESTRING_SYM","ISO646STRING_SYM",
"GRAPHICSTRING_SYM","GENERALSTRING_SYM","GENERALIZEDTIME_SYM","UTCTIME_SYM",
"EXTERNAL_SYM","OBJECTDESCRIPTOR_SYM","OPERATION_SYM","ARGUMENT_SYM","RESULT_SYM",
"ERRORS_SYM","LINKED_SYM","ERROR_SYM","PARAMETER_SYM","BIND_SYM","BINDERROR_SYM",
"UNBIND_SYM","UNBINDERROR_SYM","ASE_SYM","OPERATIONS_SYM","CONSUMERINVOKES_SYM",
"SUPPLIERINVOKES_SYM","AC_SYM","ASES_SYM","REMOTE_SYM","INITIATOR_SYM","RESPONDER_SYM",
"ABSTRACTSYNTAXES_SYM","CONSUMER_SYM","EXTENSIONS_SYM","CHOSEN_SYM","EXTENSION_SYM",
"CRITICAL_SYM","FOR_SYM","DELIVERY_SYM","SUBMISSION_SYM","TRANSFER_SYM","EXTENSIONATTRIBUTE_SYM",
"TOKEN_SYM","TOKENDATA_SYM","SECURITYCATEGORY_SYM","OBJECT_SYM","PORTS_SYM",
"BOXC_SYM","BOXS_SYM","PORT_SYM","ABSTRACTOPS_SYM","REFINE_SYM","AS_SYM","RECURRING_SYM",
"VISIBLE_SYM","PAIRED_SYM","ABSTRACTBIND_SYM","ABSTRACTUNBIND_SYM","TO_SYM",
"ABSTRACTERROR_SYM","ABSTRACTOPERATION_SYM","ALGORITHM_SYM","ENCRYPTED_SYM",
"SIGNED_SYM","SIGNATURE_SYM","PROTECTED_SYM","OBJECTTYPE_SYM","SYNTAX_SYM","ACCESS_SYM",
"STATUS_SYM","DESCRIPTION_SYM","REFERENCE_SYM","INDEX_SYM","DEFVAL_SYM","LineNo",
"ModuleDefinition","@1","TagDefault","ModuleIdentifier","AssignedIdentifier",
"ModuleBody","Exports","SymbolsExported","ExportSymbolList","Imports","SymbolsImported",
"SymbolsFromModuleList","SymbolsFromModule","SymbolList","Symbol","AssignmentList",
"AssignmentOrError","Assignment","@2","MacroReference","TypeAssignment","ExternalTypeReference",
"DefinedType","Type","BuiltinType","NamedType","BooleanType","IntegerType","NamedNumberList",
"NamedNumber","SignedNumber","EnumeratedType","RealType","BitStringType","NamedBitList",
"NullType","SequenceOpening","SequenceType","ElementTypes","ElementTypeList",
"ElementType","SequenceOfType","SetOpening","SetType","SetOfType","ChoiceType",
"AlternativeTypes","AlternativeTypeList","SelectionType","TaggedType","Tag",
"ClassNumber","Class","AnyType","ObjectIdentifierType","Subtype","SubtypeSpec",
"SubtypeValueSetList","SubtypeValueSet","SingleValue","ContainedSubtype","ValueRange",
"LowerEndPoint","UpperEndPoint","LowerEndValue","UpperEndValue","SizeConstraint",
"PermittedAlphabet","InnerTypeConstraints","SingleTypeConstraint","MultipleTypeConstraints",
"FullSpecification","PartialSpecification","TypeConstraints","NamedConstraint",
"Constraint","ValueConstraint","PresenceConstraint","ValueAssignment","Value",
"DefinedValue","ExternalValueReference","BuiltinValue","@3","BooleanValue","SpecialRealValue",
"NullValue","NamedValue","ObjectIdentifierValue","ObjIdComponentList","ObjIdComponent",
"NumberForm","NameForm","NameAndNumberForm","BinaryString","HexString","CharString",
"number","identifier","modulereference","typereference","empty","SnaccAttributes",
"SnaccAttributeCommentList","DefinedMacroType","DefinedMacroName","RosOperationMacroType",
"RosOperationMacroBody","RosOpArgument","RosOpResult","RosOpResultType","RosOpErrors",
"RosOpLinkedOps","RosErrorMacroType","RosErrParameter","RosBindMacroType","RosBindArgument",
"RosBindResult","RosBindError","RosUnbindMacroType","RosUnbindError","RosAseMacroType",
"RosAseSymmetricAse","RosAseConsumerInvokes","RosAseSupplierInvokes","RosAseOperationList",
"RosAcMacroType","RosAcNonRoElements","RosAcRoElements","RosAcSymmetricAses",
"RosAcAsymmetricAses","RosAcInitiatorConsumerOf","RosAcResponderConsumerOf",
"RosAcAbstractSyntaxes","OidList","MtsasExtensionsMacroType","PossiblyEmptyValueList",
"ValueList","PossiblyEmptyTypeOrValueList","TypeOrValueList","TypeOrValue","MtsasExtensionMacroType",
"MtsasExtDefaultVal","MtsasExtCritical","MtsasExtCriticalityList","MtsasExtCriticality",
"MtsasExtensionAttributeMacroType","MtsasTokenMacroType","MtsasTokenDataMacroType",
"MtsasSecurityCategoryMacroType","AsnObjectMacroType","AsnPorts","AsnPortList",
"AsnPort","AsnPortType","AsnPortMacroType","AsnOperations","AsnConsumer","AsnSupplier",
"AsnRefineMacroType","AsnComponentList","AsnComponent","AsnObjectSpec","AsnPortSpecList",
"AsnPortSpec","AsnPortStatus","AsnObjectList","AsnObject","AsnAbstractBindMacroType",
"AsnAbstractBindPorts","AsnAbstractUnbindMacroType","AsnAbstractUnbindPorts",
"AsnAbstractOperationMacroType","AsnAbstractErrorMacroType","AfAlgorithmMacroType",
"AfEncryptedMacroType","AfSignedMacroType","AfSignatureMacroType","AfProtectedMacroType",
"SnmpObjectTypeMacroType","SnmpAccess","SnmpStatus","SnmpDescrPart","SnmpReferPart",
"SnmpIndexPart","SnmpDefValPart", NULL
};
#endif

static const short yyr1[] = {     0,
   147,   149,   148,   150,   150,   150,   151,   152,   152,   153,
   153,   154,   154,   154,   155,   155,   156,   156,   157,   157,
   157,   158,   158,   159,   159,   160,   161,   161,   162,   162,
   162,   163,   163,   164,   164,   164,   165,   165,   166,   165,
   165,   165,   167,   167,   168,   169,   170,   170,   171,   171,
   171,   171,   172,   172,   172,   172,   172,   172,   172,   172,
   172,   172,   172,   172,   172,   172,   172,   172,   173,   173,
   174,   175,   175,   176,   176,   177,   177,   178,   178,   178,
   178,   179,   180,   181,   181,   182,   183,   184,   185,   185,
   186,   187,   187,   188,   188,   188,   188,   188,   189,   190,
   191,   191,   192,   193,   194,   195,   195,   196,   197,   197,
   197,   198,   199,   199,   200,   200,   200,   200,   201,   201,
   202,   203,   203,   203,   204,   205,   205,   206,   206,   206,
   206,   206,   206,   207,   208,   209,   210,   210,   211,   211,
   212,   212,   213,   213,   214,   215,   216,   216,   217,   218,
   218,   219,   220,   221,   221,   222,   222,   223,   224,   224,
   225,   225,   225,   225,   226,   227,   227,   228,   228,   229,
   230,   230,   230,   230,   230,   230,   230,   231,   230,   232,
   232,   233,   233,   234,   235,   235,   236,   237,   237,   238,
   238,   238,   239,   240,   241,   241,   242,   243,   244,   245,
   245,   246,   247,   248,   249,   250,   250,   251,   251,   252,
   252,   252,   252,   252,   252,   252,   252,   252,   252,   252,
   252,   252,   252,   252,   252,   252,   252,   252,   252,   252,
   252,   252,   252,   252,   253,   253,   253,   253,   253,   253,
   253,   253,   253,   253,   253,   253,   253,   253,   253,   253,
   253,   253,   253,   253,   253,   253,   253,   253,   253,   254,
   255,   256,   256,   257,   257,   258,   258,   259,   259,   260,
   260,   261,   262,   262,   263,   264,   264,   265,   265,   266,
   266,   267,   268,   268,   269,   269,   270,   271,   271,   272,
   272,   273,   274,   275,   276,   276,   277,   277,   278,   279,
   279,   280,   280,   281,   281,   282,   282,   283,   284,   284,
   285,   285,   286,   286,   287,   287,   288,   288,   289,   289,
   290,   290,   291,   291,   292,   292,   293,   293,   293,   294,
   294,   295,   295,   296,   296,   297,   297,   298,   299,   299,
   300,   300,   301,   302,   302,   302,   303,   303,   304,   304,
   304,   304,   304,   305,   306,   307,   308,   308,   309,   310,
   310,   311,   311,   312,   313,   313,   314,   314,   315,   316,
   316,   317,   317,   318,   318,   319,   319,   320,   321,   322,
   323,   324,   325,   326,   327,   328,   329,   330,   330,   331,
   331,   332,   332,   333,   333
};

static const short yyr2[] = {     0,
     0,     0,     8,     2,     2,     1,     2,     1,     1,     3,
     1,     3,     3,     1,     1,     1,     1,     4,     3,     3,
     1,     1,     1,     2,     1,     4,     3,     1,     1,     1,
     1,     2,     1,     1,     2,     2,     1,     1,     0,     6,
     3,     5,     1,     1,     6,     4,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     2,     2,     1,
     1,     1,     4,     1,     3,     4,     4,     1,     1,     2,
     2,     4,     1,     2,     5,     1,     1,     3,     3,     2,
     2,     1,     5,     1,     2,     3,     3,     4,     3,     3,
     3,     2,     3,     5,     2,     1,     4,     3,     2,     3,
     3,     4,     1,     1,     1,     1,     1,     1,     1,     4,
     1,     2,     4,     4,     3,     1,     3,     1,     1,     1,
     1,     1,     1,     1,     2,     4,     1,     2,     1,     2,
     1,     1,     1,     1,     2,     2,     3,     3,     1,     1,
     1,     3,     7,     1,     3,     2,     1,     2,     1,     1,
     1,     1,     1,     1,     5,     1,     1,     1,     1,     4,
     1,     1,     1,     1,     1,     1,     1,     0,     3,     1,
     1,     1,     1,     1,     1,     2,     3,     2,     1,     1,
     1,     1,     1,     1,     4,     4,     1,     1,     1,     1,
     1,     1,     1,     1,     0,     1,     1,     1,     2,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
     4,     2,     1,     2,     1,     1,     1,     4,     1,     4,
     1,     2,     2,     1,     4,     2,     1,     2,     1,     2,
     1,     4,     2,     1,     2,     3,     4,     4,     1,     4,
     1,     1,     8,     4,     7,     1,     5,     1,     2,     6,
     1,     6,     1,     4,     1,     1,     3,     6,     1,     1,
     1,     3,     1,     1,     1,     3,     1,     1,     4,     1,
     2,     1,     3,     1,     1,     3,     1,     1,     1,     1,
     2,     1,     2,     1,     2,     1,     2,     2,     4,     1,
     1,     3,     2,     1,     1,     1,     2,     1,     4,     1,
     1,     2,     2,     4,     4,     4,     1,     3,     2,     1,
     2,     1,     3,     3,     1,     3,     1,     3,     1,     2,
     3,     4,     1,     2,     3,     4,     1,     2,     2,     3,
     2,     2,     2,     2,    11,     1,     1,     2,     0,     2,
     0,     4,     0,     4,     0
};

static const short yydefact[] = {     0,
   203,     0,   205,   205,     0,     7,     8,     9,     0,     0,
     2,     6,   202,   201,   200,     0,   189,   190,   191,   192,
   193,   194,     5,     4,     0,   187,   188,     0,     0,     0,
   168,     0,   169,     0,   205,   196,   195,     1,     0,     0,
   205,    14,     0,     0,   204,   235,   236,   237,   238,   239,
   240,   242,   241,   243,   244,   245,   246,   247,   248,   249,
   250,   251,   253,   252,   254,   255,   256,   257,   258,   259,
     0,    15,    17,    30,    29,    16,    31,     3,     0,     0,
    21,   170,    13,    12,     1,     0,     0,    22,    25,     0,
    28,    23,     0,     0,     0,    33,    34,    37,    38,     0,
     0,     0,    20,    19,    24,     0,     1,    36,     0,    32,
    35,   204,   205,    71,    72,     0,     0,    87,     1,     1,
     1,   119,   121,     0,    83,   205,   205,   205,   205,   205,
     0,     0,   320,   330,   332,   334,   336,   205,   348,     0,
   205,   205,   205,   205,     0,     0,     0,     0,     0,     0,
    47,    51,     0,    50,    53,    54,    66,    67,    55,    56,
     0,    57,    58,     0,    59,    60,    61,    62,    63,     0,
    64,    65,    52,     0,     0,    48,    49,   210,   211,   212,
   213,   214,   215,   217,   216,   218,   219,   220,   221,   222,
   223,   224,   225,   226,   227,   228,   229,   230,   233,   232,
   231,   234,   205,    18,    27,     0,     1,    41,     0,    43,
    44,   115,   116,   117,     0,   118,     0,    84,    68,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   263,   260,   205,     0,   274,   272,     0,   277,   205,   205,
     0,     0,   289,   285,   205,     0,     0,     0,    70,   205,
     0,   331,   333,   335,   337,     0,   340,   338,     0,     0,
     0,   347,   350,   351,   197,   198,   199,    79,    78,   178,
     0,   184,   180,   181,   182,   183,   174,   369,   167,   166,
   171,   173,   172,   176,   175,   177,     0,     0,   373,   370,
     0,   377,   374,   379,   378,     0,   381,   382,   383,   384,
     0,     0,     1,   122,    90,     0,    94,     0,   205,    92,
     0,   102,     0,     0,     0,   109,     0,     1,   208,   207,
     1,   206,    26,    39,     0,     0,   114,   113,     0,    74,
     0,     0,    99,   145,    88,     0,   103,   100,     0,     0,
     0,     0,   262,   205,   265,   205,   273,   276,     0,   279,
   205,   205,     0,     0,     0,   291,   286,     0,     0,     0,
     0,   322,   205,    69,     0,     0,     0,     0,   352,   353,
     0,    81,    80,     0,     0,   371,     0,   375,   380,     0,
     0,   142,     0,     0,     0,   126,   128,   129,   130,     0,
   137,   132,   131,   133,   134,     0,     0,    95,     0,    89,
   205,    91,     0,   101,   110,   111,   108,     0,     0,   209,
     0,    42,   112,     0,    73,     0,    86,     0,   124,   123,
   106,     0,   205,   120,    82,   266,   267,   264,     0,   269,
   205,   278,     0,   281,   275,     0,   284,   282,   311,     0,
   292,     0,     0,     0,     0,   205,   321,     0,   324,   319,
   205,     0,   341,    87,   317,   318,   169,     0,     0,   315,
     0,     0,   179,   356,   357,     0,   360,     0,     0,     0,
   135,   146,     0,     0,   125,     0,     0,   138,   165,    97,
   185,    96,   169,     1,     0,    46,   205,    40,    75,     0,
     0,    85,   104,   205,   105,   205,     0,   271,   261,   280,
   283,   287,     0,   288,     0,   294,     0,   310,     0,   309,
     0,   344,   345,   346,   343,     0,   339,     1,     0,   354,
   355,   349,     0,   205,   359,   362,   361,   372,   376,   386,
     0,   205,   148,   150,   151,   149,   147,   127,     0,   186,
     0,    98,    45,    76,    77,     0,   314,     0,   313,   205,
   312,   290,   205,   308,   329,   327,   328,   323,   325,   342,
     0,   316,   358,     0,     0,     0,     0,   159,     0,   154,
   157,   205,   205,   160,     0,   144,   136,   139,   143,    93,
   107,   268,     0,     0,   296,   205,     0,   365,     0,   364,
   363,   387,   389,     0,   205,   152,   164,   161,   162,   158,
   163,   156,   140,   270,     0,     0,   305,   293,   326,     0,
     0,   391,     0,   155,     0,     0,   366,   367,   388,     0,
   393,   205,     0,   306,     0,     0,   390,     0,   395,     0,
   205,     0,   304,   368,     0,     0,   385,   153,     0,   298,
   205,   307,     0,     0,     0,     0,   301,   295,   205,   392,
     0,     0,     0,     0,   303,   299,   394,     0,     0,     0,
   297,     0,     0,     0,     0,   300,     0,   302,     0,     0,
     0
};

static const short yydefgoto[] = {    43,
   669,    25,    11,     2,     6,    40,    41,    71,    72,    80,
    87,    88,    89,    90,    91,    95,    96,    97,   411,   208,
    98,   151,   152,   249,   154,   307,   155,   156,   329,   330,
   277,   157,   158,   159,   418,   160,   161,   162,   308,   309,
   310,   163,   164,   165,   166,   167,   422,   423,   168,   169,
   170,   326,   215,   171,   172,   173,   304,   385,   386,   387,
   388,   389,   390,   577,   391,   578,   392,   393,   394,   537,
   533,   534,   535,   569,   570,   571,   572,   600,    99,   439,
   279,    31,   280,   371,   281,   282,   283,   482,     7,    16,
    17,    18,    19,    20,   284,   285,   286,    21,    33,   175,
   176,   320,   321,   322,   177,    77,   178,   232,   233,   346,
   428,   431,   499,   179,   236,   180,   239,   351,   435,   181,
   438,   182,   244,   245,   357,   440,   183,   247,   586,   641,
   648,   649,   656,   608,   625,   184,   509,   441,   548,   549,
   460,   185,   363,   450,   558,   559,   186,   187,   188,   189,
   190,   258,   452,   453,   515,   191,   262,   263,   264,   192,
   464,   465,   466,   525,   526,   590,   617,   467,   193,   290,
   194,   293,   195,   196,   197,   198,   199,   200,   201,   202,
   531,   593,   612,   621,   629,   637
};

static const short yypact[] = {    66,
-32768,    25,    20,   -12,   117,-32768,-32768,-32768,    53,    63,
-32768,-32768,-32768,-32768,-32768,    71,-32768,-32768,-32768,-32768,
-32768,    77,-32768,-32768,    86,-32768,-32768,    92,    68,    82,
-32768,   108,-32768,   121,    78,-32768,-32768,-32768,   630,   152,
    85,   156,   175,   147,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   161,   188,-32768,-32768,-32768,-32768,-32768,-32768,   893,   113,
-32768,-32768,-32768,-32768,-32768,   164,   165,   701,-32768,     5,
-32768,-32768,   167,   190,    15,-32768,   169,-32768,-32768,  1989,
   195,   701,-32768,-32768,-32768,   701,-32768,-32768,  2045,-32768,
-32768,   217,    93,-32768,   216,   205,   207,-32768,    -2,    -1,
-32768,   174,-32768,   224,-32768,   158,   151,   159,   159,    89,
   148,   139,  1989,  1989,  1989,  1989,  1989,   128,   -72,   557,
   118,   191,   151,   158,   160,  1989,  1989,  1989,  1989,   112,
-32768,-32768,    42,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  1255,-32768,-32768,  1389,-32768,-32768,-32768,-32768,-32768,  1509,
-32768,-32768,-32768,   231,   240,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   242,-32768,-32768,    66,-32768,-32768,   243,-32768,
-32768,-32768,-32768,-32768,    92,-32768,   175,   244,-32768,  1989,
   238,   245,   225,  1989,   246,   229,   248,   200,   175,  1989,
-32768,-32768,   181,  1989,-32768,-32768,  1989,-32768,   182,   182,
   253,   254,-32768,-32768,   177,   260,   186,   218,   238,   239,
  1749,   238,   238,   238,   238,   265,-32768,-32768,   267,   268,
   272,-32768,   192,   197,-32768,-32768,-32768,-32768,-32768,-32768,
   183,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,   170,   276,-32768,  1989,
   281,-32768,  1989,-32768,-32768,  1989,   238,   238,   238,   238,
  1989,  1037,-32768,-32768,-32768,   264,   168,   282,    79,-32768,
  1629,-32768,   283,  1989,  1989,   238,  1989,-32768,-32768,-32768,
-32768,   288,-32768,-32768,  2102,   284,-32768,-32768,    44,-32768,
   290,   175,   238,-32768,-32768,  1989,   238,-32768,  1989,  1989,
   175,   129,-32768,  1989,-32768,   222,-32768,-32768,  1989,-32768,
   220,   219,   557,   557,   294,-32768,-32768,   557,  1989,   302,
   557,-32768,   211,   238,   557,  1121,  1121,  1121,-32768,-32768,
   315,-32768,-32768,   557,   557,   238,   557,   238,   238,    -9,
  1989,-32768,   238,   -24,    12,-32768,-32768,-32768,-32768,   312,
   305,-32768,-32768,-32768,    13,   557,  1989,-32768,   557,-32768,
   242,-32768,   295,-32768,   238,   238,   238,   322,  1989,-32768,
   323,-32768,-32768,   175,-32768,   111,   319,   318,   238,   238,
-32768,   320,   132,-32768,-32768,-32768,-32768,-32768,   325,-32768,
   250,-32768,  1989,-32768,-32768,  1989,-32768,-32768,-32768,   326,
   321,   327,   557,   133,    -3,   557,-32768,   232,-32768,-32768,
    90,   134,-32768,-32768,   238,-32768,   231,   331,   137,-32768,
   138,   141,-32768,   324,-32768,   557,   221,   145,   146,   175,
   238,-32768,   330,   238,-32768,  1037,   334,-32768,-32768,   238,
-32768,-32768,   557,-32768,  1989,-32768,    73,-32768,-32768,   332,
   337,-32768,-32768,   242,-32768,  1121,   333,-32768,-32768,-32768,
-32768,-32768,   557,-32768,   340,-32768,  1989,-32768,   342,   321,
    30,-32768,-32768,-32768,-32768,   557,-32768,-32768,  1121,-32768,
-32768,-32768,   557,    90,   335,-32768,-32768,-32768,-32768,-32768,
   227,    55,-32768,-32768,-32768,-32768,-32768,-32768,   844,-32768,
  1869,   238,-32768,-32768,-32768,  1989,-32768,   343,   347,  1121,
-32768,-32768,    -4,-32768,-32768,-32768,-32768,   348,-32768,-32768,
   206,-32768,-32768,    98,   557,   175,   350,-32768,   149,-32768,
-32768,     4,   238,-32768,  2239,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   353,   275,-32768,   269,    30,-32768,   311,-32768,
-32768,-32768,   235,   365,    27,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   364,   366,-32768,-32768,-32768,   557,
   557,   241,   368,-32768,   557,    20,   369,-32768,-32768,   557,
   249,    27,   370,-32768,   150,   557,-32768,   372,   255,   153,
   293,    20,-32768,-32768,  1121,   379,-32768,-32768,   352,-32768,
   298,-32768,   154,   557,   388,   300,-32768,-32768,   303,-32768,
   390,   557,   374,   306,-32768,-32768,-32768,   162,   394,   378,
-32768,   557,   398,   163,   557,-32768,   166,-32768,   415,   417,
-32768
};

static const short yypgoto[] = {   -14,
-32768,-32768,-32768,   212,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   338,-32768,   -22,-32768,   328,-32768,-32768,    94,
-32768,-32768,-32768,   -94,-32768,  -122,-32768,-32768,  -219,     6,
    11,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   257,-32768,
  -113,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,  -192,-32768,   -47,-32768,
-32768,-32768,-32768,-32768,-32768,  -144,   109,-32768,-32768,-32768,
-32768,-32768,-32768,  -189,  -161,  -138,-32768,-32768,-32768,   -44,
   -23,-32768,-32768,-32768,-32768,-32768,-32768,-32768,  -586,-32768,
   420,   409,-32768,-32768,-32768,-32768,-32768,   223,   569,     0,
   -32,    46,  -290,-32768,-32768,  -101,-32768,   296,-32768,-32768,
-32768,-32768,-32768,-32768,   299,-32768,   316,   201,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,  -341,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,  -346,  -106,  -364,
   -71,-32768,-32768,-32768,-32768,  -140,-32768,-32768,-32768,-32768,
-32768,-32768,  -177,   -67,   -74,-32768,-32768,   187,   194,-32768,
-32768,   -70,-32768,-32768,  -111,-32768,-32768,  -139,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		2307


static const short yytable[] = {     3,
   287,   459,   461,   462,    30,   153,    75,   211,   302,   342,
   250,   444,   442,   302,   302,    93,    73,   473,   402,   106,
    45,    13,    94,     9,   259,   260,  -141,    34,   334,   624,
   475,   220,   224,    13,  -141,     5,   476,   474,    10,   252,
   253,   254,   255,   597,   302,   642,    75,   101,     8,    12,
   261,   297,   298,   299,   300,    75,   221,   221,   414,   302,
   415,    13,   101,   -10,   107,   303,   598,   599,   567,    75,
   102,     1,   302,    75,     4,   316,   210,    13,    23,   204,
    42,    14,    15,   205,    76,   319,    81,    26,    24,   507,
   302,   319,   206,   401,    28,   278,   584,     1,    13,   510,
    36,   505,    14,    15,   222,   225,   227,   343,   209,    29,
   484,   347,   417,    93,   348,    35,     1,    13,    45,    13,
    94,   268,   269,    13,    92,   333,    37,    14,    15,   337,
    39,   470,   495,   271,    38,   212,   213,   214,    79,    34,
   555,   556,   557,   414,   319,   425,   494,   503,   516,   506,
   517,   519,   519,   520,   521,   519,   364,   522,   216,   516,
   516,   528,   529,   595,   632,   596,   633,   595,   519,   638,
   650,   231,   235,   238,   238,   243,   503,   503,   661,   666,
   503,    13,   668,   257,   241,   242,   289,   292,   235,   231,
   472,   327,   324,   372,   373,   376,   543,   468,   378,   469,
    78,   379,    85,   546,   -11,     3,   380,   398,   399,   512,
   513,    45,    13,   109,    34,    83,   364,   421,   203,   405,
   406,   426,   407,   211,   588,   589,   432,   223,   226,    84,
  -203,   217,   103,   104,   218,   108,   219,   111,   228,   229,
   234,   419,   230,   237,   420,   248,   256,   246,   288,   296,
   291,   301,   317,   318,   319,   302,   325,   395,   336,   332,
   335,   338,   339,   340,   445,   341,   344,   349,   353,   354,
   643,   455,   455,   455,   355,   358,   359,   360,   345,   361,
   365,   536,   366,   367,   350,   350,   471,   368,   396,   260,
   356,   375,   210,   259,   374,   362,   377,   397,   400,   404,
   410,    34,   480,   408,   413,   658,   409,   416,   429,   443,
   500,   433,   436,   501,   487,   664,   447,   446,   667,   448,
   451,   456,   456,   456,   463,   477,   478,    45,   485,   278,
   451,   488,   451,   414,   492,   503,   493,   497,   523,   568,
   496,   511,   502,   504,   518,   532,   527,   539,   550,   565,
   544,   479,    34,    34,   481,   545,   552,    34,   554,   582,
    34,   519,   587,   594,    34,   458,   458,   458,   566,   604,
   605,   610,   606,    34,    34,   486,    34,   611,   613,   615,
   568,   616,   622,   626,   620,   645,   631,   635,   639,   427,
   542,   430,   491,   628,   644,    34,   434,   437,    34,   646,
   636,   455,   568,   652,   653,   654,   657,   659,   449,   662,
   660,   663,   553,   665,   670,    34,   671,   323,   412,   489,
   313,   524,   110,   581,   455,   105,   490,   580,   538,   568,
   603,   395,   630,   614,   602,    27,    32,   328,   540,   295,
   352,   294,    34,   583,   240,    34,   609,   562,   560,   564,
   370,   456,   563,   591,     0,   455,   369,     0,   551,     0,
     0,     0,     0,     0,     0,    34,     0,     0,     0,   541,
   618,   451,     0,     0,   456,    34,   498,     0,   278,     0,
     0,     0,    34,     0,     0,     0,   634,     0,     0,     0,
     0,   508,     0,     0,   579,   458,   514,     0,     0,     0,
     0,     0,    34,   561,     0,   456,     0,     0,     0,     0,
     0,     0,     0,     0,     0,    34,     0,     0,   458,     0,
   524,     0,    34,     0,     0,     0,     0,     0,   486,     0,
   579,     0,     0,     0,     0,     0,     0,     0,    34,     0,
   455,   547,     0,     0,     0,     0,     0,     0,     0,   458,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   265,
   266,   267,     1,    13,    34,   278,   619,   268,   269,   514,
   623,     0,   270,    22,    34,   627,     0,   574,     0,   271,
     0,   278,     0,     0,    22,     0,     0,     0,   272,     0,
   456,     0,     0,     0,     0,   547,     0,     0,   585,   651,
     0,     0,   273,   274,     0,     0,     0,    74,     0,    34,
    34,    82,     0,     0,    34,     0,     0,   601,   574,    34,
     0,     0,     0,   275,   276,    34,     0,     0,     0,     0,
    44,   607,     0,     0,   458,    45,    13,     0,     0,     0,
   574,     0,     0,    34,     0,     0,     0,    74,   100,     0,
     0,    34,     0,     0,     0,     0,    74,     0,     0,     0,
     0,    34,     0,   100,    34,     0,     0,   574,   174,     0,
    74,     0,     0,     0,    74,     0,   640,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   647,     0,     0,     0,
     0,     0,     0,     0,   655,     0,     0,     0,  -205,     0,
     0,   251,   174,   174,   174,   174,    45,    13,     0,     0,
     0,     0,     0,    46,   174,   174,   174,   174,    47,     0,
    48,     0,    49,     0,    50,     0,     0,     0,    51,   311,
     0,     0,   311,     0,     0,    52,     0,    53,   174,     0,
     0,     0,     0,    54,    55,    56,    57,    58,     0,     0,
     0,    59,     0,    60,     0,     0,     0,     0,    61,    62,
     0,    63,    64,    65,    66,    67,    68,    69,    70,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    46,   331,     0,     0,   174,    47,
     0,    48,   174,    49,     0,    50,     0,   331,   251,    51,
     0,     0,   251,     0,     0,   251,    52,     0,    53,     0,
     0,     0,     0,     0,    54,    55,    56,    57,    58,   174,
     0,     0,    59,     0,    60,     0,     0,     0,     0,    61,
    62,     0,    63,    64,    65,    66,    67,    68,    69,    70,
     0,     0,     0,     0,     0,     0,   265,   266,   267,     1,
    13,     0,     0,     0,   268,   269,     0,     0,   174,   270,
     0,   174,     0,     0,   174,   575,   271,     0,     0,   174,
     0,     0,     0,     0,     0,   272,     0,     0,     0,   174,
     0,     0,   174,   174,     0,   174,     0,     0,     0,   273,
   274,     0,     0,    86,     0,     0,     0,     0,    45,    13,
   331,   576,     0,     0,   174,     0,     0,   174,   251,   424,
   275,   276,   251,     0,     0,     0,     0,   251,     0,     0,
     0,     0,     0,     0,     0,     0,     0,   174,     0,     0,
     0,     0,     0,     0,   457,   457,   457,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   174,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,  -205,     0,     0,     0,   174,     0,   483,     0,     0,
     0,     0,     0,     0,     0,     0,    46,   174,     0,     0,
     0,    47,   331,    48,     0,    49,     0,    50,     0,     0,
     0,    51,     0,     0,     0,     0,     0,     0,    52,     0,
    53,   251,     0,     0,   251,     0,    54,    55,    56,    57,
    58,     0,     0,     0,    59,     0,    60,     0,     0,     0,
     0,    61,    62,     0,    63,    64,    65,    66,    67,    68,
    69,    70,     0,     0,     0,     0,     0,     0,   530,   265,
   266,   267,     1,    13,     0,     0,     0,   268,   269,     0,
     0,     0,   270,   174,     0,     0,     0,     0,     0,   271,
     0,     0,     0,     0,   457,     0,     0,     0,   272,     0,
     0,     0,     0,     0,     0,   174,     0,     0,     0,     0,
     0,     0,   273,   274,     0,     0,     0,   457,     0,     0,
     0,     0,   381,   382,     0,   221,   383,   384,     0,     0,
   573,     0,     0,   275,   276,     0,     0,     0,     0,   311,
     0,     0,     0,     0,   251,     0,     0,     0,   457,     0,
     0,     0,     0,   265,   266,   267,   112,    13,     0,    82,
     0,   268,   269,     0,   592,     0,   270,     0,     0,     0,
   113,     0,     0,   271,     0,     0,     0,   114,   115,   116,
     0,   117,   454,   119,     0,   120,     0,   121,   122,   123,
     0,     0,     0,   573,     0,     0,   273,   274,     0,     0,
     0,     0,   124,     0,     0,   125,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,   275,   276,     0,
   573,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,   457,   126,     0,     0,     0,     0,   127,
     0,   128,     0,   129,     0,   130,     0,     0,     0,   131,
     0,     0,     0,     0,     0,     0,   132,     0,   133,     0,
     0,     0,     0,     0,   134,   135,   136,   137,   138,     0,
     0,     0,   139,     0,   140,     0,     0,     0,     0,   141,
   142,     0,   143,   144,   145,   146,   147,   148,   149,   150,
   112,    13,     0,     0,     0,     0,     0,     0,     0,     0,
     0,   305,     0,     0,   113,     0,     0,     0,     0,     0,
     0,   114,   115,   116,     0,   117,   118,   119,     0,   120,
     0,   121,   122,   123,     0,     0,   306,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   124,     0,     0,   125,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   126,     0,
     0,     0,     0,   127,     0,   128,     0,   129,     0,   130,
     0,     0,     0,   131,     0,     0,     0,     0,     0,     0,
   132,     0,   133,     0,     0,     0,     0,     0,   134,   135,
   136,   137,   138,     0,     0,     0,   139,     0,   140,     0,
     0,     0,     0,   141,   142,     0,   143,   144,   145,   146,
   147,   148,   149,   150,   112,    13,     0,     0,     0,     0,
     0,     0,     0,     0,     0,   312,     0,     0,   113,     0,
     0,     0,     0,     0,     0,   114,   115,   116,     0,   117,
   118,   119,     0,   120,     0,   121,   122,   123,     0,     0,
   306,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   124,     0,     0,   125,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   126,     0,     0,     0,     0,   127,     0,   128,
     0,   129,     0,   130,     0,     0,     0,   131,     0,     0,
     0,     0,     0,     0,   132,     0,   133,     0,     0,     0,
     0,     0,   134,   135,   136,   137,   138,     0,     0,     0,
   139,     0,   140,     0,   112,    13,     0,   141,   142,     0,
   143,   144,   145,   146,   147,   148,   149,   150,   113,     0,
     0,     0,     0,     0,     0,   114,   115,   116,     0,   117,
   118,   119,     0,   120,   314,   121,   122,   123,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,   315,
   124,     0,     0,   125,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   126,     0,     0,     0,     0,   127,     0,   128,
     0,   129,     0,   130,     0,     0,     0,   131,     0,     0,
     0,     0,     0,     0,   132,     0,   133,     0,     0,     0,
     0,     0,   134,   135,   136,   137,   138,     0,     0,     0,
   139,     0,   140,     0,   112,    13,     0,   141,   142,     0,
   143,   144,   145,   146,   147,   148,   149,   150,   113,     0,
   317,     0,     0,     0,     0,   114,   115,   116,     0,   117,
   118,   119,     0,   120,     0,   121,   122,   123,     0,     0,
   403,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   124,     0,     0,   125,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   126,     0,     0,     0,     0,   127,     0,   128,
     0,   129,     0,   130,     0,     0,     0,   131,     0,     0,
     0,     0,     0,     0,   132,     0,   133,     0,     0,     0,
     0,     0,   134,   135,   136,   137,   138,     0,     0,     0,
   139,     0,   140,     0,   112,    13,     0,   141,   142,     0,
   143,   144,   145,   146,   147,   148,   149,   150,   113,     0,
   317,     0,     0,     0,     0,   114,   115,   116,     0,   117,
   118,   119,     0,   120,     0,   121,   122,   123,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   124,     0,     0,   125,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   126,     0,     0,     0,     0,   127,     0,   128,
     0,   129,     0,   130,     0,     0,     0,   131,     0,     0,
     0,     0,     0,     0,   132,     0,   133,     0,     0,     0,
     0,     0,   134,   135,   136,   137,   138,     0,     0,     0,
   139,     0,   140,     0,   112,    13,     0,   141,   142,     0,
   143,   144,   145,   146,   147,   148,   149,   150,   113,     0,
     0,     0,     0,     0,     0,   114,   115,   116,     0,   117,
   118,   119,     0,   120,     0,   121,   122,   123,     0,     0,
   306,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   124,     0,     0,   125,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   126,     0,     0,     0,     0,   127,     0,   128,
     0,   129,     0,   130,     0,     0,     0,   131,     0,     0,
     0,     0,     0,     0,   132,     0,   133,     0,     0,     0,
     0,     0,   134,   135,   136,   137,   138,     0,     0,     0,
   139,     0,   140,     0,   112,    13,     0,   141,   142,     0,
   143,   144,   145,   146,   147,   148,   149,   150,   113,     0,
     0,     0,     0,     0,     0,   114,   115,   116,     0,   117,
   118,   119,     0,   120,     0,   121,   122,   123,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
   124,     0,     0,   125,     0,     0,     0,     0,     0,     0,
   112,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,   126,     0,     0,     0,     0,   127,     0,   128,
     0,   129,     0,   130,     0,     0,     0,   131,     0,     0,
     0,     0,   207,     0,   132,     0,   133,     0,     0,     0,
     0,     0,   134,   135,   136,   137,   138,    45,     0,     0,
   139,     0,   140,     0,     0,     0,     0,   141,   142,     0,
   143,   144,   145,   146,   147,   148,   149,   150,    46,     0,
     0,     0,     0,    47,     0,    48,     0,    49,     0,    50,
     0,     0,     0,    51,     0,     0,     0,     0,     0,     0,
    52,     0,    53,     0,     0,     0,     0,     0,    54,    55,
    56,    57,    58,     0,     0,     0,    59,     0,    60,     0,
     0,     0,     0,    61,    62,     0,    63,    64,    65,    66,
    67,    68,    69,    70,     0,    46,     0,     0,     0,     0,
    47,     0,    48,     0,    49,     0,    50,     0,     0,     0,
    51,     0,     0,     0,     0,     0,     0,    52,     0,    53,
     0,     0,     0,     0,     0,    54,    55,    56,    57,    58,
     0,     0,     0,    59,     0,    60,     0,     0,     0,     0,
    61,    62,     0,    63,    64,    65,    66,    67,    68,    69,
    70,   265,   266,   267,     1,    13,     0,     0,     0,   268,
   269,     0,     0,     0,   270,     0,     0,     0,     0,     0,
     0,   271,     0,     0,     0,     0,     0,     0,     0,     0,
   272,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,   273,   274,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   576,     0,     0,     0,
     0,     0,     0,     0,     0,   275,   276
};

static const short yycheck[] = {     0,
   140,   366,   367,   368,    28,   100,    39,   109,    18,   229,
   133,   358,   354,    18,    18,     1,    39,    42,   309,    15,
     6,     7,     8,    36,    97,    98,    14,    28,   221,   616,
    19,    34,    34,     7,    22,    16,    25,    62,    51,   134,
   135,   136,   137,    40,    18,   632,    79,    80,     3,     4,
   123,   146,   147,   148,   149,    88,    59,    59,    15,    18,
    17,     7,    95,    49,    60,    24,    63,    64,    14,   102,
    85,     6,    18,   106,    50,   170,   109,     7,    26,   102,
    35,    11,    12,   106,    39,    13,    41,    17,    26,    93,
    18,    13,   107,    15,    18,   140,   101,     6,     7,   446,
    19,   443,    11,    12,   119,   120,   121,   230,   109,    24,
   401,   234,   332,     1,   237,    48,     6,     7,     6,     7,
     8,    11,    12,     7,    79,   220,    19,    11,    12,   224,
    53,   141,   423,    23,    14,    43,    44,    45,    54,   140,
   111,   112,   113,    15,    13,    17,    15,    15,    15,    17,
    17,    15,    15,    17,    17,    15,   251,    17,   113,    15,
    15,    17,    17,    15,    15,    17,    17,    15,    15,    17,
    17,   126,   127,   128,   129,   130,    15,    15,    17,    17,
    15,     7,    17,   138,    96,    97,   141,   142,   143,   144,
   383,   215,   207,    11,    12,   290,   487,   375,   293,   377,
    49,   296,    15,   494,    49,   206,   301,    40,    41,   120,
   121,     6,     7,    24,   215,    69,   311,   340,    24,   314,
   315,   344,   317,   325,   127,   128,   349,   119,   120,    69,
    14,    16,    69,    69,    30,    69,    30,    69,    65,    16,
    90,   336,    85,    85,   339,   107,   119,   100,   131,    90,
    60,   140,    22,    14,    13,    18,    14,   302,    34,    16,
    16,    16,    34,    16,   359,    66,    86,    86,    16,    16,
   635,   366,   367,   368,    98,    16,    91,    60,   233,    41,
    16,   474,    16,    16,   239,   240,   381,    16,   303,    98,
   245,    16,   325,    97,   125,   250,    16,    34,    17,    17,
    13,   302,   397,   318,    21,   652,   321,    18,    87,    16,
   433,    92,    94,   436,   409,   662,   361,    16,   665,   109,
   365,   366,   367,   368,    10,    14,    22,     6,    34,   374,
   375,     9,   377,    15,    17,    15,    17,    88,    15,   532,
    16,   110,    17,    17,    14,    16,   126,    14,    16,    15,
    19,   396,   353,   354,   399,    19,    17,   358,    17,    17,
   361,    15,    15,    14,   365,   366,   367,   368,   142,    17,
    96,    61,   104,   374,   375,   408,   377,   143,    14,    16,
   573,    16,    15,    15,   144,    34,    17,    16,    96,   344,
   485,   346,   416,   145,    16,   396,   351,   352,   399,   102,
   146,   496,   595,    16,   105,   103,    17,    34,   363,    16,
   105,    34,   507,    16,     0,   416,     0,   206,   325,   414,
   164,   466,    95,   546,   519,    88,   416,   541,   476,   622,
   575,   476,   622,   595,   573,    16,    28,   215,   483,   144,
   240,   143,   443,   550,   129,   446,   587,   519,   516,   524,
   264,   496,   523,   565,    -1,   550,   263,    -1,   503,    -1,
    -1,    -1,    -1,    -1,    -1,   466,    -1,    -1,    -1,   484,
   610,   516,    -1,    -1,   519,   476,   431,    -1,   523,    -1,
    -1,    -1,   483,    -1,    -1,    -1,   626,    -1,    -1,    -1,
    -1,   446,    -1,    -1,   539,   496,   451,    -1,    -1,    -1,
    -1,    -1,   503,   518,    -1,   550,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,   516,    -1,    -1,   519,    -1,
   565,    -1,   523,    -1,    -1,    -1,    -1,    -1,   561,    -1,
   575,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   539,    -1,
   635,   496,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   550,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
     4,     5,     6,     7,   565,   610,   611,    11,    12,   524,
   615,    -1,    16,     5,   575,   620,    -1,   532,    -1,    23,
    -1,   626,    -1,    -1,    16,    -1,    -1,    -1,    32,    -1,
   635,    -1,    -1,    -1,    -1,   550,    -1,    -1,   553,   644,
    -1,    -1,    46,    47,    -1,    -1,    -1,    39,    -1,   610,
   611,    43,    -1,    -1,   615,    -1,    -1,   572,   573,   620,
    -1,    -1,    -1,    67,    68,   626,    -1,    -1,    -1,    -1,
     1,   586,    -1,    -1,   635,     6,     7,    -1,    -1,    -1,
   595,    -1,    -1,   644,    -1,    -1,    -1,    79,    80,    -1,
    -1,   652,    -1,    -1,    -1,    -1,    88,    -1,    -1,    -1,
    -1,   662,    -1,    95,   665,    -1,    -1,   622,   100,    -1,
   102,    -1,    -1,    -1,   106,    -1,   631,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,   641,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,   649,    -1,    -1,    -1,    69,    -1,
    -1,   133,   134,   135,   136,   137,     6,     7,    -1,    -1,
    -1,    -1,    -1,    84,   146,   147,   148,   149,    89,    -1,
    91,    -1,    93,    -1,    95,    -1,    -1,    -1,    99,   161,
    -1,    -1,   164,    -1,    -1,   106,    -1,   108,   170,    -1,
    -1,    -1,    -1,   114,   115,   116,   117,   118,    -1,    -1,
    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,   129,   130,
    -1,   132,   133,   134,   135,   136,   137,   138,   139,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    84,   217,    -1,    -1,   220,    89,
    -1,    91,   224,    93,    -1,    95,    -1,   229,   230,    99,
    -1,    -1,   234,    -1,    -1,   237,   106,    -1,   108,    -1,
    -1,    -1,    -1,    -1,   114,   115,   116,   117,   118,   251,
    -1,    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,   129,
   130,    -1,   132,   133,   134,   135,   136,   137,   138,   139,
    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
     7,    -1,    -1,    -1,    11,    12,    -1,    -1,   290,    16,
    -1,   293,    -1,    -1,   296,    22,    23,    -1,    -1,   301,
    -1,    -1,    -1,    -1,    -1,    32,    -1,    -1,    -1,   311,
    -1,    -1,   314,   315,    -1,   317,    -1,    -1,    -1,    46,
    47,    -1,    -1,     1,    -1,    -1,    -1,    -1,     6,     7,
   332,    58,    -1,    -1,   336,    -1,    -1,   339,   340,   341,
    67,    68,   344,    -1,    -1,    -1,    -1,   349,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,   359,    -1,    -1,
    -1,    -1,    -1,    -1,   366,   367,   368,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   381,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    69,    -1,    -1,    -1,   397,    -1,   399,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    84,   409,    -1,    -1,
    -1,    89,   414,    91,    -1,    93,    -1,    95,    -1,    -1,
    -1,    99,    -1,    -1,    -1,    -1,    -1,    -1,   106,    -1,
   108,   433,    -1,    -1,   436,    -1,   114,   115,   116,   117,
   118,    -1,    -1,    -1,   122,    -1,   124,    -1,    -1,    -1,
    -1,   129,   130,    -1,   132,   133,   134,   135,   136,   137,
   138,   139,    -1,    -1,    -1,    -1,    -1,    -1,   470,     3,
     4,     5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,
    -1,    -1,    16,   485,    -1,    -1,    -1,    -1,    -1,    23,
    -1,    -1,    -1,    -1,   496,    -1,    -1,    -1,    32,    -1,
    -1,    -1,    -1,    -1,    -1,   507,    -1,    -1,    -1,    -1,
    -1,    -1,    46,    47,    -1,    -1,    -1,   519,    -1,    -1,
    -1,    -1,    56,    57,    -1,    59,    60,    61,    -1,    -1,
   532,    -1,    -1,    67,    68,    -1,    -1,    -1,    -1,   541,
    -1,    -1,    -1,    -1,   546,    -1,    -1,    -1,   550,    -1,
    -1,    -1,    -1,     3,     4,     5,     6,     7,    -1,   561,
    -1,    11,    12,    -1,   566,    -1,    16,    -1,    -1,    -1,
    20,    -1,    -1,    23,    -1,    -1,    -1,    27,    28,    29,
    -1,    31,    32,    33,    -1,    35,    -1,    37,    38,    39,
    -1,    -1,    -1,   595,    -1,    -1,    46,    47,    -1,    -1,
    -1,    -1,    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    -1,
   622,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,   635,    84,    -1,    -1,    -1,    -1,    89,
    -1,    91,    -1,    93,    -1,    95,    -1,    -1,    -1,    99,
    -1,    -1,    -1,    -1,    -1,    -1,   106,    -1,   108,    -1,
    -1,    -1,    -1,    -1,   114,   115,   116,   117,   118,    -1,
    -1,    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,   129,
   130,    -1,   132,   133,   134,   135,   136,   137,   138,   139,
     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    17,    -1,    -1,    20,    -1,    -1,    -1,    -1,    -1,
    -1,    27,    28,    29,    -1,    31,    32,    33,    -1,    35,
    -1,    37,    38,    39,    -1,    -1,    42,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    52,    -1,    -1,    55,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
    -1,    -1,    -1,    89,    -1,    91,    -1,    93,    -1,    95,
    -1,    -1,    -1,    99,    -1,    -1,    -1,    -1,    -1,    -1,
   106,    -1,   108,    -1,    -1,    -1,    -1,    -1,   114,   115,
   116,   117,   118,    -1,    -1,    -1,   122,    -1,   124,    -1,
    -1,    -1,    -1,   129,   130,    -1,   132,   133,   134,   135,
   136,   137,   138,   139,     6,     7,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    17,    -1,    -1,    20,    -1,
    -1,    -1,    -1,    -1,    -1,    27,    28,    29,    -1,    31,
    32,    33,    -1,    35,    -1,    37,    38,    39,    -1,    -1,
    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    84,    -1,    -1,    -1,    -1,    89,    -1,    91,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
    -1,    -1,    -1,    -1,   106,    -1,   108,    -1,    -1,    -1,
    -1,    -1,   114,   115,   116,   117,   118,    -1,    -1,    -1,
   122,    -1,   124,    -1,     6,     7,    -1,   129,   130,    -1,
   132,   133,   134,   135,   136,   137,   138,   139,    20,    -1,
    -1,    -1,    -1,    -1,    -1,    27,    28,    29,    -1,    31,
    32,    33,    -1,    35,    36,    37,    38,    39,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    51,
    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    84,    -1,    -1,    -1,    -1,    89,    -1,    91,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
    -1,    -1,    -1,    -1,   106,    -1,   108,    -1,    -1,    -1,
    -1,    -1,   114,   115,   116,   117,   118,    -1,    -1,    -1,
   122,    -1,   124,    -1,     6,     7,    -1,   129,   130,    -1,
   132,   133,   134,   135,   136,   137,   138,   139,    20,    -1,
    22,    -1,    -1,    -1,    -1,    27,    28,    29,    -1,    31,
    32,    33,    -1,    35,    -1,    37,    38,    39,    -1,    -1,
    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    84,    -1,    -1,    -1,    -1,    89,    -1,    91,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
    -1,    -1,    -1,    -1,   106,    -1,   108,    -1,    -1,    -1,
    -1,    -1,   114,   115,   116,   117,   118,    -1,    -1,    -1,
   122,    -1,   124,    -1,     6,     7,    -1,   129,   130,    -1,
   132,   133,   134,   135,   136,   137,   138,   139,    20,    -1,
    22,    -1,    -1,    -1,    -1,    27,    28,    29,    -1,    31,
    32,    33,    -1,    35,    -1,    37,    38,    39,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    84,    -1,    -1,    -1,    -1,    89,    -1,    91,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
    -1,    -1,    -1,    -1,   106,    -1,   108,    -1,    -1,    -1,
    -1,    -1,   114,   115,   116,   117,   118,    -1,    -1,    -1,
   122,    -1,   124,    -1,     6,     7,    -1,   129,   130,    -1,
   132,   133,   134,   135,   136,   137,   138,   139,    20,    -1,
    -1,    -1,    -1,    -1,    -1,    27,    28,    29,    -1,    31,
    32,    33,    -1,    35,    -1,    37,    38,    39,    -1,    -1,
    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    84,    -1,    -1,    -1,    -1,    89,    -1,    91,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
    -1,    -1,    -1,    -1,   106,    -1,   108,    -1,    -1,    -1,
    -1,    -1,   114,   115,   116,   117,   118,    -1,    -1,    -1,
   122,    -1,   124,    -1,     6,     7,    -1,   129,   130,    -1,
   132,   133,   134,   135,   136,   137,   138,   139,    20,    -1,
    -1,    -1,    -1,    -1,    -1,    27,    28,    29,    -1,    31,
    32,    33,    -1,    35,    -1,    37,    38,    39,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    52,    -1,    -1,    55,    -1,    -1,    -1,    -1,    -1,    -1,
     6,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    84,    -1,    -1,    -1,    -1,    89,    -1,    91,
    -1,    93,    -1,    95,    -1,    -1,    -1,    99,    -1,    -1,
    -1,    -1,    48,    -1,   106,    -1,   108,    -1,    -1,    -1,
    -1,    -1,   114,   115,   116,   117,   118,     6,    -1,    -1,
   122,    -1,   124,    -1,    -1,    -1,    -1,   129,   130,    -1,
   132,   133,   134,   135,   136,   137,   138,   139,    84,    -1,
    -1,    -1,    -1,    89,    -1,    91,    -1,    93,    -1,    95,
    -1,    -1,    -1,    99,    -1,    -1,    -1,    -1,    -1,    -1,
   106,    -1,   108,    -1,    -1,    -1,    -1,    -1,   114,   115,
   116,   117,   118,    -1,    -1,    -1,   122,    -1,   124,    -1,
    -1,    -1,    -1,   129,   130,    -1,   132,   133,   134,   135,
   136,   137,   138,   139,    -1,    84,    -1,    -1,    -1,    -1,
    89,    -1,    91,    -1,    93,    -1,    95,    -1,    -1,    -1,
    99,    -1,    -1,    -1,    -1,    -1,    -1,   106,    -1,   108,
    -1,    -1,    -1,    -1,    -1,   114,   115,   116,   117,   118,
    -1,    -1,    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,
   129,   130,    -1,   132,   133,   134,   135,   136,   137,   138,
   139,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
    12,    -1,    -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,
    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    32,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    46,    47,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    58,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    67,    68
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/local/lib/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/usr/local/lib/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 461 "core/parse-asn1.y"
{ yyval.intVal = myLineNoG; ;
    break;}
case 2:
#line 466 "core/parse-asn1.y"
{ modulePtrG->tagDefault = yyvsp[0].intVal; ;
    break;}
case 3:
#line 471 "core/parse-asn1.y"
{
         modulePtrG->modId      = yyvsp[-7].moduleId;

         /*
          * Set exported flags in type/value defs as appropriate
          */
         SetExports (modulePtrG, exportListG, exportsParsedG);

         /* clean up */

         /* Free Application tag list */
         FreeApplTags();

         /*
          * Add values defined in any parsed object identifiers.
          * Only the Module name and some macro oids have been parsed,
          * the rest are just "{...}" strings at this point
          * (they will be parsed in later)
          */
         modulePtrG->valueDefs =
             AsnListConcat (modulePtrG->valueDefs, oidElmtValDefsG);

         /*
          * free list head only
          */
         Free (oidElmtValDefsG);
     ;
    break;}
case 4:
#line 501 "core/parse-asn1.y"
{ yyval.intVal = EXPLICIT_TAGS; ;
    break;}
case 5:
#line 502 "core/parse-asn1.y"
{ yyval.intVal = IMPLICIT_TAGS; ;
    break;}
case 6:
#line 504 "core/parse-asn1.y"
{
        /* default is EXPLICIT TAGS */
        yyval.intVal = EXPLICIT_TAGS;
    ;
    break;}
case 7:
#line 512 "core/parse-asn1.y"
{
        yyval.moduleId = MT (ModuleId);
        yyval.moduleId->name = yyvsp[-1].charPtr;
        yyval.moduleId->oid = yyvsp[0].oidPtr;
    ;
    break;}
case 9:
#line 521 "core/parse-asn1.y"
{ yyval.oidPtr = NULL; ;
    break;}
case 12:
#line 531 "core/parse-asn1.y"
{
        /*
         *  allows differentiation between "EXPORTS;"
         *         (in which no exports allowed)
         *  and when the EXPORTS symbol does not appear
         *         (then all are exported)
         */
        exportsParsedG = TRUE;
    ;
    break;}
case 13:
#line 541 "core/parse-asn1.y"
{
        PARSE_ERROR();
        exportsParsedG = FALSE;
        exportListG = NULL;
        yyerrok;
    ;
    break;}
case 14:
#line 547 "core/parse-asn1.y"
{ exportsParsedG = FALSE; ;
    break;}
case 15:
#line 551 "core/parse-asn1.y"
{ exportListG = yyvsp[0].exportList; ;
    break;}
case 16:
#line 552 "core/parse-asn1.y"
{ exportListG = NULL; ;
    break;}
case 17:
#line 557 "core/parse-asn1.y"
{
        yyval.exportList = MT (ExportElmt);
        yyval.exportList->name = yyvsp[0].charPtr;
        yyval.exportList->lineNo = myLineNoG;
        yyval.exportList->next = NULL;
    ;
    break;}
case 18:
#line 564 "core/parse-asn1.y"
{
        yyval.exportList = MT (ExportElmt);
        yyval.exportList->name = yyvsp[0].charPtr;
        yyval.exportList->next = yyvsp[-3].exportList;
        yyval.exportList->lineNo = yyvsp[-1].intVal;
    ;
    break;}
case 20:
#line 575 "core/parse-asn1.y"
{
       PARSE_ERROR();
       yyerrok;
    ;
    break;}
case 22:
#line 583 "core/parse-asn1.y"
{ modulePtrG->imports = yyvsp[0].importModuleListPtr; ;
    break;}
case 24:
#line 589 "core/parse-asn1.y"
{
        APPEND (yyvsp[0].importModulePtr,yyvsp[-1].importModuleListPtr);
    ;
    break;}
case 25:
#line 593 "core/parse-asn1.y"
{
        yyval.importModuleListPtr = NEWLIST();
        APPEND (yyvsp[0].importModulePtr, yyval.importModuleListPtr);
    ;
    break;}
case 26:
#line 601 "core/parse-asn1.y"
{
        yyval.importModulePtr = MT (ImportModule);
        yyval.importModulePtr->modId   = yyvsp[0].moduleId;
        yyval.importModulePtr->lineNo = yyvsp[-1].intVal;
        yyval.importModulePtr->importElmts = yyvsp[-3].importElmtListPtr;
    ;
    break;}
case 27:
#line 612 "core/parse-asn1.y"
{
        ImportElmt *ie;

        ie = MT (ImportElmt);
        ie->name = yyvsp[0].charPtr;
        ie->lineNo = myLineNoG;
        APPEND (ie, yyvsp[-2].importElmtListPtr);
        yyval.importElmtListPtr = yyvsp[-2].importElmtListPtr;
    ;
    break;}
case 28:
#line 622 "core/parse-asn1.y"
{
        ImportElmt *ie;

        /* called for the first element only, so create list head */
        yyval.importElmtListPtr = NEWLIST();
        ie = MT (ImportElmt);
        ie->name = yyvsp[0].charPtr;
        ie->lineNo = myLineNoG;
        APPEND (ie, yyval.importElmtListPtr);
    ;
    break;}
case 31:
#line 638 "core/parse-asn1.y"
{
        /*
         * hack to make DefinedMacroNames "freeable"
         * like idents and typeref
         */
        yyval.charPtr = Malloc (strlen (yyvsp[0].charPtr)+1);
        strcpy (yyval.charPtr, yyvsp[0].charPtr);
    ;
    break;}
case 36:
#line 659 "core/parse-asn1.y"
{
        PARSE_ERROR();
        yyerrok;
    ;
    break;}
case 37:
#line 667 "core/parse-asn1.y"
{
        /*
         * a macro may produce a null type
         */
        if (yyvsp[0].typeDefPtr != NULL)
        {
            /*
             * add to head of  type def list
             */
            APPEND (yyvsp[0].typeDefPtr, modulePtrG->typeDefs);
        }

    ;
    break;}
case 38:
#line 681 "core/parse-asn1.y"
{
        /*
         * a macro may produce a null value
         */
        if (yyvsp[0].valueDefPtr != NULL)
        {
            /*
             * add to head of value def list
             */
            APPEND (yyvsp[0].valueDefPtr, modulePtrG->valueDefs);
        }
    ;
    break;}
case 39:
#line 693 "core/parse-asn1.y"
{ LexBeginMacroDefContext(); ;
    break;}
case 40:
#line 695 "core/parse-asn1.y"
{
        TypeDef *tmpTypeDef;

        /*
         *  LEXICAL TIE IN!!
         * create macro type to eliminate import resolution
         * errors msgs from other modules importing the macro.
         * (hopefully) Only the import list will link with
         * these type defs.
         * keeps macro def around incase of future processing needs
         *
         * NOTE: MACRODEFBODY_SYM returns the macro def body with
         * with "BEGIN" at the begininning and "END" at the end
         */

        /*
         * put lexical analyzer back in normal state
         */
        /*  BEGIN (INITIAL);  */
        LexBeginInitialContext();

        tmpTypeDef = MT (TypeDef);
        SetupType (&tmpTypeDef->type, BASICTYPE_MACRODEF, yyvsp[-2].intVal);
        tmpTypeDef->definedName = yyvsp[-5].charPtr;

        /*
         * keeps the macro def body
         * (all text between & including the BEGIN and END)
         * as a simple string - incase you want to fart around with
         * it.
         */
        tmpTypeDef->type->basicType->a.macroDef = yyvsp[0].charPtr;

        /*
         * put in type list
         */
        APPEND (tmpTypeDef, modulePtrG->typeDefs);

    ;
    break;}
case 41:
#line 735 "core/parse-asn1.y"
{
        TypeDef *tmpTypeDef;

        tmpTypeDef = MT (TypeDef);
        SetupType (&tmpTypeDef->type, BASICTYPE_MACRODEF, myLineNoG);
        tmpTypeDef->definedName = yyvsp[-2].charPtr;

        tmpTypeDef->type->basicType->a.macroDef = yyvsp[0].charPtr;

        /*
         * put in type list
         */
        APPEND (tmpTypeDef, modulePtrG->typeDefs);

    ;
    break;}
case 42:
#line 751 "core/parse-asn1.y"
{
        TypeDef *tmpTypeDef;

        tmpTypeDef = MT (TypeDef);
        SetupType (&tmpTypeDef->type, BASICTYPE_MACRODEF, myLineNoG);
        tmpTypeDef->definedName = yyvsp[-4].charPtr;

        tmpTypeDef->type->basicType->a.macroDef =
               (MyString) Malloc (strlen (yyvsp[-2].charPtr) + strlen (yyvsp[0].charPtr) + 2);

        strcpy (tmpTypeDef->type->basicType->a.macroDef, yyvsp[-2].charPtr);
        strcat (tmpTypeDef->type->basicType->a.macroDef, ".");
        strcat (tmpTypeDef->type->basicType->a.macroDef, yyvsp[0].charPtr);

       /*
         * put in type list
         */
        APPEND (tmpTypeDef, modulePtrG->typeDefs);

        Free (yyvsp[-2].charPtr);
        Free (yyvsp[0].charPtr);
    ;
    break;}
case 45:
#line 786 "core/parse-asn1.y"
{
        /*
         * a macro type may produce a null type
         */
        if (yyvsp[-1].typePtr != NULL)
        {
            yyval.typeDefPtr = MT (TypeDef);
            yyval.typeDefPtr->type =  yyvsp[-1].typePtr;
            yyval.typeDefPtr->type->lineNo = yyvsp[-2].intVal;
            yyval.typeDefPtr->type->attrList = yyvsp[0].attrList;
            yyval.typeDefPtr->definedName = yyvsp[-5].charPtr;
            yyval.typeDefPtr->attrList = yyvsp[-3].attrList;
        }
        else
            yyval.typeDefPtr = NULL;
    ;
    break;}
case 46:
#line 807 "core/parse-asn1.y"
{
        /* allocate a Type with basic type of ImportTypeRef */
        SetupType (&yyval.typePtr, BASICTYPE_IMPORTTYPEREF, yyvsp[-1].intVal);
        yyval.typePtr->basicType->a.importTypeRef = MT (TypeRef);
        yyval.typePtr->basicType->a.importTypeRef->typeName = yyvsp[0].charPtr;
        yyval.typePtr->basicType->a.importTypeRef->moduleName = yyvsp[-3].charPtr;

        /* add entry to this module's import list */
        AddPrivateImportElmt (modulePtrG, yyvsp[0].charPtr, yyvsp[-3].charPtr, yyvsp[-1].intVal);
    ;
    break;}
case 47:
#line 821 "core/parse-asn1.y"
{ yyval.typePtr = yyvsp[0].typePtr; ;
    break;}
case 48:
#line 823 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_LOCALTYPEREF, myLineNoG);
        yyval.typePtr->basicType->a.localTypeRef = MT (TypeRef);
        yyval.typePtr->basicType->a.localTypeRef->typeName = yyvsp[0].charPtr;
    ;
    break;}
case 68:
#line 856 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_OCTETSTRING, myLineNoG);
    ;
    break;}
case 69:
#line 863 "core/parse-asn1.y"
{
        yyval.namedTypePtr = MT (NamedType);
        yyval.namedTypePtr->type = yyvsp[0].typePtr;
        yyval.namedTypePtr->fieldName = yyvsp[-1].charPtr;
    ;
    break;}
case 70:
#line 869 "core/parse-asn1.y"
{
        yyval.namedTypePtr = MT (NamedType);
        yyval.namedTypePtr->type = yyvsp[0].typePtr;
    ;
    break;}
case 71:
#line 877 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_BOOLEAN, myLineNoG);
    ;
    break;}
case 72:
#line 884 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_INTEGER, myLineNoG);
        yyval.typePtr->basicType->a.integer = NEWLIST();  /* empty list */
    ;
    break;}
case 73:
#line 889 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_INTEGER, myLineNoG);
        yyval.typePtr->basicType->a.integer = yyvsp[-1].valueDefListPtr;
    ;
    break;}
case 74:
#line 898 "core/parse-asn1.y"
{
        yyval.valueDefListPtr = NEWLIST();
        APPEND (yyvsp[0].valueDefPtr, yyval.valueDefListPtr);
    ;
    break;}
case 75:
#line 903 "core/parse-asn1.y"
{
        APPEND (yyvsp[0].valueDefPtr,yyvsp[-2].valueDefListPtr);
        yyval.valueDefListPtr = yyvsp[-2].valueDefListPtr;
    ;
    break;}
case 76:
#line 911 "core/parse-asn1.y"
{
        yyval.valueDefPtr = MT (ValueDef);
        yyval.valueDefPtr->definedName = yyvsp[-3].charPtr;
        SetupValue (&yyval.valueDefPtr->value, BASICVALUE_INTEGER, myLineNoG);
        yyval.valueDefPtr->value->basicValue->a.integer = yyvsp[-1].intVal;
    ;
    break;}
case 77:
#line 918 "core/parse-asn1.y"
{
        yyval.valueDefPtr = MT (ValueDef);
        yyval.valueDefPtr->definedName = yyvsp[-3].charPtr;
        yyval.valueDefPtr->value = yyvsp[-1].valuePtr;
    ;
    break;}
case 78:
#line 927 "core/parse-asn1.y"
{
	if (yyvsp[0].uintVal>0x7FFFFFFF) {
	    yyerror("Warning: positive signed number out of range");
	    yyval.intVal = 0x7FFFFFFF;
	}
    ;
    break;}
case 79:
#line 934 "core/parse-asn1.y"
{
	yyerror ("Warning: positive signed number out of range");
	yyval.intVal = 0x7FFFFFFF;
	/* modulePtrG->status = MOD_ERROR; */
    ;
    break;}
case 80:
#line 940 "core/parse-asn1.y"
{
	if (yyvsp[0].uintVal>0x80000000) {
	    yyerror("Warning: negative signed number out of range");
	    yyval.intVal = -0x80000000;
	} else if (yyvsp[0].uintVal==0x80000000) {
	    yyval.intVal = -0x80000000;
	} else {
	    yyval.intVal = -yyvsp[0].uintVal;
	}
    ;
    break;}
case 81:
#line 951 "core/parse-asn1.y"
{
	yyerror ("Warning: negative signed number out of range");
	yyval.intVal = -0x80000000;
	/* modulePtrG->status = MOD_ERROR; */
    ;
    break;}
case 82:
#line 960 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_ENUMERATED, myLineNoG);
        yyval.typePtr->basicType->a.enumerated = yyvsp[-1].valueDefListPtr;
    ;
    break;}
case 83:
#line 969 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_REAL, myLineNoG);
    ;
    break;}
case 84:
#line 976 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_BITSTRING, myLineNoG);
        yyval.typePtr->basicType->a.bitString = NEWLIST(); /* empty list */
    ;
    break;}
case 85:
#line 981 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_BITSTRING, myLineNoG);
        yyval.typePtr->basicType->a.bitString = yyvsp[-1].valueDefListPtr;
    ;
    break;}
case 87:
#line 995 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_NULL, myLineNoG);
    ;
    break;}
case 88:
#line 1002 "core/parse-asn1.y"
{ yyval.intVal = yyvsp[-1].intVal; ;
    break;}
case 89:
#line 1007 "core/parse-asn1.y"
{
        NamedType *n;

        SetupType (&yyval.typePtr, BASICTYPE_SEQUENCE, yyvsp[-2].intVal);

        if (AsnListCount ((AsnList*)yyvsp[-1].namedTypeListPtr) != 0)
        {
            n = (NamedType*) FIRST_LIST_ELMT ((AsnList*)yyvsp[-1].namedTypeListPtr);
            n->type->lineNo = yyvsp[-2].intVal;
        }

        yyval.typePtr->basicType->a.sequence = yyvsp[-1].namedTypeListPtr;

    ;
    break;}
case 90:
#line 1022 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_SEQUENCE, yyvsp[-1].intVal);

        /* set up empty list for SEQ with no elmts */
        yyval.typePtr->basicType->a.sequence = AsnListNew (sizeof (void*));
    ;
    break;}
case 91:
#line 1037 "core/parse-asn1.y"
{
        NamedType *lastElmt;

        if (yyvsp[0].attrList != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT (yyvsp[-1].namedTypeListPtr);
            lastElmt->type->attrList = yyvsp[0].attrList;
        }
        yyval.namedTypeListPtr = yyvsp[-1].namedTypeListPtr;
    ;
    break;}
case 92:
#line 1051 "core/parse-asn1.y"
{
        yyval.namedTypeListPtr = NEWLIST();
        APPEND (yyvsp[0].namedTypePtr,yyval.namedTypeListPtr);
    ;
    break;}
case 93:
#line 1056 "core/parse-asn1.y"
{
        NamedType *lastElmt;

        if (yyvsp[-2].attrList != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT (yyvsp[-4].namedTypeListPtr);
            lastElmt->type->attrList = yyvsp[-2].attrList;
        }

       APPEND (yyvsp[0].namedTypePtr, yyvsp[-4].namedTypeListPtr);
       lastElmt = (NamedType*)LAST_LIST_ELMT (yyvsp[-4].namedTypeListPtr);
       lastElmt->type->lineNo = yyvsp[-1].intVal;
       yyval.namedTypeListPtr = yyvsp[-4].namedTypeListPtr;
    ;
    break;}
case 95:
#line 1075 "core/parse-asn1.y"
{
        yyval.namedTypePtr = yyvsp[-1].namedTypePtr;
        yyval.namedTypePtr->type->optional = TRUE;
    ;
    break;}
case 96:
#line 1080 "core/parse-asn1.y"
{
        /*
         * this rules uses NamedValue instead of Value
         * for the stupid choice value syntax (fieldname value)
         * it should be like a set/seq value (ie with
         * enclosing { }
         */
        yyval.namedTypePtr = yyvsp[-2].namedTypePtr;
        yyval.namedTypePtr->type->defaultVal = yyvsp[0].namedValuePtr;
        /*
         * could link value to the elmt type here (done in link_types.c)
         */
    ;
    break;}
case 97:
#line 1094 "core/parse-asn1.y"
{
        yyval.namedTypePtr = MT (NamedType);
        SetupType (&yyval.namedTypePtr->type, BASICTYPE_COMPONENTSOF, myLineNoG);
        yyval.namedTypePtr->type->basicType->a.componentsOf = yyvsp[0].typePtr;
    ;
    break;}
case 98:
#line 1100 "core/parse-asn1.y"
{
        yyval.namedTypePtr = MT (NamedType);
        SetupType (&yyval.namedTypePtr->type, BASICTYPE_COMPONENTSOF, myLineNoG);
        yyval.namedTypePtr->fieldName = yyvsp[-3].charPtr;
        yyval.namedTypePtr->type->basicType->a.componentsOf = yyvsp[0].typePtr;
    ;
    break;}
case 99:
#line 1112 "core/parse-asn1.y"
{
        NamedType *n;

        /* does not use SEQUENCE == SEQ OF ANY abrev*/
        SetupType (&yyval.typePtr, BASICTYPE_SEQUENCEOF, myLineNoG);

        /* grab line number from first elmt */
        if (yyvsp[0].typePtr != NULL)
            yyval.typePtr->lineNo = yyvsp[0].typePtr->lineNo - 1;

        yyval.typePtr->basicType->a.sequenceOf = yyvsp[0].typePtr;
    ;
    break;}
case 100:
#line 1127 "core/parse-asn1.y"
{ yyval.intVal = yyvsp[-1].intVal; ;
    break;}
case 101:
#line 1132 "core/parse-asn1.y"
{
        NamedType *n;

        SetupType (&yyval.typePtr, BASICTYPE_SET, yyvsp[-2].intVal);

        /* reset first elmt's line number */
        if (AsnListCount ((AsnList*)yyvsp[-1].namedTypeListPtr) != 0)
        {
            n = (NamedType*)FIRST_LIST_ELMT ((AsnList*)yyvsp[-1].namedTypeListPtr);
            n->type->lineNo = yyvsp[-2].intVal;
        }
        yyval.typePtr->basicType->a.set = yyvsp[-1].namedTypeListPtr;
    ;
    break;}
case 102:
#line 1146 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_SET, yyvsp[-1].intVal);

        /* set up empty elmt list for SET */
        yyval.typePtr->basicType->a.set = AsnListNew (sizeof (void*));
    ;
    break;}
case 103:
#line 1162 "core/parse-asn1.y"
{
       /* does not allow SET == SET OF ANY Abrev */
        SetupType (&yyval.typePtr, BASICTYPE_SETOF, myLineNoG);

        if (yyvsp[0].typePtr != NULL)
            yyval.typePtr->lineNo = yyvsp[0].typePtr->lineNo;

        yyval.typePtr->basicType->a.setOf = yyvsp[0].typePtr;
    ;
    break;}
case 104:
#line 1176 "core/parse-asn1.y"
{
        NamedType *n;

        SetupType (&yyval.typePtr, BASICTYPE_CHOICE, yyvsp[-3].intVal);

        yyval.typePtr->basicType->a.choice = yyvsp[-1].namedTypeListPtr;

        if (AsnListCount (yyvsp[-1].namedTypeListPtr) != 0)
        {
            n = (NamedType*)FIRST_LIST_ELMT (yyvsp[-1].namedTypeListPtr);
            n->type->lineNo = yyvsp[-3].intVal;
        }
    ;
    break;}
case 105:
#line 1193 "core/parse-asn1.y"
{
        NamedType *lastElmt;
        if (yyvsp[0].attrList != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT (yyvsp[-1].namedTypeListPtr);
            lastElmt->type->attrList = yyvsp[0].attrList;
        }
        yyval.namedTypeListPtr = yyvsp[-1].namedTypeListPtr;
    ;
    break;}
case 106:
#line 1206 "core/parse-asn1.y"
{
        yyval.namedTypeListPtr = NEWLIST();
        APPEND (yyvsp[0].namedTypePtr, yyval.namedTypeListPtr);
    ;
    break;}
case 107:
#line 1211 "core/parse-asn1.y"
{
        NamedType *lastElmt;

        if (yyvsp[-1].attrList != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT (yyvsp[-3].namedTypeListPtr);
            lastElmt->type->attrList = yyvsp[-1].attrList;
        }
        APPEND (yyvsp[0].namedTypePtr,yyvsp[-3].namedTypeListPtr);
        yyval.namedTypeListPtr = yyvsp[-3].namedTypeListPtr;
    ;
    break;}
case 108:
#line 1227 "core/parse-asn1.y"
{
        /*
         * the selection type should be replaced after
         * link with actual type
         */
        SetupType (&yyval.typePtr, BASICTYPE_SELECTION, myLineNoG);

        yyval.typePtr->basicType->a.selection = MT (SelectionType);
        yyval.typePtr->basicType->a.selection->typeRef = yyvsp[0].typePtr;
        yyval.typePtr->basicType->a.selection->fieldName = yyvsp[-2].charPtr;
    ;
    break;}
case 109:
#line 1242 "core/parse-asn1.y"
{
        Tag *tag;

        /* remove next tag if any  && IMPLICIT_TAGS */
 	if ((modulePtrG->tagDefault == IMPLICIT_TAGS) &&
            (yyvsp[0].typePtr->tags != NULL) && !LIST_EMPTY (yyvsp[0].typePtr->tags))
        {
            tag = (Tag*)FIRST_LIST_ELMT (yyvsp[0].typePtr->tags); /* set curr to first */
	    AsnListFirst (yyvsp[0].typePtr->tags); /* set curr to first elmt */
            AsnListRemove (yyvsp[0].typePtr->tags);      /* remove first elmt */

            /*
             * set implicit if implicitly tagged built in type (ie not ref)
             * (this simplifies the module ASN.1 printer (print.c))
             */
            if (tag->tclass == UNIV)
                 yyvsp[0].typePtr->implicit = TRUE;

            Free (tag);
        }

        PREPEND (yyvsp[-1].tagPtr, yyvsp[0].typePtr->tags);
        yyval.typePtr = yyvsp[0].typePtr;
    ;
    break;}
case 110:
#line 1267 "core/parse-asn1.y"
{
        Tag *tag;

        /* remove next tag if any */
 	if ((yyvsp[0].typePtr->tags != NULL) && !LIST_EMPTY (yyvsp[0].typePtr->tags))
        {
            tag = (Tag*)FIRST_LIST_ELMT (yyvsp[0].typePtr->tags); /* set curr to first */
	    AsnListFirst (yyvsp[0].typePtr->tags); /* set curr to first elmt */
            AsnListRemove (yyvsp[0].typePtr->tags);      /* remove first elmt */

            if (tag->tclass == UNIV)
                 yyvsp[0].typePtr->implicit = TRUE;

            Free (tag);
        }

        /*
         * must check after linking that implicitly tagged
         * local/import type refs are not untagged choice/any etc
         */
        else if ((yyvsp[0].typePtr->basicType->choiceId == BASICTYPE_IMPORTTYPEREF) ||
                 (yyvsp[0].typePtr->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
                 (yyvsp[0].typePtr->basicType->choiceId == BASICTYPE_SELECTION))
            yyvsp[0].typePtr->implicit = TRUE;

        /*
         *  all other implicitly tagable types should have tags
         *  to remove - if this else clause fires then it is
         *  probably a CHOICE or ANY type
         */
        else
        {
            PrintErrLoc (modulePtrG->asn1SrcFileName, yyvsp[0].typePtr->lineNo);
            fprintf (stderr, "ERROR - attempt to implicitly reference untagged type\n");
            smallErrG = 1;
        }

        PREPEND (yyvsp[-2].tagPtr, yyvsp[0].typePtr->tags);
        yyval.typePtr = yyvsp[0].typePtr;
    ;
    break;}
case 111:
#line 1308 "core/parse-asn1.y"
{
        /* insert tag at head of list */
        yyvsp[-2].tagPtr->explicit = TRUE;
        PREPEND (yyvsp[-2].tagPtr, yyvsp[0].typePtr->tags);
        yyval.typePtr = yyvsp[0].typePtr;
    ;
    break;}
case 112:
#line 1318 "core/parse-asn1.y"
{
        yyval.tagPtr = yyvsp[-1].tagPtr;
        yyval.tagPtr->tclass = yyvsp[-2].intVal;
        yyval.tagPtr->explicit = FALSE; /* default to false */

        /*
         *  keep track of APPLICATION Tags per module
         *  should only be used once
         */
        if (yyvsp[-2].intVal == APPL)
        {
            PushApplTag (yyval.tagPtr->code, myLineNoG);
        }
    ;
    break;}
case 113:
#line 1336 "core/parse-asn1.y"
{
        yyval.tagPtr = MT (Tag);
        yyval.tagPtr->code = yyvsp[0].intVal;
    ;
    break;}
case 114:
#line 1341 "core/parse-asn1.y"
{
        yyval.tagPtr = MT (Tag);
        yyval.tagPtr->code = NO_TAG_CODE;
        yyval.tagPtr->valueRef = yyvsp[0].valuePtr;
    ;
    break;}
case 115:
#line 1349 "core/parse-asn1.y"
{ yyval.intVal = UNIV; ;
    break;}
case 116:
#line 1350 "core/parse-asn1.y"
{ yyval.intVal = APPL; ;
    break;}
case 117:
#line 1351 "core/parse-asn1.y"
{ yyval.intVal = PRIV; ;
    break;}
case 118:
#line 1352 "core/parse-asn1.y"
{ yyval.intVal = CNTX; ;
    break;}
case 119:
#line 1358 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_ANY, myLineNoG);
    ;
    break;}
case 120:
#line 1362 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_ANYDEFINEDBY, myLineNoG);
        yyval.typePtr->basicType->a.anyDefinedBy = MT (AnyDefinedByType);
        yyval.typePtr->basicType->a.anyDefinedBy->fieldName = yyvsp[0].charPtr;
    ;
    break;}
case 121:
#line 1372 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_OID, myLineNoG);
    ;
    break;}
case 122:
#line 1380 "core/parse-asn1.y"
{
        /*
         * append new subtype list to existing one (s) if any
         * with AND relation
         */
        AppendSubtype (&yyvsp[-1].typePtr->subtypes, yyvsp[0].subtypePtr, SUBTYPE_AND);
        yyval.typePtr = yyvsp[-1].typePtr;
    ;
    break;}
case 123:
#line 1389 "core/parse-asn1.y"
{
        Subtype *s;

        SetupType (&yyval.typePtr, BASICTYPE_SETOF, myLineNoG);
        yyval.typePtr->basicType->a.setOf = yyvsp[0].typePtr;

        /* add size constraint */
        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = yyvsp[-2].subtypeValuePtr;
        AppendSubtype (&yyval.typePtr->subtypes, s, SUBTYPE_AND);
    ;
    break;}
case 124:
#line 1402 "core/parse-asn1.y"
{
        Subtype *s;

        SetupType (&yyval.typePtr, BASICTYPE_SEQUENCEOF, myLineNoG);
        yyval.typePtr->basicType->a.sequenceOf = yyvsp[0].typePtr;

        /* add size constraint */
        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = yyvsp[-2].subtypeValuePtr;
        AppendSubtype (&yyval.typePtr->subtypes, s, SUBTYPE_AND);
    ;
    break;}
case 125:
#line 1419 "core/parse-asn1.y"
{
        yyval.subtypePtr = yyvsp[-1].subtypePtr;
    ;
    break;}
case 126:
#line 1426 "core/parse-asn1.y"
{
        Subtype *s;

        /* OR relation between all elmts of in  ValueSetList */

        yyval.subtypePtr = MT (Subtype);
        yyval.subtypePtr->choiceId = SUBTYPE_OR;
        yyval.subtypePtr->a.or = NEWLIST();

        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = yyvsp[0].subtypeValuePtr;
        APPEND (s, yyval.subtypePtr->a.or);
    ;
    break;}
case 127:
#line 1441 "core/parse-asn1.y"
{
        Subtype *s;
        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = yyvsp[0].subtypeValuePtr;
        APPEND (s, yyvsp[-2].subtypePtr->a.or);
        yyval.subtypePtr = yyvsp[-2].subtypePtr;
    ;
    break;}
case 134:
#line 1463 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_SINGLEVALUE;
        yyval.subtypeValuePtr->a.singleValue = yyvsp[0].valuePtr;
    ;
    break;}
case 135:
#line 1471 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_CONTAINED;
        yyval.subtypeValuePtr->a.contained = yyvsp[0].typePtr;
    ;
    break;}
case 136:
#line 1480 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_VALUERANGE;
        yyval.subtypeValuePtr->a.valueRange = MT (ValueRangeSubtype);
        yyval.subtypeValuePtr->a.valueRange->lowerEndInclusive =
            valueRangeLowerEndInclusiveG;
        yyval.subtypeValuePtr->a.valueRange->upperEndInclusive =
            valueRangeUpperEndInclusiveG;
        yyval.subtypeValuePtr->a.valueRange->lowerEndValue  = yyvsp[-3].valuePtr;
        yyval.subtypeValuePtr->a.valueRange->upperEndValue = yyvsp[0].valuePtr;
    ;
    break;}
case 137:
#line 1495 "core/parse-asn1.y"
{
       yyval.valuePtr = yyvsp[0].valuePtr;
       valueRangeLowerEndInclusiveG = TRUE;
    ;
    break;}
case 138:
#line 1500 "core/parse-asn1.y"
{
       yyval.valuePtr = yyvsp[-1].valuePtr;
       valueRangeLowerEndInclusiveG = FALSE;
    ;
    break;}
case 139:
#line 1508 "core/parse-asn1.y"
{
       yyval.valuePtr = yyvsp[0].valuePtr;
       valueRangeUpperEndInclusiveG = TRUE;
    ;
    break;}
case 140:
#line 1513 "core/parse-asn1.y"
{
       yyval.valuePtr = yyvsp[0].valuePtr;
       valueRangeUpperEndInclusiveG = FALSE;
    ;
    break;}
case 141:
#line 1520 "core/parse-asn1.y"
{ yyval.valuePtr = yyvsp[0].valuePtr; ;
    break;}
case 142:
#line 1522 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_SPECIALINTEGER, myLineNoG);
        yyval.valuePtr->basicValue->a.specialInteger =  MIN_INT;
    ;
    break;}
case 143:
#line 1529 "core/parse-asn1.y"
{ yyval.valuePtr = yyvsp[0].valuePtr; ;
    break;}
case 144:
#line 1531 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_SPECIALINTEGER, myLineNoG);
        yyval.valuePtr->basicValue->a.specialInteger =  MAX_INT;
    ;
    break;}
case 145:
#line 1539 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_SIZECONSTRAINT;
        yyval.subtypeValuePtr->a.sizeConstraint = yyvsp[0].subtypePtr;
    ;
    break;}
case 146:
#line 1549 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_PERMITTEDALPHABET;
        yyval.subtypeValuePtr->a.permittedAlphabet = yyvsp[0].subtypePtr;
    ;
    break;}
case 147:
#line 1558 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_INNERSUBTYPE;
        yyval.subtypeValuePtr->a.innerSubtype = yyvsp[0].innerSubtypePtr;
    ;
    break;}
case 148:
#line 1564 "core/parse-asn1.y"
{
        yyval.subtypeValuePtr = MT (SubtypeValue);
        yyval.subtypeValuePtr->choiceId = SUBTYPEVALUE_INNERSUBTYPE;
        yyval.subtypeValuePtr->a.innerSubtype = yyvsp[0].innerSubtypePtr;
    ;
    break;}
case 149:
#line 1573 "core/parse-asn1.y"
{
        Constraint *constraint;

        /* this constrains the elmt of setof or seq of */
        yyval.innerSubtypePtr = MT (InnerSubtype);
        yyval.innerSubtypePtr->constraintType = SINGLE_CT;
        yyval.innerSubtypePtr->constraints = NEWLIST();
        constraint = MT (Constraint);
        APPEND (constraint, yyval.innerSubtypePtr->constraints);
        constraint->valueConstraints = yyvsp[0].subtypePtr;
    ;
    break;}
case 152:
#line 1593 "core/parse-asn1.y"
{
        yyval.innerSubtypePtr = MT (InnerSubtype);
        yyval.innerSubtypePtr->constraintType = FULL_CT;
        yyval.innerSubtypePtr->constraints = yyvsp[-1].constraintListPtr;
    ;
    break;}
case 153:
#line 1602 "core/parse-asn1.y"
{
        yyval.innerSubtypePtr = MT (InnerSubtype);
        yyval.innerSubtypePtr->constraintType = PARTIAL_CT;
        yyval.innerSubtypePtr->constraints = yyvsp[-1].constraintListPtr;
    ;
    break;}
case 154:
#line 1612 "core/parse-asn1.y"
{
        yyval.constraintListPtr = NEWLIST();
        APPEND (yyvsp[0].constraintPtr, yyval.constraintListPtr);
    ;
    break;}
case 155:
#line 1617 "core/parse-asn1.y"
{
        APPEND (yyvsp[0].constraintPtr, yyvsp[-2].constraintListPtr);
        yyval.constraintListPtr = yyvsp[-2].constraintListPtr;
    ;
    break;}
case 156:
#line 1625 "core/parse-asn1.y"
{
        yyval.constraintPtr = yyvsp[0].constraintPtr;
        yyval.constraintPtr->fieldRef = yyvsp[-1].charPtr;
    ;
    break;}
case 158:
#line 1635 "core/parse-asn1.y"
{
        yyval.constraintPtr = MT (Constraint);
        yyval.constraintPtr->presenceConstraint = yyvsp[0].intVal;
        yyval.constraintPtr->valueConstraints = yyvsp[-1].subtypePtr;
    ;
    break;}
case 159:
#line 1643 "core/parse-asn1.y"
{ yyval.subtypePtr = yyvsp[0].subtypePtr; ;
    break;}
case 160:
#line 1644 "core/parse-asn1.y"
{ yyval.subtypePtr = NULL; ;
    break;}
case 161:
#line 1648 "core/parse-asn1.y"
{ yyval.intVal = PRESENT_CT; ;
    break;}
case 162:
#line 1649 "core/parse-asn1.y"
{ yyval.intVal = ABSENT_CT; ;
    break;}
case 163:
#line 1650 "core/parse-asn1.y"
{ yyval.intVal = EMPTY_CT; ;
    break;}
case 164:
#line 1651 "core/parse-asn1.y"
{ yyval.intVal = OPTIONAL_CT; ;
    break;}
case 165:
#line 1665 "core/parse-asn1.y"
{
        yyval.valueDefPtr = MT (ValueDef);
        yyval.valueDefPtr->definedName = yyvsp[-4].charPtr;
        yyval.valueDefPtr->value = yyvsp[0].valuePtr;
        yyval.valueDefPtr->value->lineNo = yyvsp[-1].intVal;
        yyval.valueDefPtr->value->type = yyvsp[-3].typePtr;
    ;
    break;}
case 168:
#line 1681 "core/parse-asn1.y"
{ yyval.valuePtr =  yyvsp[0].valuePtr; ;
    break;}
case 169:
#line 1683 "core/parse-asn1.y"
{
        /*
         * for parse, may be set to BASICVALUE_IMPORTEDTYPEREF
         * by linker
         */
        SetupValue (&yyval.valuePtr, BASICVALUE_LOCALVALUEREF, myLineNoG);
        yyval.valuePtr->basicValue->a.localValueRef = MT (ValueRef);
        yyval.valuePtr->basicValue->a.localValueRef->valueName = yyvsp[0].charPtr;
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
   ;
    break;}
case 170:
#line 1697 "core/parse-asn1.y"
{
        /* Alloc value with basicValue of importValueRef */
        SetupValue (&yyval.valuePtr, BASICVALUE_IMPORTVALUEREF, yyvsp[-1].intVal);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.importValueRef = MT (ValueRef);
        yyval.valuePtr->basicValue->a.importValueRef->valueName = yyvsp[0].charPtr;
        yyval.valuePtr->basicValue->a.importValueRef->moduleName = yyvsp[-3].charPtr;

        /* add entry to this module's import list */
        AddPrivateImportElmt (modulePtrG, yyvsp[0].charPtr, yyvsp[-3].charPtr, yyvsp[-1].intVal);
    ;
    break;}
case 174:
#line 1715 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_INTEGER, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.integer = yyvsp[0].intVal;
    ;
    break;}
case 175:
#line 1721 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_ASCIIHEX, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.asciiHex = MT (AsnOcts);
        yyval.valuePtr->basicValue->a.asciiHex->octs = yyvsp[0].charPtr;
        yyval.valuePtr->basicValue->a.asciiHex->octetLen = strlen (yyvsp[0].charPtr);
    ;
    break;}
case 176:
#line 1729 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_ASCIIBITSTRING, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.asciiBitString = MT (AsnOcts);
        yyval.valuePtr->basicValue->a.asciiBitString->octs = yyvsp[0].charPtr;
        yyval.valuePtr->basicValue->a.asciiBitString->octetLen = strlen (yyvsp[0].charPtr);
    ;
    break;}
case 177:
#line 1737 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_ASCIITEXT, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.asciiText = MT (AsnOcts);
        yyval.valuePtr->basicValue->a.asciiText->octs = yyvsp[0].charPtr;
        yyval.valuePtr->basicValue->a.asciiText->octetLen = strlen (yyvsp[0].charPtr);
    ;
    break;}
case 178:
#line 1744 "core/parse-asn1.y"
{ LexBeginBraceBalContext(); ;
    break;}
case 179:
#line 1745 "core/parse-asn1.y"
{
        /*
         *  LEXICAL TIE IN!!
         * string returned by BRACEBAL_SYM has
         * the $1 '{' prepended and includes everything
         * upto and including '}' that balances $1
         */
        LexBeginInitialContext();
        SetupValue (&yyval.valuePtr, BASICVALUE_VALUENOTATION, myLineNoG);
        yyval.valuePtr->basicValue->a.valueNotation = MT (AsnOcts);
        yyval.valuePtr->basicValue->a.valueNotation->octs = yyvsp[0].charPtr;
        yyval.valuePtr->basicValue->a.valueNotation->octetLen = strlen (yyvsp[0].charPtr);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
    ;
    break;}
case 180:
#line 1763 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_BOOLEAN, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.boolean =  TRUE;
    ;
    break;}
case 181:
#line 1769 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_BOOLEAN, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.boolean = FALSE;
    ;
    break;}
case 182:
#line 1779 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_SPECIALREAL, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.specialReal =  PLUS_INFINITY_REAL;
    ;
    break;}
case 183:
#line 1785 "core/parse-asn1.y"
{
        SetupValue (&yyval.valuePtr, BASICVALUE_SPECIALREAL, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
        yyval.valuePtr->basicValue->a.specialReal = MINUS_INFINITY_REAL;
    ;
    break;}
case 184:
#line 1796 "core/parse-asn1.y"
{
        /* create a NULL value  */
        SetupValue (&yyval.valuePtr, BASICVALUE_NULL, myLineNoG);
        yyval.valuePtr->valueType = BASICTYPE_UNKNOWN;
    ;
    break;}
case 185:
#line 1806 "core/parse-asn1.y"
{
        yyval.namedValuePtr = MT (NamedValue);
        yyval.namedValuePtr->value = yyvsp[0].valuePtr;
    ;
    break;}
case 186:
#line 1811 "core/parse-asn1.y"
{
        yyval.namedValuePtr = MT (NamedValue);
        yyval.namedValuePtr->value = yyvsp[0].valuePtr;
        yyval.namedValuePtr->fieldName = yyvsp[-1].charPtr;
    ;
    break;}
case 187:
#line 1821 "core/parse-asn1.y"
{
        /*
         * example OID setup
         *
         * for { ccitt foo (1) bar bell (bunt) 2 }
         *
         * ccitt
         *   - arcnum is set to number from oid table (oid.c)
         * foo (1)
         *   - sets up a new value def foo defined as 1
         *   - makes oid valueref a value ref to foo (doesn't link it tho)
         * bar
         *   - makes oid valueref a value ref to bar (doesn't link it tho)
         * bell (bunt)
         *   - sets up a new value def bell defined as a val ref to bunt
         *   - makes oid valueref a value ref to bell (doesn't link it tho)
         * 2
         *  - arcnum is set to 2
         */

        yyval.oidPtr = yyvsp[-1].oidPtr;
    ;
    break;}
case 188:
#line 1848 "core/parse-asn1.y"
{
        OID *o;
        /* append component */
        for (o = yyvsp[-1].oidPtr; o->next != NULL; o = o->next)
	    ;
        o->next = yyvsp[0].oidPtr;
        yyval.oidPtr = yyvsp[-1].oidPtr;
    ;
    break;}
case 191:
#line 1864 "core/parse-asn1.y"
{
        Value *newVal;
        /*
         * if the arcName is a defined arc name like
         * ccitt or iso etc, fill in the arc number.
         * otherwise make a value ref to that named value
         */
        yyval.oidPtr = MT (OID);

        yyval.oidPtr->arcNum = OidArcNameToNum (yyvsp[0].charPtr);
        if (yyval.oidPtr->arcNum == NULL_OID_ARCNUM)
        {
            /* set up value ref to named value */
            SetupValue (&newVal, BASICVALUE_LOCALVALUEREF, myLineNoG);
            newVal->basicValue->a.localValueRef = MT (ValueRef);
            newVal->valueType = BASICTYPE_INTEGER;
            newVal->basicValue->a.localValueRef->valueName = yyvsp[0].charPtr;
            yyval.oidPtr->valueRef = newVal;
        }
    ;
    break;}
case 193:
#line 1890 "core/parse-asn1.y"
{
        yyval.oidPtr = MT (OID);
        yyval.oidPtr->arcNum = yyvsp[0].intVal;
    ;
    break;}
case 195:
#line 1903 "core/parse-asn1.y"
{
        Value *newVal;

        yyval.oidPtr = yyvsp[-1].oidPtr;

        /* shared refs to named numbers name */
        SetupValue (&newVal, BASICVALUE_INTEGER, myLineNoG);
        newVal->basicValue->a.integer = yyval.oidPtr->arcNum;
        newVal->valueType = BASICTYPE_INTEGER;
        AddNewValueDef (oidElmtValDefsG, yyvsp[-3].charPtr, newVal);

        SetupValue (&newVal, BASICVALUE_LOCALVALUEREF, myLineNoG);
        newVal->basicValue->a.localValueRef = MT (ValueRef);
        newVal->basicValue->a.localValueRef->valueName = yyvsp[-3].charPtr;

        yyval.oidPtr->valueRef = newVal;
    ;
    break;}
case 196:
#line 1921 "core/parse-asn1.y"
{
        Value *newVal;

        /* shared refs to named numbers name */
        yyval.oidPtr = MT (OID);
        yyval.oidPtr->arcNum = NULL_OID_ARCNUM;

        AddNewValueDef (oidElmtValDefsG, yyvsp[-3].charPtr, yyvsp[-1].valuePtr);

        SetupValue (&newVal, BASICVALUE_LOCALVALUEREF, myLineNoG);
        newVal->basicValue->a.localValueRef = MT (ValueRef);
        newVal->basicValue->a.localValueRef->valueName = yyvsp[-3].charPtr;

        yyval.oidPtr->valueRef = newVal;
    ;
    break;}
case 200:
#line 1955 "core/parse-asn1.y"
{
	if (yyvsp[0].uintVal>0x7FFFFFFF) {
	    yyerror("Warning: number out of range");
	    yyval.intVal = 0x7FFFFFFF;
	}
	;
    break;}
case 201:
#line 1962 "core/parse-asn1.y"
{
	yyerror ("Warning: number out of range");
	yyval.intVal = 0x7FFFFFFF;
	/* modulePtrG->status = MOD_ERROR; */
	;
    break;}
case 207:
#line 1990 "core/parse-asn1.y"
{yyval.attrList = NULL;;
    break;}
case 208:
#line 1995 "core/parse-asn1.y"
{
        yyval.attrList = NEWLIST();
        APPEND (yyvsp[0].charPtr,yyval.attrList);
    ;
    break;}
case 209:
#line 2000 "core/parse-asn1.y"
{
        APPEND (yyvsp[0].charPtr,yyvsp[-1].attrList);
        yyval.attrList = yyvsp[-1].attrList;
    ;
    break;}
case 235:
#line 2039 "core/parse-asn1.y"
{ yyval.charPtr = "OPERATION"; ;
    break;}
case 236:
#line 2040 "core/parse-asn1.y"
{ yyval.charPtr = "ERROR"; ;
    break;}
case 237:
#line 2041 "core/parse-asn1.y"
{ yyval.charPtr = "BIND"; ;
    break;}
case 238:
#line 2042 "core/parse-asn1.y"
{ yyval.charPtr = "UNBIND"; ;
    break;}
case 239:
#line 2043 "core/parse-asn1.y"
{ yyval.charPtr = "APPLICATION-SERVICE-ELEMENT"; ;
    break;}
case 240:
#line 2044 "core/parse-asn1.y"
{ yyval.charPtr = "APPLICATION-CONTEXT"; ;
    break;}
case 241:
#line 2045 "core/parse-asn1.y"
{ yyval.charPtr = "EXTENSION"; ;
    break;}
case 242:
#line 2046 "core/parse-asn1.y"
{ yyval.charPtr = "EXTENSIONS"; ;
    break;}
case 243:
#line 2047 "core/parse-asn1.y"
{ yyval.charPtr = "EXTENSION-ATTRIBUTE"; ;
    break;}
case 244:
#line 2048 "core/parse-asn1.y"
{ yyval.charPtr = "TOKEN"; ;
    break;}
case 245:
#line 2049 "core/parse-asn1.y"
{ yyval.charPtr = "TOKEN-DATA"; ;
    break;}
case 246:
#line 2050 "core/parse-asn1.y"
{ yyval.charPtr = "SECURITY-CATEGORY"; ;
    break;}
case 247:
#line 2051 "core/parse-asn1.y"
{ yyval.charPtr = "OBJECT"; ;
    break;}
case 248:
#line 2052 "core/parse-asn1.y"
{ yyval.charPtr = "PORT"; ;
    break;}
case 249:
#line 2053 "core/parse-asn1.y"
{ yyval.charPtr = "REFINE"; ;
    break;}
case 250:
#line 2054 "core/parse-asn1.y"
{ yyval.charPtr = "ABSTRACT-BIND"; ;
    break;}
case 251:
#line 2055 "core/parse-asn1.y"
{ yyval.charPtr = "ABSTRACT-UNBIND"; ;
    break;}
case 252:
#line 2056 "core/parse-asn1.y"
{ yyval.charPtr = "ABSTRACT-OPERATION"; ;
    break;}
case 253:
#line 2057 "core/parse-asn1.y"
{ yyval.charPtr = "ABSTRACT-ERROR"; ;
    break;}
case 254:
#line 2058 "core/parse-asn1.y"
{ yyval.charPtr = "ALGORITHM"; ;
    break;}
case 255:
#line 2059 "core/parse-asn1.y"
{ yyval.charPtr = "ENCRYPTED"; ;
    break;}
case 256:
#line 2060 "core/parse-asn1.y"
{ yyval.charPtr = "SIGNED"; ;
    break;}
case 257:
#line 2061 "core/parse-asn1.y"
{ yyval.charPtr = "SIGNATURE"; ;
    break;}
case 258:
#line 2062 "core/parse-asn1.y"
{ yyval.charPtr = "PROTECTED"; ;
    break;}
case 259:
#line 2063 "core/parse-asn1.y"
{ yyval.charPtr = "OBJECT-TYPE"; ;
    break;}
case 260:
#line 2072 "core/parse-asn1.y"
{ yyval.typePtr = yyvsp[0].typePtr; ;
    break;}
case 261:
#line 2077 "core/parse-asn1.y"
{
        RosOperationMacroType *r;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSOPERATION, myLineNoG);
        r = yyval.typePtr->basicType->a.macroType->a.rosOperation  =
            MT (RosOperationMacroType);
        r->arguments = yyvsp[-3].namedTypePtr;
        r->result    = yyvsp[-2].namedTypePtr;
        r->errors    = yyvsp[-1].typeOrValueListPtr;
        r->linkedOps = yyvsp[0].typeOrValueListPtr;
    ;
    break;}
case 262:
#line 2092 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 263:
#line 2093 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 264:
#line 2097 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 265:
#line 2098 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 267:
#line 2104 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 268:
#line 2110 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = yyvsp[-1].typeOrValueListPtr;
    ;
    break;}
case 269:
#line 2113 "core/parse-asn1.y"
{ yyval.typeOrValueListPtr = NULL; ;
    break;}
case 270:
#line 2120 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = yyvsp[-1].typeOrValueListPtr;
    ;
    break;}
case 271:
#line 2123 "core/parse-asn1.y"
{ yyval.typeOrValueListPtr = NULL; ;
    break;}
case 272:
#line 2136 "core/parse-asn1.y"
{
        RosErrorMacroType *r;
        /*
         * defines error macro type
         */
        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSERROR, myLineNoG);
        r = yyval.typePtr->basicType->a.macroType->a.rosError = MT (RosErrorMacroType);
        r->parameter = yyvsp[0].namedTypePtr;
    ;
    break;}
case 273:
#line 2149 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 274:
#line 2150 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 275:
#line 2160 "core/parse-asn1.y"
{
        RosBindMacroType *r;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSBIND, myLineNoG);

        r = yyval.typePtr->basicType->a.macroType->a.rosBind = MT (RosBindMacroType);
        r->argument  = yyvsp[-2].namedTypePtr;
        r->result = yyvsp[-1].namedTypePtr;
        r->error  = yyvsp[0].namedTypePtr;
    ;
    break;}
case 276:
#line 2173 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 277:
#line 2174 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 278:
#line 2179 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 279:
#line 2180 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 280:
#line 2185 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 281:
#line 2186 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 282:
#line 2196 "core/parse-asn1.y"
{
        RosBindMacroType *r;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSUNBIND, myLineNoG);

        r = yyval.typePtr->basicType->a.macroType->a.rosUnbind = MT (RosBindMacroType);
        r->argument = yyvsp[-2].namedTypePtr;
        r->result = yyvsp[-1].namedTypePtr;
        r->error  = yyvsp[0].namedTypePtr;
    ;
    break;}
case 283:
#line 2210 "core/parse-asn1.y"
{ yyval.namedTypePtr = yyvsp[0].namedTypePtr; ;
    break;}
case 284:
#line 2211 "core/parse-asn1.y"
{ yyval.namedTypePtr = NULL; ;
    break;}
case 285:
#line 2221 "core/parse-asn1.y"
{
        RosAseMacroType *r;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSASE, myLineNoG);
        r = yyval.typePtr->basicType->a.macroType->a.rosAse  = MT (RosAseMacroType);
        r->operations = yyvsp[0].valueListPtr;
    ;
    break;}
case 286:
#line 2229 "core/parse-asn1.y"
{
        RosAseMacroType *r;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSASE, myLineNoG);
        r = yyval.typePtr->basicType->a.macroType->a.rosAse  = MT (RosAseMacroType);
        r->consumerInvokes = yyvsp[-1].valueListPtr;
        r->supplierInvokes = yyvsp[0].valueListPtr;
    ;
    break;}
case 287:
#line 2242 "core/parse-asn1.y"
{
        yyval.valueListPtr = yyvsp[-1].valueListPtr;
    ;
    break;}
case 288:
#line 2250 "core/parse-asn1.y"
{
        yyval.valueListPtr = yyvsp[-1].valueListPtr;
    ;
    break;}
case 289:
#line 2253 "core/parse-asn1.y"
{ yyval.valueListPtr = NULL; ;
    break;}
case 290:
#line 2259 "core/parse-asn1.y"
{
        yyval.valueListPtr = yyvsp[-1].valueListPtr;
    ;
    break;}
case 291:
#line 2262 "core/parse-asn1.y"
{ yyval.valueListPtr = NULL; ;
    break;}
case 293:
#line 2282 "core/parse-asn1.y"
{
        RosAcMacroType *r;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ROSAC, myLineNoG);
        r = yyval.typePtr->basicType->a.macroType->a.rosAc = MT (RosAcMacroType);
        r->nonRoElements = yyvsp[-6].valueListPtr;
        r->bindMacroType = yyvsp[-4].typePtr;
        r->unbindMacroType = yyvsp[-2].typePtr;
        r->remoteOperations = yyvsp[-1].valuePtr;
        r->operationsOf = rosAcSymmetricAsesG;
        r->initiatorConsumerOf = rosAcInitiatorConsumerOfG;
        r->responderConsumerOf = rosAcResponderConsumerOfG;
        r->abstractSyntaxes = yyvsp[0].oidListPtr;
    ;
    break;}
case 294:
#line 2301 "core/parse-asn1.y"
{
        yyval.valueListPtr = yyvsp[-1].valueListPtr;
    ;
    break;}
case 295:
#line 2310 "core/parse-asn1.y"
{
        yyval.valuePtr = yyvsp[-3].valuePtr;
    ;
    break;}
case 296:
#line 2314 "core/parse-asn1.y"
{
        yyval.valuePtr = NULL;
        rosAcSymmetricAsesG = NULL;
        rosAcInitiatorConsumerOfG = NULL;
        rosAcResponderConsumerOfG = NULL;
    ;
    break;}
case 297:
#line 2324 "core/parse-asn1.y"
{
        rosAcSymmetricAsesG = yyvsp[-1].valueListPtr;
    ;
    break;}
case 298:
#line 2327 "core/parse-asn1.y"
{ rosAcSymmetricAsesG = NULL; ;
    break;}
case 300:
#line 2336 "core/parse-asn1.y"
{
        rosAcInitiatorConsumerOfG = yyvsp[-1].valueListPtr;
    ;
    break;}
case 301:
#line 2339 "core/parse-asn1.y"
{ rosAcInitiatorConsumerOfG = NULL; ;
    break;}
case 302:
#line 2344 "core/parse-asn1.y"
{
        rosAcResponderConsumerOfG = yyvsp[-1].valueListPtr;
    ;
    break;}
case 303:
#line 2347 "core/parse-asn1.y"
{ rosAcResponderConsumerOfG = NULL; ;
    break;}
case 304:
#line 2352 "core/parse-asn1.y"
{
        yyval.oidListPtr = yyvsp[-1].oidListPtr;
    ;
    break;}
case 305:
#line 2355 "core/parse-asn1.y"
{ yyval.oidListPtr = NULL; ;
    break;}
case 306:
#line 2361 "core/parse-asn1.y"
{
        yyval.oidListPtr = NEWLIST();
        APPEND (yyvsp[0].oidPtr,yyval.oidListPtr);
    ;
    break;}
case 307:
#line 2366 "core/parse-asn1.y"
{
        APPEND (yyvsp[0].oidPtr, yyvsp[-2].oidListPtr);
        yyval.oidListPtr = yyvsp[-2].oidListPtr;
    ;
    break;}
case 308:
#line 2380 "core/parse-asn1.y"
{
          MtsasExtensionsMacroType *m;

          SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASEXTENSIONS, myLineNoG);
          m = yyval.typePtr->basicType->a.macroType->a.mtsasExtensions =
              MT (MtsasExtensionsMacroType);
          m->extensions = yyvsp[-1].valueListPtr;
      ;
    break;}
case 310:
#line 2393 "core/parse-asn1.y"
{ yyval.valueListPtr = NULL; ;
    break;}
case 311:
#line 2398 "core/parse-asn1.y"
{
         yyval.valueListPtr = NEWLIST();
         APPEND (yyvsp[0].valuePtr, yyval.valueListPtr);
     ;
    break;}
case 312:
#line 2403 "core/parse-asn1.y"
{
         APPEND (yyvsp[0].valuePtr,yyvsp[-2].valueListPtr);
         yyval.valueListPtr = yyvsp[-2].valueListPtr;
     ;
    break;}
case 314:
#line 2411 "core/parse-asn1.y"
{ yyval.typeOrValueListPtr = NULL; ;
    break;}
case 315:
#line 2416 "core/parse-asn1.y"
{
         yyval.typeOrValueListPtr = NEWLIST();
         APPEND (yyvsp[0].typeOrValuePtr, yyval.typeOrValueListPtr);
     ;
    break;}
case 316:
#line 2421 "core/parse-asn1.y"
{
         APPEND (yyvsp[0].typeOrValuePtr,yyvsp[-2].typeOrValueListPtr);
         yyval.typeOrValueListPtr = yyvsp[-2].typeOrValueListPtr;
     ;
    break;}
case 317:
#line 2429 "core/parse-asn1.y"
{
         yyval.typeOrValuePtr = MT (TypeOrValue);
         yyval.typeOrValuePtr->choiceId = TYPEORVALUE_TYPE;
         yyval.typeOrValuePtr->a.type = yyvsp[0].typePtr;
     ;
    break;}
case 318:
#line 2435 "core/parse-asn1.y"
{
         yyval.typeOrValuePtr = MT (TypeOrValue);
         yyval.typeOrValuePtr->choiceId = TYPEORVALUE_VALUE;
         yyval.typeOrValuePtr->a.value = yyvsp[0].valuePtr;
     ;
    break;}
case 319:
#line 2448 "core/parse-asn1.y"
{
        MtsasExtensionMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASEXTENSION, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasExtension =
            MT (MtsasExtensionMacroType);
        m->elmtType = yyvsp[-2].namedTypePtr;
        m->defaultValue = yyvsp[-1].valuePtr;
        m->criticalForSubmission = mtsasCriticalForSubmissionG;
        m->criticalForTransfer = mtsasCriticalForTransferG;
        m->criticalForDelivery = mtsasCriticalForDeliveryG;

        mtsasCriticalForSubmissionG = NULL;  /* set up for next parse */
        mtsasCriticalForTransferG = NULL;
        mtsasCriticalForDeliveryG = NULL;
    ;
    break;}
case 320:
#line 2465 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASEXTENSION, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.mtsasExtension =
            MT (MtsasExtensionMacroType);
        /*
         * all fields are NULL in the MtsasExtensionsMacroType
         * for this production
         */
    ;
    break;}
case 321:
#line 2477 "core/parse-asn1.y"
{ yyval.valuePtr = yyvsp[0].valuePtr; ;
    break;}
case 322:
#line 2478 "core/parse-asn1.y"
{ yyval.valuePtr = NULL; ;
    break;}
case 327:
#line 2494 "core/parse-asn1.y"
{
        mtsasCriticalForSubmissionG = MT (AsnBool);
        *mtsasCriticalForSubmissionG = TRUE;
    ;
    break;}
case 328:
#line 2499 "core/parse-asn1.y"
{
        mtsasCriticalForTransferG = MT (AsnBool);
        *mtsasCriticalForTransferG = TRUE;
    ;
    break;}
case 329:
#line 2504 "core/parse-asn1.y"
{
        mtsasCriticalForDeliveryG = MT (AsnBool);
        *mtsasCriticalForDeliveryG = TRUE;
    ;
    break;}
case 330:
#line 2518 "core/parse-asn1.y"
{
        MtsasExtensionAttributeMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASEXTENSIONATTRIBUTE, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasExtensionAttribute =
            MT (MtsasExtensionAttributeMacroType);
        m->type = NULL;
    ;
    break;}
case 331:
#line 2527 "core/parse-asn1.y"
{
        MtsasExtensionAttributeMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASEXTENSIONATTRIBUTE, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasExtensionAttribute =
            MT (MtsasExtensionAttributeMacroType);
        m->type = yyvsp[0].typePtr;
    ;
    break;}
case 332:
#line 2543 "core/parse-asn1.y"
{
        MtsasTokenMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASTOKEN, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasToken = MT (MtsasTokenMacroType);
        m->type = NULL;
    ;
    break;}
case 333:
#line 2551 "core/parse-asn1.y"
{
        MtsasTokenMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASTOKEN, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasToken = MT (MtsasTokenMacroType);
        m->type = yyvsp[0].typePtr;
    ;
    break;}
case 334:
#line 2566 "core/parse-asn1.y"
{
        MtsasTokenDataMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASTOKENDATA, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasTokenData =
            MT (MtsasTokenDataMacroType);
        m->type = NULL;
    ;
    break;}
case 335:
#line 2575 "core/parse-asn1.y"
{
        MtsasTokenDataMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASTOKENDATA, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasTokenData =
            MT (MtsasTokenDataMacroType);
        m->type = yyvsp[0].typePtr;
    ;
    break;}
case 336:
#line 2591 "core/parse-asn1.y"
{
        MtsasSecurityCategoryMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASSECURITYCATEGORY, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasSecurityCategory =
            MT (MtsasSecurityCategoryMacroType);
        m->type = NULL;
    ;
    break;}
case 337:
#line 2600 "core/parse-asn1.y"
{
        MtsasSecurityCategoryMacroType *m;

        SetupMacroType (&yyval.typePtr, MACROTYPE_MTSASSECURITYCATEGORY, myLineNoG);
        m = yyval.typePtr->basicType->a.macroType->a.mtsasSecurityCategory =
            MT (MtsasSecurityCategoryMacroType);
        m->type = yyvsp[0].typePtr;
    ;
    break;}
case 338:
#line 2622 "core/parse-asn1.y"
{
        AsnObjectMacroType *a;
        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNOBJECT, myLineNoG);
        a = yyval.typePtr->basicType->a.macroType->a.asnObject = MT (AsnObjectMacroType);
        a->ports = yyvsp[0].asnPortListPtr;
    ;
    break;}
case 339:
#line 2632 "core/parse-asn1.y"
{
        yyval.asnPortListPtr = yyvsp[-1].asnPortListPtr;
    ;
    break;}
case 340:
#line 2635 "core/parse-asn1.y"
{ yyval.asnPortListPtr = NULL; ;
    break;}
case 341:
#line 2640 "core/parse-asn1.y"
{
        yyval.asnPortListPtr = NEWLIST();
        APPEND (yyvsp[0].asnPortPtr, yyval.asnPortListPtr);
    ;
    break;}
case 342:
#line 2645 "core/parse-asn1.y"
{
        APPEND (yyvsp[0].asnPortPtr, yyvsp[-2].asnPortListPtr);
        yyval.asnPortListPtr = yyvsp[-2].asnPortListPtr;
    ;
    break;}
case 343:
#line 2653 "core/parse-asn1.y"
{
        yyval.asnPortPtr = MT (AsnPort);
        yyval.asnPortPtr->portValue = yyvsp[-1].valuePtr;
        yyval.asnPortPtr->portType = yyvsp[0].intVal;
    ;
    break;}
case 344:
#line 2662 "core/parse-asn1.y"
{
        /* [C] consumer */
        yyval.intVal = CONSUMER_PORT;
    ;
    break;}
case 345:
#line 2667 "core/parse-asn1.y"
{
        /* [S] supplier */
        yyval.intVal = SUPPLIER_PORT;
    ;
    break;}
case 346:
#line 2672 "core/parse-asn1.y"
{
       /* symmetric */
        yyval.intVal = SYMMETRIC_PORT;
    ;
    break;}
case 347:
#line 2685 "core/parse-asn1.y"
{
        AsnPortMacroType *a;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNPORT, myLineNoG);
        a = yyval.typePtr->basicType->a.macroType->a.asnPort = MT (AsnPortMacroType);
        a->abstractOps = yyvsp[0].typeOrValueListPtr;
        a->consumerInvokes = asnConsumerG;
        a->supplierInvokes = asnSupplierG;
    ;
    break;}
case 348:
#line 2695 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNPORT, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.asnPort = MT (AsnPortMacroType);
    ;
    break;}
case 349:
#line 2704 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = yyvsp[-1].typeOrValueListPtr;
    ;
    break;}
case 350:
#line 2708 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = NULL;
        asnConsumerG = yyvsp[0].typeOrValueListPtr;
        asnSupplierG = NULL;
    ;
    break;}
case 351:
#line 2714 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = NULL;
        asnConsumerG = yyvsp[0].typeOrValueListPtr;
        asnSupplierG = NULL;
    ;
    break;}
case 352:
#line 2720 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = NULL;
        asnConsumerG = yyvsp[-1].typeOrValueListPtr;
        asnSupplierG = NULL;
    ;
    break;}
case 353:
#line 2726 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = NULL;
        asnConsumerG = yyvsp[-1].typeOrValueListPtr;
        asnSupplierG = NULL;
    ;
    break;}
case 354:
#line 2735 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = yyvsp[-1].typeOrValueListPtr;
    ;
    break;}
case 355:
#line 2742 "core/parse-asn1.y"
{
        yyval.typeOrValueListPtr = yyvsp[-1].typeOrValueListPtr;
    ;
    break;}
case 356:
#line 2758 "core/parse-asn1.y"
{
        SetupType (&yyval.typePtr, BASICTYPE_UNKNOWN, myLineNoG);
    ;
    break;}
case 364:
#line 2784 "core/parse-asn1.y"
{
       yyval.intVal = 0; /* just to quiet yacc warning */
    ;
    break;}
case 369:
#line 2802 "core/parse-asn1.y"
{
        yyval.intVal = 0; /* just to quiet yacc warning */
    ;
    break;}
case 370:
#line 2815 "core/parse-asn1.y"
{
        AsnAbstractBindMacroType *a;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNABSTRACTBIND, myLineNoG);
        a = yyval.typePtr->basicType->a.macroType->a.asnAbstractBind =
            MT (AsnAbstractBindMacroType);
        a->ports = yyvsp[0].asnPortListPtr;
    ;
    break;}
case 371:
#line 2824 "core/parse-asn1.y"
{
        AsnAbstractBindMacroType *a;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNABSTRACTBIND, myLineNoG);
        a = yyval.typePtr->basicType->a.macroType->a.asnAbstractBind =
            MT (AsnAbstractBindMacroType);
        a->ports = yyvsp[-1].asnPortListPtr;
        a->type = yyvsp[0].typePtr;
    ;
    break;}
case 372:
#line 2837 "core/parse-asn1.y"
{
        yyval.asnPortListPtr = yyvsp[-1].asnPortListPtr;
    ;
    break;}
case 373:
#line 2840 "core/parse-asn1.y"
{ yyval.asnPortListPtr = NULL; ;
    break;}
case 374:
#line 2851 "core/parse-asn1.y"
{
        AsnAbstractBindMacroType *a;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNABSTRACTUNBIND, myLineNoG);
        a = yyval.typePtr->basicType->a.macroType->a.asnAbstractUnbind =
            MT (AsnAbstractBindMacroType);

        a->ports = yyvsp[0].asnPortListPtr;
    ;
    break;}
case 375:
#line 2861 "core/parse-asn1.y"
{
        AsnAbstractBindMacroType *a;

        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNABSTRACTUNBIND, myLineNoG);
        a = yyval.typePtr->basicType->a.macroType->a.asnAbstractUnbind =
            MT (AsnAbstractBindMacroType);

        a->ports = yyvsp[-1].asnPortListPtr;
        a->type = yyvsp[0].typePtr;
    ;
    break;}
case 376:
#line 2875 "core/parse-asn1.y"
{
        yyval.asnPortListPtr = yyvsp[-1].asnPortListPtr;
    ;
    break;}
case 377:
#line 2878 "core/parse-asn1.y"
{ yyval.asnPortListPtr = NULL; ;
    break;}
case 378:
#line 2888 "core/parse-asn1.y"
{
       yyval.typePtr = yyvsp[0].typePtr;
       yyvsp[0].typePtr->basicType->a.macroType->choiceId = MACROTYPE_ASNABSTRACTOPERATION;
    ;
    break;}
case 379:
#line 2900 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_ASNABSTRACTERROR, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.asnAbstractError = MT (RosErrorMacroType);
        yyval.typePtr->basicType->a.macroType->a.asnAbstractError->parameter = yyvsp[0].namedTypePtr;
    ;
    break;}
case 380:
#line 2913 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_AFALGORITHM, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.afAlgorithm = yyvsp[0].typePtr;
    ;
    break;}
case 381:
#line 2924 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_AFENCRYPTED, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.afEncrypted = yyvsp[0].typePtr;
    ;
    break;}
case 382:
#line 2936 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_AFSIGNED, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.afSigned = yyvsp[0].typePtr;
    ;
    break;}
case 383:
#line 2947 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_AFSIGNATURE, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.afSignature = yyvsp[0].typePtr;
    ;
    break;}
case 384:
#line 2961 "core/parse-asn1.y"
{
        SetupMacroType (&yyval.typePtr, MACROTYPE_AFPROTECTED, myLineNoG);
        yyval.typePtr->basicType->a.macroType->a.afProtected = yyvsp[0].typePtr;
    ;
    break;}
case 385:
#line 2978 "core/parse-asn1.y"
{
        SnmpObjectTypeMacroType *s;

        SetupMacroType (&yyval.typePtr, MACROTYPE_SNMPOBJECTTYPE, myLineNoG);
        s = yyval.typePtr->basicType->a.macroType->a.snmpObjectType =
            MT (SnmpObjectTypeMacroType);

        s->syntax = yyvsp[-8].typePtr;
        s->access = yyvsp[-6].intVal;
        s->status = yyvsp[-4].intVal;
        s->description = yyvsp[-3].valuePtr;
        s->reference = yyvsp[-2].valuePtr;
        s->index = yyvsp[-1].typeOrValueListPtr;
        s->defVal = yyvsp[0].valuePtr;
    ;
    break;}
case 386:
#line 2997 "core/parse-asn1.y"
{
        if (strcmp (yyvsp[0].charPtr, "read-only") == 0)
            yyval.intVal = SNMP_READ_ONLY;
        else if (strcmp (yyvsp[0].charPtr, "read-write") == 0)
            yyval.intVal = SNMP_READ_WRITE;
        else if (strcmp (yyvsp[0].charPtr, "write-only") == 0)
            yyval.intVal = SNMP_WRITE_ONLY;
        else if (strcmp (yyvsp[0].charPtr, "not-accessible") == 0)
            yyval.intVal = SNMP_NOT_ACCESSIBLE;
        else
        {
            yyerror ("ACCESS field of SNMP OBJECT-TYPE MACRO can only be one of \"read-write\", \"write-only\" or \"not-accessible\"");
            yyval.intVal = -1;
            modulePtrG->status = MOD_ERROR;
        }
        Free (yyvsp[0].charPtr);
   ;
    break;}
case 387:
#line 3019 "core/parse-asn1.y"
{
        if (strcmp (yyvsp[0].charPtr, "mandatory") == 0)
            yyval.intVal = SNMP_MANDATORY;
        else if (strcmp (yyvsp[0].charPtr, "optional") == 0)
            yyval.intVal = SNMP_OPTIONAL;
        else if (strcmp (yyvsp[0].charPtr, "obsolete") == 0)
            yyval.intVal = SNMP_OBSOLETE;
        else if (strcmp (yyvsp[0].charPtr, "deprecated") == 0)
            yyval.intVal = SNMP_DEPRECATED;
        else
        {
            yyerror ("STATUS field of SNMP OBJECT-TYPE MACRO can only be one of \"optional\", \"obsolete\" or \"deprecated\"");
            yyval.intVal = -1;
            modulePtrG->status = MOD_ERROR;
        }
        Free (yyvsp[0].charPtr);
   ;
    break;}
case 388:
#line 3039 "core/parse-asn1.y"
{ yyval.valuePtr = yyvsp[0].valuePtr; ;
    break;}
case 389:
#line 3040 "core/parse-asn1.y"
{ yyval.valuePtr = NULL; ;
    break;}
case 390:
#line 3044 "core/parse-asn1.y"
{ yyval.valuePtr = yyvsp[0].valuePtr; ;
    break;}
case 391:
#line 3045 "core/parse-asn1.y"
{ yyval.valuePtr = NULL; ;
    break;}
case 392:
#line 3050 "core/parse-asn1.y"
{
       yyval.typeOrValueListPtr  = yyvsp[-1].typeOrValueListPtr;
   ;
    break;}
case 393:
#line 3053 "core/parse-asn1.y"
{ yyval.typeOrValueListPtr = NULL; ;
    break;}
case 394:
#line 3058 "core/parse-asn1.y"
{
       yyval.valuePtr  = yyvsp[-1].valuePtr;
   ;
    break;}
case 395:
#line 3061 "core/parse-asn1.y"
{ yyval.valuePtr = NULL; ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/local/lib/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 3064 "core/parse-asn1.y"


yyerror (s)
char*s;
{
	fprintf (stderr,"file \"%s\", line %d: %s at symbol \"%s\"\n\n", modulePtrG->asn1SrcFileName, myLineNoG, s, yytext);
}


/*
 * given a Module*, the file name associated witht the open
 * FILE *fPtr, InitAsn1Parser sets up the yacc/lex parser
 * to parse an ASN.1 module read from fPtr and write the
 * parse results into the given Module *mod.
 */
int
InitAsn1Parser PARAMS ((mod, fileName, fPtr),
    Module	*mod _AND_
    char	*fileName _AND_
    FILE	*fPtr)
{
    yyin = fPtr;

    /*
     * reset lexical analyzer input file ptr
     * (only do this on succesive calls ow yyrestart seg faults
     */
#ifdef FLEX_IN_USE
    if (!firstTimeThroughG)
        yyrestart (fPtr);

    firstTimeThroughG = FALSE;
#endif


    /*
     * init modulePtr
     */
    memzero (mod, sizeof (Module));
    modulePtrG = mod;
    mod->asn1SrcFileName = fileName;
    mod->status = MOD_NOT_LINKED;
    mod->hasAnys = FALSE;

    /* init lists to empty */
    mod->typeDefs = AsnListNew (sizeof (void*));
    mod->valueDefs = AsnListNew (sizeof (void*));

    /*
     * init export list stuff
     */
    exportListG = NULL;
    exportsParsedG = FALSE;

    /*
     * reset line number to 1
     */
    myLineNoG = 1;

    /*
     * reset error count
     */
    parseErrCountG = 0;

   /*
    * set up list to hold values defined in parsed oids
    */
    oidElmtValDefsG = AsnListNew (sizeof (void *));

    smallErrG = 0;

    return 0;

}  /* InitAsn1Parser */


/*
 * puts the applicatin tag code, tagCode, and line number it was
 * parsed at into the applTagsG list.  If the APPLICATION tag code
 * is already in the applTagsG list then an error is printed.
 * and the smallErrG flag set to prevent code production.
 */
void
PushApplTag PARAMS ((tagCode, lineNo),
    unsigned long int tagCode _AND_
    unsigned long int lineNo)
{
    ApplTag *l;
    ApplTag *new;
    int wasDefined = 0;

    /* make sure not already in list */
    for (l = applTagsG; l != NULL; l = l->next)
    {
        if (l->tagCode == tagCode)
        {
            PrintErrLoc (modulePtrG->asn1SrcFileName, lineNo);
            fprintf (stderr,"ERROR - APPLICATION tags can be used only once per ASN.1 module.  The tag \"[APPLICATION %d]\" was previously used on line %d.\n", tagCode, l->lineNo);
            wasDefined = 1;
            smallErrG = 1;
        }
    }
    if (!wasDefined)
    {
        new = MT (ApplTag);
        new->lineNo = lineNo;
        new->tagCode = tagCode;
        new->next = applTagsG;
        applTagsG = new;
    }
}  /* PushApplTag */


/*
 * Empties the applTagsG list.  Usually done between modules.
 */
void
FreeApplTags()
{
    ApplTag *l;
    ApplTag *lTmp;

    for (l = applTagsG; l != NULL; )
    {
        lTmp = l->next;
        Free (l);
        l = lTmp;
    }
    applTagsG = NULL;
}  /* FreeApplTags */
