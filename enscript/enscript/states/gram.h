typedef union
{
  List *lst;
  Node *node;
  Cons *cons;
  Stmt *stmt;
  Expr *expr;
} YYSTYPE;
#define	tSYMBOL	258
#define	tREGEXP	259
#define	tSTRING	260
#define	tINTEGER	261
#define	tREAL	262
#define	tSUB	263
#define	tSTATE	264
#define	tSTART	265
#define	tSTARTRULES	266
#define	tNAMERULES	267
#define	tBEGIN	268
#define	tEND	269
#define	tRETURN	270
#define	tIF	271
#define	tELSE	272
#define	tLOCAL	273
#define	tWHILE	274
#define	tFOR	275
#define	tADDASSIGN	276
#define	tSUBASSIGN	277
#define	tMULASSIGN	278
#define	tDIVASSIGN	279
#define	tOR	280
#define	tAND	281
#define	tEQ	282
#define	tNE	283
#define	tGE	284
#define	tLE	285
#define	tDIV	286
#define	tPLUSPLUS	287
#define	tMINUSMINUS	288


extern YYSTYPE yylval;
