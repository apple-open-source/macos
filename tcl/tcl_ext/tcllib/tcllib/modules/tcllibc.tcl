# Umbrella, i.e. Bundle, to put all of the critcl modules which are
# found in Tcllib in one shared library.

package require critcl
package provide tcllibc 0.3.4

namespace eval ::tcllib {
    variable tcllibc_rcsid {$Id: tcllibc.tcl,v 1.8 2008/07/02 23:35:52 andreas_kupries Exp $}
    critcl::ccode {
        /* no code required in this file */
    }
}
