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


extern YYSTYPE yylval;
