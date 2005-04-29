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

#include	"render.h"
#include	"agxbuf.h"
#include	"utils.h"
#include	"htmltable.h"
#ifndef MSWIN32
#include	<unistd.h>
#endif


/* local funcs */
static double dist(pointf, pointf);

void *zmalloc(size_t nbytes)
{
	char	*rv = malloc(nbytes);
	if (nbytes == 0) return 0;
	if (rv == NULL) {fprintf(stderr, "out of memory\n"); abort();}
	memset(rv,0,nbytes);
	return rv;
}

void *zrealloc(void *ptr, size_t size, size_t elt, size_t osize)
{
	void	*p = realloc(ptr,size*elt);
	if (p == NULL && size) {fprintf(stderr, "out of memory\n"); abort();}
	if (osize < size) memset((char*)p+(osize*elt),'\0',(size-osize)*elt);
	return p;
}

void *gmalloc(size_t nbytes)
{
	char	*rv = malloc(nbytes);
	if (nbytes == 0) return 0;
	if (rv == NULL) {fprintf(stderr, "out of memory\n"); abort();}
	return rv;
}

void *grealloc(void *ptr, size_t size)
{
	void	*p = realloc(ptr,size);
	if (p == NULL && size) {fprintf(stderr, "out of memory\n"); abort();}
	return p;
}

/*
 *  a queue of nodes
 */
queue *
new_queue(int sz)
{
	queue		*q = NEW(queue);

	if (sz <= 1) sz = 2;
	q->head = q->tail = q->store = N_NEW(sz,node_t*);
	q->limit = q->store + sz;
	return q;
}

void
free_queue(queue* q)
{
	free(q->store);
	free(q);
}

void
enqueue(queue* q, node_t* n)
{
	*(q->tail++) = n;
	if (q->tail >= q->limit) q->tail = q->store;
}

node_t *
dequeue(queue* q)
{
	node_t	*n;
	if (q->head == q->tail) n = NULL;
	else {
		n = *(q->head++);
		if (q->head >= q->limit) q->head = q->store;
	}
	return n;
}

/* returns index of an attribute if bound, else -1 */
int
late_attr(void* obj, char* name)
{
	attrsym_t	*a;
	if ((a = agfindattr(obj,name)) != 0) return a->index;
	else return -1;
}

int
late_int(void *obj, attrsym_t *attr, int def, int low)
{
	char	*p;
	int		rv;
	if (attr == NULL) return def;
	p = agxget(obj,attr->index);
	if (p[0]  == '\0') return def;
	if ((rv = atoi(p)) < low) rv = low;
	return rv;
}

double
late_double(void *obj,attrsym_t *attr, double def, double low)
{
	char		*p;
	double		rv;

	if (attr == NULL) return def;
	p = agxget(obj,attr->index);
	if (p[0]  == '\0') return def;
	if ((rv = atof(p)) < low) rv = low;
	return rv;
}

char *
late_string(void* obj, attrsym_t* attr, char* def)
{
	if (attr == NULL) return def;
	return agxget(obj,attr->index);
}

char *
late_nnstring(void* obj, attrsym_t* attr, char* def)
{
	char	*rv = late_string(obj,attr,def);
	if (rv[0] == '\0') rv = def;
	return rv;
}

int
late_bool(void *obj, attrsym_t *attr, int def)
{
	if (attr == NULL) return def;
	return mapbool(agxget(obj,attr->index));
}

/* counts occurences of 'c' in string 'p' */
int
strccnt(char *p, char c)
{
	int		rv = 0;
	while (*p) if (*p++ == c) rv++;
	return 	rv;
}

/* union-find */
node_t	*
UF_find(node_t* n)
{
	while (ND_UF_parent(n) && (ND_UF_parent(n) != n)) {
		if (ND_UF_parent(n)->u.UF_parent) ND_UF_parent(n) = ND_UF_parent(n)->u.UF_parent;
		n = ND_UF_parent(n);
	}
	return n;
}

node_t	*
UF_union(node_t *u, node_t *v)
{
	if (u == v) return u;
	if (ND_UF_parent(u) == NULL) {ND_UF_parent(u) = u; ND_UF_size(u) = 1;}
	else u = UF_find(u);
	if (ND_UF_parent(v) == NULL) {ND_UF_parent(v) = v; ND_UF_size(v) = 1;}
	else v = UF_find(v);
	if (u->id > v->id) { ND_UF_parent(u) = v; ND_UF_size(v) += ND_UF_size(u);}
	else {ND_UF_parent(v) = u; ND_UF_size(u) += ND_UF_size(v); v = u;}
	return v;
}

void
UF_remove(node_t *u, node_t *v)
{
	assert(ND_UF_size(u) == 1);
	ND_UF_parent(u) = u;
	ND_UF_size(v) -= ND_UF_size(u);
}

void
UF_singleton(node_t* u)
{
	ND_UF_size(u) = 1;
	ND_UF_parent(u) = NULL;
	ND_ranktype(u) = NORMAL;
}

void
UF_setname(node_t *u, node_t *v)
{
	assert(u == UF_find(u));
	ND_UF_parent(u) = v;
	ND_UF_size(v) += ND_UF_size(u);
}

point coord(node_t *n)
{
     pointf      pf;
     pf.x = ND_pos(n)[0];
     pf.y = ND_pos(n)[1];
     return cvt2pt(pf);
}

point pointof(int x, int y)
{
	point rv;
	rv.x = x, rv.y = y;
	return rv;
}

point
cvt2pt(pointf p)
{
	point	rv;
	rv.x = POINTS(p.x);
	rv.y = POINTS(p.y);
	return rv;
}

pointf
cvt2ptf(point p)
{
	pointf	rv;
	rv.x = PS2INCH(p.x);
	rv.y = PS2INCH(p.y);
	return rv;
}

box boxof (int llx, int lly, int urx, int ury)
{
	box b;

	b.LL.x = llx, b.LL.y = lly;
	b.UR.x = urx, b.UR.y = ury;
	return b;
}

box mkbox(point p0, point p1)
{
	box		rv;

	if (p0.x < p1.x)	{	rv.LL.x = p0.x; rv.UR.x = p1.x; }
	else				{	rv.LL.x = p1.x; rv.UR.x = p0.x; }
	if (p0.y < p1.y)	{	rv.LL.y = p0.y; rv.UR.y = p1.y; }
	else				{	rv.LL.y = p1.y; rv.UR.y = p0.y; }
	return rv;
}

point add_points(point p0, point p1)
{
	p0.x += p1.x;
	p0.y += p1.y;
	return p0;
}

point sub_points(point p0, point p1)
{
	p0.x -= p1.x;
	p0.y -= p1.y;
	return p0;
}

/* from Glassner's Graphics Gems */
#define W_DEGREE 5

/*
 *  Bezier : 
 *	Evaluate a Bezier curve at a particular parameter value
 *      Fill in control points for resulting sub-curves if "Left" and
 *	"Right" are non-null.
 * 
 */
pointf Bezier (pointf *V, int degree, double t, pointf* Left, pointf* Right)
{
	int i, j;		/* Index variables	*/
	pointf Vtemp[W_DEGREE + 1][W_DEGREE + 1];

	/* Copy control points	*/
	for (j =0; j <= degree; j++) {
		Vtemp[0][j] = V[j];
	}

	/* Triangle computation	*/
	for (i = 1; i <= degree; i++) {	
		for (j =0 ; j <= degree - i; j++) {
	    	Vtemp[i][j].x =
	      		(1.0 - t) * Vtemp[i-1][j].x + t * Vtemp[i-1][j+1].x;
	    	Vtemp[i][j].y =
	      		(1.0 - t) * Vtemp[i-1][j].y + t * Vtemp[i-1][j+1].y;
		}
	}
	
	if (Left != NULL)
		for (j = 0; j <= degree; j++)
	    		Left[j] = Vtemp[j][0];
	if (Right != NULL)
		for (j = 0; j <= degree; j++)
	    		Right[j] = Vtemp[degree-j][j];

	return (Vtemp[degree][0]);
}

char *strdup_and_subst_graph(char *str, Agraph_t *g)
{
	char c, *s, *p, *t, *newstr;
	char *g_str=NULL;
	int g_len=0, newlen=0;

	/* two passes over str.
	 *
	 * first pass prepares substitution strings and computes 
	 * total length for newstring required from malloc.
	 */ 
	for (s=str;(c=*s++);) {
		if (c=='\\') {
			switch (c=*s++) {
			case 'G':
				if (! g_str) {
					g_str = g->name;
					g_len = strlen(g_str);
				}
				newlen += g_len;
				break;
			default:
				newlen +=2;
			}
		}
		else {
			newlen++;
		}
	}
	/* allocate new string */
	newstr=gmalloc(newlen+1);

	/* second pass over str assembles new string */
	for (s=str,p=newstr;(c=*s++);) {
		if (c=='\\') {
			switch (c=*s++) {
			case 'G':
				for (t=g_str;(*p=*t++);p++);
				break;

			default:
				*p++ = '\\';
				*p++ = c;
			}
		}
		else {
			*p++ = c;
		}
	}
	*p++ = '\0';
	return newstr;
}

char *strdup_and_subst_node(char *str, Agnode_t *n)
{
	char c, *s, *p, *t, *newstr;
	char *g_str=NULL, *n_str=NULL;
	int g_len=0, n_len=0, newlen=0;

	/* two passes over str.
	 *
	 * first pass prepares substitution strings and computes 
	 * total length for newstring required from malloc.
	 */ 
	for (s=str;(c=*s++);) {
		if (c=='\\') {
			switch (c=*s++) {
			case 'G':
				if (! g_str) {
					g_str = n->graph->name;
					g_len = strlen(g_str);
				}
				newlen += g_len;
				break;
			case 'N':
				if (! n_str) {
					n_str = n->name;
					n_len = strlen(n_str);
				}
				newlen += n_len;
				break;
			default:
				newlen +=2;
			}
		}
		else {
			newlen++;
		}
	}
	/* allocate new string */
	newstr=gmalloc(newlen+1);

	/* second pass over str assembles new string */
	for (s=str,p=newstr;(c=*s++);) {
		if (c=='\\') {
			switch (c=*s++) {
			case 'G':
				for (t=g_str;(*p=*t++);p++);
				break;

			case 'N':
				for (t=n_str;(*p=*t++);p++);
				break;
			default:
				*p++ = '\\';
				*p++ = c;
			}
		}
		else {
			*p++ = c;
		}
	}
	*p++ = '\0';
	return newstr;
}

char *strdup_and_subst_edge(char *str, Agedge_t *e)
{
	char c, *s, *p, *t, *newstr;
	char *e_str=NULL, *h_str=NULL, *t_str=NULL;
	int e_len=0, h_len=0, t_len=0, newlen=0;

	/* two passes over str.
	 *
	 * first pass prepares substitution strings and computes 
	 * total length for newstring required from malloc.
	 */ 
	for (s=str;(c=*s++);) {
		if (c=='\\') {
			switch (c=*s++) {
			case 'E':
				if (! e_str) {
					t_str = e->tail->name;
					t_len = strlen(t_str);
					h_str = e->head->name;
					h_len = strlen(h_str);
					if (e->tail->graph->root->kind & AGFLAG_DIRECTED)
						e_str = "->";
					else
						e_str = "--";
					e_len = t_len +2 +h_len;
				}
				newlen += e_len;
				break;
			case 'H':
				if (! h_str) {
					h_str = e->head->name;
					h_len = strlen(h_str);
				}
				newlen += h_len;
				break;
			case 'T':
				if (! t_str) {
					t_str = e->tail->name;
					t_len = strlen(t_str);
				}
				newlen += t_len;
				break;
			default:
				newlen +=2;
			}
		}
		else {
			newlen++;
		}
	}
	/* allocate new string */
	newstr=gmalloc(newlen+1);

	/* second pass over str assembles new string */
	for (s=str,p=newstr;(c=*s++);) {
		if (c=='\\') {
			switch (c=*s++) {
			case 'E':
				for (t=t_str;(*p=*t++);p++);
				for (t=e_str;(*p=*t++);p++);
				for (t=h_str;(*p=*t++);p++);
				break;
			case 'H':
				for (t=h_str;(*p=*t++);p++);
				break;
			case 'T':
				for (t=t_str;(*p=*t++);p++);
				break;
			default:
				*p++ = '\\';
				*p++ = c;
			}
		}
		else {
			*p++ = c;
		}
	}
	*p++ = '\0';
	return newstr;
}

/* return true if *s points to &[a-z]*;  (e.g. &amp; )
 *                          or &#[0-9]*; (e.g. &#38; )
 */
static int
xml_isentity(char *s)
{
	s++;  /* already known to be '&' */
	if (*s == '#') {
		s++;
		while (*s >= '0' && *s <= '9') s++;
	}
	else {
		while (*s >= 'a' && *s <= 'z') s++;
	}
	if (*s == ';') return 1;
	return 0;
}


char * xml_string(char *s)
{
	static char	*buf=NULL;
	static int	bufsize=0;
	char		*p, *sub;
	int		len, pos=0;

	if (!buf) {
		bufsize = 64;
		buf = gmalloc(bufsize);
	}

	p = buf;
	while (*s) {
		if (pos > (bufsize-8)) {
			bufsize *= 2;
			buf = grealloc(buf,bufsize);
			p = buf + pos;
		}
		/* these are safe even if string is already UTF-8 coded
		 * since UTF-8 strings won't contain '<' or '>' */
		if (*s == '<') {
			sub = "&lt;";
			len = 4;
		}
		else if (*s == '>') {
			sub = "&gt;";
			len = 4;
		}
		else if (*s == '"') {
			sub = "&quot;";
			len = 6;
		}
		else if (*s == '\'') {
			sub = "&apos;";
			len = 6;
		}
		/* escape '&' only if not part of a legal entity sequence */
		else if (*s == '&' && ! (xml_isentity(s))) {
			sub = "&amp;";
			len = 5;
		}
		else {
			sub = s;
			len = 1;
		}
		while (len--) {
			*p++ = *sub++;
			pos++;
		}
		s++;
	}
	*p = '\0';
	return buf;
}

/* make_label:
 * Assume str is freshly allocated for this instance, so it
 * can be freed in free_label.
 */
textlabel_t	*
make_label(GVC_t *gvc, int html, char *str, 
           double fontsize, char *fontname, char *fontcolor, graph_t *g)
{
	textlabel_t	*rv = NEW(textlabel_t);
	rv->text = str;
	rv->fontname = fontname;
	rv->fontcolor = fontcolor;
	rv->fontsize = fontsize;
	if (html) {
		rv->html = TRUE;
	}
	else
		label_size(gvc, str, rv, g);
	return rv;
}

void free_label(textlabel_t* p)
{
	if (p) {
		free(p->text);
		if (p->html) {
			free_html_label (p->u.html, 1);
		}
		else {
			if (p->u.txt.nlines != '\0')
				free(p->u.txt.line[0].str);
			free(p->u.txt.line);
		}
		free(p);
	}
}

#ifdef DEBUG
edge_t	* debug_getedge(graph_t *g, char *s0, char *s1)
{
	node_t	*n0,*n1;
	n0 = agfindnode(g,s0);
	n1 = agfindnode(g,s1);
	if (n0 && n1) return agfindedge(g,n0,n1);
	else return NULL;
}
#endif

#ifndef MSWIN32
#include	<pwd.h>
static unsigned char userbuf[SMALLBUF];
static agxbuf xb;

static void
cleanup()
{
  agxbfree(&xb);
}
#endif

char * username()
{
	char*          user = NULL;
#ifndef MSWIN32
    static int     first = 1;
	struct passwd* p;
    if (first) {
      agxbinit (&xb, SMALLBUF, userbuf);
      atexit(cleanup);
      first = 0;
    }
	p = (struct passwd *) getpwuid(getuid());
    if (p) {
      agxbputc (&xb, '(');
      agxbput (&xb, p->pw_name);
      agxbput (&xb, ") ");
#ifdef SVR4
      agxbput (&xb, p->pw_comment);
#else
      agxbput (&xb, p->pw_gecos);
#endif
      user = agxbuse (&xb);
    }
#endif
	if (user == NULL) user = "Bill Gates";
	return user;
}

/* Fgets:
 * Read a complete line.
 * Return pointer to line, 
 * or 0 on EOF
 */
static char* 
Fgets (FILE *fp)
{
  static int bsize = 0;
  static char* buf;
  char* lp;
  int len;

  len = 0;
  do {
    if (bsize - len < BUFSIZ) {
      bsize += BUFSIZ;
      buf = grealloc (buf, bsize);
    }
    lp = fgets (buf + len, bsize - len , fp);
    if (lp == 0) break;
    len += strlen(lp);        /* since lp != NULL, len > 0 */
  } while (buf[len-1] != '\n');

  if (len > 0) return buf;
  else return 0;
}

char* safefile(char *filename)
{
        static int onetime = TRUE;
	static char *safefilename = NULL;
	char *str, *p;

	if (!filename || !filename[0]) 
		return NULL;
        if (HTTPServerEnVar) {
		/* 
		 * If we are running in an http server we allow
		 * files only from the directory specified in
		 * the GV_FILE_PATH environment variable.
		 */
		if (!Gvfilepath) {
			if (onetime) {
				agerr(AGWARN, "file loading is disabled because the environment contains: %s\n"
                                "and there is no GV_FILE_PATH variable.\n",HTTPServerEnVar);
                		onetime = FALSE;
			}
			return NULL;
		}

		/* allocate a buffer that we are sure is big enough */
		safefilename = realloc(safefilename,
			 (strlen(Gvfilepath)+strlen(filename)));
                                                                                
		strcpy(safefilename,Gvfilepath);
		str = filename;
		if ((p=strrchr(str,'/')))
			str = ++p;
		if ((p=strrchr(str,'\\')))
			str = ++p;
		if ((p=strrchr(str,':')))
			str = ++p;
		strcat(safefilename,str);
		
                if (onetime && str != filename) {
			agerr(AGWARN, "Path provided to file: \"%s\" has been ignored" 
				" because files are only permitted to be loaded from the \"%s\""
				" directory when running in an http server.\n",filename,Gvfilepath);
                	onetime = FALSE;
		}

                return safefilename;
        }
	/* else, not in server, use original filename without modification. */
	return filename;
}

void cat_libfile(FILE *ofp, char **arglib, char **stdlib)
{
	FILE	*fp;
	char	*p,**s, *bp;
	int		i,use_stdlib = TRUE;

	if (arglib) {
		for (i = 0; (p = arglib[i]) != 0; i++)
			if (safefile(arglib[i])) use_stdlib = FALSE;
	}
	if (use_stdlib) for (s = stdlib; *s; s++) {fputs(*s,ofp); fputc('\n',ofp);}
	if (arglib) for (i = 0; (p = safefile(arglib[i])) != 0; i++) {
		if (p[0] && ((fp = fopen(p,"r")) != 0)) {
			while ((bp = Fgets(fp))) fputs(bp,ofp);
		}
		else agerr(AGWARN, "can't open library file %s\n", p);
	}
}

int
rect_overlap(box b0, box b1)
{
	if ((b0.UR.x < b1.LL.x) || (b1.UR.x < b0.LL.x) 
		|| (b0.UR.y < b1.LL.y) || (b1.UR.y < b0.LL.y)) return FALSE;
	return TRUE;
}

int
maptoken(char *p,char **name, int *val)
{
	int		i;
	char	*q;

	for (i = 0; (q = name[i]) != 0; i++)
		if (p && streq(p,q)) break;
	return val[i];
}

int
mapbool(char* p)
{
	if (p == NULL) return FALSE;
	if (!strcasecmp(p,"false")) return FALSE;
	if (!strcasecmp(p,"true")) return TRUE;
	return atoi(p);
}

#ifdef OLD
/* There are duplicate definitions of strcasecmp and strncasecmp 
 * in strcasecmp.c and strncasecmp.c 
 * Note also that the use of plain chars means these definitions
 * are not correct for systems with char != unsigned char.
 */
#ifdef MSWIN32
strcasecmp(s0,s1)
char	*s0,*s1;
{
	char		c0,c1;
	do {
		c0 = *s0++;
		c1 = *s1++;
		if (isupper(c0)) c0 = (char)tolower(c0);
		if (isupper(c1)) c1 = (char)tolower(c1);
		if (c0 != c1) break;
	} while (c0 && c1);
	return c0 - c1;
}
strncasecmp(s0,s1,n)
char	*s0,*s1;
int		n;
{
	char		c0,c1;
	int			m = n;
	while (m--) {
		c0 = *s0++;
		c1 = *s1++;
		if (isupper(c0)) c0 = (char)tolower(c0);
		if (isupper(c1)) c1 = (char)tolower(c1);
		if (c0 != c1) break;
	}
	return c0 - c1;
}
#endif
#endif

static double dist(p,q)
pointf  p,q;
{
    double  d0,d1;
    d0 = p.x - q.x;
    d1 = p.y - q.y;
    return sqrt(d0*d0 + d1*d1);
}

point dotneato_closest(splines* spl, point p)
{
	int		i, j, k, besti, bestj;
	double	bestdist, d, dlow, dhigh;
	double low, high, t;
	pointf c[4], pt2, pt;
	point rv;
	bezier bz;

	besti = bestj = -1;
	bestdist = 1e+38;
	pt.x = p.x; pt.y = p.y;
	for (i = 0; i < spl->size; i++) {
		bz = spl->list[i];
		for (j = 0; j < bz.size; j++) {
			pointf b;

			b.x = bz.list[j].x; b.y = bz.list[j].y;
			d = dist(b,pt);
			if ((bestj == -1) || (d < bestdist)) {
				besti = i;
				bestj = j;
				bestdist = d;
			}
		}
	}

	bz = spl->list[besti];
	j = bestj/3; if (j >= spl->size) j--;
	for (k = 0; k < 4; k++) {
		c[k].x = bz.list[j + k].x;
		c[k].y = bz.list[j + k].y;
	}
	low = 0.0; high = 1.0;
	dlow = dist(c[0],pt);
	dhigh = dist(c[3],pt);
	do {
		t = (low + high) / 2.0;
		pt2 = Bezier (c, 3, t, NULL, NULL);
		if (fabs(dlow - dhigh) < 1.0) break;
		if (low == high) break;
		if (dlow < dhigh) {high = t; dhigh = dist(pt2,pt);}
		else {low = t; dlow = dist(pt2,pt); }
	} while (1);
	rv.x = pt2.x;
	rv.y = pt2.y;
	return rv;
}

point spline_at_y(splines *spl, int y)
{
	int i,j;
	double low, high, d, t;
	pointf c[4], pt2;
	point pt;
	static bezier bz;

/* this caching seems to prevent pt.x from getting set from bz.list[0].x
	- optimizer problem ? */

#if 0
	static splines *mem = NULL;

	if (mem != spl) {
		mem = spl;
#endif
		for (i = 0; i < spl->size; i++) {
			bz = spl->list[i];
			if (BETWEEN (bz.list[bz.size-1].y, y, bz.list[0].y))
				break;
		}
#if 0
	}
#endif
	if (y > bz.list[0].y)
		pt = bz.list[0];
	else if (y < bz.list[bz.size-1].y)
		pt = bz.list[bz.size - 1];
	else {
		for (i = 0; i < bz.size; i += 3) {
			for (j = 0; j < 3; j++) {
				if ((bz.list[i+j].y <= y) && (y <= bz.list[i+j+1].y))
					break;
				if ((bz.list[i+j].y >= y) && (y >= bz.list[i+j+1].y))
					break;
			}
			if (j < 3)
				break;
		}
		assert (i < bz.size);
		for (j = 0; j < 4; j++) {
			c[j].x = bz.list[i + j].x;
			c[j].y = bz.list[i + j].y;
			/* make the spline be monotonic in Y, awful but it works for now */
			if ((j > 0) && (c[j].y > c[j - 1].y))
				c[j].y = c[j - 1].y;
		}
		low = 0.0; high = 1.0;
		do {
			t = (low + high) / 2.0;
			pt2 = Bezier (c, 3, t, NULL, NULL);
			d = pt2.y - y;
			if (ABS(d) <= 1)
				break;
			if (d < 0)
				high = t;
			else
				low = t;
		} while (1);
		pt.x = pt2.x;
		pt.y = pt2.y;
	}
	pt.y = y;
	return pt;
}

point neato_closest (splines *spl, point p)
{
/* this is a stub so that we can share a common emit.c between dot and neato */

	return spline_at_y(spl, p.y);
}

static int       Tflag;
void toggle(int s)
{
        Tflag = !Tflag;
#ifndef MSWIN32
        signal (SIGUSR1, toggle);
#endif
}

int test_toggle()
{
	return Tflag;
}

void common_init_node(node_t *n)
{
	char    *str;
	int     html = 0;
	GVC_t *gvc = GD_gvc(n->graph->root);

	gvc->n = n;

	ND_width(n) = late_double(n,N_width,DEFAULT_NODEWIDTH,MIN_NODEWIDTH);
	ND_height(n) = late_double(n,N_height,DEFAULT_NODEHEIGHT,MIN_NODEHEIGHT);
	if (N_label == NULL) str = NODENAME_ESC;
	else {
		str = agxget(n,N_label->index);
		html = aghtmlstr(str);
	}
	if (html)
		str = strdup(str);
	else
		str = strdup_and_subst_node(str,n);
	ND_label(n) = make_label(gvc, html,str,
		late_double(n,N_fontsize,DEFAULT_FONTSIZE,MIN_FONTSIZE),
		late_nnstring(n,N_fontname,DEFAULT_FONTNAME),
		late_nnstring(n,N_fontcolor,DEFAULT_COLOR), n->graph);
	if (html) {
		if (make_html_label(gvc, ND_label(n), n))
			agerr (AGPREV, "in label of node %s\n", n->name);
	}
	ND_shape(n) = bind_shape(late_nnstring(n,N_shape,DEFAULT_NODESHAPE));
	ND_showboxes(n) = late_int(n,N_showboxes,0,0);
	ND_shape(n)->fns->initfn(gvc);
}

static void
edgeError (edge_t *e, char* msg)
{
	char* edgeop;

	if (AG_IS_DIRECTED(e->tail->graph)) edgeop = "->";
	else edgeop = "--";
	agerr (AGPREV, "for %s of edge %s %s %s\n", 
		msg, e->tail->name, edgeop, e->head->name);
}

/* return TRUE if edge has label */
int common_init_edge(edge_t *e)
{
	char    *s;
	int     html = 0, r = 0;
	GVC_t *gvc = GD_gvc(e->tail->graph->root);
                                                                                        
        gvc->e = e;

	if (E_label && (s = agxget(e,E_label->index)) && (s[0])) {
		r = 1;
		html = aghtmlstr(s);
		if (html)
			s = strdup(s);
		else
			s = strdup_and_subst_edge(s,e);
		ED_label(e) = make_label(gvc, html, s,
			late_double(e,E_fontsize,DEFAULT_FONTSIZE,MIN_FONTSIZE),
			late_nnstring(e,E_fontname,DEFAULT_FONTNAME),
			late_nnstring(e,E_fontcolor,DEFAULT_COLOR),e->tail->graph);
		if (html) {
			if (make_html_label(gvc, ED_label(e), e))
				edgeError(e, "label");
		}
		GD_has_labels(e->tail->graph) |= EDGE_LABEL;
		ED_label_ontop(e) = mapbool(late_string(e,E_label_float,"false"));
	}

    /* vladimir */
	if (E_headlabel && (s = agxget(e,E_headlabel->index)) && (s[0])) {
		html = aghtmlstr(s);
		if (html)
			s = strdup(s);
		else
			s = strdup_and_subst_edge(s,e);
		ED_head_label(e) = make_label(gvc, 0, s,
			late_double(e,E_labelfontsize,DEFAULT_LABEL_FONTSIZE,MIN_FONTSIZE),
			late_nnstring(e,E_labelfontname,DEFAULT_FONTNAME),
			late_nnstring(e,E_labelfontcolor,DEFAULT_COLOR),e->tail->graph);
		if (html) {
			if (make_html_label(gvc, ED_head_label(e), e))
				edgeError(e, "head label");
		}
		GD_has_labels(e->tail->graph) |= HEAD_LABEL;
	}
	if (E_taillabel && (s = agxget(e,E_taillabel->index)) && (s[0])) {
		html = aghtmlstr(s);
		if (html)
			s = strdup(s);
		else
			s = strdup_and_subst_edge(s,e);
		ED_tail_label(e) = make_label(gvc, 0, s,
			late_double(e,E_labelfontsize,DEFAULT_LABEL_FONTSIZE,MIN_FONTSIZE),
			late_nnstring(e,E_labelfontname,DEFAULT_FONTNAME),
			late_nnstring(e,E_labelfontcolor,DEFAULT_COLOR),e->tail->graph);
		if (html) {
			if (make_html_label(gvc, ED_tail_label(e), e))
				edgeError(e, "tail label");
		}
		GD_has_labels(e->tail->graph) |= TAIL_LABEL;
	}
    /* end vladimir */

        return r;
}

point 
flip_pt(point p)
{ int		t = p.x; p.x = -p.y; p.y = t; return p; }

pointf
flip_ptf(pointf p)
{ double	t = p.x; p.x = -p.y; p.y = t; return p; }

point
invflip_pt(point p)
{ int		t = p.x; p.x = p.y; p.y = -t; return p; }

box 
flip_rec_box(box b, point p)
{
	box	rv;
		/* flip box */
	rv.UR.x = b.UR.y; rv.UR.y = b.UR.x;
	rv.LL.x = b.LL.y; rv.LL.y = b.LL.x;
		/* move box */
	rv.LL.x  += p.x; rv.LL.y += p.y;
	rv.UR.x  += p.x; rv.UR.y += p.y;
	return rv;
}

