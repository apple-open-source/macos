
$Id: README.txt,v 1.1 2003/04/02 22:48:13 jenglish Exp $ 

BUILD INSTRUCTIONS [2 Apr 2003]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This subdirectory (tcldom/src) is independant of the
main TclDOM code base.  The only external dependencies
are Tcl and expat.

The usual configure/make process is supported.  You may need 
to specify the following additional arguments to 'configure':

    --with-tcl=...
    	Specify directory containing tclConfig.sh

    --with-expat-include=...
    	Specify directory containing Expat header file (expat.h)

    --with-expat-lib=...
    	Full path to Expat library (libexpat.a, expat.lib).

You may also need to edit the generated Makefile, especially
on Windows. 

On Windows, if you have cygwin and MSVC, set CC=cl in the
environment prior to running 'sh configure'.  The following
worked for me (2 Apr 2003) after installing the expat_win32bin
distribution from expat.sourceforge.net:

    vcvars32
    set CC=cl
    set expat=C:\Expat-1.95.6
    sh configure --with-tcl=C:\Tcl\lib --with-expat-include=%expat%\Source\lib --with-expat-lib=%expat%\Libs\libexpat.lib

If you don't have MSVC, then MinGW (v2.0.0) + MSYS (v1.0.8)
also works [2 Apr 2003].  MinGW and MSYS are available at at
<URL: http://mingw.sourceforge.net>.  Be sure to install the
MSYS version of expat; the expat_win32bin distribution from 
expat.sourceforge.net appears not to work with this combination.


