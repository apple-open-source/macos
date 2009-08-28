------------------------------------------------------------------------
                         Ffidl Version 0.6.1
                      Darwin 9 Universal version
    Copyright (c) 1999 by Roger E Critchlow Jr, Santa Fe, NM, USA
                           rec@elf.org

Changes since ffidl 0.5 are under BSD License and are
Copyright (c) 2005-2008, Daniel A. Steffen <das@users.sourceforge.net>

------------------------------------------------------------------------

Ffidl allows you to define Tcl/Tk extensions with pure Tcl wrappers
calling any shared library installed on your system, including the Tcl
and Tk core libraries.

Documentation can be found at doc/ffidl.html.

license.terms specifies the license under which Ffidl is distributed,
and that there is NO WARRANTY.

libffi/LICENSE specifies the license under which libffi is distributed,
and that there is NO WARRANTY.

------------------------------------------------------------------------

Changes since ffidl 0.6:
 - support for 4-way universal builds on Darwin
 - support for Leopard libffi
 - removal of ffcall and other code unused in Darwin universal build
 - support for Darwin Intel
 - Tcl ObjType bugfixes
 - TEA 3.6 buildsystem

Changes since ffidl 0.5:
 - updates for 2005 version of libffi
 - TEA 3.2 buildsystem, testsuite
 - support for Tcl 8.4, Tcl_WideInt, TclpDlopen
 - support for Darwin PowerPC
 - fixes for 64bit (LP64)
 - callouts & callbacks are created/used relative to current namespace
   (for unqualified names)
 - addition of [ffidl::stubsymbol] for Tcl/Tk symbol resolution via stubs
   tables
 - callbacks can be called anytime, not just from inside callouts
   (using Tcl_BackgroundError to report errors)

------------------------------------------------------------------------
