# graph.tcl --
#
#	Implementation of a graph data structure for Tcl.
#
# Copyright (c) 2000 by Andreas Kupries
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: graph.tcl,v 1.13 2004/02/09 09:32:13 andreas_kupries Exp $

# Create the namespace before determining cgraph vs. tcl
# Otherwise the loading 'struct.tcl' may get into trouble
# when trying to import commands from them

namespace eval ::struct {}
namespace eval ::struct::graph {}

# Try to load the cgraph package
# Get it at http://physnet.uni-oldenburg.de/~schlenk/tcl/graph/ 
#
# ** NOTE ** ATTENTION **
#
# For the 2.0 version of the graph interface 'cgraph 0.6' is _not_
# useable anymore.
#
# '[package vcompare $version 0.6] > 0' <=> '$version > 0.6'

if {
    ![catch {package require cgraph} version] &&
    [package vcompare $version 0.6] > 0
} {
    # the cgraph package takes over, so we can return
    return
}

namespace eval ::struct {}
namespace eval ::struct::graph {
    # Data storage in the graph module
    # -------------------------------
    #
    # There's a lot of bits to keep track of for each graph:
    #	nodes
    #	node values
    #	node relationships (arcs)
    #   arc values
    #
    # It would quickly become unwieldy to try to keep these in arrays or lists
    # within the graph namespace itself.  Instead, each graph structure will
    # get its own namespace.  Each namespace contains:
    #	node:$node	array mapping keys to values for the node $node
    #	arc:$arc	array mapping keys to values for the arc $arc
    #	inArcs		array mapping nodes to the list of incoming arcs
    #	outArcs		array mapping nodes to the list of outgoing arcs
    #	arcNodes	array mapping arcs to the two nodes (start & end)
    
    # counter is used to give a unique name for unnamed graph
    variable counter 0


    # Only export one command, the one used to instantiate a new graph
    namespace export graph
}

# ::struct::graph::graph --
#
#	Create a new graph with a given name; if no name is given, use
#	graphX, where X is a number.
#
# Arguments:
#	name	name of the graph; if null, generate one.
#
# Results:
#	name	name of the graph created

proc ::struct::graph::graph {args} {
    variable counter
    
    set src     {}
    set srctype {}

    switch -exact -- [llength [info level 0]] {
	1 {
	    # Missing name, generate one.
	    incr counter
	    set name "graph${counter}"
	}
	2 {
	    # Standard call. New empty graph.
	    set name [lindex $args 0]
	}
	4 {
	    # Copy construction.
	    foreach {name as src} $args break
	    switch -exact -- $as {
		= - := - as {
		    set srctype graph
		}
		deserialize {
		    set srctype serial
		}
		default {
		    return -code error \
			    "wrong # args: should be \"graph ?name ?=|:=|as|deserialize source??\""
		}
	    }
	}
	default {
	    # Error.
	    return -code error \
		    "wrong # args: should be \"graph ?name ?=|:=|as|deserialize source??\""
	}
    }

    # FIRST, qualify the name.
    if {![string match "::*" $name]} {
        # Get caller's namespace; append :: if not global namespace.
        set ns [uplevel 1 namespace current]
        if {"::" != $ns} {
            append ns "::"
        }

        set name "$ns$name"
    }
    if {[llength [info commands $name]]} {
	return -code error "command \"$name\" already exists, unable to create graph"
    }

    # Set up the namespace
    namespace eval $name {

	# Set up the map for values associated with the graph itself
	variable  graphAttr
	array set graphAttr {}

	# Set up the node attribute mapping
	variable  nodeAttr
	array set nodeAttr {}

	# Set up the arc attribute mapping
	variable  arcAttr
	array set arcAttr {}

	# Set up the map from nodes to the arcs coming to them
	variable  inArcs
	array set inArcs {}

	# Set up the map from nodes to the arcs going out from them
	variable  outArcs
	array set outArcs {}

	# Set up the map from arcs to the nodes they touch.
	variable  arcNodes
	array set arcNodes {}

	# Set up a value for use in creating unique node names
	variable nextUnusedNode
	set      nextUnusedNode 1

	# Set up a value for use in creating unique arc names
	variable nextUnusedArc
	set      nextUnusedArc 1

	# Set up a counter for use in creating attribute arrays.
	variable nextAttr
	set      nextAttr 0
    }

    # Create the command to manipulate the graph
    interp alias {} $name {} ::struct::graph::GraphProc $name

    # Automatic execution of assignment if a source
    # is present.
    if {$src != {}} {
	switch -exact -- $srctype {
	    graph  {_= $name $src}
	    serial {_deserialize $name $src}
	    default {
		return -code error \
			"Internal error, illegal srctype \"$srctype\""
	    }
	}
    }

    return $name
}

##########################
# Private functions follow

# ::struct::graph::GraphProc --
#
#	Command that processes all graph object commands.
#
# Arguments:
#	name	name of the graph object to manipulate.
#	args	command name and args for the command
#
# Results:
#	Varies based on command to perform

proc ::struct::graph::GraphProc {name {cmd ""} args} {
    # Do minimal args checks here
    if { [llength [info level 0]] == 2 } {
	return -code error "wrong # args: should be \"$name option ?arg arg ...?\""
    }
    
    # Split the args into command and args components
    set sub _$cmd
    if { [llength [info commands ::struct::graph::$sub]] == 0 } {
	set optlist [lsort [info commands ::struct::graph::_*]]
	set xlist {}
	foreach p $optlist {
	    set p [namespace tail $p]
	    if {[string match __* $p]} {continue}
	    lappend xlist [string range $p 1 end]
	}
	set optlist [linsert [join $xlist ", "] "end-1" "or"]
	return -code error \
		"bad option \"$cmd\": must be $optlist"
    }
    uplevel 1 [linsert $args 0 ::struct::graph::$sub $name]
}

# ::struct::graph::_= --
#
#	Assignment operator. Copies the source graph into the
#       destination, destroying the original information.
#
# Arguments:
#	name	Name of the graph object we are copying into.
#	source	Name of the graph object providing us with the
#		data to copy.
#
# Results:
#	Nothing.

proc ::struct::graph::_= {name source} {
    _deserialize $name [$source serialize]
    return
}

# ::struct::graph::_--> --
#
#	Reverse assignment operator. Copies this graph into the
#       destination, destroying the original information.
#
# Arguments:
#	name	Name of the graph object to copy
#	dest	Name of the graph object we are copying to.
#
# Results:
#	Nothing.

proc ::struct::graph::_--> {name dest} {
    $dest deserialize [_serialize $name]
    return
}

# ::struct::graph::_append --
#
#	Append a value for an attribute in a graph.
#
# Arguments:
#	name	name of the graph.
#	args	key value
#
# Results:
#	val	value associated with the given key of the given arc

proc ::struct::graph::_append {name key value} {
    variable ${name}::graphAttr
    return [append    graphAttr($key) $value]
}

# ::struct::graph::_lappend --
#
#	lappend a value for an attribute in a graph.
#
# Arguments:
#	name	name of the graph.
#	args	key value
#
# Results:
#	val	value associated with the given key of the given arc

proc ::struct::graph::_lappend {name key value} {
    variable ${name}::graphAttr
    return [lappend   graphAttr($key) $value]
}

# ::struct::graph::_arc --
#
#	Dispatches the invocation of arc methods to the proper handler
#	procedure.
#
# Arguments:
#	name	name of the graph.
#	cmd	arc command to invoke
#	args	arguments to propagate to the handler for the arc command
#
# Results:
#	As of the invoked handler.

proc ::struct::graph::_arc {name cmd args} {
    # Split the args into command and args components

    set sub __arc_$cmd
    if { [llength [info commands ::struct::graph::$sub]] == 0 } {
	set optlist [lsort [info commands ::struct::graph::__arc_*]]
	set xlist {}
	foreach p $optlist {
	    set p [namespace tail $p]
	    lappend xlist [string range $p 6 end]
	}
	set optlist [linsert [join $xlist ", "] "end-1" "or"]
	return -code error \
		"bad option \"$cmd\": must be $optlist"
    }
    uplevel 1 [linsert $args 0 ::struct::graph::$sub $name]
}

# ::struct::graph::__arc_delete --
#
#	Remove an arc from a graph, including all of its values.
#
# Arguments:
#	name	name of the graph.
#	args	list of arcs to delete.
#
# Results:
#	None.

proc ::struct::graph::__arc_delete {name args} {

    foreach arc $args {
	if { ![__arc_exists $name $arc] } {
	    return -code error "arc \"$arc\" does not exist in graph \"$name\""
	}
    }

    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes
    variable ${name}::arcAttr

    foreach arc $args {
	foreach {source target} $arcNodes($arc) break ; # lassign

	unset arcNodes($arc)

	if {[info exists arcAttr($arc)]} {
	    unset ${name}::$arcAttr($arc)
	    unset arcAttr($arc)
	}

	# Remove arc from the arc lists of source and target nodes.

	set index [lsearch -exact $outArcs($source) $arc]
	ldelete outArcs($source) $index

	set index [lsearch -exact $inArcs($target)  $arc]
	ldelete inArcs($target) $index
    }

    return
}

# ::struct::graph::__arc_exists --
#
#	Test for existance of a given arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to look for.
#
# Results:
#	1 if the arc exists, 0 else.

proc ::struct::graph::__arc_exists {name arc} {
    return [info exists ${name}::arcNodes($arc)]
}

# ::struct::graph::__arc_get --
#
#	Get a keyed value from an arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to query.
#	key	key to lookup
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::__arc_get {name arc key} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attribute data for this arc, key has to be invalid.
	return -code error "invalid key \"$key\" for arc \"$arc\""
    }

    upvar ${name}::$arcAttr($arc) data
    if { ![info exists data($key)] } {
	return -code error "invalid key \"$key\" for arc \"$arc\""
    }
    return $data($key)
}

# ::struct::graph::__arc_getall --
#
#	Get a serialized array of key/value pairs from an arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to query.
#	pattern	optional glob pattern to restrict retrieval
#
# Results:
#	value	serialized array of key/value pairs.

proc ::struct::graph::__arc_getall {name arc {pattern *}} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attributes ...
	return {}
    }

    upvar ${name}::$arcAttr($arc) data
    return [array get data $pattern]
}

# ::struct::graph::__arc_keys --
#
#	Get a list of keys for an arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to query.
#	pattern	optional glob pattern to restrict retrieval
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::__arc_keys {name arc {pattern *}} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attributes ...
	return {}
    }

    upvar ${name}::$arcAttr($arc) data
    return [array names data $pattern]
}

# ::struct::graph::__arc_keyexists --
#
#	Test for existance of a given key for a given arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to query.
#	key	key to lookup
#
# Results:
#	1 if the key exists, 0 else.

proc ::struct::graph::__arc_keyexists {name arc key} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attribute data for this arc, key cannot exist.
	return 0
    }

    upvar ${name}::$arcAttr($arc) data
    return [info exists data($key)]
}

# ::struct::graph::__arc_insert --
#
#	Add an arc to a graph.
#
# Arguments:
#	name		name of the graph.
#	source		source node of the new arc
#	target		target node of the new arc
#	args		arc to insert; must be unique.  If none is given,
#			the routine will generate a unique node name.
#
# Results:
#	arc		The name of the new arc.

proc ::struct::graph::__arc_insert {name source target args} {

    if { [llength $args] == 0 } {
	# No arc name was given; generate a unique one
	set arc [__generateUniqueArcName $name]
    } else {
	set arc [lindex $args 0]
    }

    if { [__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" already exists in graph \"$name\""
    }
    
    if { ![__node_exists $name $source] } {
	return -code error "source node \"$source\" does not exist in graph \"$name\""
    }
    
    if { ![__node_exists $name $target] } {
	return -code error "target node \"$target\" does not exist in graph \"$name\""
    }
    
    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes

    # Set up the new arc
    set arcNodes($arc) [list $source $target]

    # Add this arc to the arc lists of its source resp. target nodes.
    lappend outArcs($source) $arc
    lappend inArcs($target)  $arc

    return $arc
}

# ::struct::graph::__arc_rename --
#
#	Rename a arc in place.
#
# Arguments:
#	name	name of the graph.
#	arc	Name of the arc to rename
#	newname	The new name of the arc.
#
# Results:
#	The new name of the arc.

proc ::struct::graph::__arc_rename {name arc newname} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }
    if {[__arc_exists $name $newname]} {
	return -code error "unable to rename arc to \"$newname\",\
		arc of that name already present in the graph \"$name\""
    }

    set oldname  $arc

    # Perform the rename in the internal
    # data structures.

    # - graphAttr - not required, arc independent.
    # - nodeAttr  - not required, arc independent.
    # - counters  - not required

    variable ${name}::arcAttr
    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes

    # Arc relocation

    set arcNodes($newname) [set nodes $arcNodes($oldname)]
    unset                              arcNodes($oldname)

    # Update the two nodes ...
    foreach {start end} $nodes break

    set pos [lsearch -exact $inArcs($end) $oldname]
    lset inArcs($end) $pos $newname

    set pos [lsearch -exact $outArcs($start) $oldname]
    lset outArcs($start) $pos $newname

    if {[info exists arcAttr($oldname)]} {
	set arcAttr($newname) $arcAttr($oldname)
	unset                  arcAttr($oldname)
    }

    return $newname
}

# ::struct::graph::__arc_set --
#
#	Set or get a value for an arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to modify or query.
#	key	attribute to modify or query
#	args	?value?
#
# Results:
#	val	value associated with the given key of the given arc

proc ::struct::graph::__arc_set {name arc key args} {
    if { [llength $args] > 1 } {
	return -code error "wrong # args: should be \"$name arc set $arc key ?value?\""
    }
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    if { [llength $args] > 0 } {
	# Setting the value. This may have to create
	# the attribute array for this particular
	# node

	variable ${name}::arcAttr
	if {![info exists arcAttr($arc)]} {
	    # No attribute data for this node,
	    # so create it as we need it now.
	    GenAttributeStorage $name arc $arc
	}

	upvar ${name}::$arcAttr($arc) data
	return [set data($key) [lindex $args end]]
    } else {
	# Getting a value
	return [__arc_get $name $arc $key]
    }
}

# ::struct::graph::__arc_append --
#
#	Append a value for an arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to modify or query.
#	args	key value
#
# Results:
#	val	value associated with the given key of the given arc

proc ::struct::graph::__arc_append {name arc key value} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attribute data for this arc,
	# so create it as we need it.
	GenAttributeStorage $name arc $arc
    }

    upvar ${name}::$arcAttr($arc) data
    return [append data($key) $value]
}

# ::struct::graph::__arc_attr --
#
#	Return attribute data for one key and multiple arcs, possibly all.
#
# Arguments:
#	name	Name of the graph object.
#	key	Name of the attribute to retrieve.
#
# Results:
#	children	Dictionary mapping arcs to attribute data.

proc ::struct::graph::__arc_attr {name key args} {
    # Syntax:
    #
    # t attr key
    # t attr key -arcs {arclist}
    # t attr key -glob arcpattern
    # t attr key -regexp arcpattern

    variable ${name}::arcAttr

    set usage "wrong # args: should be \"[list $name] arc attr key ?-arcs list|-glob pattern|-regexp pattern?\""
    if {([llength $args] != 0) && ([llength $args] != 2)} {
	return -code error $usage
    } elseif {[llength $args] == 0} {
	# This automatically restricts the list
	# to arcs which can have the attribute
	# in question.

	set arcs [array names arcAttr]
    } else {
	# Determine a list of arcs to look at
	# based on the chosen restriction.

	foreach {mode value} $args break
	switch -exact -- $mode {
	    -arcs {
		# This is the only branch where we have to
		# perform an explicit restriction to the
		# arcs which have attributes.
		set arcs {}
		foreach n $value {
		    if {![info exists arcAttr($n)]} continue
		    lappend arcs $n
		}
	    }
	    -glob {
		set arcs [array names arcAttr $value]
	    }
	    -regexp {
		set arcs {}
		foreach n [array names arcAttr] {
		    if {![regexp -- $value $n]} continue
		    lappend arcs $n
		}
	    }
	    default {
		return -code error $usage
	    }
	}
    }

    # Without possibly matching arcs
    # the result has to be empty.

    if {![llength $arcs]} {
	return {}
    }

    # Now locate matching keys and their values.

    set result {}
    foreach n $arcs {
	upvar ${name}::$arcAttr($n) data
	if {[info exists data($key)]} {
	    lappend result $n $data($key)
	}
    }

    return $result
}

# ::struct::graph::__arc_lappend --
#
#	lappend a value for an arc in a graph.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to modify or query.
#	args	key value
#
# Results:
#	val	value associated with the given key of the given arc

proc ::struct::graph::__arc_lappend {name arc key value} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attribute data for this arc,
	# so create it as we need it.
	GenAttributeStorage $name arc $arc
    }

    upvar ${name}::$arcAttr($arc) data
    return [lappend data($key) $value]
}

# ::struct::graph::__arc_source --
#
#	Return the node at the beginning of the specified arc.
#
# Arguments:
#	name	name of the graph object.
#	arc	arc to look up.
#
# Results:
#	node	name of the node.

proc ::struct::graph::__arc_source {name arc} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcNodes
    return [lindex $arcNodes($arc) 0]
}

# ::struct::graph::__arc_target --
#
#	Return the node at the end of the specified arc.
#
# Arguments:
#	name	name of the graph object.
#	arc	arc to look up.
#
# Results:
#	node	name of the node.

proc ::struct::graph::__arc_target {name arc} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcNodes
    return [lindex $arcNodes($arc) 1]
}

# ::struct::graph::__arc_unset --
#
#	Remove a keyed value from a arc.
#
# Arguments:
#	name	name of the graph.
#	arc	arc to modify.
#	key	attribute to remove
#
# Results:
#	None.

proc ::struct::graph::__arc_unset {name arc key} {
    if { ![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcAttr
    if {![info exists arcAttr($arc)]} {
	# No attribute data for this arc,
	# nothing to do.
	return
    }

    upvar ${name}::$arcAttr($arc) data
    catch {unset data($key)}

    if {[array size data] == 0} {
	# No attributes stored for this arc, squash the whole array.
	set arcAttr($arc) {}
	unset data
    }
    return
}

# ::struct::graph::_arcs --
#
#	Return a list of all arcs in a graph satisfying some
#	node based restriction.
#
# Arguments:
#	name	name of the graph.
#
# Results:
#	arcs	list of arcs

proc ::struct::graph::_arcs {name args} {

    # Discriminate between conditions and nodes

    set haveCond 0
    set haveKey 0
    set haveValue 0
    set haveFilter 0
    set cond "none"
    set condNodes [list]

    for {set i 0} {$i < [llength $args]} {incr i} {
	set arg [lindex $args $i]
	switch -glob -- $arg {
	    -in -
	    -out -
	    -adj -
	    -inner -
	    -embedding {
		set haveCond 1
		set cond [string range $arg 1 end]
	    }
	    -key {
		incr i
		set key [lindex $args $i]
		set haveKey 1
	    }
	    -value {
		incr i
		set value [lindex $args $i]
		set haveValue 1
	    }
	    -filter {
		incr i
		set fcmd [lindex $args $i]
		set haveFilter 1
	    }
	    -* {
		return -code error "invalid restriction \"$arg\": should be -in, -out,\
			-adj, -inner, -embedding, -key, -value, or -filter"
	    }
	    default {
		lappend condNodes $arg
	    }
	}
    }

    # Validate that there are nodes to use in the restriction.
    # otherwise what's the point?
    if {$haveCond} {
	if {[llength $condNodes] == 0} {
	    set usage "$name arcs ?-key key? ?-value value? ?-filter cmd? ?-in|-out|-adj|-inner|-embedding node node...?"
	    return -code error "no nodes specified: should be \"$usage\""
	}

	# Make sure that the specified nodes exist!
	foreach node $condNodes {
	    if { ![__node_exists $name $node] } {
		return -code error "node \"$node\" does not exist in graph \"$name\""
	    }
	}
    }

    # Now we are able to go to work
    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes

    set       arcs [list]

    switch -exact -- $cond {
	in {
	    # Result is all arcs going to at least one node
	    # in the list of arguments.

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    # As an arc has only one destination, i.e. is the
		    # in-arc of exactly one node it is impossible to
		    # count an arc twice. IOW the [info exists] below
		    # is never true. Found through coverage analysis
		    # and then trying to think up a testcase invoking
		    # the continue.
		    # if {[info exists coll($e)]} {continue}
		    lappend arcs    $e
		    #set     coll($e) .
		}
	    }
	}
	out {
	    # Result is all arcs coming from at least one node
	    # in the list of arguments.

	    foreach node $condNodes {
		foreach e $outArcs($node) {
		    # See above 'in', same reasoning, one source per arc.
		    # if {[info exists coll($e)]} {continue}
		    lappend arcs    $e
		    #set     coll($e) .
		}
	    }
	}
	adj {
	    # Result is all arcs coming from or going to at
	    # least one node in the list of arguments.

	    array set coll  {}
	    # Here we do need 'coll' as each might be an in- and
	    # out-arc for one or two nodes in the list of arguments.

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    if {[info exists coll($e)]} {continue}
		    lappend arcs    $e
		    set     coll($e) .
		}
		foreach e $outArcs($node) {
		    if {[info exists coll($e)]} {continue}
		    lappend arcs    $e
		    set     coll($e) .
		}
	    }
	}
	inner {
	    # Result is all arcs running between nodes in the list.

	    array set coll  {}
	    # Here we do need 'coll' as each might be an in- and
	    # out-arc for one or two nodes in the list of arguments.

	    array set group {}
	    foreach node $condNodes {
		set group($node) .
	    }

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    set n [lindex $arcNodes($e) 0]
		    if {![info exists group($n)]} {continue}
		    if { [info exists coll($e)]}  {continue}
		    lappend arcs    $e
		    set     coll($e) .
		}
		foreach e $outArcs($node) {
		    set n [lindex $arcNodes($e) 1]
		    if {![info exists group($n)]} {continue}
		    if { [info exists coll($e)]}  {continue}
		    lappend arcs    $e
		    set     coll($e) .
		}
	    }
	}
	embedding {
	    # Result is all arcs from -adj minus the arcs from -inner.
	    # IOW all arcs going from a node in the list to a node
	    # which is *not* in the list

	    # This also means that no arc can be counted twice as it
	    # is either going to a node, or coming from a node in the
	    # list, but it can't do both, because then it is part of
	    # -inner, which was excluded!

	    array set group {}
	    foreach node $condNodes {
		set group($node) .
	    }

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    set n [lindex $arcNodes($e) 0]
		    if {[info exists group($n)]} {continue}
		    # if {[info exists coll($e)]}  {continue}
		    lappend arcs    $e
		    # set     coll($e) .
		}
		foreach e $outArcs($node) {
		    set n [lindex $arcNodes($e) 1]
		    if {[info exists group($n)]} {continue}
		    # if {[info exists coll($e)]}  {continue}
		    lappend arcs    $e
		    # set     coll($e) .
		}
	    }
	}
	none {
	    set arcs [array names arcNodes]
	}
	default {return -code error "Can't happen, panic"}
    }

    #
    # We have a list of arcs that match the relation to the nodes.
    # Now filter according to -key and -value.
    #


    if {$haveKey} {
	set filteredArcs [list]
	foreach arc $arcs {
	    catch {
		set aval [__arc_get $name $arc $key]
		if {$haveValue} {
		    if {$aval == $value} {
			lappend filteredArcs $arc
		    }
		} else {
		    lappend filteredArcs $arc
		}
	    }
	}
	set arcs $filteredArcs
    }

    #
    # Apply the general filter command, if specified.
    #

    if {$haveFilter} {
	lappend fcmd $name
	set arcs [uplevel 1 [list ::struct::list filter $arcs $fcmd]]
    }

    return $arcs
}


# ::struct::graph::_deserialize --
#
#	Assignment operator. Copies a serialization into the
#       destination, destroying the original information.
#
# Arguments:
#	name	Name of the graph object we are copying into.
#	serial	Serialized graph to copy from.
#
# Results:
#	Nothing.

proc ::struct::graph::_deserialize {name serial} {
    # As we destroy the original graph as part of
    # the copying process we don't have to deal
    # with issues like node names from the new graph
    # interfering with the old ...

    # I. Get the serialization of the source graph
    #    and check it for validity.

    CheckSerialization $serial \
	    gattr nattr aattr ina outa arcn

    # Get all the relevant data into the scope

    variable ${name}::graphAttr
    variable ${name}::nodeAttr
    variable ${name}::arcAttr
    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes
    variable ${name}::nextAttr

    # Kill the existing information and insert the new
    # data in their place.

    foreach n [array names inArcs] {
	unset inArcs($n) outArcs($n)
    }
    array set inArcs   [array get ina]
    array set outArcs  [array get outa]
    unset ina outa

    foreach a [array names arcNodes] {
	unset arcNodes($a)
    }
    array set arcNodes [array get arcn]
    unset arcn

    set nextAttr 0
    foreach a [array names nodeAttr] {
	unset ${name}::$nodeAttr($a)
    }
    foreach a [array names arcAttr] {
	unset ${name}::$arcAttr($a)
    }
    foreach n [array names nattr] {
	GenAttributeStorage $name node $n
	array set ${name}::$nodeAttr($n) $nattr($n)
    }
    foreach a [array names aattr] {
	GenAttributeStorage $name arc $a
	array set ${name}::$arcAttr($a) $aattr($a)
    }
    foreach k [array names graphAttr] {
	unset graphAttr($k)
    }
    array set graphAttr $gattr

    ## Debug ## Dump internals ...
    if {0} {
	puts "___________________________________ $name"
	parray inArcs
	parray outArcs
	parray arcNodes
	parray nodeAttr
	parray arcAttr
	parray graphAttr
	puts ___________________________________
    }
    return
}

# ::struct::graph::_destroy --
#
#	Destroy a graph, including its associated command and data storage.
#
# Arguments:
#	name	name of the graph.
#
# Results:
#	None.

proc ::struct::graph::_destroy {name} {
    namespace delete $name
    interp alias {} $name {}
}

# ::struct::graph::__generateUniqueArcName --
#
#	Generate a unique arc name for the given graph.
#
# Arguments:
#	name	name of the graph.
#
# Results:
#	arc	name of a arc guaranteed to not exist in the graph.

proc ::struct::graph::__generateUniqueArcName {name} {
    variable ${name}::nextUnusedArc
    while {[__arc_exists $name "arc${nextUnusedArc}"]} {
	incr nextUnusedArc
    }
    return "arc${nextUnusedArc}"
}

# ::struct::graph::__generateUniqueNodeName --
#
#	Generate a unique node name for the given graph.
#
# Arguments:
#	name	name of the graph.
#
# Results:
#	node	name of a node guaranteed to not exist in the graph.

proc ::struct::graph::__generateUniqueNodeName {name} {
    variable ${name}::nextUnusedNode
    while {[__node_exists $name "node${nextUnusedNode}"]} {
	incr nextUnusedNode
    }
    return "node${nextUnusedNode}"
}

# ::struct::graph::_get --
#
#	Get a keyed value from the graph itself
#
# Arguments:
#	name	name of the graph.
#	key	key to lookup
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::_get {name key} {
    variable  ${name}::graphAttr
    if { ![info exists graphAttr($key)] } {
	return -code error "invalid key \"$key\" for graph \"$name\""
    }
    return $graphAttr($key)
}

# ::struct::graph::_getall --
#
#	Get an attribute dictionary from a graph.
#
# Arguments:
#	name	name of the graph.
#	pattern	optional, glob pattern
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::_getall {name {pattern *}} { 
    variable ${name}::graphAttr
    return [array get graphAttr $pattern]
}

# ::struct::graph::_keys --
#
#	Get a list of keys from a graph.
#
# Arguments:
#	name	name of the graph.
#	pattern	optional, glob pattern
#
# Results:
#	value	list of known keys

proc ::struct::graph::_keys {name {pattern *}} { 
    variable   ${name}::graphAttr
    return [array names graphAttr $pattern]
}

# ::struct::graph::_keyexists --
#
#	Test for existance of a given key in a graph.
#
# Arguments:
#	name	name of the graph.
#	key	key to lookup
#
# Results:
#	1 if the key exists, 0 else.

proc ::struct::graph::_keyexists {name key} {
    variable   ${name}::graphAttr
    return [info exists graphAttr($key)]
}

# ::struct::graph::_node --
#
#	Dispatches the invocation of node methods to the proper handler
#	procedure.
#
# Arguments:
#	name	name of the graph.
#	cmd	node command to invoke
#	args	arguments to propagate to the handler for the node command
#
# Results:
#	As of the the invoked handler.

proc ::struct::graph::_node {name cmd args} {
    # Split the args into command and args components
    set sub __node_$cmd
    if { [llength [info commands ::struct::graph::$sub]] == 0 } {
	set optlist [lsort [info commands ::struct::graph::__node_*]]
	set xlist {}
	foreach p $optlist {
	    set p [namespace tail $p]
	    lappend xlist [string range $p 7 end]
	}
	set optlist [linsert [join $xlist ", "] "end-1" "or"]
	return -code error \
		"bad option \"$cmd\": must be $optlist"
    }
    uplevel 1 [linsert $args 0 ::struct::graph::$sub $name]
}

# ::struct::graph::__node_degree --
#
#	Return the number of arcs adjacent to the specified node.
#	If one of the restrictions -in or -out is given only
#	incoming resp. outgoing arcs are counted.
#
# Arguments:
#	name	name of the graph.
#	args	option, followed by the node.
#
# Results:
#	None.

proc ::struct::graph::__node_degree {name args} {

    if {([llength $args] < 1) || ([llength $args] > 2)} {
	return -code error "wrong # args: should be \"$name node degree ?-in|-out? node\""
    }

    switch -exact -- [llength $args] {
	1 {
	    set opt {}
	    set node [lindex $args 0]
	}
	2 {
	    set opt  [lindex $args 0]
	    set node [lindex $args 1]
	}
	default {return -code error "Can't happen, panic"}
    }

    # Validate the option.

    switch -exact -- $opt {
	{}   -
	-in  -
	-out {}
	default {
	    return -code error "invalid option \"$opt\": should be -in or -out"
	}
    }

    # Validate the node

    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    variable ${name}::inArcs
    variable ${name}::outArcs

    switch -exact -- $opt {
	-in  {
	    set result [llength $inArcs($node)]
	}
	-out {
	    set result [llength $outArcs($node)]
	}
	{} {
	    set result [expr {[llength $inArcs($node)] \
		    + [llength $outArcs($node)]}]

	    # loops count twice, don't do <set> arithmetics, i.e. no union!
	    if {0} {
		array set coll  {}
		set result [llength $inArcs($node)]

		foreach e $inArcs($node) {
		    set coll($e) .
		}
		foreach e $outArcs($node) {
		    if {[info exists coll($e)]} {continue}
		    incr result
		    set     coll($e) .
		}
	    }
	}
	default {return -code error "Can't happen, panic"}
    }

    return $result
}

# ::struct::graph::__node_delete --
#
#	Remove a node from a graph, including all of its values.
#	Additionally removes the arcs connected to this node.
#
# Arguments:
#	name	name of the graph.
#	args	list of the nodes to delete.
#
# Results:
#	None.

proc ::struct::graph::__node_delete {name args} {

    foreach node $args {
	if { ![__node_exists $name $node] } {
	    return -code error "node \"$node\" does not exist in graph \"$name\""
	}
    }

    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::nodeAttr

    foreach node $args {
	# Remove all the arcs connected to this node
	foreach e $inArcs($node) {
	    __arc_delete $name $e
	}
	foreach e $outArcs($node) {
	    # Check existence to avoid problems with
	    # loops (they are in and out arcs! at
	    # the same time and thus already deleted)
	    if { [__arc_exists $name $e] } {
		__arc_delete $name $e
	    }
	}

	unset inArcs($node)
	unset outArcs($node)

	if {[info exists nodeAttr($node)]} {
	    unset ${name}::$nodeAttr($node)
	    unset nodeAttr($node)
	}
    }

    return
}

# ::struct::graph::__node_exists --
#
#	Test for existance of a given node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to look for.
#
# Results:
#	1 if the node exists, 0 else.

proc ::struct::graph::__node_exists {name node} {
    return [info exists ${name}::inArcs($node)]
}

# ::struct::graph::__node_get --
#
#	Get a keyed value from a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to query.
#	key	key to lookup
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::__node_get {name node key} {
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }
 
    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attribute data for this node, key has to be invalid.
	return -code error "invalid key \"$key\" for node \"$node\""
    }

    upvar ${name}::$nodeAttr($node) data
    if { ![info exists data($key)] } {
	return -code error "invalid key \"$key\" for node \"$node\""
    }
    return $data($key)
}

# ::struct::graph::__node_getall --
#
#	Get a serialized list of key/value pairs from a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to query.
#	pattern	optional glob pattern to restrict retrieval
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::__node_getall {name node {pattern *}} { 
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attributes ...
	return {}
    }

    upvar ${name}::$nodeAttr($node) data
    return [array get data $pattern]
}

# ::struct::graph::__node_keys --
#
#	Get a list of keys from a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to query.
#	pattern	optional glob pattern to restrict retrieval
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::__node_keys {name node {pattern *}} { 
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attributes ...
	return {}
    }

    upvar ${name}::$nodeAttr($node) data
    return [array names data $pattern]
}

# ::struct::graph::__node_keyexists --
#
#	Test for existance of a given key for a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to query.
#	key	key to lookup
#
# Results:
#	1 if the key exists, 0 else.

proc ::struct::graph::__node_keyexists {name node key} {
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }
    
    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attribute data for this node, key cannot exist.
	return 0
    }

    upvar ${name}::$nodeAttr($node) data
    return [info exists data($key)]
}

# ::struct::graph::__node_insert --
#
#	Add a node to a graph.
#
# Arguments:
#	name		name of the graph.
#	args		node to insert; must be unique.  If none is given,
#			the routine will generate a unique node name.
#
# Results:
#	node		The namee of the new node.

proc ::struct::graph::__node_insert {name args} {

    if { [llength $args] == 0 } {
	# No node name was given; generate a unique one
	set node [__generateUniqueNodeName $name]
    } else {
	set node [lindex $args 0]
    }

    if { [__node_exists $name $node] } {
	return -code error "node \"$node\" already exists in graph \"$name\""
    }
    
    variable ${name}::inArcs
    variable ${name}::outArcs

    # Set up the new node
    set inArcs($node)  [list]
    set outArcs($node) [list]

    return $node
}

# ::struct::graph::__node_opposite --
#
#	Retrieve node opposite to the specified one, along the arc.
#
# Arguments:
#	name		name of the graph.
#	node		node to look up.
#	arc		arc to look up.
#
# Results:
#	nodex	Node opposite to <node,arc>

proc ::struct::graph::__node_opposite {name node arc} {
    if {![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }
    
    if {![__arc_exists $name $arc] } {
	return -code error "arc \"$arc\" does not exist in graph \"$name\""
    }

    variable ${name}::arcNodes

    # Node must be connected to at least one end of the arc.

    if {[string equal $node [lindex $arcNodes($arc) 0]]} {
	set result [lindex $arcNodes($arc) 1]
    } elseif {[string equal $node [lindex $arcNodes($arc) 1]]} {
	set result [lindex $arcNodes($arc) 0]
    } else {
	return -code error "node \"$node\" and arc \"$arc\" are not connected\
		in graph \"$name\""
    }

    return $result
}

# ::struct::graph::__node_set --
#
#	Set or get a value for a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to modify or query.
#	key	attribute to modify or query
#	args	?value?
#
# Results:
#	val	value associated with the given key of the given node

proc ::struct::graph::__node_set {name node key args} {
    if { [llength $args] > 1 } {
	return -code error "wrong # args: should be \"$name node set $node key ?value?\""
    }
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }
    
    if { [llength $args] > 0 } {
	# Setting the value. This may have to create
	# the attribute array for this particular
	# node

	variable ${name}::nodeAttr
	if {![info exists nodeAttr($node)]} {
	    # No attribute data for this node,
	    # so create it as we need it now.
	    GenAttributeStorage $name node $node
	}
	upvar ${name}::$nodeAttr($node) data

	return [set data($key) [lindex $args end]]
    } else {
	# Getting a value
	return [__node_get $name $node $key]
    }
}

# ::struct::graph::__node_append --
#
#	Append a value for a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to modify or query.
#	args	key value
#
# Results:
#	val	value associated with the given key of the given node

proc ::struct::graph::__node_append {name node key value} {
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attribute data for this node,
	# so create it as we need it.
	GenAttributeStorage $name node $node
    }

    upvar ${name}::$nodeAttr($node) data
    return [append data($key) $value]
}

# ::struct::graph::__node_attr --
#
#	Return attribute data for one key and multiple nodes, possibly all.
#
# Arguments:
#	name	Name of the graph object.
#	key	Name of the attribute to retrieve.
#
# Results:
#	children	Dictionary mapping nodes to attribute data.

proc ::struct::graph::__node_attr {name key args} {
    # Syntax:
    #
    # t attr key
    # t attr key -nodes {nodelist}
    # t attr key -glob nodepattern
    # t attr key -regexp nodepattern

    variable ${name}::nodeAttr

    set usage "wrong # args: should be \"[list $name] node attr key ?-nodes list|-glob pattern|-regexp pattern?\""
    if {([llength $args] != 0) && ([llength $args] != 2)} {
	return -code error $usage
    } elseif {[llength $args] == 0} {
	# This automatically restricts the list
	# to nodes which can have the attribute
	# in question.

	set nodes [array names nodeAttr]
    } else {
	# Determine a list of nodes to look at
	# based on the chosen restriction.

	foreach {mode value} $args break
	switch -exact -- $mode {
	    -nodes {
		# This is the only branch where we have to
		# perform an explicit restriction to the
		# nodes which have attributes.
		set nodes {}
		foreach n $value {
		    if {![info exists nodeAttr($n)]} continue
		    lappend nodes $n
		}
	    }
	    -glob {
		set nodes [array names nodeAttr $value]
	    }
	    -regexp {
		set nodes {}
		foreach n [array names nodeAttr] {
		    if {![regexp -- $value $n]} continue
		    lappend nodes $n
		}
	    }
	    default {
		return -code error $usage
	    }
	}
    }

    # Without possibly matching nodes
    # the result has to be empty.

    if {![llength $nodes]} {
	return {}
    }

    # Now locate matching keys and their values.

    set result {}
    foreach n $nodes {
	upvar ${name}::$nodeAttr($n) data
	if {[info exists data($key)]} {
	    lappend result $n $data($key)
	}
    }

    return $result
}

# ::struct::graph::__node_lappend --
#
#	lappend a value for a node in a graph.
#
# Arguments:
#	name	name of the graph.
#	node	node to modify or query.
#	args	key value
#
# Results:
#	val	value associated with the given key of the given node

proc ::struct::graph::__node_lappend {name node key value} {
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attribute data for this node,
	# so create it as we need it.
	GenAttributeStorage $name node $node
    }

    upvar ${name}::$nodeAttr($node) data
    return [lappend data($key) $value]
}

# ::struct::graph::__node_unset --
#
#	Remove a keyed value from a node.
#
# Arguments:
#	name	name of the graph.
#	node	node to modify.
#	key	attribute to remove
#
# Results:
#	None.

proc ::struct::graph::__node_unset {name node key} {
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    variable ${name}::nodeAttr
    if {![info exists nodeAttr($node)]} {
	# No attribute data for this node,
	# nothing to do.
	return
    }

    upvar ${name}::$nodeAttr($node) data
    catch {unset data($key)}

    if {[array size data] == 0} {
	# No attributes stored for this node, squash the whole array.
	set nodeAttr($node) {}
	unset data
    }
    return
}

# ::struct::graph::_nodes --
#
#	Return a list of all nodes in a graph satisfying some restriction.
#
# Arguments:
#	name	name of the graph.
#	args	list of options and nodes specifying the restriction.
#
# Results:
#	nodes	list of nodes

proc ::struct::graph::_nodes {name args} {

    # Discriminate between conditions and nodes

    set haveCond 0
    set haveKey 0
    set haveValue 0
    set haveFilter 0
    set cond "none"
    set condNodes [list]

    for {set i 0} {$i < [llength $args]} {incr i} {
	set arg [lindex $args $i]
	switch -glob -- $arg {
	    -in -
	    -out -
	    -adj -
	    -inner -
	    -embedding {
		set haveCond 1
		set cond [string range $arg 1 end]
	    }
	    -key {
		incr i
		set key [lindex $args $i]
		set haveKey 1
	    }
	    -value {
		incr i
		set value [lindex $args $i]
		set haveValue 1
	    }
	    -filter {
		incr i
		set fcmd [lindex $args $i]
		set haveFilter 1
	    }
	    -* {
		return -code error "invalid restriction \"$arg\": should be -in, -out,\
			-adj, -inner, -embedding, -key, -value, or -filter"
	    }
	    default {
		lappend condNodes $arg
	    }
	}
    }

    # Validate that there are nodes to use in the restriction.
    # otherwise what's the point?
    if {$haveCond} {
	if {[llength $condNodes] == 0} {
	    set usage "$name nodes ?-key key? ?-value value? ?-filter cmd? ?-in|-out|-adj|-inner|-embedding node node...?"
	    return -code error "no nodes specified: should be \"$usage\""
	}

	# Make sure that the specified nodes exist!
	foreach node $condNodes {
	    if { ![__node_exists $name $node] } {
		return -code error "node \"$node\" does not exist in graph \"$name\""
	    }
	}
    }

    # Now we are able to go to work
    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes

    set       nodes [list]
    array set coll  {}

    switch -exact -- $cond {
	in {
	    # Result is all nodes with at least one arc going to
	    # at least one node in the list of arguments.

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    set n [lindex $arcNodes($e) 0]
		    if {[info exists coll($n)]} {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
	    }
	}
	out {
	    # Result is all nodes with at least one arc coming from
	    # at least one node in the list of arguments.

	    foreach node $condNodes {
		foreach e $outArcs($node) {
		    set n [lindex $arcNodes($e) 1]
		    if {[info exists coll($n)]} {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
	    }
	}
	adj {
	    # Result is all nodes with at least one arc coming from
	    # or going to at least one node in the list of arguments.

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    set n [lindex $arcNodes($e) 0]
		    if {[info exists coll($n)]} {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
		foreach e $outArcs($node) {
		    set n [lindex $arcNodes($e) 1]
		    if {[info exists coll($n)]} {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
	    }
	}
	inner {
	    # Result is all nodes from the list! with at least one arc
	    # coming from or going to at least one node in the list of
	    # arguments.

	    array set group {}
	    foreach node $condNodes {
		set group($node) .
	    }

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    set n [lindex $arcNodes($e) 0]
		    if {![info exists group($n)]} {continue}
		    if { [info exists coll($n)]}  {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
		foreach e $outArcs($node) {
		    set n [lindex $arcNodes($e) 1]
		    if {![info exists group($n)]} {continue}
		    if { [info exists coll($n)]}  {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
	    }
	}
	embedding {
	    # Result is all nodes with at least one arc coming from
	    # or going to at least one node in the list of arguments,
	    # but not in the list itself!

	    array set group {}
	    foreach node $condNodes {
		set group($node) .
	    }

	    foreach node $condNodes {
		foreach e $inArcs($node) {
		    set n [lindex $arcNodes($e) 0]
		    if {[info exists group($n)]} {continue}
		    if {[info exists coll($n)]}  {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
		foreach e $outArcs($node) {
		    set n [lindex $arcNodes($e) 1]
		    if {[info exists group($n)]} {continue}
		    if {[info exists coll($n)]}  {continue}
		    lappend nodes    $n
		    set     coll($n) .
		}
	    }
	}
	none {
	    set nodes [array names inArcs]
	}
	default {return -code error "Can't happen, panic"}
    }

    #
    # We have a list of nodes that match the relation to the nodes.
    # Now filter according to -key and -value.
    #

    if {$haveKey} {
	set filteredNodes [list]
	foreach node $nodes {
	    catch {
		set nval [__node_get $name $node $key]
		if {$haveValue} {
		    if {$nval == $value} {
			lappend filteredNodes $node
		    }
		} else {
		    lappend filteredNodes $node
		}
	    }
	}
	set nodes $filteredNodes
    }

    #
    # Apply the general filter command, if specified.
    #

    if {$haveFilter} {
	lappend fcmd $name
	set nodes [uplevel 1 [list ::struct::list filter $nodes $fcmd]]
    }

    return $nodes
}

# ::struct::graph::__node_rename --
#
#	Rename a node in place.
#
# Arguments:
#	name	name of the graph.
#	node	Name of the node to rename
#	newname	The new name of the node.
#
# Results:
#	The new name of the node.

proc ::struct::graph::__node_rename {name node newname} {
    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }
    if {[__node_exists $name $newname]} {
	return -code error "unable to rename node to \"$newname\",\
		node of that name already present in the graph \"$name\""
    }

    set oldname  $node

    # Perform the rename in the internal
    # data structures.

    # - graphAttr - not required, node independent.
    # - arcAttr   - not required, node independent.
    # - counters  - not required

    variable ${name}::nodeAttr
    variable ${name}::inArcs
    variable ${name}::outArcs
    variable ${name}::arcNodes

    # Node relocation

    set inArcs($newname)    [set in $inArcs($oldname)]
    unset                            inArcs($oldname)
    set outArcs($newname) [set out $outArcs($oldname)]
    unset                           outArcs($oldname)

    if {[info exists nodeAttr($oldname)]} {
	set nodeAttr($newname) $nodeAttr($oldname)
	unset                   nodeAttr($oldname)
    }

    # Update all relevant arcs.
    # 8.4: lset ...

    foreach a $in {
	set arcNodes($a) [list [lindex $arcNodes($a) 0] $newname]
    }
    foreach a $out {
	set arcNodes($a) [list $newname [lindex $arcNodes($a) 1]]
    }

    return $newname
}

# ::struct::graph::_serialize --
#
#	Serialize a graph object (partially) into a transportable value.
#	If only a subset of nodes is serialized the result will be a sub-
#	graph in the mathematical sense of the word: These nodes and all
#	arcs which are only between these nodes. No arcs to modes outside
#	of the listed set.
#
# Arguments:
#	name	Name of the graph.
#	args	list of nodes to place into the serialized graph
#
# Results:
#	A list structure describing the part of the graph which was serialized.

proc ::struct::graph::_serialize {name args} {

    # all - boolean flag - set if and only if the all nodes of the
    # graph are chosen for serialization. Because if that is true we
    # can skip the step finding the relevant arcs and simply take all
    # arcs.

    variable ${name}::arcNodes
    variable ${name}::inArcs

    set all 0
    if {[llength $args] > 0} {
	set nodes [luniq $args]
	foreach n $nodes {
	    if {![__node_exists $name $n]} {
		return -code error "node \"$n\" does not exist in graph \"$name\""
	    }
	}
	if {[llength $nodes] == [array size inArcs]} {
	    set all 1
	}
    } else {
	set nodes [array names inArcs]
	set all 1
    }

    if {$all} {
	set arcs [array names arcNodes]
    } else {
	set arcs [eval [linsert $nodes 0 _arcs $name -inner]]
    }

    variable ${name}::nodeAttr
    variable ${name}::arcAttr
    variable ${name}::graphAttr

    set na {}
    set aa {}
    array set np {}

    # node indices, attribute data ...
    set i 0
    foreach n $nodes {
	set np($n) [list $i]
	incr i 3

	if {[info exists nodeAttr($n)]} {
	    upvar ${name}::$nodeAttr($n) data
	    lappend np($n) [array get data]
	} else {
	    lappend np($n) {}
	}
    }

    # arc dictionary
    set arcdata  {}
    foreach a $arcs {
	foreach {src dst} $arcNodes($a) break
	# Arc information

	set     arc $a
	lappend arc [lindex $np($dst) 0]
	if {[info exists arcAttr($a)]} {
	    upvar ${name}::$arcAttr($a) data
	    lappend arc [array get data]
	} else {
	    lappend arc {}
	}

	# Add the information to the node
	# indices ...

	lappend np($src) $arc
    }

    # Combine the transient data into one result.

    set result [list]
    foreach n $nodes {
	lappend result $n
	lappend result [lindex $np($n) 1]
	lappend result [lrange $np($n) 2 end]
    }
    lappend result [array get graphAttr]

    return $result
}

# ::struct::graph::_set --
#
#	Set or get a keyed value from the graph itself
#
# Arguments:
#	name	name of the graph.
#	key	attribute to modify or query
#	args	?value?
#
# Results:
#	value	value associated with the key given.

proc ::struct::graph::_set {name key args} {
    if { [llength $args] > 1 } {
	return -code error "wrong # args: should be \"$name set key ?value?\""
    }
    if { [llength $args] > 0 } {
	variable ${name}::graphAttr
	return [set graphAttr($key) [lindex $args end]]
    } else {
	# Getting a value
	return [_get $name $key]
    }
}

# ::struct::graph::_swap --
#
#	Swap two nodes in a graph.
#
# Arguments:
#	name	name of the graph.
#	node1	first node to swap.
#	node2	second node to swap.
#
# Results:
#	None.

proc ::struct::graph::_swap {name node1 node2} {
    # Can only swap two real nodes
    if { ![__node_exists $name $node1] } {
	return -code error "node \"$node1\" does not exist in graph \"$name\""
    }
    if { ![__node_exists $name $node2] } {
	return -code error "node \"$node2\" does not exist in graph \"$name\""
    }

    # Can't swap a node with itself
    if { [string equal $node1 $node2] } {
	return -code error "cannot swap node \"$node1\" with itself"
    }

    # Swapping nodes means swapping their labels, values and arcs
    variable ${name}::outArcs
    variable ${name}::inArcs
    variable ${name}::arcNodes
    variable ${name}::nodeAttr

    # Redirect arcs to the new nodes.

    foreach e $inArcs($node1)  {lset arcNodes($e) end $node2}
    foreach e $inArcs($node2)  {lset arcNodes($e) end $node1}
    foreach e $outArcs($node1) {lset arcNodes($e) 0 $node2}
    foreach e $outArcs($node2) {lset arcNodes($e) 0 $node1}

    # Swap arc lists

    set tmp            $inArcs($node1)
    set inArcs($node1) $inArcs($node2)
    set inArcs($node2) $tmp

    set tmp             $outArcs($node1)
    set outArcs($node1) $outArcs($node2)
    set outArcs($node2) $tmp

    # Swap the values
    # More complicated now with the possibility that nodes do not have
    # attribute storage associated with them. But also
    # simpler as we just have to swap/move the array
    # reference

    if {
	[set ia [info exists nodeAttr($node1)]] ||
	[set ib [info exists nodeAttr($node2)]]
    } {
	# At least one of the nodes has attribute data. We simply swap
	# the references to the arrays containing them. No need to
	# copy the actual data around.

	if {$ia && $ib} {
	    set tmp               $nodeAttr($node1)
	    set nodeAttr($node1) $nodeAttr($node2)
	    set nodeAttr($node2) $tmp
	} elseif {$ia} {
	    set   nodeAttr($node2) $nodeAttr($node1)
	    unset nodeAttr($node1)
	} elseif {$ib} {
	    set   nodeAttr($node1) $nodeAttr($node2)
	    unset nodeAttr($node2)
	} else {
	    return -code error "Impossible condition."
	}
    } ; # else: No attribute storage => Nothing to do {}

    return
}

# ::struct::graph::_unset --
#
#	Remove a keyed value from the graph itself
#
# Arguments:
#	name	name of the graph.
#	key	attribute to remove
#
# Results:
#	None.

proc ::struct::graph::_unset {name key} {
    variable ${name}::graphAttr
    if {[info exists  graphAttr($key)]} {
	unset graphAttr($key)
    }
    return
}

# ::struct::graph::_walk --
#
#	Walk a graph using a pre-order depth or breadth first
#	search. Pre-order DFS is the default.  At each node that is visited,
#	a command will be called with the name of the graph and the node.
#
# Arguments:
#	name	name of the graph.
#	node	node at which to start.
#	args	additional args: ?-order pre|post|both? ?-type {bfs|dfs}?
#		-command cmd
#
# Results:
#	None.

proc ::struct::graph::_walk {name node args} {
    set usage "$name walk $node ?-dir forward|backward?\
	    ?-order pre|post|both? ?-type {bfs|dfs}? -command cmd"

    if {[llength $args] > 8 || [llength $args] < 2} {
	return -code error "wrong # args: should be \"$usage\""
    }

    if { ![__node_exists $name $node] } {
	return -code error "node \"$node\" does not exist in graph \"$name\""
    }

    # Set defaults
    set type  dfs
    set order pre
    set cmd   ""
    set dir   forward

    # Process specified options
    for {set i 0} {$i < [llength $args]} {incr i} {
	set flag [lindex $args $i]
	incr i
	if { $i >= [llength $args] } {
	    return -code error "value for \"$flag\" missing: should be \"$usage\""
	}
	switch -glob -- $flag {
	    "-type" {
		set type [string tolower [lindex $args $i]]
	    }
	    "-order" {
		set order [string tolower [lindex $args $i]]
	    }
	    "-command" {
		set cmd [lindex $args $i]
	    }
	    "-dir" {
		set dir [string tolower [lindex $args $i]]
	    }
	    default {
		return -code error "unknown option \"$flag\": should be \"$usage\""
	    }
	}
    }
    
    # Make sure we have a command to run, otherwise what's the point?
    if { [string equal $cmd ""] } {
	return -code error "no command specified: should be \"$usage\""
    }

    # Validate that the given type is good
    switch -glob -- $type {
	"dfs" {
	    set type "dfs"
	}
	"bfs" {
	    set type "bfs"
	}
	default {
	    return -code error "invalid search type \"$type\": should be dfs, or bfs"
	}
    }
    
    # Validate that the given order is good
    switch -glob -- $order {
	"both" {
	    set order both
	}
	"pre" {
	    set order pre
	}
	"post" {
	    set order post
	}
	default {
	    return -code error "invalid search order \"$order\": should be both,\
		    pre or post"
	}
    }

    # Validate that the given direction is good
    switch -glob -- $dir {
	"forward" {
	    set dir -out
	}
	"backward" {
	    set dir -in
	}
	default {
	    return -code error "invalid search direction \"$dir\": should be\
		    forward or backward"
	}
    }

    # Do the walk

    set st [list ]
    lappend st $node
    array set visited {}

    if { [string equal $type "dfs"] } {
	if { [string equal $order "pre"] } {
	    # Pre-order Depth-first search

	    while { [llength $st] > 0 } {
		set node [lindex   $st end]
		ldelete st end

		# Evaluate the command at this node
		set cmdcpy $cmd
		lappend cmdcpy enter $name $node
		uplevel 1 $cmdcpy

		set visited($node) .

		# Add this node's neighbours (according to direction)
		#  Have to add them in reverse order
		#  so that they will be popped left-to-right

		set next [_nodes $name $dir $node]
		set len  [llength $next]

		for {set i [expr {$len - 1}]} {$i >= 0} {incr i -1} {
		    set nextnode [lindex $next $i]
		    if {[info exists visited($nextnode)]} {
			# Skip nodes already visited
			continue
		    }
		    lappend st $nextnode
		}
	    }
	} elseif { [string equal $order "post"] } {
	    # Post-order Depth-first search

	    while { [llength $st] > 0 } {
		set node [lindex $st end]

		if {[info exists visited($node)]} {
		    # Second time we are here, pop it,
		    # then evaluate the command.

		    ldelete st end

		    # Evaluate the command at this node
		    set cmdcpy $cmd
		    lappend cmdcpy leave $name $node
		    uplevel 1 $cmdcpy
		} else {
		    # First visit. Remember it.
		    set visited($node) .
	    
		    # Add this node's neighbours.
		    set next [_nodes $name $dir $node]
		    set len  [llength $next]

		    for {set i [expr {$len - 1}]} {$i >= 0} {incr i -1} {
			set nextnode [lindex $next $i]
			if {[info exists visited($nextnode)]} {
			    # Skip nodes already visited
			    continue
			}
			lappend st $nextnode
		    }
		}
	    }
	} else {
	    # Both-order Depth-first search

	    while { [llength $st] > 0 } {
		set node [lindex $st end]

		if {[info exists visited($node)]} {
		    # Second time we are here, pop it,
		    # then evaluate the command.

		    ldelete st end

		    # Evaluate the command at this node
		    set cmdcpy $cmd
		    lappend cmdcpy leave $name $node
		    uplevel 1 $cmdcpy
		} else {
		    # First visit. Remember it.
		    set visited($node) .

		    # Evaluate the command at this node
		    set cmdcpy $cmd
		    lappend cmdcpy enter $name $node
		    uplevel 1 $cmdcpy
	    
		    # Add this node's neighbours.
		    set next [_nodes $name $dir $node]
		    set len  [llength $next]

		    for {set i [expr {$len - 1}]} {$i >= 0} {incr i -1} {
			set nextnode [lindex $next $i]
			if {[info exists visited($nextnode)]} {
			    # Skip nodes already visited
			    continue
			}
			lappend st $nextnode
		    }
		}
	    }
	}

    } else {
	if { [string equal $order "pre"] } {
	    # Pre-order Breadth first search
	    while { [llength $st] > 0 } {
		set node [lindex $st 0]
		ldelete st 0
		# Evaluate the command at this node
		set cmdcpy $cmd
		lappend cmdcpy enter $name $node
		uplevel 1 $cmdcpy
	    
		set visited($node) .

		# Add this node's neighbours.
		foreach child [_nodes $name $dir $node] {
		    if {[info exists visited($child)]} {
			# Skip nodes already visited
			continue
		    }
		    lappend st $child
		}
	    }
	} else {
	    # Post-order Breadth first search
	    # Both-order Breadth first search
	    # Haven't found anything in Knuth
	    # and unable to define something
	    # consistent for myself. Leave it
	    # out.

	    return -code error "unable to do a ${order}-order breadth first walk"
	}
    }
    return
}

# ::struct::graph::Union --
#
#	Return a list which is the union of the elements
#	in the specified lists.
#
# Arguments:
#	args	list of lists representing sets.
#
# Results:
#	set	list representing the union of the argument lists.

proc ::struct::graph::Union {args} {
    switch -- [llength $args] {
	0 {
	    return {}
	}
	1 {
	    return [lindex $args 0]
	}
	default {
	    foreach set $args {
		foreach e $set {
		    set tmp($e) .
		}
	    }
	    return [array names tmp]
	}
    }
}

# ::struct::graph::GenAttributeStorage --
#
#	Create an array to store the attrributes of a node in.
#
# Arguments:
#	name	Name of the graph containing the node
#	type	Type of object for the attribute
#	obj	Name of the node or arc which got attributes.
#
# Results:
#	none

proc ::struct::graph::GenAttributeStorage {name type obj} {
    variable ${name}::nextAttr
    upvar    ${name}::${type}Attr attribute

    set   attr "a[incr nextAttr]"
    set   attribute($obj) $attr
    return
}



proc ::struct::graph::CheckSerialization {ser gavar navar aavar inavar outavar arcnvar} {
    upvar 1 \
	    $gavar   graphAttr \
	    $navar   nodeAttr  \
	    $aavar   arcAttr   \
	    $inavar  inArcs    \
	    $outavar outArcs   \
	    $arcnvar arcNodes

    # Overall length ok ?
    if {[llength $ser] % 3 != 1} {
	return -code error \
		"error in serialization: list length not 1 mod 3."
    }

    # Attribute length ok ? Dictionary!
    set graphAttr [lindex $ser end]
    if {[llength $graphAttr] % 2} {
	return -code error \
		"error in serialization: malformed graph attribute dictionary."
    }

    # Basic decoder pass

    foreach {node attr narcs} [lrange $ser 0 end-1] {
	if {![info exists inArcs($node)]} {
	    set inArcs($node)  [list]
	}
	set outArcs($node) [list]

	# Attribute length ok ? Dictionary!
	if {[llength $attr] % 2} {
	    return -code error \
		    "error in serialization: malformed node attribute dictionary."
	}
	# Remember attribute data only for non-empty nodes
	if {[llength $attr]} {
	    set nodeAttr($node) $attr
	}

	foreach arcd $narcs {
	    foreach {arc dst attr} $arcd break

	    if {[info exists arcNodes($arc)]} {
		return -code error \
			"error in serialization: duplicate definition of arc \"$arc\"."
	    }

	    # Attribute length ok ? Dictionary!
	    if {[llength $attr] % 2} {
		return -code error \
			"error in serialization: malformed arc attribute dictionary."
	    }
	    # Remember attribute data only for non-empty nodes
	    if {[llength $attr]} {
		set arcAttr($arc) $attr
	    }

	    # Destination reference ok ?
	    if {
		![string is integer -strict $dst] ||
		($dst % 3) ||
		($dst < 0) ||
		($dst >= [llength $ser])
	    } {
		return -code error \
			"error in serialization: bad arc destination reference \"$dst\"."
	    }

	    # Get destination and reconstruct the
	    # various relationships.

	    set dstnode [lindex $ser $dst]

	    set arcNodes($arc) [list $node $dstnode]
	    lappend inArcs($dstnode) $arc
	    lappend outArcs($node)   $arc
	}
    }

    # Duplicate node names ?

    if {[array size outArcs] < ([llength $ser] / 3)} {
	return -code error \
		"error in serialization: duplicate node names."
    }

    # Ok. The data is now ready for the caller.
    return
}

##########################
# Private functions follow
#
# Do a compatibility version of [lset] for pre-8.4 versions of Tcl.
# This version does not do multi-arg [lset]!

proc ::struct::graph::K { x y } { set x }

if { [package vcompare [package provide Tcl] 8.4] < 0 } {
    proc ::struct::graph::lset { var index arg } {
	upvar 1 $var list
	set list [::lreplace [K $list [set list {}]] $index $index $arg]
    }
}

proc ::struct::graph::ldelete {var index {end {}}} {
    upvar 1 $var list
    if {$end == {}} {set end $index}
    set list [lreplace [K $list [set list {}]] $index $end]
    return
}

proc ::struct::graph::luniq {list} {
    array set _ {}
    set result [list]
    foreach e $list {
	if {[info exists _($e)]} {continue}
	lappend result $e
	set _($e) .
    }
    return $result
}
