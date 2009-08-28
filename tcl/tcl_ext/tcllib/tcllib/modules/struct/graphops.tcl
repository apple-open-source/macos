# graphops.tcl --
#
#	Operations on and algorithms for graph data structures.
#
# Copyright (c) 2008 Alejandro Paz <vidriloco@gmail.com>, algorithm implementation
# Copyright (c) 2008 Andreas Kupries, integration with Tcllib's struct::graph
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: graphops.tcl,v 1.10 2008/11/20 07:26:43 andreas_kupries Exp $

# ### ### ### ######### ######### #########
## Requisites

package require Tcl 8.4

package require struct::disjointset ; # Used by kruskal
package require struct::prioqueue   ; # Used by kruskal, prim
package require struct::queue       ; # Used by isBipartite?, connectedComponent(Of)
package require struct::stack       ; # Used by tarjan
package require struct::graph       ; # isBridge, isCutVertex

# ### ### ### ######### ######### #########
##

namespace eval ::struct::graph::op {}

# ### ### ### ######### ######### #########
##

# This command constructs an adjacency matrix representation of the
# graph argument.

# Reference: http://en.wikipedia.org/wiki/Adjacency_matrix
#
# Note: The reference defines the matrix in such a way that some of
#       the limitations of the code here are not present. I.e. the
#       definition at wikipedia deals properly with arc directionality
#       and parallelism.
#
# TODO: Rework the code so that the result is in line with the reference.
#       Add features to handle weights as well.

proc ::struct::graph::op::toAdjacencyMatrix {g} {
    set nodeList [lsort -dict [$g nodes]]
    # Note the lsort. This is used to impose some order on the matrix,
    # for comparability of results. Otherwise different versions of
    # Tcl and struct::graph (critcl) may generate different, yet
    # equivalent matrices, dependent on things like the order a hash
    # search is done, or nodes have been added to the graph, or ...

    # Fill an array for index tracking later. Note how we start from
    # index 1. This allows us avoid multiple expr+1 later on when
    # iterating over the nodes and converting the names to matrix
    # indices. See (*).

    set i 1
    foreach n  $nodeList {
	set nodeDict($n) $i
	incr i
    }

    set matrix {}
    lappend matrix [linsert $nodeList 0 {}]

    # Setting up a template row with all of it's elements set to zero.

    set baseRow 0
    foreach n $nodeList {
	lappend baseRow 0
    }

    foreach node $nodeList {

	# The first element in every row is the name of its
	# corresponding node. Using lreplace to overwrite the initial
	# data in the template we get a copy apart from the template,
	# which we can then modify further.

	set currentRow [lreplace $baseRow 0 0 $node]

	# Iterate over the neighbours, also known as 'adjacent'
	# rows. The exact set of neighbours depends on the mode.

	foreach neighbour [$g nodes -adj $node] {
	    # Set value for neighbour on this node list
	    set at $nodeDict($neighbour)

	    # (*) Here we avoid +1 due to starting from index 1 in the
	    #     initialization of nodeDict.
	    set currentRow [lreplace $currentRow $at $at 1]
	}
	lappend matrix $currentRow
    }

    # The resulting matrix is a list of lists, size (n+1)^2 where n =
    # number of nodes. First row and column (index 0) are node
    # names. The other entries are boolean flags. True when an arc is
    # present, False otherwise. The matrix represents an
    # un-directional form of the graph with parallel arcs collapsed.

    return $matrix
}

# ### ### ### ######### ######### #########
##

# This command finds a minimum spanning tree/forest (MST) of the graph
# argument, using the algorithm developed by Joseph Kruskal. The
# result is a set (as list) containing the names of the arcs in the
# MST. The set of nodes of the MST is implied by set of arcs, and thus
# not given explicitly. The algorithm does not consider arc
# directions. Note that unconnected nodes are left out of the result.

# Reference: http://en.wikipedia.org/wiki/Kruskal%27s_algorithm

proc ::struct::graph::op::kruskal {g} {
    # Check graph argument for proper configuration.

    VerifyWeightsAreOk $g

    # Transient helper data structures. A priority queue for the arcs
    # under consideration, using their weights as priority, and a
    # disjoint-set to keep track of the forest of partial minimum
    # spanning trees we are working with.

    set consider [::struct::prioqueue -dictionary consider]
    set forest   [::struct::disjointset forest]

    # Start with all nodes in the graph each in their partition.

    foreach n [$g nodes] {
	$forest add-partition $n
    }

    # Then fill the queue with all arcs, using their weight to
    # prioritize. The weight is the cost of the arc. The lesser the
    # better.

    foreach {arc weight} [$g arc weights] {
	$consider put $arc $weight
    }

    # And now we can construct the tree. This is done greedily. In
    # each round we add the arc with the smallest weight to the
    # minimum spanning tree, except if doing so would violate the tree
    # condition.

    set result {}

    while {[$consider size]} {
	set minarc [$consider get]
	set origin [$g arc source $minarc]
	set destin [$g arc target $minarc]

	# Ignore the arc if both ends are in the same partition. Using
	# it would add a cycle to the result, i.e. it would not be a
	# tree anymore.

	if {[$forest equal $origin $destin]} continue

	# Take the arc for the result, and merge the trees both ends
	# are in into a single tree.

	lappend result $minarc
	$forest merge $origin $destin
    }

    # We are done. Get rid of the transient helper structures and
    # return our result.

    $forest   destroy
    $consider destroy

    return $result
}

# ### ### ### ######### ######### #########
##

# This command finds a minimum spanning tree/forest (MST) of the graph
# argument, using the algorithm developed by Prim. The result is a
# set (as list) containing the names of the arcs in the MST. The set
# of nodes of the MST is implied by set of arcs, and thus not given
# explicitly. The algorithm does not consider arc directions.

# Reference: http://en.wikipedia.org/wiki/Prim%27s_algorithm

proc ::struct::graph::op::prim {g} {
    VerifyWeightsAreOk $g

    # Fill an array with all nodes, to track which nodes have been
    # visited at least once. When the inner loop runs out of nodes and
    # we still have some left over we restart using one of the
    # leftover as new starting point. In this manner we get the MST of
    # the whole graph minus unconnected nodes, instead of only the MST
    # for the component the initial starting node is in.

    array set unvisited {}
    foreach n [$g nodes] { set unvisited($n) . }

    # Transient helper data structure. A priority queue for the nodes
    # and arcs under consideration for inclusion into the MST. Each
    # element of the queue is a list containing node name, a flag bit,
    # and arc name, in this order. The associated priority is the
    # weight of the arc. The flag bit is set for the initial queue
    # entry only, containing a fake (empty) arc, to trigger special
    # handling.

    set consider [::struct::prioqueue -dictionary consider]

    # More data structures, the result arrays.
    array set weightmap {} ; # maps nodes to min arc weight seen so
			     # far. This is the threshold other arcs
			     # on this node will have to beat to be
			     # added to the MST.
    array set arcmap    {} ; # maps arcs to nothing, these are the
			     # arcs in the MST.

    while {[array size unvisited]} {
	# Choose a 'random' node as the starting point for the inner
	# loop, prim's algorithm, and put it on the queue for
	# consideration. Then we iterate until we have considered all
	# nodes in the its component.

	set startnode [lindex [array names unvisited] 0]
	$consider put [list $startnode 1 {}] 0

	while {[$consider size] > 0} {
	    # Pull the next minimum weight to look for. This is the
	    # priority of the next item we can get from the queue. And the
	    # associated node/decision/arc data.

	    set arcweight [$consider peekpriority 1]

	    foreach {v arcundefined arc} [$consider get] break
	    #8.5: lassign [$consider get] v arcundefined arc

	    # Two cases to consider: The node v is already part of the
	    # MST, or not. If yes we check if the new arcweight is better
	    # than what we have stored already, and update accordingly.

	    if {[info exists weightmap($v)]} {
		set currentweight $weightmap($v)
		if {$arcweight < $currentweight} {
		    # The new weight is better, update to use it as
		    # the new threshold. Note that this fill not touch
		    # any other arcs found for this node, as these are
		    # still minimal.

		    set weightmap($v) $arcweight
		    set arcmap($arc)  .
		}
	    } else {
		# Node not yet present. Save weight and arc. The
		# latter if and only the arc is actually defined. For
		# the first, initial queue entry, it is not.  Then we
		# add all the arcs adjacent to the current node to the
		# queue to consider them in the next rounds.

		set weightmap($v) $arcweight
		if {!$arcundefined} {
		    set arcmap($arc) .
		}
		foreach adjacentarc [$g arcs -adj $v] {
		    set weight    [$g arc  getweight   $adjacentarc]
		    set neighbour [$g node opposite $v $adjacentarc]
		    $consider put [list $neighbour 0 $adjacentarc] $weight
		}
	    }

	    # Mark the node as visited, belonging to the current
	    # component. Future iterations will ignore it.
	    unset -nocomplain unvisited($v)
	}
    }

    # We are done. Get rid of the transient helper structure and
    # return our result.

    $consider destroy

    return [array names arcmap]
}

# ### ### ### ######### ######### #########
##

# This command checks whether the graph argument is bi-partite or not,
# and returns the result as a boolean value, true for a bi-partite
# graph, and false otherwise. A variable can be provided to store the
# bi-partition into.
#
# Reference: http://en.wikipedia.org/wiki/Bipartite_graph

proc ::struct::graph::op::isBipartite? {g {bipartitionvar {}}} {

    # Handle the special cases of empty graphs, or one without arcs
    # quickly. Both are bi-partite.

    if {$bipartitionvar ne ""} {
	upvar 1 $bipartitionvar bipartitions
    }
    if {![llength [$g nodes]]} {
	set  bipartitions {{} {}}
	return 1
    } elseif {![llength [$g arcs]]} {
	if {$bipartitionvar ne ""} {
	    set  bipartitions [list [$g nodes] {}]
	}
	return 1
    }

    # Transient helper data structure, a queue of the nodes waiting
    # for processing.

    set pending [struct::queue pending]
    set nodes   [$g nodes]

    # Another structure, a map from node names to their 'color',
    # indicating which of the two partitions a node belngs to. All
    # nodes start out as undefined (0). Traversing the arcs we
    # set and flip them as needed (1,2).

    array set color {}
    foreach node $nodes {
	set color($node) 0
    }

    # Iterating over all nodes we use their connections to traverse
    # the components and assign colors. We abort when encountering
    # paradox, as that means that the graph is not bi-partite.

    foreach node $nodes {
	# Ignore nodes already in the second partition.
	if {$color($node)} continue

	# Flip the color, then travel the component and check for
	# conflicts with the neighbours.

	set color($node) 1 

	$pending put $node
	while {[$pending size]} {
	    set current [$pending get]
	    foreach neighbour [$g nodes -adj $current] {
		if {!$color($neighbour)} {
		    # Exchange the color between current and previous
		    # nodes, and remember the neighbour for further
		    # processing.
		    set color($neighbour) [expr {3 - $color($current)}]
		    $pending put $neighbour
		} elseif {$color($neighbour) == $color($current)} {
		    # Color conflict between adjacent nodes, should be
		    # different.  This graph is not bi-partite. Kill
		    # the data structure and abort.

		    $pending destroy
		    return 0
		}
	    }
	}
    }

    # The graph is bi-partite. Kill the transient data structure, and
    # move the partitions into the provided variable, if there is any.

    $pending destroy

    if {$bipartitionvar ne ""} {
	# Build bipartition, then set the data into the variable
	# passed as argument to this command.

	set X {}
	set Y {}

	foreach {node partition} [array get color] {
	    if {$partition == 1} {
		lappend X $node
	    } else {
		lappend Y $node
	    }
	}
	set bipartitions [list $X $Y]
    }

    return 1 
}

# ### ### ### ######### ######### #########
##

# This command computes a maximal matching, if it exists, for the
# graph argument G and its bi-partition as specified through the node
# sets X and Y. As is implied, this method requires that the graph is
# bi-partite. Use the command 'isBipartite?' to check for this
# property, and to obtain the bi-partition.
if 0 {
proc ::struct::graph::op::maxMatching {g X Y} {
    return -code error "not implemented yet"
}}

# ### ### ### ######### ######### #########
##

# This command computes the strongly connected components (SCCs) of
# the graph argument G. The result is a list of node-sets, each set
# containing the nodes of one SCC of G. In any SCC there is a directed
# path between any two nodes U, V from U to V. If all SCCs contain
# only a single node the graph is acyclic.

proc ::struct::graph::op::tarjan {g} {
    set all [$g nodes]

    # Quick bailout for simple special cases, i.e. graphs without
    # nodes or arcs.
    if {![llength $all]} {
	# No nodes => no SCCs
	return {}
    } elseif {![llength [$g arcs]]} {
	# Have nodes, but no arcs => each node is its own SCC.
	set r {} ; foreach a $all { lappend r [list $a] }
	return $r
    }

    # Transient data structures. Stack of nodes to consider, the
    # result, and various state arrays. TarjanSub upvar's all them
    # into its scope.

    set pending [::struct::stack pending]
    set result  {}

    array set index   {}
    array set lowlink {}
    array set instack {}

    # Invoke the main search system while we have unvisited
    # nodes. TarjanSub will remove all visited nodes from 'all',
    # ensuring termination.

    while {[llength $all]} {
	TarjanSub [lindex $all 0] 0
    }

    # Release the transient structures and return result.
    $pending destroy
    return $result
}

proc ::struct::graph::op::TarjanSub {start counter} {
    # Import the tracer state from our caller.
    upvar 1 g g index index lowlink lowlink instack instack result result pending pending all all

    struct::set subtract all $start

    set component {}
    set   index($start) $counter
    set lowlink($start) $counter
    incr counter

    $pending push $start
    set instack($start) 1

    foreach outarc [$g arcs -out $start] {
	set neighbour [$g arc target $outarc]

	if {![info exists index($neighbour)]} {
	    # depth-first-search of reachable nodes from the neighbour
	    # node. Original from the chosen startnode.
	    TarjanSub $neighbour $counter
	    set lowlink($start) [Min $lowlink($start) $lowlink($neighbour)]

	} elseif {[info exists instack($neighbour)]} {
	    set lowlink($start) [Min $lowlink($start) $lowlink($neighbour)]
	}
    }

    # Check if the 'start' node on this recursion level is the root
    # node of a SCC, and collect the component if yes.

    if {$lowlink($start) == $index($start)} {
	while {1} {
	    set v [$pending pop]
	    unset instack($v)
	    lappend component $v
	    if {$v eq $start} break
	}
	lappend result $component
    }

    return
}

# ### ### ### ######### ######### #########
##

# This command computes the connected components (CCs) of the graph
# argument G. The result is a list of node-sets, each set containing
# the nodes of one CC of G. In any CC there is UN-directed path
# between any two nodes U, V.

proc ::struct::graph::op::connectedComponents {g} {
    set all [$g nodes]

    # Quick bailout for simple special cases, i.e. graphs without
    # nodes or arcs.
    if {![llength $all]} {
	# No nodes => no CCs
	return {}
    } elseif {![llength [$g arcs]]} {
	# Have nodes, but no arcs => each node is its own CC.
	set r {} ; foreach a $all { lappend r [list $a] }
	return $r
    }

    # Invoke the main search system while we have unvisited
    # nodes.

    set result  {}
    while {[llength $all]} {
	set component [ComponentOf $g [lindex $all 0]]
	lappend result $component
	# all = all - component
	struct::set subtract all $component
    }
    return $result
}

# A derivative command which computes the connected component (CC) of
# the graph argument G containing the node N. The result is a node-set
# containing the nodes of the CC of N in G.

proc ::struct::graph::op::connectedComponentOf {g n} {
    # Quick bailout for simple special cases
    if {![$g node exists $n]} {
	return -code error "node \"$n\" does not exist in graph \"$g\""
    } elseif {![llength [$g arcs -adj $n]]} {
	# The chosen node has no neighbours, so is its own CC.
	return [list $n]
    }

    # Invoke the main search system for the chosen node.

    return [ComponentOf $g $n]
}

# Internal helper for finding connected components. 

proc ::struct::graph::op::ComponentOf {g start} {
    set pending [::struct::queue pending]
    $pending put $start

    array set visited {}
    set visited($start) .

    while {[$pending size]} {
	set current [$pending get 1]
	foreach neighbour [$g nodes -adj $current] {
	    if {[info exists visited($neighbour)]} continue
	    $pending put $neighbour
	    set visited($neighbour) 1
	}
    }
    $pending destroy
    return [array names visited]
}

# ### ### ### ######### ######### #########
##

# This command determines if the specified arc A in the graph G is a
# bridge, i.e. if its removal will split the connected component its
# end nodes belong to, into two. The result is a boolean value. Uses
# the 'ComponentOf' helper command.

proc ::struct::graph::op::isBridge? {g arc} {
    if {![$g arc exists $arc]} {
	return -code error "arc \"$arc\" does not exist in graph \"$g\""
    }

    # Note: We could avoid the need for a copy of the graph if we were
    # willing to modify G (*). As we are not willing using a copy is
    # the easiest way to allow us a trivial modification. For the
    # future consider the creation of a graph class which represents
    # virtual graphs over a source, generated by deleting nodes and/or
    # arcs. without actually modifying the source.
    #
    # (Ad *): Create a new unnamed helper node X. Move the arc
    #         destination to X. Recompute the component and ignore
    #         X. Then move the arc target back to its original node
    #         and remove X again.

    set src        [$g arc source $arc]
    set compBefore [ComponentOf $g $src]
    if {[llength $compBefore] == 1} {
	# Special case, the arc is a loop on an otherwise unconnected
	# node. The component will not split, this is not a bridge.
	return 0
    }

    set copy       [struct::graph BridgeCopy = $g]
    $copy arc delete $arc
    set compAfter  [ComponentOf $copy $src]
    $copy destroy

    return [expr {[llength $compBefore] != [llength $compAfter]}]
}

# This command determines if the specified node N in the graph G is a
# cut vertex, i.e. if its removal will split the connected component
# it belongs to into two. The result is a boolean value. Uses the
# 'ComponentOf' helper command.

proc ::struct::graph::op::isCutVertex? {g n} {
    if {![$g node exists $n]} {
	return -code error "node \"$n\" does not exist in graph \"$g\""
    }

    # Note: We could avoid the need for a copy of the graph if we were
    # willing to modify G (*). As we are not willing using a copy is
    # the easiest way to allow us a trivial modification. For the
    # future consider the creation of a graph class which represents
    # virtual graphs over a source, generated by deleting nodes and/or
    # arcs. without actually modifying the source.
    #
    # (Ad *): Create two new unnamed helper nodes X and Y. Move the
    #         icoming and outgoing arcs to these helpers. Recompute
    #         the component and ignore the helpers. Then move the arcs
    #         back to their original nodes and remove the helpers
    #         again.

    set compBefore [ComponentOf $g $n]

    if {[llength $compBefore] == 1} {
	# Special case. The node is unconnected. Its removal will
	# cause no changes. Therefore not a cutvertex.
	return 0
    }

    # We remove the node from the original component, so that we can
    # select a new start node without fear of hitting on the
    # cut-vertex candidate. Also makes the comparison later easier
    # (straight ==).
    struct::set subtract compBefore $n

    set copy       [struct::graph CutVertexCopy = $g]
    $copy node delete $n
    set compAfter  [ComponentOf $copy [lindex $compBefore 0]]
    $copy destroy

    return [expr {[llength $compBefore] != [llength $compAfter]}]
}

# This command determines if the graph G is connected.

proc ::struct::graph::op::isConnected? {g} {
    return [expr { [llength [connectedComponents $g]] == 1 }]
}

# ### ### ### ######### ######### #########
##

# This command determines if the specified graph G has an eulerian
# cycle (aka euler tour, <=> g is eulerian) or not. If yes, it can
# return the cycle through the named variable, as a list of arcs
# traversed.
#
# Note that for a graph to be eulerian all nodes have to have an even
# degree, and the graph has to be connected. And if more than two
# nodes have an odd degree the graph is not even semi-eulerian (cannot
# even have an euler path).

proc ::struct::graph::op::isEulerian? {g {eulervar {}}} {
    set nodes [$g nodes]
    if {![llength $nodes] || ![llength [$g arcs]]} {
	# Quick bailout for special cases. No nodes, or no arcs imply
	# that no euler cycle is present.
	return 0
    }

    # Check the condition regarding even degree nodes, then
    # connected-ness.

    foreach n $nodes {
	if {([$g node degree $n] % 2) == 0} continue
	# Odd degree node found, not eulerian.
	return 0
    }

    if {![isConnected? $g]} {
	return 0
    }

    # At this point the graph is connected, with all nodes of even
    # degree. As per Carl Hierholzer the graph has to have an euler
    # tour. If the user doesn't request it we do not waste the time to
    # actually compute one.

    if {$eulervar eq ""} {
	return 1
    }

    upvar 1 $eulervar tour

    # We start the tour at an arbitrary node.

    Fleury $g [lindex $nodes 0] tour
    return 1
}

# This command determines if the specified graph G has an eulerian
# path (<=> g is semi-eulerian) or not. If yes, it can return the
# path through the named variable, as a list of arcs traversed.
#
# (*) Aka euler tour.
#
# Note that for a graph to be semi-eulerian at most two nodes are
# allowed to have an odd degree, all others have to be of even degree,
# and the graph has to be connected.

proc ::struct::graph::op::isSemiEulerian? {g {eulervar {}}} {
    set nodes [$g nodes]
    if {![llength $nodes] || ![llength [$g arcs]]} {
	# Quick bailout for special cases. No nodes, or no arcs imply
	# that no euler path is present.
	return 0
    }

    # Check the condition regarding oddd/even degree nodes, then
    # connected-ness.

    set odd 0
    foreach n $nodes {
	if {([$g node degree $n] % 2) == 0} continue
	incr odd
	set lastodd $n
    }
    if {($odd > 2) || ![isConnected? $g]} {
	return 0
    }

    # At this point the graph is connected, with the node degrees
    # supporting existence of an euler path. If the user doesn't
    # request it we do not waste the time to actually compute one.

    if {$eulervar eq ""} {
	return 1
    }

    upvar 1 $eulervar path

    # We start at either an odd-degree node, or any node, if there are
    # no odd-degree ones. In the last case we are actually
    # constructing an euler tour, i.e. a closed path.

    if {$odd} {
	set start $lastodd
    } else {
	set start [lindex $nodes 0]
    }

    Fleury $g $start path
    return 1
}

proc ::struct::graph::op::Fleury {g start eulervar} {
    upvar 1 $eulervar path

    # We start at the chosen node.

    set copy  [struct::graph FleuryCopy = $g]
    set path  {}

    # Edges are chosen per Fleury's algorithm. That is easy,
    # especially as we already have a command to determine whether an
    # arc is a bridge or not.

    set arcs [$copy arcs]
    while {![struct::set empty $arcs]} {
	set adjacent [$copy arcs -adj $start]

	if {[llength $adjacent] == 1} {
	    # No choice in what arc to traverse.
	    set arc [lindex $adjacent 0]
	} else {
	    # Choose first non-bridge arcs. The euler conditions force
	    # that at least two such are present.

	    set has 0
	    foreach arc $adjacent {
		if {[isBridge? $copy $arc]} {
		    continue
		}
		set has 1
		break
	    }
	    if {!$has} { return -code error {Internal error} }
	}

	set start [$copy node opposite $start $arc]
	$copy arc delete $arc
	struct::set subtract arcs $arc
	lappend path $arc
    }

    $copy destroy
    return
}

# ### ### ### ######### ######### #########
##

# This command uses dijkstra's algorithm to find all shortest paths in
# the graph G starting at node N. The operation can be configured to
# traverse arcs directed and undirected, and the format of the result.

proc ::struct::graph::op::dijkstra {g node args} {
    # Default traversal is undirected.
    # Default output format is tree.

    set arcTraversal undirected
    set resultFormat tree

    # Process options to override the defaults, if any.
    foreach {option param} $args {
	switch -exact -- $option {
	    -arcmode {
		switch -exact -- $param {
		    directed -
		    undirected {
			set arcTraversal $param
		    }
		    default {
			return -code error "Bad value for -arcmode, expected one of \"directed\" or \"undirected\""
		    }
		}
	    }
	    -outputformat {
		switch -exact -- $param {
		    tree -
		    distances {
			set resultFormat $param
		    }
		    default {
			return -code error "Bad value for -outputformat, expected one of \"distances\" or \"tree\""
		    }
		}
	    }
	    default {		
		return -code error "Bad option \"$option\", expected one of \"-arcmode\" or \"-outputformat\""
	    }
	}
    }

    # We expect that all arcs of g are given a weight.
    VerifyWeightsAreOk $g

    # And the start node has to belong to the graph too, of course.
    if {![$g node exists $node]} {
	return -code error "node \"$node\" does not exist in graph \"$g\""
    }

    # TODO: Quick bailout for special cases (no arcs).

    # Transient and other data structures for the core algorithm.
    set pending [::struct::prioqueue -dictionary DijkstraQueue]
    array set distance {} ; # array: node -> distance to 'n'
    array set previous {} ; # array: node -> parent in shortest path to 'n'.
    array set visited  {} ; # array: node -> bool, true when node processed

    # Initialize the data structures.
    foreach n [$g nodes] {
	set distance($n) Inf
	set previous($n) undefined
	set  visited($n) 0
    }

    # Compute the distances ...
    $pending put $node 0
    set distance($node) 0
    set previous($node) none

    while {[$pending size]} {
	set current [$pending get]
	set visited($current) 1

	# Traversal to neighbours according to the chosen mode.
	if {$arcTraversal eq "undirected"} {
	    set arcNeighbours [$g arcs -adj $current]
	} else {
	    set arcNeighbours [$g arcs -out $current]
	}

	# Compute distances, record newly discovered nodes, minimize
	# distances for nodes reachable through multiple paths.
	foreach arcNeighbour $arcNeighbours {
	    set cost      [$g arc getweight $arcNeighbour]
	    set neighbour [$g node opposite $current $arcNeighbour]
	    set delta     [expr {$distance($current) + $cost}]

	    if {
		($distance($neighbour) eq "Inf") ||
		($delta < $distance($neighbour))
	    } {
		# First path, or better path to the node folund,
		# update our records.

		set distance($neighbour) $delta
		set previous($neighbour) $current
		if {!$visited($neighbour)} {
		    $pending put $neighbour $delta
		}
	    }
	}
    }

    $pending destroy

    # Now generate the result based on the chosen format.
    if {$resultFormat eq "distances"} {
	return [array get distance]
    } else {
	array set listofprevious {}
	foreach n [$g nodes] {
	    set current $n
	    while {1} {
		if {$current eq "undefined"} break
		if {$current eq $node} {
		    lappend listofprevious($n) $current
		    break
		}
		if {$current ne $n} {
		    lappend listofprevious($n) $current
		}
		set current $previous($current)
	    }
	}
	return [array get listofprevious]
    }
}

# This convenience command is a wrapper around dijkstra's algorithm to
# find the (un)directed distance between two nodes in the graph G.

proc ::struct::graph::op::distance {g origin destination args} {
    if {![$g node exists $origin]} {
	return -code error "node \"$origin\" does not exist in graph \"$g\""
    }
    if {![$g node exists $destination]} {
	return -code error "node \"$destination\" does not exist in graph \"$g\""
    }

    set arcTraversal undirected

    # Process options to override the defaults, if any.
    foreach {option param} $args {
	switch -exact -- $option {
	    -arcmode {
		switch -exact -- $param {
		    directed -
		    undirected {
			set arcTraversal $param
		    }
		    default {
			return -code error "Bad value for -arcmode, expected one of \"directed\" or \"undirected\""
		    }
		}
	    }
	    default {		
		return -code error "Bad option \"$option\", expected \"-arcmode\""
	    }
	}
    }

    # Quick bailout for special case: the distance from a node to
    # itself is zero

    if {$origin eq $destination} {
	return 0
    }

    # Compute all distances, then pick and return the one we are
    # interested in.
    array set distance [dijkstra $g $origin -outputformat distances -arcmode $arcTraversal]
    return $distance($destination)
}

# This convenience command is a wrapper around dijkstra's algorithm to
# find the (un)directed eccentricity of the node N in the graph G. The
# eccentricity is the maximal distance to any other node in the graph.

proc ::struct::graph::op::eccentricity {g node args} {
    if {![$g node exists $node]} {
	return -code error "node \"$node\" does not exist in graph \"$g\""
    }

    set arcTraversal undirected

    # Process options to override the defaults, if any.
    foreach {option param} $args {
	switch -exact -- $option {
	    -arcmode {
		switch -exact -- $param {
		    directed -
		    undirected {
			set arcTraversal $param
		    }
		    default {
			return -code error "Bad value for -arcmode, expected one of \"directed\" or \"undirected\""
		    }
		}
	    }
	    default {		
		return -code error "Bad option \"$option\", expected \"-arcmode\""
	    }
	}
    }

    # Compute all distances, then pick out the max

    set ecc 0
    foreach {n distance} [dijkstra $g $node -outputformat distances -arcmode $arcTraversal] {
	if {$distance eq "Inf"} { return Inf }
	if {$distance > $ecc} { set ecc $distance }
    }

    return $ecc
}

# This convenience command is a wrapper around eccentricity to find
# the (un)directed radius of the graph G. The radius is the minimal
# eccentricity over all nodes in the graph.

proc ::struct::graph::op::radius {g args} {
    return [lindex [RD $g $args] 0]
}

# This convenience command is a wrapper around eccentricity to find
# the (un)directed diameter of the graph G. The diameter is the
# maximal eccentricity over all nodes in the graph.

proc ::struct::graph::op::diameter {g args} {
    return [lindex [RD $g $args] 1]
}

proc ::struct::graph::op::RD {g options} {
    set arcTraversal undirected

    # Process options to override the defaults, if any.
    foreach {option param} $options {
	switch -exact -- $option {
	    -arcmode {
		switch -exact -- $param {
		    directed -
		    undirected {
			set arcTraversal $param
		    }
		    default {
			return -code error "Bad value for -arcmode, expected one of \"directed\" or \"undirected\""
		    }
		}
	    }
	    default {		
		return -code error "Bad option \"$option\", expected \"-arcmode\""
	    }
	}
    }

    set radius   Inf
    set diameter 0
    foreach n [$g nodes] {
	set e [eccentricity $g $n -arcmode $arcTraversal]
	#puts "$n ==> ($e)"
	if {($e eq "Inf") || ($e > $diameter)} {
	    set diameter $e
	}
	if {($radius eq "Inf") || ($e < $radius)} {
	    set radius $e
	}
    }

    return [list $radius $diameter]
}

#
## place holder for operations to come
#

# ### ### ### ######### ######### #########
## Internal helpers

proc ::struct::graph::op::Min {first second} {
    if {$first > $second} {
	return $second
    } else {
	return $first
    }
}

# This method verifies that every arc on the graph has a weight
# assigned to it. This is required for some algorithms.
proc ::struct::graph::op::VerifyWeightsAreOk {g} {
    if {![llength [$g arc getunweighted]]} return
    return -code error "Operation invalid for graph with unweighted arcs."
}

# ### ### ### ######### ######### #########
## Ready

namespace eval ::struct::graph::op {
    #namespace export ...
}

package provide struct::graph::op 0.9
