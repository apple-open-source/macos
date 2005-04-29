#ifndef _EXPARSE_H
#define _EXPARSE_H
#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef EXSTYPE
typedef union
{
	struct Exnode_s*expr;
	double		floating;
	struct Exref_s*	reference;
	struct Exid_s*	id;
	Sflong_t	integer;
	int		op;
	char*		string;
	void*		user;
	struct Exbuf_s*	buffer;
} exstype;
# define EXSTYPE exstype
# define EXSTYPE_IS_TRIVIAL 1
#endif
# define	MINTOKEN	257
# define	CHAR	258
# define	INT	259
# define	INTEGER	260
# define	UNSIGNED	261
# define	FLOATING	262
# define	STRING	263
# define	VOID	264
# define	ARRAY	265
# define	BREAK	266
# define	CALL	267
# define	CASE	268
# define	CONSTANT	269
# define	CONTINUE	270
# define	DECLARE	271
# define	DEFAULT	272
# define	DYNAMIC	273
# define	ELSE	274
# define	EXIT	275
# define	FOR	276
# define	FUNCTION	277
# define	GSUB	278
# define	ITERATE	279
# define	ID	280
# define	IF	281
# define	LABEL	282
# define	MEMBER	283
# define	NAME	284
# define	POS	285
# define	PRAGMA	286
# define	PRE	287
# define	PRINT	288
# define	PRINTF	289
# define	PROCEDURE	290
# define	QUERY	291
# define	RAND	292
# define	RETURN	293
# define	SRAND	294
# define	SUB	295
# define	SUBSTR	296
# define	SPRINTF	297
# define	SWITCH	298
# define	WHILE	299
# define	F2I	300
# define	F2S	301
# define	I2F	302
# define	I2S	303
# define	S2B	304
# define	S2F	305
# define	S2I	306
# define	F2X	307
# define	I2X	308
# define	S2X	309
# define	X2F	310
# define	X2I	311
# define	X2S	312
# define	X2X	313
# define	XPRINT	314
# define	OR	315
# define	AND	316
# define	EQ	317
# define	NE	318
# define	LE	319
# define	GE	320
# define	LS	321
# define	RS	322
# define	UNARY	323
# define	INC	324
# define	DEC	325
# define	CAST	326
# define	MAXTOKEN	327


extern EXSTYPE exlval;

#endif /* not BISON_Y_TAB_H */
#endif /* _EXPARSE_H */
