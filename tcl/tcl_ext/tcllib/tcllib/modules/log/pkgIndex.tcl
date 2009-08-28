if {![package vsatisfies [package provide Tcl] 8]} {return}
package ifneeded log 1.2.1 [list source [file join $dir log.tcl]]

if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded logger           0.8 [list source [file join $dir logger.tcl]]
package ifneeded logger::appender 1.3   [list source [file join $dir loggerAppender.tcl]]

if {![package vsatisfies [package provide Tcl] 8.4]} {return}
package ifneeded logger::utils    1.3   [list source [file join $dir loggerUtils.tcl]]
