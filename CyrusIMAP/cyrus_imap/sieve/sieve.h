typedef union {
    int nval;
    char *sval;
    stringlist_t *sl;
    test_t *test;
    testlist_t *testl;
    commandlist_t *cl;
    struct vtags *vtag;
    struct aetags *aetag;
    struct htags *htag;
    struct ntags *ntag;
    struct dtags *dtag;
} YYSTYPE;
#define	NUMBER	257
#define	STRING	258
#define	IF	259
#define	ELSIF	260
#define	ELSE	261
#define	REJCT	262
#define	FILEINTO	263
#define	REDIRECT	264
#define	KEEP	265
#define	STOP	266
#define	DISCARD	267
#define	VACATION	268
#define	REQUIRE	269
#define	SETFLAG	270
#define	ADDFLAG	271
#define	REMOVEFLAG	272
#define	MARK	273
#define	UNMARK	274
#define	NOTIFY	275
#define	DENOTIFY	276
#define	ANYOF	277
#define	ALLOF	278
#define	EXISTS	279
#define	SFALSE	280
#define	STRUE	281
#define	HEADER	282
#define	NOT	283
#define	SIZE	284
#define	ADDRESS	285
#define	ENVELOPE	286
#define	COMPARATOR	287
#define	IS	288
#define	CONTAINS	289
#define	MATCHES	290
#define	REGEX	291
#define	COUNT	292
#define	VALUE	293
#define	OVER	294
#define	UNDER	295
#define	GT	296
#define	GE	297
#define	LT	298
#define	LE	299
#define	EQ	300
#define	NE	301
#define	ALL	302
#define	LOCALPART	303
#define	DOMAIN	304
#define	USER	305
#define	DETAIL	306
#define	DAYS	307
#define	ADDRESSES	308
#define	SUBJECT	309
#define	MIME	310
#define	METHOD	311
#define	ID	312
#define	OPTIONS	313
#define	LOW	314
#define	NORMAL	315
#define	HIGH	316
#define	ANY	317
#define	MESSAGE	318


extern YYSTYPE yylval;
