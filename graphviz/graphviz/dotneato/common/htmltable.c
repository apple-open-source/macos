/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

/* Implementation of HTML-like tables.
 * 
 * The CodeGen graphics model, especially with integral coodinates, is
 * not adequate to handle this as we would like. In particular, it is
 * difficult to handle notions of adjacency and correct rounding to pixels.
 * For example, if 2 adjacent boxes bb1.UR.x == bb2.LL.x, the rectangles
 * may be drawn overlapping. However, if we use bb1.UR.x+1 == bb2.LL.x
 * there may or may not be a gap between them, even in the same device
 * depending on their positions. When CELLSPACING > 1, this isn't as much
 * of a problem.
 *
 * We allow negative spacing as a hack to allow overlapping cell boundaries.
 * For the reasons discussed above, this is difficult to get correct.
 * This is an important enough case we should extend the table model to
 * support it correctly. This could be done by allowing a table attribute,
 * e.g., CELLGRID=n, which sets CELLBORDER=0 and has the border drawing
 * handled correctly by the table.
 */
#include "render.h"
#include "htmltable.h"
#include "agxbuf.h"
#include "pointset.h"
#include "utils.h"
#include <assert.h>

#define DEFAULT_BORDER    1
#define DEFAULT_CELLPADDING  2
#define DEFAULT_CELLSPACING  2

typedef struct {
  textlabel_t* lp;
  void*        obj;
} htmlenv_t;

static void
emit_html_txt (GVC_t *gvc, htmltxt_t* tp, htmlenv_t* env)
{
  textlabel_t*  lp = env->lp;
  int  i, linespacing, left_x, center_x, right_x;
  point  p;

  /* make sure that there is something to do */
  if (tp->nlines < 1) return;

  /* set x positions for label */
  left_x = tp->box.LL.x + lp->p.x;
  right_x = tp->box.UR.x + lp->p.x;
  center_x = (left_x + right_x)/2;

  /* set linespacing */
  linespacing = (int)(lp->fontsize * LINESPACING);

  /* position for first line */
  p.y = lp->p.y + (tp->box.UR.y + tp->box.LL.y)/2
    + (linespacing * (tp->nlines -1) / 2)  /* cl of topline */
           - lp->fontsize / 3.0 ; /* cl to baseline */

  gvrender_begin_context(gvc);
  gvrender_set_pencolor(gvc, lp->fontcolor);
  gvrender_set_font(gvc, lp->fontname, lp->fontsize*GD_drawing(gvc->g)->font_scale_adj);

  for (i = 0; i < tp->nlines; i++) {
    switch(tp->line[i].just) {
      case 'l':
        p.x = left_x;
        break;
      case 'r':
        p.x = right_x;
        break;
      default:
      case 'n':
        p.x = center_x;
        break;
    }
    gvrender_textline(gvc, p, &(tp->line[i]));

    /* position for next line */
    p.y -= linespacing;
  }

  gvrender_end_context(gvc);
}

static void
doSide (GVC_t *gvc, point p, int wd, int ht)
{
  point     A[4];

  A[0] = p;
  A[1].x = p.x;
  A[1].y = p.y + ht;
  A[2].y = A[1].y;
  A[2].x = p.x + wd;
  A[3].x = A[2].x;
  A[3].y = p.y;
  gvrender_polygon(gvc, A, 4, 1);
}

/* doBorder:
 * Draw rectangle of width border inside rectangle given
 * by box. If border is 1, we call use a single call to gvrender_polygon.
 * (We have set linewidth to 1 below.) Otherwise, we use four separate
 * filled rectangles. We could use a richer graphics model, as things
 * can go wrong when cell spacing and borders are small.
 * We decrement the border value by 1, as typically a filled rectangle
 * from x to x+border will all pixels from x to x+border, and thus have
 * width border+1.
 */
static void
doBorder (GVC_t *gvc, char* color, int border, box pts)
{
  point     pt;
  int       wd, ht;

  gvrender_begin_context(gvc);

  if (!color) color = "black";
  gvrender_set_fillcolor(gvc, color);
  gvrender_set_pencolor(gvc, color);

  if (border == 1) {
    point     A[4];

    A[0] = pts.LL;
    A[2] = pts.UR;
    A[1].x = A[0].x;
    A[1].y = A[2].y;
    A[3].x = A[2].x;
    A[3].y = A[0].y;
    gvrender_polygon(gvc, A, 4, 0);
  }
  else {
    border--;
    ht = pts.UR.y - pts.LL.y;
    wd = pts.UR.x - pts.LL.x;
    doSide (gvc, pts.LL, border, ht);
    pt.x= pts.LL.x;
    pt.y= pts.UR.y;
    doSide (gvc, pt, wd, -border);
    doSide (gvc, pts.UR, -border, -ht);
    pt.x= pts.UR.x;
    pt.y= pts.LL.y;
    doSide (gvc, pt, -wd, border);
  }
  
  gvrender_end_context(gvc);
}

static void
doFill (GVC_t *gvc, char* color, box pts)
{
  point     A[4];

  gvrender_set_fillcolor(gvc, color);
  gvrender_set_pencolor(gvc, color);
  A[0] = pts.LL;
  A[1].x = pts.LL.x;
  A[1].y = pts.UR.y;
  A[2] = pts.UR;
  A[3].x = pts.UR.x;
  A[3].y = pts.LL.y;
  gvrender_polygon(gvc, A, 4, 1);
}

  /* forward declaration */
static void emit_html_cell (GVC_t *gvc, htmlcell_t*, htmlenv_t*);

static void
emit_html_tbl (GVC_t *gvc, htmltbl_t* tbl, htmlenv_t* env)
{
  box           pts = tbl->data.box;
  point         p = env->lp->p;
  htmlcell_t**  cells = tbl->u.n.cells;

  pts.LL.x += p.x;
  pts.UR.x += p.x;
  pts.LL.y += p.y;
  pts.UR.y += p.y;

  /* gvrender_begin_context(gvc); */

  if (tbl->data.bgcolor)
    doFill (gvc, tbl->data.bgcolor, pts);

  while (*cells) {
    emit_html_cell (gvc, *cells, env);
    cells++;
  }

  if (tbl->data.border)
    doBorder (gvc, NULL, tbl->data.border, pts);

  /* gvrender_end_context(gvc); */
}

static void
emit_html_cell (GVC_t *gvc, htmlcell_t* cp, htmlenv_t* env)
{
  box           pts = cp->data.box;
  point         p = env->lp->p;

  pts.LL.x += p.x;
  pts.UR.x += p.x;
  pts.LL.y += p.y;
  pts.UR.y += p.y;

  /* gvrender_begin_context(); */

  if (cp->data.bgcolor)
    doFill (gvc, cp->data.bgcolor, pts);

  if (cp->child.kind == HTML_TBL)
    emit_html_tbl (gvc, cp->child.u.tbl, env);
  else
    emit_html_txt (gvc, cp->child.u.txt, env);
  
  if (cp->data.border)
    doBorder (gvc, NULL, cp->data.border, pts);

  /* gvrender_end_context(); */
}

void 
emit_html_label(GVC_t *gvc, htmllabel_t* lp, textlabel_t* tp)
{
  htmlenv_t    env;

  env.lp = tp;
  if (lp->kind == HTML_TBL) {
    htmltbl_t*  tbl = lp->u.tbl;

      /* set basic graphics context */
    gvrender_begin_context(gvc);
	/* Need to override line style set by node. */
    gvrender_set_style(gvc, BaseLineStyle);
    if (tbl->data.pencolor)
      gvrender_set_pencolor(gvc, tbl->data.pencolor);
    else
      gvrender_set_pencolor(gvc, DEFAULT_COLOR);
    emit_html_tbl (gvc, tbl, &env);
    gvrender_end_context(gvc);
  }
  else {
    emit_html_txt (gvc, lp->u.txt, &env);
  }
}

void 
free_html_data (htmldata_t* dp)
{
  free (dp->href);
  free (dp->port);
  free (dp->bgcolor);
}

void
free_html_text (htmltxt_t* tp)
{
  textline_t* lp;

  if (!tp) return;
  lp = tp->line;
  while (lp->str) {
    free (lp->str);
    lp++;
  }
  free(tp->line);
  free(tp);
}

static void 
free_html_cell (htmlcell_t* cp)
{
  free_html_label (&cp->child, 0);
  free_html_data (&cp->data);
  free (cp);
}

/* free_html_tbl:
 * If tbl->n_rows is negative, table is in initial state from
 * HTML parse, with data stored in u.p. Once run through processTable,
 * data is stored in u.n and tbl->n_rows is > 0.
 */
static void 
free_html_tbl (htmltbl_t* tbl)
{
  htmlcell_t** cells;

  if (tbl->rc == -1) {
    dtclose (tbl->u.p.rows);
  }
  else {
    cells = tbl->u.n.cells;

    free (tbl->heights);
    free (tbl->widths);
    while (*cells) {
      free_html_cell (*cells);
      cells++;
    }
    free (tbl->u.n.cells);
  }
  free_html_data (&tbl->data);
  free (tbl);
}

void 
free_html_label (htmllabel_t* lp, int root)
{
  if (lp->kind == HTML_TBL)
    free_html_tbl (lp->u.tbl);
  else
    free_html_text (lp->u.txt);
  if (root) free (lp);
}

static box* portToTbl (htmltbl_t*, char*); /* forward declaration */

static box*
portToCell (htmlcell_t* cp, char *id)
{
  box*  rv;

  if (cp->data.port && (strcasecmp (cp->data.port, id) == 0))
    rv = &cp->data.box;
  else if (cp->child.kind == HTML_TBL)
    rv = portToTbl (cp->child.u.tbl, id);
  else
    rv = NULL;

  return rv;
}

/* portToTbl:
 * See if tp or any of its child cells has the given port id.
 * If true, return corresponding box.
 */
static box*
portToTbl (htmltbl_t* tp, char *id)
{
  box*  rv;
  htmlcell_t** cells;
  htmlcell_t*  cp;

  if (tp->data.port && (strcasecmp (tp->data.port, id) == 0))
    rv = &tp->data.box;
  else {
    rv = NULL;
    cells = tp->u.n.cells;
    while ((cp = *cells++)) {
      if ((rv = portToCell (cp, id))) break;
    }
  }

  return rv;
}

/* html_port:
 * See if edge port corresponds to part of the html node.
 * The initial ':' character has been stripped. 
 * If successful, fill in pp and return 1.
 */
int
html_port (node_t* n, char* pname, port* pp)
{
  box* bp;
  box b;
  port rv;
  htmllabel_t* lbl = ND_label(n)->u.html;

  if ((lbl->kind == HTML_TEXT) || 
      ((bp = portToTbl (lbl->u.tbl, pname)) == NULL)) {
    return 0;
  }

  b = *bp;
  rv.p = pointof ((b.LL.x + b.UR.x) / 2, (b.LL.y + b.UR.y) / 2);
  if (GD_left_to_right(n->graph)) rv.p = invflip_pt(rv.p);
  rv.order = (MC_SCALE * (ND_lw_i(n) + rv.p.x)) / (ND_lw_i(n) + ND_rw_i(n));
  rv.bp = bp;
  rv.constrained = FALSE;
  rv.defined = TRUE;
  *pp = rv;
  return 1;
}

/* html_path:
 * Return a box in a table containing the given endpoint.
 * If the top flow is text (no internal structure), return 
 * the box of the flow
 * Else return the box of the subtable containing the point.
 * Because of spacing, the point might not be in any subtable.
 * In that case, return the top flow's box.
 * Note that box[0] must contain the edge point. Additional boxes
 * move out to the boundary.
 *
 * At present, unimplemented, since the label may be inside a
 * non-box node and we need to figure out what this means.
 */
int
html_path (node_t* n, edge_t* e, int pt, box* rv, int* k)
{
#ifdef UNIMPL
  point        p;
  tbl_t*       info;
  tbl_t*       t;
  box          b;
  int          i;

  info = (tbl_t*) ND_shape_info(n);
  assert (info->tbls);
  info = info->tbls[0];   /* top-level flow */
  assert (IS_FLOW(info));

  b = info->box;
  if (info->tbl) {
    info = info->tbl;
    if (pt == 1) p = ED_tail_port(e).p;
    else p = ED_head_port(e).p;
    if (GD_left_to_right(n->graph))
      p = flip_pt (p);    /* move p to node's coordinate system */
    for (i = 0; (t = info->tbls[i]) != 0; i++)
      if (INSIDE(p,t->box)) {
        b = t->box;
        break;
      }
  }

    /* move box into layout coordinate system */
  if (GD_left_to_right(n->graph))
    b = flip_trans_box(b,ND_coord_i(n));
  else
    b = move_box(b,ND_coord_i(n));

  *k = 1;
  *rv = b;
  if (pt == 1) return BOTTOM; 
  else return TOP;
#endif
  return 0;
}

int
size_html_txt (GVC_t *gvc, htmltxt_t* txt, htmlenv_t* env)
{
  double  xsize = 0.0;
  double  fsize = env->lp->fontsize;
  char*   fname = env->lp->fontname;
  char*   news = NULL;
  int     width;

  textline_t*   lp = txt->line;
  while (lp->str) {
    switch (agobjkind(env->obj)) {
      case AGGRAPH: 
        news = strdup_and_subst_graph(lp->str, (Agraph_t*)(env->obj));
        break;
      case AGNODE:
        news = strdup_and_subst_node(lp->str, (Agnode_t*)(env->obj));
        break;
      case AGEDGE:
        news = strdup_and_subst_edge(lp->str, (Agedge_t*)(env->obj));
        break;
    }
    free (lp->str);
    lp->str = news;
    
    width = textwidth (gvc, news, fname, fsize);
    lp->width = width;
    width += 2;  /* margins - additional space can be specified by padding */
    if (width > xsize) xsize = width;
    lp++;
  }
  txt->box.UR.x = xsize;
  txt->box.UR.y = txt->nlines*(int)(fsize*LINESPACING) + 2;
  return 0;
}

/* forward declarion for recursive usage */
static int size_html_tbl (GVC_t *gvc, htmltbl_t* tbl, htmlcell_t* parent, htmlenv_t* env);

static int
size_html_cell (GVC_t *gvc, htmlcell_t* cp, htmltbl_t* parent, htmlenv_t* env)
{
  int   rv;
  point sz, child_sz;
  int   margin;

  cp->parent = parent;
  if (!(cp->data.flags & PAD_SET)) {
    if (parent->data.flags & PAD_SET)
      cp->data.pad = parent->data.pad;
    else
      cp->data.pad = DEFAULT_CELLPADDING;
  }
  if (!(cp->data.flags & BORDER_SET)) {
    if (parent->cb >= 0)
      cp->data.border = parent->cb;
    else if (parent->data.flags & BORDER_SET)
      cp->data.border = parent->data.border;
    else
      cp->data.border = DEFAULT_BORDER;
  }

  if (cp->child.kind == HTML_TBL) {
    rv = size_html_tbl (gvc, cp->child.u.tbl, cp, env);
    child_sz = cp->child.u.tbl->data.box.UR;
  }
  else {
    rv = size_html_txt (gvc, cp->child.u.txt, env);
    child_sz = cp->child.u.txt->box.UR;
  }
  margin = 2*(cp->data.pad + cp->data.border);
  sz.x = child_sz.x + margin;
  sz.y = child_sz.y + margin;

  if (cp->data.flags & FIXED_FLAG) {
    if (cp->data.width && cp->data.height) {
      if ((cp->data.width < sz.x) || (cp->data.height < sz.y)) {
        agerr(AGWARN, "cell size too small for content\n");
        rv = 1;
      }
      sz.x = sz.y = 0;

    }
    else {
      agerr(AGWARN, "fixed cell size with unspecified width or height\n");
      rv = 1;
    }
  }
  cp->data.box.UR.x = MAX(sz.x, cp->data.width);
  cp->data.box.UR.y = MAX(sz.y, cp->data.height);

  return rv;
}

static int
findCol (PointSet* ps, int row, int col, htmlcell_t* cellp)
{
  int    notFound = 1;
  int    lastc;
  int    i, j, c;
  int    end = cellp->cspan-1;
  
  while (notFound) {
    lastc = col+end;
    for (c = lastc; c >= col; c--) {
      if (isInPS(ps,c,row)) break;
    }
    if (c >= col)  /* conflict : try column after */
      col = c+1;
    else
      notFound = 0;
  }
  for (j = col; j < col+cellp->cspan; j++) {
    for (i = row; i < row+cellp->rspan; i++) {
      addPS (ps, j, i);
    }
  }
  return col;
}

/* processTbl:
 * Convert parser representation of cells into final form.
 * Find column and row positions of cells.
 * Recursively size cells.
 * Return 1 if problem sizing a cell.
 */
static int
processTbl (GVC_t *gvc, htmltbl_t* tbl, htmlenv_t* env)
{
  pitem*       rp;
  pitem*       cp;
  Dt_t*        cdict;
  int          r,c,cnt;
  htmlcell_t*  cellp;
  htmlcell_t** cells;
  Dt_t*        rows = tbl->u.p.rows;
  int          rv = 0;
  int          n_rows = 0;
  int          n_cols = 0;
  PointSet*    ps = newPS();

  rp = (pitem*)dtflatten(rows);
  cnt = 0;
  while (rp) {
    cdict = rp->u.rp;
    cp = (pitem*)dtflatten(cdict);
    while (cp) {
      cellp = cp->u.cp;
      cnt++;
      cp = (pitem*)dtlink(cdict,(Dtlink_t*)cp);
    }
    rp = (pitem*)dtlink(rows,(Dtlink_t*)rp);
  }

  cells = tbl->u.n.cells = N_NEW(cnt+1,htmlcell_t*);
  rp = (pitem*)dtflatten(rows);
  r = 0;
  while (rp) {
    cdict = rp->u.rp;
    cp = (pitem*)dtflatten(cdict);
    c = 0;
    while (cp) {
      cellp = cp->u.cp;
      *cells++ = cellp;
      rv |= size_html_cell (gvc, cellp, tbl, env);
      c = findCol (ps, r, c, cellp);
      cellp->row = r;
      cellp->col = c;
      c += cellp->cspan;
      n_cols = MAX(c, n_cols);
      n_rows = MAX(r+cellp->rspan, n_rows);
      cp = (pitem*)dtlink(cdict,(Dtlink_t*)cp);
    }
    rp = (pitem*)dtlink(rows,(Dtlink_t*)rp);
    r++;
  }
  tbl->rc = n_rows;
  tbl->cc = n_cols;
  dtclose (rows);
  freePS(ps);
  return rv;
}

/* Split size x over n pieces with spacing s.
 * We substract s*(n-1) from x, divide by n and 
 * take the ceiling.
 */
#define SPLIT(x,n,s) (((x) - ((s)-1)*((n)-1)) / (n))

/* sizeArray:
 * Determine sizes of rows and columns. The size of a column is the
 * maximum width of any cell in it. Similarly for rows.
 * A cell spanning columns contributes proportionately to each column
 * it is in.
 */
void
sizeArray (htmltbl_t* tbl)
{
  htmlcell_t*  cp;
  htmlcell_t** cells;
  int          wd, ht, i, x, y;

  tbl->heights = N_NEW (tbl->rc+1, int);
  tbl->widths = N_NEW (tbl->cc+1, int);

  for (cells = tbl->u.n.cells; *cells; cells++) {
    cp = *cells;
    if (cp->rspan == 1)
      ht = cp->data.box.UR.y;
    else {
      ht = SPLIT(cp->data.box.UR.y,cp->rspan,tbl->data.space);
      ht = MAX(ht,1);
    }
    if (cp->cspan == 1)
      wd = cp->data.box.UR.x;
    else {
      wd = SPLIT(cp->data.box.UR.x,cp->cspan,tbl->data.space);
      wd = MAX(wd,1);
    }
    for (i = cp->row; i < cp->row + cp->rspan; i++) {
      y = tbl->heights[i];
      tbl->heights[i] = MAX(y,ht);
    }
    for (i = cp->col; i < cp->col + cp->cspan; i++) {
      x = tbl->widths[i];
      tbl->widths[i] = MAX(x,wd);
    }
  }
}

static void pos_html_tbl (htmltbl_t*, box); /* forward declaration */

static void
pos_html_cell (htmlcell_t* cp, box pos)
{
  int           delx, dely;
  point         oldsz;
  box           cbox;

    /* If fixed, align cell */
  if (cp->data.flags & FIXED_FLAG) {
    oldsz = cp->data.box.UR;
    delx = (pos.UR.x - pos.LL.x) - oldsz.x;
    if (delx > 0) {
      switch (cp->data.flags & HALIGN_MASK) {
        case HALIGN_LEFT :
          pos.UR.x = pos.LL.x + oldsz.x;
          break;
        case HALIGN_RIGHT :
          pos.UR.x += delx;
          pos.LL.x += delx;
          break;
        default :
          pos.LL.x += delx/2;
          pos.UR.x -= delx/2;
          break;
      }
    }
    dely = (pos.UR.y - pos.LL.y) - oldsz.y;
    if (dely > 0) {
      switch (cp->data.flags & VALIGN_MASK) {
        case VALIGN_BOTTOM :
          pos.UR.y = pos.LL.y + oldsz.y;
          break;
        case VALIGN_TOP :
          pos.UR.y += dely;
          pos.LL.y += dely;
          break;
        default :
          pos.LL.y += dely/2;
          pos.UR.y -= dely/2;
          break;
      }
    }
  }
  cp->data.box = pos;

    /* set up child's position */
  cbox.LL.x = pos.LL.x + cp->data.border + cp->data.pad;
  cbox.LL.y = pos.LL.y + cp->data.border + cp->data.pad;
  cbox.UR.x = pos.UR.x - cp->data.border - cp->data.pad;
  cbox.UR.y = pos.UR.y - cp->data.border - cp->data.pad;

  if (cp->child.kind == HTML_TBL) {
    pos_html_tbl (cp->child.u.tbl, cbox);
  }
  else {
    oldsz = cp->child.u.txt->box.UR;
    delx = (cbox.UR.x - cbox.LL.x) - oldsz.x;
    if (delx > 0) {
      switch (cp->data.flags & HALIGN_MASK) {
        case HALIGN_LEFT :
          cbox.UR.x -= delx;
          break;
        case HALIGN_RIGHT :
          cbox.LL.x += delx;
          break;
        default :
          cbox.LL.x += delx/2;
          cbox.UR.x -= delx/2;
          break;
      }
    }
    dely = (cbox.UR.y - cbox.LL.y) - oldsz.y;
    if (dely > 0) {
      switch (cp->data.flags & VALIGN_MASK) {
        case VALIGN_BOTTOM :
          cbox.UR.y -= dely;
          break;
        case VALIGN_TOP :
          cbox.LL.y += dely;
          break;
        default :
          cbox.LL.y += dely/2;
          cbox.UR.y -= dely/2;
          break;
      }
    }
    cp->child.u.txt->box = cbox;
  }
}

/* pos_html_tbl:
 * Position table given its box, then calculate
 * the position of each cell.
 */
static void
pos_html_tbl (htmltbl_t* tbl, box pos)
{
  int           x, y, delx, dely;
  int           i, plus, extra, oldsz;
  htmlcell_t**  cells = tbl->u.n.cells;
  htmlcell_t*   cp;
  box           cbox;

  oldsz = tbl->data.box.UR.x;
  delx = (pos.UR.x - pos.LL.x) - oldsz;
  assert (delx >= 0);
  oldsz = tbl->data.box.UR.y;
  dely = (pos.UR.y - pos.LL.y) - oldsz;
  assert (dely >= 0);

    /* If fixed, align box */
  if (tbl->data.flags & FIXED_FLAG) {
    if (delx > 0) {
      switch (tbl->data.flags & HALIGN_MASK) {
        case HALIGN_LEFT :
          pos.UR.x = pos.LL.x + oldsz;
          break;
        case HALIGN_RIGHT :
          pos.UR.x += delx;
          pos.LL.x += delx;
          break;
        default :
          pos.LL.x += delx/2;
          pos.UR.x -= delx/2;
          break;
      }
      delx = 0;
    }
    if (dely > 0) {
      switch (tbl->data.flags & VALIGN_MASK) {
        case VALIGN_BOTTOM :
          pos.UR.y = pos.LL.y + oldsz;
          break;
        case VALIGN_TOP :
          pos.UR.y += dely;
          pos.LL.y += dely;
          break;
        default :
          pos.LL.y += dely/2;
          pos.UR.y -= dely/2;
          break;
      }
      dely = 0;
    }
  }

    /* change sizes to start positions and distribute extra space */
  x = pos.LL.x + tbl->data.border + tbl->data.space;
  extra = delx/(tbl->cc);
  plus = delx - extra*(tbl->cc);
  for (i = 0; i <= tbl->cc; i++) {
    delx = tbl->widths[i] + extra + (i<plus?1:0);
    tbl->widths[i] = x;
    x += delx + tbl->data.space;
  }
  y = pos.UR.y - tbl->data.border - tbl->data.space;
  extra = dely/(tbl->rc);
  plus = dely - extra*(tbl->rc);
  for (i = 0; i <= tbl->rc; i++) {
    dely = tbl->heights[i] + extra + (i<plus?1:0);
    tbl->heights[i] = y;
    y -= dely + tbl->data.space;
  }

  while ((cp = *cells++)) {
    cbox.LL.x = tbl->widths[cp->col];
    cbox.UR.x = tbl->widths[cp->col+cp->cspan] - tbl->data.space;
    cbox.UR.y = tbl->heights[cp->row];
    cbox.LL.y = tbl->heights[cp->row+cp->rspan] + tbl->data.space;
    pos_html_cell (cp, cbox);
  }

  tbl->data.box = pos;
}

/* size_html_tbl:
 * Determine the size of a table by first determining the
 * size of each cell.
 */
static int
size_html_tbl (GVC_t *gvc, htmltbl_t* tbl, htmlcell_t* parent, htmlenv_t* env)
{
  int    i, wd, ht;
  int    rv = 0;

  tbl->u.n.parent = parent;
  rv = processTbl (gvc, tbl, env);

    /* Set up border and spacing */
  if (!(tbl->data.flags & SPACE_SET)) {
    tbl->data.space = DEFAULT_CELLSPACING;
  }
  if (!(tbl->data.flags & BORDER_SET)) {
    tbl->data.border = DEFAULT_BORDER;
  }

  sizeArray (tbl);

  wd = (tbl->cc + 1)*tbl->data.space + 2*tbl->data.border;
  ht = (tbl->rc + 1)*tbl->data.space + 2*tbl->data.border;
  for (i = 0; i < tbl->cc; i++)
    wd += tbl->widths[i];
  for (i = 0; i < tbl->rc; i++)
    ht += tbl->heights[i];

  if (tbl->data.flags & FIXED_FLAG) {
    if (tbl->data.width && tbl->data.height) {
      if ((tbl->data.width < wd) || (tbl->data.height < ht)) {
        agerr(AGWARN, "table size too small for content\n");
        rv = 1;
      }
      wd = ht = 0;
    }
    else {
      agerr(AGWARN, "fixed table size with unspecified width or height\n");
      rv = 1;
    }
  }
  tbl->data.box.UR.x = MAX(wd, tbl->data.width);
  tbl->data.box.UR.y = MAX(ht, tbl->data.height);

  return rv;
}

static char*
nameOf (void* obj, agxbuf* xb)
{
  Agedge_t*  ep;
  switch (agobjkind(obj)) {
    case AGGRAPH: agxbput (xb, ((Agraph_t*)obj)->name); break;
    case AGNODE:  agxbput (xb, ((Agnode_t*)obj)->name); break;
    case AGEDGE:
      ep = (Agedge_t*)obj;
      agxbput(xb, ep->tail->name);
      agxbput(xb, ep->head->name);
      if (AG_IS_DIRECTED(ep->tail->graph)) agxbput (xb, "->");
      else agxbput (xb, "--");
      break;
  }
  return agxbuse(xb);
}

#ifdef DEBUG
void
indent (int i)
{
  while (i--)
    fprintf (stderr, "  ");
}
void
printBox (box b)
{
  fprintf (stderr, "(%d,%d)(%d,%d)", b.LL.x, b.LL.y, b.UR.x, b.UR.y);
}

void
printTxt (htmltxt_t* tp, int ind)
{
  int i;
  indent (ind);
  fprintf (stderr, "txt ");
  printBox (tp->box);
  fputs ("\n", stderr);
  for (i = 0; i < tp->nlines; i++) {
    indent (ind+1);
    fprintf (stderr, "(%c) \"%s\"\n", tp->line[i].just, tp->line[i].str);
  }
}

void
printData (htmldata_t* dp)
{
  unsigned char flags = dp->flags;
  char    c;

  fprintf (stderr, "s%d(%d) ", dp->space, (flags & SPACE_SET ? 1 : 0));
  fprintf (stderr, "b%d(%d) ", dp->border, (flags & BORDER_SET ? 1 : 0));
  fprintf (stderr, "p%d(%d) ", dp->pad, (flags & PAD_SET ? 1 : 0));
  switch (flags & HALIGN_MASK) {
    case HALIGN_RIGHT :
      c = 'r';
      break;
    case HALIGN_LEFT :
      c = 'l';
      break;
    default :
      c = 'c';
      break;
  }
  fprintf (stderr, "%c", c);
  switch (flags & VALIGN_MASK) {
    case VALIGN_TOP :
      c = 't';
      break;
    case VALIGN_BOTTOM :
      c = 'b';
      break;
    default :
      c = 'c';
      break;
  }
  fprintf (stderr, "%c ", c);
  printBox (dp->box);
}

void printCell (htmlcell_t* cp, int ind);

void
printTbl (htmltbl_t* tbl, int ind)
{
  htmlcell_t**   cells = tbl->u.n.cells;
  indent (ind);
  fprintf(stderr, "tbl %d %d ", tbl->cc, tbl->rc);
  printData (&tbl->data);
  fputs ("\n", stderr);
  while (*cells)
    printCell (*cells++, ind+1);
}

void
printCell (htmlcell_t* cp, int ind)
{
  indent (ind);
  fprintf(stderr, "cell %d %d %d %d ", cp->cspan, cp->rspan, cp->col, cp->row);
  printData (&cp->data);
  fputs ("\n", stderr);
  if (cp->child.kind == HTML_TBL)
    printTbl (cp->child.u.tbl, ind+1);
  else
    printTxt (cp->child.u.txt, ind+1);
}

void
printLbl (htmllabel_t* lbl)
{
  if (lbl->kind == HTML_TBL)
    printTbl (lbl->u.tbl, 0);
  else
    printTxt (lbl->u.txt, 0);
}
#endif /* DEBUG */

static char*
getPenColor (void* obj)
{
  char*   str;

  if (((str = agget(obj,"pencolor")) != 0) && str[0])
    return str;
  else if (((str = agget(obj,"color")) != 0) && str[0])
    return str;
  else
    return NULL;
}

/* make_html_label:
 * Return 1 if problem parsing HTML. In this case, use object name.
 */
int
make_html_label(GVC_t *gvc, textlabel_t *lp, void* obj)
{
  int   rv;
  int   wd2, ht2;
  box   box;
  htmllabel_t* lbl = parseHTML (lp->text, &rv);
  htmlenv_t    env;

  if (!lbl) {
    agxbuf   xb;
    unsigned char buf[SMALLBUF];
    agxbinit (&xb, SMALLBUF, buf);
    lbl = parseHTML (nameOf (obj, &xb),&rv);
    assert (lbl);
    rv = 1;
    agxbfree (&xb);
  }

  env.lp = lp;
  env.obj = obj;
  if (lbl->kind == HTML_TBL) {
    lbl->u.tbl->data.pencolor = getPenColor (obj);
    rv |= size_html_tbl (gvc, lbl->u.tbl, NULL, &env);
    wd2 = (lbl->u.tbl->data.box.UR.x +1)/2;
    ht2 = (lbl->u.tbl->data.box.UR.y +1)/2;
    box = boxof (-wd2, -ht2, wd2, ht2);
    pos_html_tbl (lbl->u.tbl, box);
    lp->dimen.x = PS2INCH (box.UR.x - box.LL.x);
    lp->dimen.y = PS2INCH (box.UR.y - box.LL.y);
  }
  else {
    rv |= size_html_txt (gvc, lbl->u.txt, &env);
    wd2 = (lbl->u.txt->box.UR.x +1)/2;
    ht2 = (lbl->u.txt->box.UR.y +1)/2;
    box = boxof (-wd2, -ht2, wd2, ht2);
    lbl->u.txt->box = box;
    lp->dimen.x = PS2INCH (box.UR.x - box.LL.x);
    lp->dimen.y = PS2INCH (box.UR.y - box.LL.y);
  }
  lp->u.html = lbl;
  return rv;
}

