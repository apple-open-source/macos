typedef union
{
  List *lst;
  Node *node;
  Cons *cons;
  Stmt *stmt;
  Expr *expr;
} YYSTYPE;
#define	tSYMBOL	257
#define	tREGEXP	258
#define	tSTRING	259
#define	tINTEGER	260
#define	tREAL	261
#define	tSUB	262
#define	tSTATE	263
#define	tSTART	264
#define	tSTARTRULES	265
#define	tNAMERULES	266
#define	tBEGIN	267
#define	tEND	268
#define	tRETURN	269
#define	tIF	270
#define	tELSE	271
#define	tLOCAL	272
#define	tWHILE	273
#define	tFOR	274
#define	tEXTENDS	275
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
