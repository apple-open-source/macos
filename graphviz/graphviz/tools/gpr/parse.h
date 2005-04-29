#ifndef PARSE_H
#define PARSE_H

typedef enum {Begin = 0, End, BeginG, EndG, Node, Edge, Eof, Error} case_t;

typedef struct _case_info {
  int                gstart;
  char*              guard;
  int                astart;
  char*              action;
  struct _case_info* next;
} case_info;

typedef struct {
  char*       source;
  int         l_begin, l_beging, l_end, l_endg;
  char*       begin_stmt;
  char*       begg_stmt;
  int         n_nstmts;
  int         n_estmts;
  case_info*  node_stmts;
  case_info*  edge_stmts;
  char*       endg_stmt;
  char*       end_stmt;
} parse_prog;


extern parse_prog* parseProg (char* , int);

#endif
