/* 
 * tclInitScript.h --
 *
 *	This file contains Unix & Windows common init script
 *      It is not used on the Mac. (the mac init script is in tclMacInit.c)
 *	This file should only be included once in the entire set of C
 *	source files for Tcl (by the respective platform initialization
 *	C source file, tclUnixInit.c and tclWinInit.c) and thus the
 *	presence of the routine, TclSetPreInitScript, below, should be
 *	harmless.
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclInitScript.h,v 1.2 2001/09/14 01:42:58 zlaski Exp $
 */

/*
 * In order to find init.tcl during initialization, the following script
 * is invoked by Tcl_Init().  It looks in several different directories:
 *
 *	$tcl_library		- can specify a primary location, if set
 *				  no other locations will be checked
 *
 *	$env(TCL_LIBRARY)	- highest priority so user can always override
 *				  the search path unless the application has
 *				  specified an exact directory above
 *
 *	$tclDefaultLibrary	- this value is initialized by TclPlatformInit
 *				  from a static C variable that was set at
 *				  compile time
 *
 *	<executable directory>/../lib/tcl$tcl_version
 *				- look for a lib/tcl<ver> in a sibling of
 *				  the bin directory (e.g. install hierarchy)
 *
 *	<executable directory>/../../lib/tcl$tcl_version
 *				- look for a lib/tcl<ver> in a sibling of
 *				  the bin/arch directory
 *
 *	<executable directory>/../library
 *				- look in build directory
 *
 *	<executable directory>/../../library
 *				- look in build directory from unix/arch
 *
 *	<executable directory>/../../tcl$tcl_patchLevel/library
 *				- look for tcl build directory relative
 *				  to a parallel build directory (e.g. Tk)
 *
 *	<executable directory>/../../../tcl$tcl_patchLevel/library
 *				- look for tcl build directory relative
 *				  to a parallel build directory from
 *				  down inside unix/arch directory
 *
 * The first directory on this path that contains a valid init.tcl script
 * will be set as the value of tcl_library.
 *
 * Note that this entire search mechanism can be bypassed by defining an
 * alternate tclInit procedure before calling Tcl_Init().
 */

static char initScript[] = "if {[info proc tclInit]==\"\"} {\n\
  proc tclInit {} {\n\
    global tcl_library tcl_version tcl_patchLevel errorInfo\n\
    global tcl_pkgPath env tclDefaultLibrary\n\
    global tcl_platform\n\
    rename tclInit {}\n\
    set errors {}\n\
    set dirs {}\n\
    if {[info exists tcl_library]} {\n\
	lappend dirs $tcl_library\n\
    } else {\n\
	if {[info exists env(TCL_LIBRARY)]} {\n\
	    lappend dirs $env(TCL_LIBRARY)\n\
	}\n\
        # CYGNUS LOCAL: I've changed this alot.  \n\
        # Basically we only care about two cases,\n\
        # if we are installed, and if we are in the devo tree...\n\
        # The next few are for if we are installed:\n\
        # NB. We also want this to work if the user is actually\n\
        # running a link and not the actual program.  So look for a\n\
        # link and chase it down to its source.\n\
        set execName [info nameofexecutable]\n\
        if {[string compare [file type $execName] \"link\"] == 0} {\n\
            set execName [file readlink $execName]\n\
            if {[string compare [file pathtype $execName] \"relative\"] == 0} {\n\
              set execName [file join [pwd] $execName]\n\
            }\n\
        }\n\
	set parentDir [file dirname [file dirname $execName]]\n\
        lappend dirs [file join $parentDir share tcl$tcl_version]\n\
        lappend dirs [file join $parentDir \"usr\" share tcl$tcl_version]\n\
	lappend dirs [file join [file dirname $parentDir] share tcl$tcl_version]\n\
        # NOW, let's try to find it in the build tree...\n\
        # Rather than play all the games Scriptics does, if we are in the build\n\
        # tree there will be a tclConfig.sh relative to the executible's directory, and we \n\
        # can read it and get the source dir from there...\n\
        #\n\
        # We duplicate all the directories in the search, one w/o the version and one with.\n\
        # Most modules use ../../tcl/{unix,win}\n\
        lappend configDirs [file join [file dirname $parentDir] tcl$tcl_version $tcl_platform(platform)]\n\
        lappend configDirs [file join [file dirname $parentDir] tcl             $tcl_platform(platform)]\n\
        # This one gets tclsh...\n\
        lappend configDirs $execName \n\
        # This one is for gdb, and any other app which has its executible in the top directory.\n\
        lappend configDirs [file join $parentDir tcl$tcl_version $tcl_platform(platform)]\n\
        lappend configDirs [file join $parentDir tcl             $tcl_platform(platform)]\n\
        # This last will handle itclsh & itkwish (../../../tcl/{unix,win}):\n\
        lappend configDirs [file join [file dirname [file dirname $parentDir]] tcl$tcl_version $tcl_platform(platform)]\n\
        lappend configDirs [file join [file dirname [file dirname $parentDir]] tcl             $tcl_platform(platform)]\n\
        \n\
        foreach i $configDirs {\n\
            set configFile [file join $i tclConfig.sh]\n\
            if {[file exists $configFile]} {\n\
                if {![catch {open $configFile r} fileH]} {\n\
                    set srcDir {}\n\
                    while {[gets $fileH line] >= 0} {\n\
                        if {[regexp {^TCL_SRC_DIR='([^']*)'} $line dummy srcDir]} {\n\
                            break\n\
                        }\n\
                    }\n\
                    close $fileH\n\
                    if {$srcDir != \"\"} {\n\
                        lappend dirs [file join $srcDir library]\n\
                        break\n\
                    }\n\
                }\n\
            }\n\
        }\n\
    }\n\
    # I also moved this from just after TCL_LIBRARY to last.\n\
    # I only want to use the compiled in library if I am really lost, because\n\
    # otherwise if I have installed once, but am working in the build directory,\n\
    # I will always pick up the installed files, which will be very confusing...\n\
    lappend dirs $tclDefaultLibrary\n\
    unset tclDefaultLibrary\n\
    foreach i $dirs {\n\
	set tcl_library $i\n\
	set tclfile [file join $i init.tcl]\n\
	if {[file exists $tclfile]} {\n\
	    if {![catch {uplevel #0 [list source $tclfile]} msg]} {\n\
	        return\n\
	    } else {\n\
		append errors \"$tclfile: $msg\n$errorInfo\n\"\n\
	    }\n\
            set tcl_pkgPath [lreplace $tcl_pkgPath end end]\n\
	}\n\
    }\n\
    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\n\"\n\
    append msg \"$errors\n\n\"\n\
    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n\
    error $msg\n\
  }\n\
}\n\
tclInit";

/*
 * A pointer to a string that holds an initialization script that if non-NULL
 * is evaluated in Tcl_Init() prior to the the built-in initialization script
 * above.  This variable can be modified by the procedure below.
 */
 
static char *          tclPreInitScript = NULL;


/*
 *----------------------------------------------------------------------
 *
 * TclSetPreInitScript --
 *
 *	This routine is used to change the value of the internal
 *	variable, tclPreInitScript.
 *
 * Results:
 *	Returns the current value of tclPreInitScript.
 *
 * Side effects:
 *	Changes the way Tcl_Init() routine behaves.
 *
 *----------------------------------------------------------------------
 */

char *
TclSetPreInitScript (string)
    char *string;		/* Pointer to a script. */
{
    char *prevString = tclPreInitScript;
    tclPreInitScript = string;
    return(prevString);
}
