/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <stdlib.h>
#include "simple.h"

#define ABS(x) (((x)>0)?(x):(-(x)))
static void
sgnarea(l,m,i) struct vertex *l,*m; int i[];
	/* find the sign of the area of each of the triangles
		formed by adding a vertex of m to l	
	also find the sign of their product	*/
{	float a,b,c,d,e,f,g,h,t;
	a = l->pos.x; b = l->pos.y;
	c = after(l)->pos.x - a; d = after(l)->pos.y - b;
	e = m->pos.x - a ; f = m->pos.y - b ;
	g = after(m)->pos.x - a; h = after(m)->pos.y - b ;
	t = (c*f) - (d*e); i[0] = ((t == 0)?0:(t>0?1:-1));
	t = (c*h) - (d*g); i[1] = ((t == 0)?0:(t>0?1:-1));
	i[2] = i[0]*i[1];
}
static int
between(f,g,h)	/* determine if g lies between f and h	*/
float f,g,h;
{	if((f==g)||(g==h)) return(0); return((f<g)?(g<h?1:-1): (h<g?1:-1)); }
static int
online(l,m,i)	/* determine if vertex i of line m is on line l	*/
struct vertex *l,*m; 
int i;
{	struct position a,b,c;
	a = l->pos; b = after(l)->pos; c = (i == 0) ? m->pos : after(m)->pos ;
	return((a.x == b.x) ? ((a.x == c.x) && (-1 != between(a.y,c.y,b.y))) : 
			between(a.x,c.x,b.x));
}
static int
intpoint(l,m,x,y,cond)
struct vertex *l,*m;	 
float *x,*y;
int cond; /* determine point of detected intersections	*/ 
{	struct position ls,le,ms,me,pt1,pt2;
	float m1,m2,c1,c2;

	if (cond <= 0) return(0);
	ls = l->pos ; le = after(l)->pos ;
	ms = m->pos ; me = after(m)->pos;

	switch(cond)	{

	case 3:	     /* a simple intersection        */
		if (ls.x == le.x)   
		   {  *x = ls.x; *y = me.y + SLOPE(ms,me) * (*x - me.x); }
		else if (ms.x == me.x)   
		   {  *x = ms.x; *y = le.y + SLOPE(ls,le) * (*x - le.x); }
		else 
		 {  m1 = SLOPE(ms,me); m2 = SLOPE(ls,le);
		    c1 = ms.y - (m1*ms.x); c2 = ls.y - (m2*ls.x);
		    *x = (c2-c1)/(m1-m2); *y = ((m1*c2) -(c1*m2))/(m1-m2); }
		break;

	case 2:     /*     the two lines  have a common segment  */
		if (online(l,m,0) == -1)  /* ms between ls and le */
		{  pt1 = ms;
		   pt2 = (online(m,l,1) == -1)?((online(m,l,0) == -1)?le:ls):me;
		}
		else if (online(l,m,1) == -1)	/* me between ls and le */
		{ pt1 = me ;
		  pt2 = (online(l,m,0) == -1)?((online(m,l,0) == -1)?le:ls):ms;}
		else  {
			/* may be degenerate? */
			if (online(m,l,0) != -1) return 0;
			pt1 = ls; pt2 = le;
		}

		*x = (pt1.x + pt2.x)/2;	*y = (pt1.y + pt2.y)/2;
		break;

	case 1:   /* a vertex of line m is on line l */
		if ((ls.x-le.x)*(ms.y-ls.y) == (ls.y-le.y)*(ms.x-ls.x))
			{ *x = ms.x; *y = ms.y; }
		else 	{ *x = me.x; *y = me.y; }
	}/* end switch	*/
	return(1);
}
#define max(a,b)            (((a) > (b)) ? (a) : (b))

/*detect whether lines l and m intersect      */
static void
find_intersection(struct vertex *l,
                  struct vertex *m,
                  struct intersection ilist[],
                  struct data *input)
{       float x,y; 
	int i[3]; 
	sgnarea(l,m,i);

	if (i[2] > 0) return;

	if (i[2] < 0) 	{
	    sgnarea(m,l,i);
	    if (i[2] > 0) return;
	    if (!intpoint(l,m,&x,&y,(i[2]<0)?3:online(m,l,ABS(i[0])))) return; }

	else if (!intpoint(l,m,&x,&y,(i[0] == i[1])? 
		2*max(online(l,m,0),online(l,m,1)) : online(l,m,ABS(i[0]))))
			return;

	if (input->ninters >= MAXINTS) 	{
		fprintf(stderr,"\n**ERROR**\n using too many intersections\n");
		exit(1);		}

	ilist[input->ninters].firstv  = l;
	ilist[input->ninters].secondv = m;
	ilist[input->ninters].firstp  = l->poly;
	ilist[input->ninters].secondp = m->poly;
	ilist[input->ninters].x = x;
	ilist[input->ninters].y = y;
	input->ninters++;
}
int gt(const void *vi,const void *vj)

{     /* i > j if i.x > j.x or i.x = j.x and i.y > j.y	*/ 
	struct vertex **i = (struct vertex**)vi,**j = (struct vertex**)vj;
	double t;
	if ((t = (*i)->pos.x - (*j)->pos.x) != 0.) return( (t > 0.) ? 1 : -1 );
	if ((t = (*i)->pos.y - (*j)->pos.y) == 0.) return(0); 
	else return( (t > 0.) ? 1 : -1 );
}
void find_ints(struct vertex vertex_list[],
          struct polygon* polygon_list,
          struct data *input,
          struct intersection   ilist[])
{       
	int i,j,k;
	struct active_edge_list all;
	struct active_edge *new,*tempa;
	struct vertex *pt1,*pt2,*templ,**pvertex;

//    NOTUSED(polygon_list);
	input->ninters = 0;
	all.first = all.final = 0;	 all.number = 0;

	pvertex = (struct vertex **) 
		malloc((input->nvertices) * sizeof(struct vertex *));

	for (i = 0; i < input->nvertices ; i++ )
		pvertex[i] = vertex_list + i ;	

/* sort vertices by x coordinate	*/
	qsort(pvertex,input->nvertices,sizeof(struct vertex *),gt);

/* walk through the vertices in order of increasing x coordinate	*/
	for (i = 0 ; i < input->nvertices ; i++ )  {
		pt1 = pvertex[i]; templ = pt2 = prior(pvertex[i]);
		for (k = 0 ; k < 2 ; k++ )      {/* each vertex has 2 edges*/
			switch (gt(&pt1,&pt2))              {

case -1:  /* forward edge, test and insert	*/	

	for (tempa=all.first,j=0 ; j<all.number ; j++ , tempa = tempa->next )
		find_intersection(tempa->name,templ,ilist,input);   /* test*/

	new = (struct active_edge *) malloc(sizeof(struct active_edge));
	if (all.number == 0) { all.first = new; new->last = 0; } /* insert */
		else { all.final->next = new; new->last = all.final; }

	new->name = templ;	 new->next = 0;
	templ->active = new; all.final = new;	 all.number++;

	break;	/* end of case -1	*/

case 1:	/* backward edge, delete	*/

	if( (tempa = templ->active) == 0) 	{
		fprintf(stderr,"\n***ERROR***\n trying to delete a non line\n");
		exit(1);			 }
	if (all.number == 1) all.final = all.first = 0;  /* delete the line*/
	    else if (tempa == all.first)     
		    { all.first =  all.first->next; all.first->last = 0; }
		else if (tempa == all.final)  {
		        all.final = all.final->last; all.final->next = 0;  }
    		    else        { tempa->last->next = tempa->next;
				tempa->next->last = tempa->last; }
	free((char *) tempa);
	all.number--; templ->active = 0;
	break;	/* end of case 1	*/

}       /* end switch   */

	pt2 = after(pvertex[i]);	 templ =  pvertex[i];/*second neighbor*/
	}       /* end k for loop       */
	}       /* end i for loop       */
	free(pvertex);
}
int 
Plegal_arrangement( Ppoly_t	**polys, int	n_polys)	 {

	int	i, j, vno, nverts, rv;

	struct vertex 		*vertex_list;
	struct polygon 		*polygon_list;
	struct data 		input ;
	struct intersection   	ilist[10000];

	polygon_list = (struct polygon *)
		malloc(n_polys * sizeof(struct polygon));

	for (i = nverts = 0 ; i < n_polys; i++ )
		nverts += polys[i]->pn;

	vertex_list = (struct vertex *)
		malloc(nverts * sizeof(struct vertex));

	for (i = vno = 0 ; i < n_polys; i++ )	{
		polygon_list[i].start = &vertex_list[vno];
		for (j = 0 ; j < polys[i]->pn ; j++ )	{
			vertex_list[vno].pos.x = (float)polys[i]->ps[j].x;
			vertex_list[vno].pos.y = (float)polys[i]->ps[j].y;
			vertex_list[vno].poly = &polygon_list[i];
			vno++;
			}
		polygon_list[i].finish = &vertex_list[vno-1];
		}

	input.nvertices = nverts;
	input.npolygons = n_polys;

	find_ints(vertex_list, polygon_list, &input, ilist);

#define EQ_PT(v,w) (((v).x == (w).x) && ((v).y == (w).y))
	rv = 1;
	{
	int	i;
	struct position	 vft, vsd, avft, avsd;
	for (i = 0; i < input.ninters; i++) {
		vft = ilist[i].firstv->pos;
		avft = after(ilist[i].firstv)->pos;
		vsd = ilist[i].secondv->pos;
		avsd = after(ilist[i].secondv)->pos;
		if ( ((vft.x != avft.x) && (vsd.x != avsd.x)) ||
			((vft.x == avft.x)  &&
				!EQ_PT(vft,ilist[i]) &&
				!EQ_PT(avft,ilist[i])) || 
			((vsd.x == avsd.x)  &&
				!EQ_PT(vsd,ilist[i]) &&
				!EQ_PT(avsd,ilist[i])) )  {
			rv = 0;
/*
            if (Verbose) {
			  fprintf(stderr,"\nintersection %d at %.3lf %.3lf\n",
				  i,ilist[i].x,ilist[i].y);
			  fprintf(stderr,"seg#1 : (%.3lf, %.3lf) (%.3lf, %.3lf)\n"
				  ,(double)(ilist[i].firstv->pos.x) 
				  ,(double)(ilist[i].firstv->pos.y)
				  ,(double)(after(ilist[i].firstv)->pos.x)
				  ,(double)(after(ilist[i].firstv)->pos.y));
			  fprintf(stderr,"seg#2 : (%.3lf, %.3lf) (%.3lf, %.3lf)\n"
				  ,(double)(ilist[i].secondv->pos.x)
				  ,(double)(ilist[i].secondv->pos.y)
				  ,(double)(after(ilist[i].secondv)->pos.x)
				  ,(double)(after(ilist[i].secondv)->pos.y));
			}
*/
            }
	}
	}
	free(polygon_list);
	free(vertex_list);
	return rv;
}
