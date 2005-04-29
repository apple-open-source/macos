#include <dotneato.h>

int main(int argc, char** argv)
{
    Agraph_t *g, *prev=NULL;
    GVC_t *gvc;

    gvc = gvContext();

    dotneato_initialize(gvc,argc,argv);
    while ((g = next_input_graph())) {
        if (prev) {
            dot_cleanup(prev);
            agclose(prev);
        }
	prev = g;

	gvBindContext(gvc, g);

        dot_layout(g);
        dotneato_write(gvc);
    }
    dotneato_terminate(gvc);
    return 1;
}    
