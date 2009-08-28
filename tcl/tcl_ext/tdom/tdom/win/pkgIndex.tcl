# tDOM Tcl package index file

package ifneeded tdom 0.8.3 \
    "[list load   [file join $dir tdom083[info sharedlibextension] ] tdom];\
     [list source [file join $dir tdom.tcl]]"
