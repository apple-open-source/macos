# preamble, included by all Metakit tests

source defs.tcl

if {[info commands mk::file] == ""} {
  set x [info sharedlibextension]
    # assume we're in tcl/test/, try debug version first if it exists
    # normally, builds happen in builds/ but check unix/ just in case
  foreach d {Mk4tcl_d Mk4tcl .libs/libmk4tcl} {
    if {![catch {load ../../builds/$d$x Mk4tcl}]} {
      #puts "using [file join [file dirname [file dirname [pwd]]] builds/$d$x]"
      break
    }
  }
  unset d x
}

package require Mk4tcl

S { 
  # do this before each test
} {
  # do this after each test
  foreach {db path} [mk::file open] {
    if {[string match db* $db]} {
      mk::file close $db
    }
  }
}
