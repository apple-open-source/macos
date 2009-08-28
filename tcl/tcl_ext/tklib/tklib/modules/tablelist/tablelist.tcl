#==============================================================================
# Main Tablelist package module.
#
# Copyright (c) 2000-2008  Csaba Nemethi (E-mail: csaba.nemethi@t-online.de)
#==============================================================================

package require Tcl 8
package require Tk  8
package require -exact tablelist::common 4.10

package provide Tablelist $::tablelist::version
package provide tablelist $::tablelist::version

::tablelist::useTile 0
::tablelist::createBindings
