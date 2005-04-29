# tDOM Tcl package index file

package ifneeded tdom 0.8.0       \
    "[list source [file join $dir tdom.tcl]]; \
     [list load   [file join $dir libtdom080[info sharedlibextension] ] tdom]"
