#!/bin/sh
# The next line is executed by /bin/sh, but not tcl \
exec wish "$0" ${1+"$@"}

set appDir [file dirname [info script]]
lappend auto_path [file join $appDir ".."]


package require BWidget 1.9.1

::BWidget::use \
    -package   "ttk" \
    -style     "native" \
    -themedirs [list [file join $appDir "themes"]] \
    -setoptdb  yes

source [file join $appDir "demo_main.tcl"]

Demo::main
after idle Demo::setTheme

wm geom . [wm geom .]
