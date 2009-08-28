# pkgIndex.tcl - index for http package
#
# Use to use lazy loading by defining the load command as:
# package ifneeded http 2.6 [list tclPkgSetup $dir http 2.6 {{http.tcl source {::http::config ::http::formatQuery ::http::geturl ::http::reset ::http::wait ::http::register ::http::unregister}}}]
#
if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded http 2.6.9 [list source [file join $dir http.tcl]]

