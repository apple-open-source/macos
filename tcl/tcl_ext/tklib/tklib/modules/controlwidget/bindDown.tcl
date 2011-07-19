

#
#    This software is Copyright by the Board of Trustees of Michigan
#    State University (c) Copyright 2005.
#
#    You may use this software under the terms of the GNU public license
#    (GPL) ir the Tcl BSD derived license  The terms of these licenses
#     are described at:
#
#     GPL:  http://www.gnu.org/licenses/gpl.txt
#     Tcl:  http://www.tcl.tk/softare/tcltk/license.html
#     Start with the second paragraph under the Tcl/Tk License terms
#     as ownership is solely by Board of Trustees at Michigan State University.
#
#     Author:
#             Ron Fox
#	     NSCL
#	     Michigan State University
#	     East Lansing, MI 48824-1321
#

#
# bindDown is a simple package that allows the user to attach
# bind tags to a hieararchy of widgets starting with the top of
# a widget tree.  The most common use of this is in snit::widgets
# to allow a binding to be placed on the widget itself e.g:
#  bindDown $win $win
#
#   where the first item is the top of the widget tree, the second the
#   bindtag to add to each widget in the subtree.
#   This will allow bind $win <yada> yada to apply to the widget
#   children.
#
#
package provide bindDown 1.0

proc bindDown {top tag} {
    foreach widget [winfo children $top] {
	set wtags [bindtags $widget]
	lappend   wtags $tag
	bindtags $widget [lappend wtags $tag]
	bindDown $widget $tag
    }
}
