# Tcl package index file, version 1.0
#if {[info tclversion] < 8.0} return

proc mc_ifneeded dir {
    rename mc_ifneeded {}
    regsub {\.} [info tclversion] {} version
    package ifneeded Memchan 2.2 "load [list [file join $dir memchan22$version.dll]] Memchan"
}

mc_ifneeded $dir
