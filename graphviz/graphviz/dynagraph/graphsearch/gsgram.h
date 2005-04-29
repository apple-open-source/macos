#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union	{
			int				i;
			char			*str;
} gs_yystype;
# define YYSTYPE gs_yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	T_graph	257
# define	T_node	258
# define	T_edge	259
# define	T_view	260
# define	T_pattern	261
# define	T_search	262
# define	T_input	263
# define	T_open	264
# define	T_close	265
# define	T_insert	266
# define	T_delete	267
# define	T_modify	268
# define	T_lock	269
# define	T_unlock	270
# define	T_segue	271
# define	T_define	272
# define	T_id	273
# define	T_edgeop	274
# define	T_subgraph	275


extern YYSTYPE gs_yylval;

#endif /* not BISON_Y_TAB_H */
