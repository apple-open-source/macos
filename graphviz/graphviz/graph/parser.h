#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union	{
			int					i;
			char				*str;
			struct objport_t	obj;
			struct Agnode_t		*n;
} agstype;
# define YYSTYPE agstype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	T_graph	257
# define	T_digraph	258
# define	T_strict	259
# define	T_node	260
# define	T_edge	261
# define	T_edgeop	262
# define	T_symbol	263
# define	T_qsymbol	264
# define	T_subgraph	265


extern YYSTYPE aglval;

#endif /* not BISON_Y_TAB_H */
