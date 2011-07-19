# node.tcl --
#
# Package that defines the menubar::Node class. This class is a
# privite class used by the menubar::Tree class.
#
# Copyright (c) 2009 Tom Krehbiel <tomk@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: node.tcl,v 1.4 2010/01/06 20:55:54 tomk Exp $

package require TclOO

package provide menubar::node 0.5

# --------------------------------------------------
#
# manubar::Node class - used by menubar::Tree class
#
# --------------------------------------------------

# --
# parent   - contains the parent node instance
# children - contains list of child node instances
# attrs    - a dictionary of attribute/value pairs
oo::class create ::menubar::node {

	# --
	# create a named node
	constructor { pnode } {
		variable parent
		variable children
		variable attrs

		set parent ${pnode}
		set children {}
		set attrs [dict create]
	}

	# --
	# If 'pnode' isn't blank, set the node's parent to its
	# value; return the current parent.
	method parent { {pnode ""} } {
		variable parent
		if { ${pnode} ne "" } {
			set parent ${pnode}
		}
		return ${parent}
	}

	# --
	# If 'clist' is empty then return the current childern list else
	# set the node's children to 'clist' and return the current childern list.
	# If the option '-force' is found then set the node's children even
	# if 'clist' is blank.
	method children { {clist ""} args } {
		variable children
		if { [llength ${clist}] != 0 || "-force" in ${args} } {
			set children ${clist}
		}
		return ${children}
	}

	# --
	# Insert a list of node instances ('args') into the
	# child list at location 'index'.
	method insert { index args } {
		variable children
		set children [linsert ${children} ${index} {*}${args}]
		return
	}

	# --
	# If 'kdict' isn't blank set the node attributes to its
	# value; return the current value of attributes.
	method attrs { {kdict ""} {force ""} } {
		variable attrs
		if { ${kdict} ne "" || ${force} eq "-force" } {
			set attrs ${kdict}
		}
		return ${attrs}
	}

	# --
	# Return the node's attributes as a dict of key/value pairs. If
	# globpat exists, only keys that match the glob pattern will be
	# returned.
	method attrs.filter { {globpat ""} } {
		variable attrs
		if { ${globpat} eq "" } {
			return ${attrs}
		} else {
			return [dict filter ${attrs} key ${globpat}]
		}
	}

	# --
	# Return the node's attribute keys as a list. If globpat exists,
	# only return keys that match the glob pattern.
	method attr.keys { {globpat ""} } {
		variable attrs
		if { ${globpat} eq "" } {
			return [dict keys ${attrs}]
		} else {
			return [dict keys ${attrs} ${globpat}]
		}
	}

	# --
	# Set the value of the attribute 'key' to 'value'. If 'key
	# doesn't exist add it to the node.
	method attr.set { key value } {
		variable attrs
		dict set attrs ${key} ${value}
		return ${value}
	}

	# --
	#
	method attr.unset { key } {
		variable attrs
		dict unset attrs ${key}
		return
	}

	# --
	# Return true of attribute 'key' exists for node else return false.
	method attr.exists { key } {
		variable attrs
		return [dict exist ${attrs} ${key}]
	}

	# --
	# Return the value of the attribute 'key' for node.
	method attr.get { key } {
		variable attrs
		if { [dict exist ${attrs} ${key}] } {
			return [dict get ${attrs} ${key}]
		}
		error "attribute '${key}' - not found"
	}

	# --
	# Do a string append of 'value' to the value of attribute 'key' for
	# node. Return the resulting string value.
	method attr.append { key value } {
		variable attrs
		dict append attrs ${key} ${value}
		return
	}

	# --
	# Do a list append of 'value' to the value of attribute 'key' for
	# node. Return the resulting list value.
	method attr.lappend { key value } {
		variable attrs
		dict lappend attrs ${key} ${value}
		return
	}
}
