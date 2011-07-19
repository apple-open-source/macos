# debug.tcl --
#
# Package that add debugging procedures to the global namespace
# and to the menubar::Tree class.
#
# Copyright (c) 2009 Tom Krehbiel <tomk@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: debug.tcl,v 1.5 2010/01/06 20:55:54 tomk Exp $

package require TclOO
package require menubar

package provide menubar::debug 0.5

# The ::oo namespace contains Tcloo commands that must be preceeded by 'my'.
# as the default namespace for callback commands

# --
#
# Generic debugging method for TclOO object instance.
#
oo::define ::oo::object method debug {{pat *}} {
	set res [list class [info object class [self]]]
	foreach i [info object vars [self] $pat] {
		variable $i
		lappend res $i [set $i]
	}
	set res
}

# -- pdict
#
# A pretty printer for dict object, similar to parray.
#
# Usage:
#
#   pdict <dict> [d [i [p [s]]]]
#
# Where:
#  d - dict to be printed
#  i - indent level
#  p - prefix string for one level of indent
#  s - seperator string between key and value
#
# Examples:
# % set d [dict create a {1 i 2 j 3 k} b {x y z} c {i m j {q w e r} k o}]
# % a {1 i 2 j 3 k} b {x y z} c {i m j {q w e r} k o}
# % pdict $d
# a ->
#   1 -> 'i'
#   2 -> 'j'
#   3 -> 'k'
# b -> 'x y z'
# c ->
#   i -> 'm'
#   j ->
#      q -> 'w'
#      e -> 'r'
#   k -> 'o'
#
proc ::pdict { d {i 0} {p "  "} {s " -> "} } {
	if { [catch {dict keys ${d}}] } {
		error "error: pdict - argument is not a dict"
	}
	set result ""
	set prefix [string repeat ${p} ${i}]
	set max 0
	foreach key [dict keys ${d}] {
		if { [string length ${key}] > ${max} } {
			set max [string length ${key}]
		}
	}
	dict for {key val} ${d} {
		append result "${prefix}[format "%-${max}s" ${key}]${s}"
		if { [catch {dict keys ${val}}] } {
			append result "'${val}'\n"
		} else {
			append result "\n"
			append result "[pdict ${val} [expr ${i}+1] ${p} ${s}]\n"
		}
	}
	return ${result}
}

# ------------------------------------------------------------
#
# Add debugging methods to ::menubar::tree class
#
# ------------------------------------------------------------

# -- ptree
# debugging utility
oo::define ::menubar::tree method ptree { {name ""} } {
	variable root
	if { ${name} eq "" } {
		my DumpSubtree ${root}
	} else {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		my DumpSubtree ${name}
	}
}

# -- pnodes
# debugging utility
oo::define ::menubar::tree method pnodes { } {
	variable nodes
	foreach name [lsort -dictionary [dict keys ${nodes}]] {
		set node [dict get ${nodes} ${name}]
		set pnode [${node} parent]
		set children [my children ${name}]
		puts [format "(%-12s) %-12s %s -> %s" ${pnode} ${node} ${name} [join ${children} {, }]]
	}
}

# -- pkeys
# debugging utility
oo::define ::menubar::tree method pkeys { args } {
	if { [llength ${args}] == 0 } {
		set args [my nodes]
	} else {
		set notfound [my exists {*}${args}]
		if { ${notfound} ne ""  } {
			error "node (${notfound}) - not found"
		}
	}
	foreach name ${args} {
		set node [my Name2Node ${name}]
		puts "node(${name})"
		set width 0
		foreach key  [${node} attr.keys] {
			set len [string length ${key}]
			if { ${len} > ${width} } { set width ${len} }
		}
		foreach {key val} [${node} attrs.filter] {
			puts "  [format "%-${width}s" ${key}]: '${val}'"
		}
	}
}

# -- pstream
# debugging utility
oo::define ::menubar::tree method pstream { stream } {
	lassign ${stream} name attrs children
	my Pstream ${name} ${attrs} ${children} 0
}

# ------------------------------------------------------------
#
# Add debugging methods to ::menubar class
#
# ------------------------------------------------------------
oo::define ::menubar method debug { {type tree} } {
	variable mtree
	variable installs
	variable notebookVals
	
	set result ""
	if { ${type} eq "tree" } {
		lappend result "##### tag tree #####"
		lappend result "menubar"
		lappend result {*}[my children menubar]

	} elseif { ${type} eq "nodes" } {
		lappend result "##### tag defs #####"
		foreach node [lsort -dictionary [${mtree} nodes]] {
			lappend result ${node}
			foreach {attr val} [${mtree} key.getall ${node} +*] {
				lappend result "  ${attr}: ${val}" 
			}
			foreach {opt val} [${mtree} key.getall ${node} -*] {
				lappend result "  ${opt}: ${val}" 
			}
		}
	} elseif { ${type} eq "installs" } {
		lappend result "##### installs #####"
		lappend result [pdict ${installs}]
	} elseif { ${type} eq "notebook" } {
		lappend result "##### notebookVals #####"
		lappend result [pdict ${notebookVals}]
	}
	return ${result}
}
oo::define ::menubar method children { node {indent 1} } {
	variable mtree
	set result ""
	foreach _node [${mtree} children ${node}] {
		lappend result [string repeat "  " ${indent}]${_node}
		set more [my children ${_node} [expr ${indent}+1]]
		if { [string trim ${more}] ne "" } {
			lappend result {*}${more}
		}
	}
	return ${result}
}
oo::define ::menubar method debug_node { node } {
	variable mtree
	lappend result "==== node: ${node}"
	foreach {attr val} [${mtree} key.getall ${node} +*] {
		lappend result "  ${attr}: ${val}" 
	}
	foreach {opt val} [${mtree} key.getall ${node} -*] {
		lappend result "  ${opt}: ${val}" 
	}
	return ${result}
}

oo::define ::menubar method print { type } {
	variable mtree
	switch -exact ${type} {
	"tree" {
		${mtree} ptree
	}
	"nodes" {
		${mtree} pnodes
	}
	"keys" {
		${mtree} pkeys
	}
	default {
	}}
}
