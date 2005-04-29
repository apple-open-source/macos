#include <dotneato.h>

int main(int argc, char** argv)
{
    Agraph_t *g;
    Agnode_t *n,*m;
    Agedge_t *e;
    Agsym_t  *a;
    GVC_t    *gvc;

    /* set up renderer context */
    gvc = gvContext();

    /* Accept -T and -o options like dot.
     * Input files are ignored in this demo. */
    dotneato_initialize(gvc, argc,argv);

    /* Create a simple digraph */
    g = agopen("g",AGDIGRAPH);
    n = agnode(g,"n");
    m = agnode(g,"m");
    e = agedge(g,n,m);

    /* Set an attribute - in this case one that affects the visible rendering */
    if (!(a = agfindattr(g->proto->n, "color")))
        a = agnodeattr(g, "color", "");
    agxset(n, a->index, "red");

    /* bind graph to GV context - currently must be done before layout */
    gvBindContext(gvc,g);

    /* Compute a layout */
    neato_layout(g);
    /* twopi_layout(g); */
    /* dot_layout(g); */

    /* Write the graph according to -T and -o options */
    dotneato_write(gvc);

    /* Clean out layout data */
    /* neato_cleanup(g); */
    /* twopi_cleanup(g); */
    /* dot_cleanup(g); */

    /* Free graph structures */
    agclose(g);

    /* Clean up output file and errors */
    dotneato_terminate(gvc);

    return 1;
}    
