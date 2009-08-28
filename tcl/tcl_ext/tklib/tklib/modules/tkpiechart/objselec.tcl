# copyright (C) 1997-2004 Jean-Luc Fontaine (mailto:jfontain@free.fr)
# this program is free software: please read the COPYRIGHT file enclosed in this
# package or use the Help Copyright menu

package require Tk 8.3
package require stooop

# $Id: objselec.tcl,v 1.13 2006/01/27 19:05:52 andreas_kupries Exp $

# implements selection on a list of object identifiers (sortable list of
# integers), for a listbox implementation, for example

::stooop::class objectSelector {

    proc objectSelector {this args} selector {$args} {}

    proc ~objectSelector {this} {}

    ### public procedures follow:

    proc extend {this id} {
        if {[info exists selector::($this,lastSelected)]} {
            set list [lsort -integer [selector::list $this]]
            set last [lsearch -exact $list $selector::($this,lastSelected)]
            set index [lsearch -exact $list $id]
            selector::clear $this
            if {$index > $last} {
                selector::set $this [lrange $list $last $index] 1
            } else {
                selector::set $this [lrange $list $index $last] 1
            }
        } else {
            selector::select $this $id
        }
    }

}
