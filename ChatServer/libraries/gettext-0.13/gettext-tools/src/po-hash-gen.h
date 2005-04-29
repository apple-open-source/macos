#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union
{
  char *string;
  size_t number;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	STRING	257
# define	NUMBER	258
# define	COLON	259
# define	COMMA	260
# define	FILE_KEYWORD	261
# define	LINE_KEYWORD	262
# define	NUMBER_KEYWORD	263


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
