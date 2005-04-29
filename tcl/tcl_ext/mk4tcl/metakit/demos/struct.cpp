//  This command-line utility displays the data structure of a datafile
//  created with the Metakit library as a one-line description.

#include "mk4.h"

#include <stdio.h>

#if defined (macintosh)
#include /**/ <console.h>
#define d4_InitMain(c,v)  c = ccommand(&v)
#endif

/////////////////////////////////////////////////////////////////////////////
  
int main(int argc, char** argv)
{
#ifdef d4_InitMain
  d4_InitMain(argc, argv);
#endif

  if (argc != 2)
    fputs("Usage: STRUCT datafile", stderr);
  else
  {
    c4_Storage store (argv[1], false);
    puts(store.Description());
  }
    
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
