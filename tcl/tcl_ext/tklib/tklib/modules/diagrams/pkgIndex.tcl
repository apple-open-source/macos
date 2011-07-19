if {![package vsatisfies [package provide Tcl] 8.5]} {
    # PRAGMA: returnok
    return
}
package ifneeded diagram::navigation  1 [list source [file join $dir navigation.tcl]]
package ifneeded diagram::direction   1 [list source [file join $dir direction.tcl]]
package ifneeded diagram::element     1 [list source [file join $dir element.tcl]]
package ifneeded diagram::attribute   1 [list source [file join $dir attributes.tcl]]
package ifneeded diagram::point       1 [list source [file join $dir point.tcl]]
package ifneeded diagram::core        1 [list source [file join $dir core.tcl]]
package ifneeded diagram::basic       1 [list source [file join $dir basic.tcl]]
package ifneeded diagram              1 [list source [file join $dir diagram.tcl]]

package ifneeded diagram::application 1.1 [list source [file join $dir application.tcl]]

