#ifndef GPRSTATE_H
#define GPRSTATE_H

#include <sfio.h>
#include <agraph.h>
#include <ast.h>
#include <vmalloc.h>

typedef enum {TV_flat, TV_dfs, TV_fwd, TV_rev, TV_ne, TV_en} trav_type;

typedef struct {
  Agraph_t*    curgraph;
  Agraph_t*    target;
  Agraph_t*    outgraph;
  Agobj_t*     curobj;
  Sfio_t*      tmp;  
  char*        tgtname;
  char*        infname;
  Sfio_t*      outFile;
  trav_type    tvt;
  Agnode_t*    tvroot;
  int          name_used;
  int          argc;
  char**       argv;
} Gpr_t;
 
typedef struct {
  Sfio_t*      outFile;
  int          argc;
  char**       argv;
} gpr_info;

extern Gpr_t* openGPRState ();
extern void initGPRState (Gpr_t*, Vmalloc_t*, gpr_info*);
extern int  validTVT(int);

#endif
