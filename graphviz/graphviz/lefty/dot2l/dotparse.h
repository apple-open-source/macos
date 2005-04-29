#ifndef BISON_DOTPARSE_H
# define BISON_DOTPARSE_H

#ifndef YYSTYPE
typedef union {
    long i;
    char *s;
    void *o;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	T_graph	257
# define	T_digraph	258
# define	T_strict	259
# define	T_node	260
# define	T_edge	261
# define	T_edgeop	262
# define	T_id	263
# define	T_subgraph	264


extern YYSTYPE yylval;

#endif /* not BISON_DOTPARSE_H */
