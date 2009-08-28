#==============================================================================
# Main Tablelist_tile package module.
#
# Copyright (c) 2000-2008  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require Tcl 8.4
package require Tk  8.4
if {$::tk_version < 8.5 || [regexp {^8\.5a[1-5]$} $::tk_patchLevel]} {
    package require tile 0.6
}
package require -exact tablelist::common 4.10

package provide Tablelist_tile $::tablelist::version
package provide tablelist_tile $::tablelist::version

::tablelist::useTile 1
::tablelist::createBindings

namespace eval ::tablelist {
    #
    # Commands related to tile themes:
    #
    namespace export	getThemes getCurrentTheme setTheme setThemeDefaults
}
