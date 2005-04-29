/*
 * this implements the resistor circuit current model for
 * computing node distance, as an alternative to shortest-path.
 * likely it could be improved by using edge weights, somehow.
 * Return 1 if successful; 0 otherwise (e.g., graph is disconnected).
 */
#include	"neato.h"
int circuit_model(graph_t *g, int nG)
{
	double	**Gm, **Gm_inv, sum;
	int i, j;
	node_t	*v;
	edge_t *e;

    if (Verbose) fprintf (stderr, "Calculating circuit model\n");
	Gm = new_array(nG, nG, 0.0);
	Gm_inv = new_array(nG, nG, 0.0);

	/* set non-diagonal entries */
	for (v = agfstnode(g); v; v = agnxtnode(g,v)) {
		for (e = agfstedge(g,v); e; e = agnxtedge(g,e,v)) {
			i = ND_id(e->tail);
			j = ND_id(e->head);
			if (i == j) continue;
			/* conductance is 1/resistance */
			Gm[i][j] = Gm[j][i] = -1.0 / ED_dist(e);	/* negate */
		}
	}
	/* set diagonal entries to sum of conductances but ignore nth node */
	for (i = 0; i < nG ; i++) {
		sum = 0.0;
		for (j = 0; j < nG ; j++)
			if (i != j) sum += Gm[i][j];
		Gm[i][i] = -sum;
	}

	if (!matinv(Gm, Gm_inv, nG - 1)) return 0;
	for (i = 0; i < nG ; i++) {
		for (j = 0; j < nG ; j++) {
			GD_dist(g)[i][j] = Gm_inv[i][i] + Gm_inv[j][j] - 2.0 * Gm_inv[i][j];
		}
	}
    return 1;
}
