#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union	{
			int				i;
			char			*str;
			struct Agnode_s	*n;
} aagstype;
# define YYSTYPE aagstype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	T_graph	257
# define	T_node	258
# define	T_edge	259
# define	T_digraph	260
# define	T_subgraph	261
# define	T_strict	262
# define	T_edgeop	263
# define	T_list	264
# define	T_attr	265
# define	T_atom	266
# define	T_qatom	267


extern YYSTYPE aaglval;

#endif /* not BISON_Y_TAB_H */
