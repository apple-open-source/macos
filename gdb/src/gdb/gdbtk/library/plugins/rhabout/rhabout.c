/* Sample command procedure library for a plug-in. */

/* You have to include the Tcl headers, of course. */
#include <tcl.h>

/* Define the functions that implement your commands as required by Tcl */

int extra_text (ClientData clientData,
                Tcl_Interp *interp,
                int argc, char *argv[]);

/* Here you actually do whatever you want, like calling your target 
   libraries etc.  Here we just return a string. */

int
extra_text (ClientData clientData,
                Tcl_Interp *interp,
                int argc, char *argv[])
{
  interp->result = "\nThis is a sample plug-in\n";
  return TCL_OK;
}

/* Initialization function required in Tcl libraries. */

int
Rhabout_Init (Tcl_Interp *interp)
{
  /* Register your command as a Tcl command with this interpreter. */
  Tcl_CreateCommand (interp, "rhabout_extra_text", extra_text,
                     (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

  return TCL_OK;
}
