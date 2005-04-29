# Tcl package index file, version 1.1

# We don't really want to throw an error with older versions of
# Tcl, they should just ignore us.
if {[package provide Tcl] < 8.4} {
    return
}

package require Tcl 8.4
if {[info tclversion] == 8.4} {
    if {[regexp {8.4a(1|2|3|4)} [info patchlevel]]} {
	error "Tcl 8.4a5 (March 20th 2002) or newer is required"
    }
}

package ifneeded vfs 1.0 "load [list [file join $dir Vfs[info sharedlibextension]]] vfs
source -rsrc vfs:tclIndex"

package ifneeded mk4vfs 1.5 [list source -rsrc mk4vfs]
