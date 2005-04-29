#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union	{
			int				i;
			char			*str;
} incr_yystype;
# define YYSTYPE incr_yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	T_graph	257
# define	T_node	258
# define	T_edge	259
# define	T_view	260
# define	T_open	261
# define	T_close	262
# define	T_insert	263
# define	T_delete	264
# define	T_modify	265
# define	T_lock	266
# define	T_unlock	267
# define	T_segue	268
# define	T_message	269
# define	T_id	270
# define	T_edgeop	271
# define	T_subgraph	272


extern YYSTYPE incr_yylval;

#endif /* not BISON_Y_TAB_H */
