# tree.tcl --
#
# Package that defines the menubar::Tree class. This class is a
# privite class used by the menubar class.
#
# Copyright (c) 2009 Tom Krehbiel <tomk@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: tree.tcl,v 1.5 2010/01/06 20:55:54 tomk Exp $

package require TclOO
package require menubar::node

package provide menubar::tree 0.5

# --------------------------------------------------
#
# menubar::Tree class - used by menubar class
#
# --------------------------------------------------

# --
#
# nid   - integer value used to create unique node names
# root  - name of tree's root node
# nodes - index of node names and node instances
#
oo::class create ::menubar::tree {

   self export varname

	constructor { } {
		variable root
		variable nodes

		my eval upvar [[self class] varname nid] nid
		set nid 0
		set root "root"
		set nodes [dict create "root" [::menubar::node new ""]]
	}

	destructor {
		variable nodes
		dict for {name node} ${nodes} {
			${node} destroy
		}
	}

	##### PRIVITE ##############################

	# --
	# used by debugging utility
	method DumpSubtree { parent {indent 0} } {
		set pnode [my Name2Node ${parent}]
		puts "[format "%-12s" ${pnode}]- [string repeat {  } ${indent}]${parent}"
		incr indent
		foreach child [${pnode} children] {
			my DumpSubtree [my Node2Name ${child}] ${indent}
		}
	}

	# --
	# check args for a node that exists and return its name
	# else return ""
	method NotUsed { args } {
		variable nodes
		foreach name ${args} {
			if { [dict exists ${nodes} ${name}] } {
				return ${name}
			}
		}
		return ""
	}

	# --
	# return a node instance given a node name
	method Name2Node { name } {
		variable nodes
		return [dict get ${nodes} ${name}]
	}

	# --
	# return a node name given a node instance
	method Node2Name { node } {
		variable nodes
		dict for {name node} [dict filter ${nodes} value ${node}] {
			return ${name}
		}
		error "node (${node}) - not found"
	}

	# --
	# return a list of node instances given a list of node names
	method Names2NodeList { args } {
		set nlist {}
		foreach name ${args} {
			lappend nlist [my Name2Node ${name}]
		}
		return ${nlist}
	}

	# --
	# return a list of node names given a list of node instances
	method Nodes2NameList { args } {
		set nlist {}
		foreach node ${args} {
			lappend nlist [my Node2Name ${node}]
		}
		return ${nlist}
	}

	# --
	# return the list of all nodes below parent node
	# optionaly filter nodes useing procedure 'filter'
	method GetSubtree { parent {filter ""} } {
		variable nodes
		set pnode [my Name2Node ${parent}]
		set children [my Nodes2NameList {*}[${pnode} children]]
		set subtree ""
		foreach child ${children}  {
			if { ${filter} eq "" || [eval [list ${filter} [self object] ${child}]] == 0 } {
				lappend subtree ${child}
				lappend subtree {*}[my GetSubtree ${child} ${filter}]
			}
		}
		return ${subtree}
	}

	# --
	# completely delete one node
	method DeleteNode { name } {
		variable root
		variable nodes
		set node [my Name2Node ${name}]
		# delete node from index
		set nodes [dict remove ${nodes} ${name}]
		# create a new root node if it was deleted
		if { ${name} eq ${root} } {
			dict set nodes ${name} [::menubar::node new ""]
		}
		${node} destroy
	}

	# --
	# replace the child entry for 'name' in its parent
	# with 0 or more new children
	method ReplaceParentLink { name args } {
		set cnode [my Name2Node ${name}]
		set pnode [${cnode} parent]
		if { ${pnode} eq "" } { return }
		set children [${pnode} children]
		set idx [lsearch -exact ${children} ${cnode}]
		if { ${idx} < 0 } {
			error "node (${name}) - not found"
		}
		if { [llength ${args}] == 0 } {
			set children [lreplace ${children} ${idx} ${idx}]
		} else {
			set nlist [my Names2NodeList {*}${args}]
			set children [lreplace ${children} ${idx} ${idx} {*}${nlist}]
		}
		${pnode} children ${children} -force
	}

	# --
	# Serialize a node and add it to stream.
	#
	# The result is a 3 element list haveing the following entries.
	#
	# 1) node name
	# 2) the node's attributes in dictionary form
	# 3) a recursive serialization of all children of the node
	#
	method SerializeNode { stream name {isroot 0}} {
		variable root
		variable nodes
		# serialize the children
		set children {}
		foreach child [my children ${name}] {
			lappend children {*}[my SerializeNode ${stream} ${child}]
		}
		set node [my Name2Node ${name}]
		lappend stream ${name} [${node} attrs.filter] ${children}
		return ${stream}
	}

	# --
	# Unlink a list of nodes from their parents. Note that a node
	# may be in the subtree of a node that is being unlinked.
	method UnlinkNodes { args } {
		set notfound [my exists {*}${args}]
		if { ${notfound} ne ""  } {
			error "node (${notfound}) - not found"
		}
		# Break the links to the parents
		foreach name ${args} {
			my ReplaceParentLink ${name}
			set pnode [my Name2Node ${name}]
			${pnode} parent ""
		}
	}

	# -- Pstream
	# Pretty print a node from a serialization stream.
	method Pstream { name attrs children indent } {
		set pad [string repeat "  " ${indent}]
		puts "${pad}${name}"
		puts "${pad}  ${attrs}"
		incr indent
		foreach {n a c} ${children} {
			my Pstream ${n} ${a} ${c} ${indent}
		}
	}

	# --
	# pnode		- parent node
	# name		- name of new node
	# attrs		- attribure dict for new node
	# children	- recursive list of child node serializations
	method DeserializeNode { pnode name attrs children } {
		variable nodes
		# create the a node and set it's parent
		set cnode [::menubar::node new ${pnode}]
		# add the node to the index
		dict set nodes ${name} ${cnode}
		# set the node's attributes
		${cnode} attrs ${attrs} -force
		# create all the children for the node
		set cnodes {}
		foreach {n a c} ${children} {
			lappend cnodes [my DeserializeNode ${cnode} ${n} ${a} ${c}]
		}
		${cnode} children ${cnodes} -force
		return ${cnode}
	}

	##### PUBLIC ##############################


	# --
	#
	method ancestors { child } {
		if { [my exists ${child}] ne ""  } {
			error "node (${child}) - not found"
		}
		set ancestors {}
		while { true } {
			set ancestor [my parent ${child}]
			if { ${ancestor} eq ""  } {
				break
			} else {
				lappend ancestors ${ancestor}
				set child ${ancestor}
			}
		}
		return ${ancestors}
	}

	# --
	#
	method children { parent } {
		variable nodes
		if { [my exists ${parent}] ne ""  } {
			error "node (${parent}) - not found"
		}
		set pnode [my Name2Node ${parent}]
		set children [${pnode} children]
		return [my Nodes2NameList {*}${children}]
	}

	# --
	# Remove a node from the tree and move its
	# children into the parent. Ignore cut on
	# the root node.
	method cut { name {opt ""} } {
		variable nodes
		if { ${name} eq [my rootname] } { return }
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		# get the children for the node
		set children [my children ${name}]
		# replace the node with its childer in the parent
		my ReplaceParentLink ${name} {*}${children}
		if { ${opt} eq "-delete" } {
			# delete the node
			set node [my Name2Node ${name}]
			dict unset nodes ${name}
			${node} destroy
		}
		return
	}

	# --
	#
	method delete { args } {
		set notfound [my exists {*}${args}]
		if { ${notfound} ne ""  } {
			error "node (${notfound}) - not found"
		}
		# Remove all the subtree nodes.
		# This code accounts for the possibility that
		# one of the args is in the subtree of another arg.
		set names {}
		foreach name ${args} {
			lappend names {*}[my descendants ${name}]
		}
		foreach name [lsort -unique ${names}] {
			my DeleteNode ${name}
		}
		# Now remove the nodes themselves and their child
		# entry in their parent
		foreach name ${args} {
			my ReplaceParentLink ${name}
			my DeleteNode ${name}
		}
		return
	}

	# --
	#
	method depth { name } {
		return [llength [my ancestors ${name}]]
	}

	# --
	#
	method descendants { parent {opt ""} {arg ""} } {
		variable nodes
		if { [my exists ${parent}] ne ""  } {
			error "node (${parent}) - not found"
		}
		if { ${opt} eq "-filter" } {
			set filter ${arg}
			return [my GetSubtree ${parent} ${filter}]
		} else {
			return [my GetSubtree ${parent}]
		}
	}

	# --
	# Replace the attribute and subtree definitions of node
	# 'lname' with the definitions found in 'stream'. The 'lname'
	# node must be a leaf node unless the '-force' option is is
	# used.
	method deserialize { lname stream {opt ""} } {
		variable root
		variable nodes
		if { [my exists ${lname}] ne "" } {
			error "node (${lname}) - not found"
		}
		if { ${opt} eq "-force" } {
			# force lname to be a leaf
			set parent [my parent ${lname}]
			my delete ${lname}
			set node [::menubar::node new [my Name2Node ${parent}]]
			dict set nodes ${lname} ${node}
		}
		if { ![my isleaf ${lname}] } {
			error "node (${lname}) - is not a leaf node"
		}
		# get the leaf node
		set lnode [my Name2Node ${lname}]
		# get the root of the serialization
		lassign ${stream} rname attrs children
		# put attributes in the leaf node
		${lnode} attrs ${attrs} -force
		# deserialize all the children into the leaf node
		set cnodes {}
		foreach {n a c} ${children} {
			lappend cnodes [my DeserializeNode ${lnode} ${n} ${a} ${c}]
		}
		${lnode} children ${cnodes} -force
		return
	}

	# --
	# return "" if all exist else return name that isn't found
	method exists { args } {
		variable nodes
		foreach name ${args} {
			if { ![dict exists ${nodes} ${name}] } {
				return ${name}
			}
		}
		return ""
	}

	# --
	#
	method index { name } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set cnode [my Name2Node ${name}]
		set pnode [${cnode} parent]
		set children [${pnode} children]
		return [lsearch -exact ${children} ${cnode}]
	}

	# --
	#
	method insert { parent index args } {
		variable nid
		variable nodes
		if { [llength ${args}] == 0 } {
			incr nid
			set args "node${nid}"
		} else {
			if { ${parent} in ${args} } {
				error "parent (${parent}) - found in insert list"
			}
		}
		set pnode [my Name2Node ${parent}]
		set nlist ""
		foreach name ${args} {
			if { [my exists ${name}] ne ""  } {
				# create a new child that references the parent
				set node [::menubar::node new ${pnode}]
				# add the node to the index
				dict set nodes ${name} ${node}
			} else {
				# child already exists so it must be cut from its
				# current location
				my UnlinkNodes ${name}
				set node [my Name2Node ${name}]
				${node} parent ${pnode}
			}
			lappend nlist ${node}
		}
		# insert the list of child nodes into the
		# parent's list of children
		if { [llength ${nlist}] > 0 } {
			${pnode} insert ${index} {*}${nlist}
		}
		return ${args}
	}

	# --
	#
	method isleaf { name } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		return [expr ( [llength [${node} children]] > 0 ) ? 0 : 1]
	}

	# --
	#
	method keys { {name ""} {gpat ""} } {
		if { ${name} eq "" } {
			set nlist [my nodes]
		} else {
			set nlist ${name}
		}
		set result {}
		foreach name ${nlist} {
			set node [my Name2Node ${name}]
			if { ${gpat} eq "" } {
				lappend result {*}[${node} attr.keys]
			} else {
				set d [dict create {*}[${node} attrs.filter ${gpat}]]
				lappend result {*}[dict keys ${d}]
			}
		}
		return [lsort -unique ${result}]
	}

	# --
	#
	method key.append { name key value } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		${node} attr.append ${key} ${value}
		return
	}

	# --
	#
	method key.exists { name key } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		return [${node} attr.exists ${key}]
	}

	# --
	#
	method key.get { name key } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		return [${node} attr.get ${key}]
	}

	# --
	#
	method key.getall { name {globpat ""} } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		return [${node} attrs.filter ${globpat}]
	}

	# --
	#
	method key.lappend { name key value } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		${node} attr.lappend ${key} ${value}
		return [${node} attr.get ${key}]
	}

	# --
	#
	method key.nodes { key {flag ""} {arg ""} } {
		set result {}
		set names [my nodes]
		switch -exact ${flag} {
		"-nodes" {
			set names ${arg}
		}
		"-glob" {
			set nlist {}
			set gpat ${arg}
			foreach name ${names} {
				if { [string match ${gpat} ${name}] == 1 } {
					lappend nlist ${name}
				}
			}
			set names ${nlist}
		}
		"-regexp" {
			set nlist {}
			set rpat ${arg}
			foreach name ${names} {
				if { [regexp ${rpat} ${name}] == 1 } {
					lappend nlist ${name}
				}
			}
			set names ${nlist}
		}
		default {
		}}
		foreach name ${names} {
			if { [my key.exists ${name} ${key}] } {
				lappend result ${name} [my key.get ${name} ${key}]
			}
		}
		return ${result}
	}

	# --
	#
	method key.set { name key args } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		if { [llength ${args}] == 1 } {
			${node} attr.set ${key} [lindex ${args} 0]
		}
		return [${node} attr.get ${key}]
	}


	# --
	#
	method key.unset { name key } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		${node} attr.unset ${key}
	}
	# --
	#
	method leaves { } {
		set leaves {}
		foreach name [my nodes] {
			if { [my isleaf ${name}] == 1 } {
				lappend leaves ${name}
			}
		}
		return ${leaves}
	}

	# --
	#
	method move { parent index args } {
		set pnode [my Name2Node ${parent}]
		# Make sure the list of nodes doesn't contain an
		# ancestor of the parent. If this were allowed the
		# subtree would become disconnected.
		set alist [my ancestors ${parent}]
		foreach name ${args} {
			if { [my exists ${name}] ne ""  } {
				error "node (${name}) - not found"
			}
			if { ${name} in ${alist} } {
				error "node (${name}) is an ancestor of node (${parent})"
			}
		}
		# unlink the nodes
		set nlist {}
		foreach name ${args} {
			my UnlinkNodes ${name}
			set node [my Name2Node ${name}]
			${node} parent ${pnode}
			lappend nlist ${node}
		}
		# link the nodes into the parent at location 'index'
		set children [${pnode} children]
		${pnode} children [linsert ${children} ${index} {*}${nlist}]
		return
	}

	# --
	#
	method next { name } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set cnode [my Name2Node ${name}]
		set pnode [${cnode} parent]
		set children [${pnode} children]
		set idx [lsearch -exact ${children} ${cnode}]
		incr idx
		if { ${idx} < [llength ${children}] } {
			return [my Node2Name [lindex ${children} ${idx}]]
		} else {
			return ""
		}
	}

	# --
	#
	method numchildren { name } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set node [my Name2Node ${name}]
		return [llength [${node} children]]
	}

	# --
	#
	method nodes { } {
		variable nodes
		return [dict keys ${nodes}]
	}

	# --
	#
	method parent { child } {
		variable nodes
		if { [my exists ${child}] ne ""  } {
			error "node (${child}) - not found"
		}
		set cnode [my Name2Node ${child}]
		set pnode [${cnode} parent]
		if { ${pnode} eq "" } {
			return ""
		} else {
			return [my Node2Name ${pnode}]
		}
	}

	# --
	#
	method previous { name } {
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		set cnode [my Name2Node ${name}]
		set pnode [${cnode} parent]
		set children [${pnode} children]
		set idx [lsearch -exact ${children} ${cnode}]
		incr idx -1
		if { ${idx} >= 0 } {
			return [my Node2Name [lindex ${children} ${idx}]]
		} else {
			return ""
		}
	}

	# --
	#
	method rename { from to } {
		variable root
		variable nodes
		if { ![dict exists ${nodes} ${from}] } {
			error "node (${to}) - not found"
		}
		if { [dict exists ${nodes} ${to}] } {
			error "node (${to}) - already exists"
		}
		set node [dict get ${nodes} ${from}]
		set nodes [dict remove ${nodes} ${from}]
		dict set nodes ${to} ${node}
		if { ${from} eq ${root} } {
			set root ${to}
		}
		return
	}

	# --
	#
	method rootname { } {
		variable root
		return ${root}
	}

	# --
	# Return a serialization of the subtree starting at 'name'.
	#
	# The result is a list containing three element. The elements
	# are (1) a node name (2) the node's attributes in dictionary
	# form (3) zero or more additional three element lists that
	# recursivly serialize the children of the node.
	#
	method serialize { name } {
		variable root
		variable nodes
		if { ${name} ne "root" && [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		# create the null node
		set stream {}
		set stream [my SerializeNode ${stream} ${name} 1]
		return ${stream}
	}

	# --
	#
	method size { {name ""} } {
		if { ${name} eq "" } {
			set name [my rootname]
		} else {
			if { [my exists ${name}] ne ""  } {
				error "node (${name}) - not found"
			}
		}
		return [llength [my descendants ${name}]]

	}

	# --
	#
	method splice { parent from {to ""} {child ""} } {
		variable nid
		variable nodes
		if { ${parent} eq "root" } {
			set parent [my rootname]
		} else {
			if { [my exists ${parent}] ne ""  } {
				error "node (${parent}) - not found"
			}
		}
		if { ${to} eq "" } {
			set to "end"
		}
		if { ${child} eq "" } {
			incr nid
			set child "node${nid}"
		} else {
			if { [my NotUsed ${child}] ne ""  } {
				error "node (${child}) - already exists"
			}
		}
		# get the parent information
		set pnode [my Name2Node ${parent}]
		# create the new child
		set node [::menubar::node new ${pnode}]
		# add the node to the index
		dict set nodes ${child} ${node}
		# get the parents children
		set children [${pnode} children]
		# put the range of childern in the new node
		${node} children [lrange ${children} ${from} ${to}] -force
		# remove the range of children from the parent and insert the new node
		${pnode} children [lreplace ${children} ${from} ${to} ${node}] -force
		return ${child}
	}

	# --
	#
	method swap { name1 name2 } {
		if { ${name1} eq ${name2} } { return }
		# make sure the nodes exist
		if { [my exists ${name1}] ne ""  } {
			error "node (${name1}) - not found"
		}
		if { [my exists ${name2}] ne ""  } {
			error "node (${name2}) - not found"
		}
		# make sure one node isn't in the the other node's subtree
		# (this also precludes a swap with 'root')
		set node1 [my Name2Node ${name1}]
		set node2 [my Name2Node ${name2}]
		if { [lsearch -exact [my descendants ${name1}] ${name2}] != -1 } {
			error "node (${name2}) in subtree of node (${name1})"
		}
		if { [lsearch -exact [my descendants ${name2}] ${name1}] != -1 } {
			error "node (${name1}) in subtree of node (${name2})"
		}
		# check to see if the nodes have a common parent
		set pnode1 [${node1} parent]
		set pnode2 [${node2} parent]
		if { ${pnode1} eq ${pnode2} } {
			# nodes have a common parent node
			set children [${pnode1} children]
			set idx1 [lsearch -exact ${children} ${node1}]
			set idx2 [lsearch -exact ${children} ${node2}]
			set children [lreplace ${children} ${idx1} ${idx1} ${node2}]
			set children [lreplace ${children} ${idx2} ${idx2} ${node1}]
			${pnode1} children ${children} -force
		} else {
			# nodes have different parent nodes
			set children1 [${pnode1} children]
			set children2 [${pnode2} children]
			set idx1 [lsearch -exact ${children1} ${node1}]
			set idx2 [lsearch -exact ${children2} ${node2}]
			set children1 [lreplace ${children1} ${idx1} ${idx1} ${node2}]
			set children2 [lreplace ${children2} ${idx2} ${idx2} ${node1}]
			${pnode1} children ${children1} -force
			${pnode2} children ${children2} -force
			${node1} parent ${pnode2}
			${node2} parent ${pnode1}
		}
		return
	}

	##### WALKPROC CODE (DEPTH FIRST) ############################

	# --
	#
	method DfsPreOrderWalk { name cmdprefix } {
		variable nodes
		if { [catch {${cmdprefix} [self object] ${name} "enter"} bool] || ${bool} != 0 } {
			#puts "bool: $bool"
			# shutdown the walk
			return 1
		}
		set node [my Name2Node ${name}]
		for {set idx 0} { true } {incr idx} {
			set children [my children ${name}]
			if { ${idx} >= [llength ${children}] } {
				break
			}
			set child [lindex [my children ${name}] ${idx}]
			if { [my PreOrderWalk ${child} ${cmdprefix}] != 0 } {
				return 1
			}
		}
		return 0
	}

	# --
	#
	method DfsPostOrderWalk { name cmdprefix } {
		variable nodes
		variable nodes
		set node [my Name2Node ${name}]
		for {set idx 0} { true } {incr idx} {
			set children [my children ${name}]
			if { ${idx} >= [llength ${children}] } {
				break
			}
			set child [lindex [my children ${name}] ${idx}]
			if { [my PostOrderWalk ${child} ${cmdprefix}] != 0 } {
				return 1
			}
		}
		if { [catch {${cmdprefix} [self object] ${name} "leave"} bool] || ${bool} != 0 } {
			#puts "bool: $bool"
			# shutdown the walk
			return 1
		}
		return 0
	}

	# --
	#
	method DfsBothOrderWalk { name cmdprefix } {
		variable nodes
		if { [catch {${cmdprefix} [self object] ${name} "enter"} bool] || ${bool} != 0 } {
			#puts "bool: $bool"
			# shutdown the walk
			return 1
		}
		set node [my Name2Node ${name}]
		for {set idx 0} { true } {incr idx} {
			set children [my children ${name}]
			if { ${idx} >= [llength ${children}] } {
				break
			}
			set child [lindex [my children ${name}] ${idx}]
			if { [my BothOrderWalk ${child} ${cmdprefix}] != 0 } {
				return 1
			}
		}
		if { [catch {${cmdprefix} [self object] ${name} "leave"} bool] || ${bool} != 0 } {
			#puts "bool: $bool"
			# shutdown the walk
			return 1
		}
		return 0
	}

	# --
	#
	method DfsInOrderWalk { name cmdprefix } {
		variable nodes
		set node [my Name2Node ${name}]
		for {set idx 0} { true } {incr idx} {
			if { ${idx} == 1 } {
				if { [catch {${cmdprefix} [self object] ${name} "visit"} bool] || ${bool} != 0 } {
					#puts "bool: $bool"
					# shutdown the walk
					return 1
				}
			}
			set children [my children ${name}]
			if { ${idx} >= [llength ${children}] } {
				break
			}
			set child [lindex [my children ${name}] ${idx}]
			if { [my InOrderWalk ${child} ${cmdprefix}] != 0 } {
				return 1
			}
		}
		if { ${idx} == 0 } {
			if { [catch {${cmdprefix} [self object] ${name} "visit"} bool] || ${bool} != 0 } {
				#puts "bool: $bool"
				# shutdown the walk
				return 1
			}
		}
		return 0
	}

	##### WALKPROC CODE (BREADTH FIRST) ############################

	# --
	# This method takes as input a list of nodes (nlist) and returns
	# a new list that is the list of all children for the input list.
	method DecendOneLevelForward { nlist } {
		set result {}
		foreach node ${nlist} {
			lappend result {*}[${node} children]
		}
		return ${result}
	}
	# --
	# This method takes as input a list of nodes (nlist) and returns
	# a new list that is the list of all children for the input list.
	method DecendOneLevelBackward { nlist } {
		set result {}
		foreach node ${nlist} {
			lappend result {*}[lreverse [${node} children]]
		}
		return ${result}
	}


	# --
	#
	method BfsPreOrderWalk { nlist cmdprefix } {
		if { [llength ${nlist}] == 0 } { return 0 }
		foreach node ${nlist} {
			if { [catch {${cmdprefix} [self object] [my Node2Name ${node}] "enter"} bool] || ${bool} != 0 } {
				#puts "bool: $bool"
				# shutdown the walk
				return 1
			}
		}
		if { [my BfsPreOrderWalk [my DecendOneLevelForward ${nlist}] ${cmdprefix}] != 0 } {
			return 1
		}
		return 0
	}

	# --
	#
	method BfsPostOrderWalk { nlist cmdprefix } {
		if { [llength ${nlist}] == 0 } { return 0 }
		if { [my BfsPostOrderWalk [my DecendOneLevelBackward ${nlist}] ${cmdprefix}] != 0 } {
			return 1
		}
		foreach node ${nlist} {
			if { [catch {${cmdprefix} [self object] [my Node2Name ${node}] "leave"} bool] || ${bool} != 0 } {
				#puts "bool: $bool"
				# shutdown the walk
				return 1
			}
		}
		return 0
	}

	# --
	#
	method BfsBothOrderWalk { nlist cmdprefix } {
		if { [llength ${nlist}] == 0 } { return 0 }
		foreach node ${nlist} {
			if { [catch {${cmdprefix} [self object] [my Node2Name ${node}] "enter"} bool] || ${bool} != 0 } {
				#puts "bool: $bool"
				# shutdown the walk
				return 1
			}
		}
		my BfsBothOrderWalk [my DecendOneLevelForward ${nlist}] ${cmdprefix}
		foreach node [lreverse ${nlist}] {
			if { [catch {${cmdprefix} [self object] [my Node2Name ${node}] "leave"} bool] || ${bool} != 0 } {
				#puts "bool: $bool"
				# shutdown the walk
				return 1
			}
		}
		return 0
	}

	# --
	#
	method BfsInOrderWalk { } {
		error "unable to do a in-order breadth first walk"
	}


	# --
	#
	method walkproc { name cmdprefix args } {
		set types {bfs dfs}
		set orders {pre post both in}
		set type "dfs"
		set order "pre"
		if { [my exists ${name}] ne ""  } {
			error "node (${name}) - not found"
		}
		foreach {opt val} ${args} {
			switch -exact -- ${opt} {
			"-order" {
				if { ${val} ni ${orders} } {
 				   error "-order ${val} - must be oneof: [join ${orders} {, }]"
				}
				set order ${val}
			}
			"-type" {
				if { ${val} ni ${types} } {
 				   error "-type ${val} - must be oneof: [join ${types} {, }]"
				}
				set type ${val}
			}
			default {
			}}
		}

		if { ${type} eq "dfs"  } {
			switch -exact -- ${order}  {
			"post" {
				my DfsPostOrderWalk ${name} ${cmdprefix}
			}
			"both" {
				my DfsBothOrderWalk ${name} ${cmdprefix}
			}
			"in" {
				my DfsInOrderWalk ${name} ${cmdprefix}
			}
			"pre" -
			default {
				my DfsPreOrderWalk ${name} ${cmdprefix}
			}}
		} else  {
			switch -exact -- ${order}  {
			"post" {
				my BfsPostOrderWalk [my Name2Node ${name}] ${cmdprefix}
			}
			"both" {
				my BfsBothOrderWalk [my Name2Node ${name}] ${cmdprefix}
			}
			"in" {
				my BfsInOrderWalk
			}
			"pre" -
			default {
				my BfsPreOrderWalk [my Name2Node ${name}] ${cmdprefix}
			}}
		}
		return
	}
}
