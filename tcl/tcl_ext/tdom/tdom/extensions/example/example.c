
#include <tcl.h>
#include <string.h>
#include <expat.h>
#include <tclexpat.h>

/*
 * Beginning with 8.4, Tcl API is CONST'ified
 */
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION <= 3)
# define CONST84
#endif


typedef struct simpleCounter 
{
    int elementCounter;
} simpleCounter;

static char example_usage[] = 
               "Usage example <expat parser obj> <subCommand>, where subCommand can be: \n"
               "        enable    \n"
               "        getresult \n"
               "        remove    \n"
               ;



/*
 *----------------------------------------------------------------------------
 *
 * ExampleElementStartCommand --
 *
 *	This procedure is called for every element start event
 *      while parsing XML Data with an "example" enabled tclexpat
 *      parser.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Uses the "userData" field as int and increments it by every call.
 *
 *----------------------------------------------------------------------------
 */

void
ExampleElementStartCommand (userData, name, atts)
    void *userData;
    const char *name;
    const char **atts;
{
    simpleCounter *counter = (simpleCounter*) userData;
    
    counter->elementCounter++;
}


/*
 *----------------------------------------------------------------------------
 *
 * ExampleResetProc
 *
 *	Called for C handler set specific reset actions in case of
 *      parser reset.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resets the "userData" of the C handler set parser extension.
 *
 *----------------------------------------------------------------------------
 */

void
ExampleResetProc (interp, userData)
    Tcl_Interp *interp;
    void *userData;
{
    simpleCounter *counter = (simpleCounter*) userData;
    
    counter->elementCounter = 0;
}



/*
 *----------------------------------------------------------------------------
 *
 * ExampleFreeProc
 *
 *	Called for C handler set specific cleanup in case of parser
 *      delete.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	C handler set specific userData gets free'd.
 *
 *----------------------------------------------------------------------------
 */

void
ExampleFreeProc (interp, userData)
    Tcl_Interp *interp;
    void *userData;
{
    free (userData);
}


/*
 *----------------------------------------------------------------------------
 *
 * TclExampleObjCmd --
 *
 *	This procedure is invoked to process the "example" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The expat parser object provided as argument is enhanced by
 *      by the "example" handler set.
 *
 *----------------------------------------------------------------------------
 */

int
TclExampleObjCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
    char          *method;
    CHandlerSet   *handlerSet;
    int            methodIndex, result;
    simpleCounter *counter;
    

    static CONST84 char *exampleMethods[] = {
        "enable", "getresult", "remove",
        NULL
    };
    enum exampleMethod {
        m_enable, m_getresult, m_remove
    };

    if (objc != 3) {
        Tcl_WrongNumArgs (interp, 1, objv, example_usage);
        return TCL_ERROR;
    }

    if (!CheckExpatParserObj (interp, objv[1])) {
        Tcl_SetResult (interp, "First argument has to be a expat parser object", NULL);
        return TCL_ERROR;
    }

    method = Tcl_GetStringFromObj (objv[2], NULL);
    if (Tcl_GetIndexFromObj (interp, objv[2], exampleMethods, "method", 0,
                             &methodIndex) != TCL_OK)
    {
        Tcl_SetResult (interp, example_usage, NULL);
        return TCL_ERROR;
    }

    switch ((enum exampleMethod) methodIndex) {
    case m_enable:
        counter = (simpleCounter *) malloc (sizeof (simpleCounter));
        counter->elementCounter = 0;

        handlerSet = CHandlerSetCreate ("example");
        handlerSet->userData = counter;
        handlerSet->resetProc = ExampleResetProc;
        handlerSet->freeProc = ExampleFreeProc;
        handlerSet->elementstartcommand = ExampleElementStartCommand;

        result = CHandlerSetInstall (interp, objv[1], handlerSet);
        if (result == 1) {
            /* This should not happen if CheckExpatParserObj() is used. */
            Tcl_SetResult (interp, "argument has to be a expat parser object", NULL);
            return TCL_ERROR;
        }
        if (result == 2) {
            Tcl_SetResult (interp, "there is already a C handler set with this name installed", NULL);
            /* In error case malloc'ed memory should be free'ed */
            free (handlerSet->name);
            Tcl_Free ( (char *) handlerSet);
            return TCL_ERROR;
        }
        return TCL_OK;
    case m_getresult:
        counter = CHandlerSetGetUserData (interp, objv[1], "example");
        Tcl_SetIntObj (Tcl_GetObjResult (interp), counter->elementCounter);
        return TCL_OK;
    case m_remove:
        result = CHandlerSetRemove (interp, objv[1], "example");
        if (result == 1) {
            /* This should not happen if CheckExpatParserObj() is used. */
            Tcl_SetResult (interp, "argument has to be a expat parser object", NULL);
            return TCL_ERROR;
        }
        if (result == 2) {
            Tcl_SetResult (interp, "expat parser obj hasn't a C handler set named \"example\"", NULL);
            return TCL_ERROR;
        }
        return TCL_OK;
    default:
        Tcl_SetResult (interp, "unknown method", NULL);
        return TCL_ERROR;
    }
    
}

/*
 *----------------------------------------------------------------------------
 *
 * Example_Init --
 *
 *	Initialization routine for loadable module
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Defines "example" enhancement command for expat parser obj
 *
 *----------------------------------------------------------------------------
 */

int
Example_Init (interp)
    Tcl_Interp *interp;
{
    Tcl_PkgRequire (interp, "expat", "2.0", 0);
    Tcl_CreateObjCommand (interp, "example", TclExampleObjCmd, NULL, NULL );
    Tcl_PkgProvide (interp, "example", "1.0");
}

