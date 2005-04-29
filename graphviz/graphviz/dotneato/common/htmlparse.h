#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union  {
  int    i;
  htmltxt_t*  txt;
  htmlcell_t*  cell;
  htmltbl_t*   tbl;
} htmlstype;
# define YYSTYPE htmlstype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	T_end_br	257
# define	T_row	258
# define	T_end_row	259
# define	T_html	260
# define	T_end_html	261
# define	T_end_table	262
# define	T_end_cell	263
# define	T_string	264
# define	T_error	265
# define	T_BR	266
# define	T_br	267
# define	T_table	268
# define	T_cell	269


extern YYSTYPE htmllval;

#endif /* not BISON_Y_TAB_H */
