# domtree.tcl --
#
#	Extension to tree (mega)widget to support display of a DOM hierarchy.
#
#	This widget both generates and reacts to DOM Events.
#
# This package features ordered and non-unique directories and items.
# Paths are managed as live links into a DOM hierarchy.
#
# Copyright (c) 1999-2003 Zveno Pty Ltd
# http://www.zveno.com/
#
# Zveno makes this software and all associated data and documentation
# ('Software') available free of charge for any purpose.
# Copies may be made of this software but all of this notice must be included
# on any copy.
#
# The software was developed for research purposes only and Zveno does not
# warrant that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying this software.
#
# $Id: domtree.tcl,v 1.4 2003/01/09 01:12:00 balls Exp $

package provide domtree 2.5

# We need BWidgets

package require BWidget 1.4

# We need the DOM
# V2.0 gives us Level 2 event model
# V2.1 gives us libxml2

package require dom 2.5

namespace eval domtree {
    Tree::use

    Widget::bwinclude domtree Tree .tree

    Widget::declare domtree {
        {-rootnode         String  ""        0}
	{-showlength       Int     20        0}
	{-showtextcontent  Boolean 0         0}
	{-showelementid    Boolean 0         0}
    }

    proc ::domtree { path args } { return [eval domtree::create $path $args] }
    proc use {} {}

    variable eventTypeMap
    array set eventTypeMap {
	ButtonPress	mousedown
	ButtonRelease	mouseup
	Enter		mouseover
	Leave		mouseout
	Motion		mousemove
	FocusIn		DOMFocusIn
	FocusOut	DOMFocusOut
    }
}

# domtree::create --
#
#	Create a DOM Tree widget
#
# Arguments:
#	path	widget path
#	args	configuration options
#
# Results:
#	Tree widget created

proc domtree::create {path args} {
    array set maps [list Tree {} :cmd {} .tree {}]
    array set maps [Widget::parseArgs domtree $args]

    eval frame $path $maps(:cmd) -bd 0 -relief flat -takefocus 0 \
	    -class domtree -highlightthickness 0

    Widget::initFromODB domtree $path $maps(Tree)

    bindtags $path [list $path Bwdomtree [winfo toplevel $path] all]

    set tree [eval ::Tree::create $path.tree $maps(.tree) \
	    -opencmd [list [namespace code [list _node_open $path]]] \
	    -selectcommand [list [namespace code [list _select_node $path]]]]

    $tree configure -xscrollcommand [Widget::cget $path -xscrollcommand] \
	-yscrollcommand [Widget::cget $path -yscrollcommand]

    # Set various bindings to generate DOM events

    foreach event {ButtonRelease ButtonPress Enter Leave Motion} {
	$path.tree bindImage <$event> [namespace code [list _node_mouse_event $event {} $path]]
	$path.tree bindText <$event> [namespace code [list _node_mouse_event $event {} $path]]
	foreach modifier {Control Shift Alt Meta Double} {
	    $path.tree bindImage <$modifier-$event> [namespace code [list _node_mouse_event $event $modifier $path]]
	    $path.tree bindText <$modifier-$event> [namespace code [list _node_mouse_event $event $modifier $path]]
	}
    }

    grid $tree -row 0 -column 0 -sticky news
    grid rowconfigure $path 0 -weight 1
    grid columnconfigure $path 0 -weight 1

    $path configure -background [Widget::cget $path -background]
	    
    rename $path ::$path:cmd
    proc ::$path { cmd args } "return \[eval domtree::\$cmd $path \$args\]"

    set root [Widget::getMegawidgetOption $path -rootnode]
    if {[string length $root]} {
	_add_node $path root $root
    }

    return $path
}

# domtree::cget --
#
#	Implements the cget method
#
# Arguments:
#	path	widget path
#	option	configuration option
#
# Results:
#	Returns value of option

proc domtree::cget {path option} {
    return [Widget::getoption $path $option]
}

# domtree::configure --
#
#	Implements the configure method
#
# Arguments:
#	path	widget path
#	args	configuration options
#
# Results:
#	Sets value of options

proc domtree::configure {path args} {
    if {[catch {eval configure:dbg [list $path] $args} msg]} {
	puts stderr "domtree::configure incurred error\n$msg"
    } else {
	#puts stderr [list domtree::configure ran OK]
    }
}

proc domtree::configure:dbg {path args} {
    set res [Widget::configure $path $args]

    set rn [Widget::hasChanged $path -rootnode root]
    if {$rn} {

	eval Tree::delete $path.tree [Tree::nodes $path.tree root]
	# Remove event listeners from previous DOM tree

	if {[llength $root]} {
	    #puts stderr [list domtree::configure root $root]
	    set docel [dom::document cget $root -documentElement]
	    if {[string length $docel]} {
		_refresh $path $root
		_add_node $path root $root

		# Listen for UI events
		dom::node addEventListener $docel DOMActivate [namespace code [list _node_selected $path]] -usecapture 1

		# Listen for mutation events
		dom::node addEventListener $docel DOMSubtreeModified [namespace code [list _node_tree_modified $path]] -usecapture 1
		dom::node addEventListener $docel DOMNodeInserted [namespace code [list _node_inserted $path]] -usecapture 1
		dom::node addEventListener $docel DOMNodeRemoved [namespace code [list _node_removed $path]] -usecapture 1
		dom::node addEventListener $docel DOMCharacterDataModified [namespace code [list _node_data_modified $path]] -usecapture 1
		dom::node addEventListener $docel DOMAttrModified [namespace code [list _node_attr_modified $path]] -usecapture 1
		dom::node addEventListener $docel DOMAttrRemoved [namespace code [list _node_attr_removed $path]] -usecapture 1
	    }
	}
    }

    if {!$rn && [Widget::hasChanged $path -showtextcontent showtext]} {
	_refresh_all $path root
    }

    return $res
}

# domtree::refresh --
#
#	Updates the Tree display with the value of a node
#
# Arguments:
#	path	widget path
#	node	DOM node
#
# Results:
#	May change node display

proc domtree::refresh {path node} {
    _refresh $path $node
    return {}
}

# domtree::xview --
#
#	Implement xview method
#
# Arguments:
#	path	widget path
#	args	additional arguments
#
# Results:
#	Depends on Tree xview method

proc domtree::xview {path args} {
    eval $path.tree xview $args
}

# domtree::yview --
#
#	Implement yview method
#
# Arguments:
#	path	widget path
#	args	additional arguments
#
# Results:
#	Depends on Tree yview method

proc domtree::yview {path args} {
    eval $path.tree yview $args
}

# domtree::find --
#
#	Find DOM node at given location
#
# Arguments:
#	path	widget path
#	findInfo	location
#	confine
#
# Results:
#	DOM node at location

proc domtree::find {path findInfo {confine {}}} {
    set tnode [$path.tree find $findInfo $confine]
    return [_tree_to_dom $tnode]
}

# domtree::_dom_to_tree --
#
#	Map a DOM node to a tree node
#
# Arguments:
#	node	DOM node id
#
# Results:
#	A string suitable for use as a tree node path

proc domtree::_dom_to_tree node {
    if {[catch {dom::document cget $node -documentElement} docel]} {
	return $node
    } else {
	return root
    }
}

# domtree::_tree_to_dom --
#
#	Map a tree node to a DOM node
#
# Arguments:
#	node	Tree node path
#
# Results:
#	A string suitable for use as a DOM node id

proc domtree::_tree_to_dom node {
    if {[string compare $node "root"]} {
	return $node
    } else {
	# Really want to return the document node,
	# but don't know it here
	return {}
    }
}

# domtree::_open_ancestors --
#
#	Make sure that all ancestors of the given node are open.
#	Don't use "opentree", since that will recursively open
#	all children.
#
# Arguments:
#	path	widget path
#	node	tree node
#
# Results:
#	All ancestors of the node are opened

proc domtree::_open_ancestors {path node} {
    while {[string compare $node root]} {
	#$path.tree opentree $node
	if {[namespace eval ::Tree [list Widget::getoption $path.tree.$node -open]] != 1} {
	    namespace eval ::Tree [list Widget::setoption $path.tree.$node -open 1]
	    uplevel #0 [namespace eval ::Tree [list Widget::getoption $path.tree -opencmd]] [list $node]
	}
	set node [$path.tree parent $node]
    }
}

# Procedures to implement display

# domtree::_refresh --
#
#	Configure node with appropriate images, labels, etc
#
# Arguments:
#	path	widget path
#	node	DOM node
#	args	additional options
#
# Results:
#	Tree node may have image or label changed

proc domtree::_refresh {path node args} {

    #puts stderr [list domtree::_refresh $path $node]

    switch [set nodetype [::dom::node cget $node -nodeType]] {
	document -
	documentFragment -
	element {
	    set label [dom::node cget $node -nodeName]
	    set icon ::domtree::element

	    if {![string compare $nodetype element]} {

		# ID attribute display
		if {[Widget::getoption $path -showelementid]} {
		    array set attributes [array get [::dom::node cget $node -attributes]]
		    if {[catch {
			append label "    (id $attributes(id))"
		    }] && [catch {
			append label "    (ID $attributes(ID))"
		    }]} {}
		}

		if {[Widget::getoption $path -showtextcontent]} {
		    # Text content display
		    set temp [_refresh_text_content_display_find_text $node [Widget::getoption $path -showlength]]
		    if {[string length $temp]} {
			append label "    \[ [_refresh_string_trim $temp [Widget::getoption $path -showlength]] \]"
		    }
		}

	    }

	    if {![string length [dom::node parent $node]]} {
		# Root node is special
		return {}
	    }

	}
	textNode {
	    array set opts [list -label [dom::node cget $node -nodeValue]]
	    array set opts $args
	    set label [_refresh_string_trim [string trim $opts(-label)] [Widget::getoption $path -showlength]]
	    set icon ::domtree::textNode

	    # Also do the ancestors
	    foreach ancestor [lrange [lreplace [::dom::node path $node] end end] 1 end] {
		_refresh $path $ancestor
	    }
	}
	processingInstruction {
	    set label [string trim [dom::node cget $node -nodeName]]
	    set icon ::domtree::PI
	}
	docType {
	    set label {}
	    set icon ::domtree::DocType
	}
	comment {
	    set label [_refresh_string_trim [string trim [::dom::node cget $node -nodeValue]] [Widget::getoption $path -showlength]]
	    set icon ::domtree::Comment
	}
	entityReference {
	    set label [::dom::node cget $node -nodeName]
	    set icon ::domtree::EntityReference
	}
	default {
	    set label $nodetype
	    set icon ::domtree::other
	}
    }

    catch {
	$path.tree itemconfigure [_dom_to_tree $node] -image $icon
	$path.tree itemconfigure [_dom_to_tree $node] -text $label
    }

    return {}
}

# domtree::_refresh_text_content_display_find_text --
#
#	Searches given element for text.
#	In future could use XPath - just get the string value
#	of the node.
#
# Arguments:
#	node	DOM element node to search
#	len	amount of text to return
#
# Results:
#	Returns string

proc domtree::_refresh_text_content_display_find_text {node len} {
    switch -- $len {
	0 {
	    return {}
	}
	default {
	    set text {}
	    foreach child [::dom::node children $node] {
		switch [::dom::node cget $child -nodeType] {
		    document -
		    documentFragment -
		    element {
			append text \
				[_refresh_text_content_display_find_text $child [expr $len - [string length $text]]]
		    }
		    textNode {
			append text [string range \
				[::dom::node cget $child -nodeValue] \
				0 [expr $len - [string length $text]] \
			]
		    }
		    default {
			# Nothing to do
		    }
		}
		if {[string length $text] >= $len} {
		    return $text
		}
	    }

	    return $text

	}
    }

    return {}
}

# domtree::_refresh_all --
#
#	Updates display of all tree nodes
#
# Arguments:
#	path	widget pathname
#	node	Tree node
#
# Results:
#	Returns empty string

proc domtree::_refresh_all {path node} {
    foreach child [$path.tree nodes $node] {
	_refresh $path [_tree_to_dom $child]
	_refresh_all $path $child
    }

    return {}
}

# domtree::_refresh_string_trim --
#
#	Massage text for display
#
# Arguments:
#	text	text string
#	max	maximum length for string
#
# Results:
#	Returns string

proc domtree::_refresh_string_trim {text max} {
    if {[string length $text] > $max} {
	set text [string range $text 0 [expr $max - 3]]...
    }
    if {[info tclversion] >= 8.1} {
	set dot \u2022
    } else {
	set dot { }
    }
    regsub -all [format {[%s%s%s%s]+} \n \r { } \t] $text $dot text
    return $text
}

# domtree::_node_selected --
#
#	A node has been selected.
#
#	This is invoked via a DOM event.
#
# Arguments:
#	path	widget path
#	evid	event node

proc domtree::_node_selected {path evid} {

    set domnode [dom::event cget $evid -target]

    # Temporarily remove the -selectcommand callback
    # to avoid an infinite loop (continually posting DOM click events)
    set cmd [$path.tree cget -selectcommand]
    $path.tree configure -selectcommand {}

    $path.tree selection set [_dom_to_tree $domnode]

    $path.tree configure -selectcommand $cmd

    return {}
}

# domtree::_select_node --
#
#	A tree node has been selected.
#
# Arguments:
#	path	widget path
#	tree	tree path
#	tnode	tree node

proc domtree::_select_node {path tree tnode} {

    dom::event postMouseEvent [_tree_to_dom $tnode] click -detail 1

    return {}
}

# domtree::_node_mouse_event --
#
#	Generate DOM Mouse Event
#
# Arguments:
#	event	event type
#	mod	modifier
#	path	widget path
#	tnode	tree node
#
# Results:
#	Event synthesized for DOM

proc domtree::_node_mouse_event {event mod path tnode} {
    variable eventTypeMap

    set type $event
    catch {set type $eventTypeMap($event)}

    set evid [dom::document createEvent [_tree_to_dom $tnode] $type]
    dom::event initMouseEvent $evid $type 1 1 {} 0 0 0 0 0 \
	    [expr {$mod == "Control"}] \
	    [expr {$mod == "Alt"}] \
	    [expr {$mod == "Shift"}] \
	    [expr {$mod == "Meta"}] \
	    0 {}
    dom::node dispatchEvent [_tree_to_dom $tnode] $evid
    dom::destroy $evid

    # ButtonRelease events also generate DOMActivate events

    if {![string compare $event "ButtonRelease"]} {
	set detail 1
	if {![string compare $mod "Double"]} {
	    set detail 2
	}
	dom::event postUIEvent [_tree_to_dom $tnode] DOMActivate -detail $detail
    }

    return {}
}

# domtree::_node_ui_event --
#
#	Generate DOM UI Event
#
# Arguments:
#	event	event type
#	path	widget path
#	tnode	tree node
#
# Results:
#	Event synthesized for DOM

proc domtree::_node_ui_event {event path tnode} {
    variable eventTypeMap

    set type $event
    catch {set type $eventTypeMap($event)}
    dom::event postUIEvent [_tree_to_dom $tnode] $type

    return {}
}

# domtree::_add_node --
#
#	Recurse DOM structure, inserting tree nodes as we go.
#
#	Trouble is, this is very expensive when beyond a certain
#	depth we're not going to see the nodes anyway.
#	Instead, build the tree as the user opens nodes.
#	This will spread the cost of building the tree, and the
#	user may never open certain nodes anyway.
#
# Arguments:
#	path	widget path
#	tnode	tree node to add children to
#	dnode	DOM node corresponding to tree path above
#
# Results:
#	Nodes added to tree

proc domtree::_add_node {path tnode dnode} {

    if {[string compare $tnode "root"]} {

	# If the grandparent is open, then do add the items
	# This is so that the open/close toggle is added to the parent

	set parent [$path.tree parent $tnode]

	if {[string compare $parent "root"]} {
	    set grandParent [$path.tree parent $parent]
	    if {[string compare $grandParent "root"]} {

		# We are beyond the second level from the root

		array set ParentInfo [$path.tree itemcget $parent -data]
		array set GrandParentInfo [$path.tree itemcget $grandParent -data]
		if {[info exists ParentInfo(nodeOpen)]} {
		    # Parent is open, so add ourself
		} elseif {[info exists GrandParentInfo(nodeOpen)]} {
		    # Grandparent is open, so add ourself
		} else {
		    return {}
		}

	    } else {
		# We are two levels down from the root.  Go ahead and build,
		# but don't automatically mark this node as being open.
	    }

	} else {
	    # We are a child of the root, so go ahead and build here
	    _set_client_data $path $tnode nodeOpen 1
	}

    } else {
	# We are the root node, so definitely go ahead and build children
    }

    foreach child [::dom::node children $dnode] {
	# Due to lazy population of the tree, the node may already exist
	catch {$path.tree insert end $tnode [_dom_to_tree $child]}
	_refresh $path $child

	_add_node $path [_dom_to_tree $child] $child
    }

    return {}
}

# domtree::_set_client_data --
#
#	Manage data for tree nodes
#
# Arguments:
#	path	widget path
#	node	tree node
#	field	field name
#	value	value for field
#
# Results:
#	Item's configuration changed

proc domtree::_set_client_data {path node field value} {
    array set nodeinfo [$path.tree itemcget $node -data]
    set nodeinfo($field) $value
    $path.tree itemconfigure $node -data [array get nodeinfo]
}

# domtree::_unset_client_data --
#
#	Manage data for tree nodes
#
# Arguments:
#	path	widget path
#	node	tree node
#	field	field name to unset
#
# Results:
#	Item's configuration changed

proc domtree::_unset_client_data {path node field} {
    array set nodeinfo [$path.tree itemcget $node -data]
    catch {unset nodeinfo($field)}
    $path.tree itemconfigure $node -data [array get nodeinfo]
}

# domtree::_node_open --
#
#	Invoked when a tree item is opened.
#	If we haven't done this node's children yet, 
#	do it now.  This lazily populates the tree.
#
# Arguments:
#	path	widget path
#	node	tree node
#
# Results:
#	Tree nodes may be added

proc domtree::_node_open {path node} {

    _set_client_data $path $node nodeOpen 1
    _add_node $path $node [_tree_to_dom $node]

    return {}
}

# domtree::_node_tree_modified --
#
#	Invoked when the node's subtree has changed.
#	Could be because a child node has been removed.
#
#	Refresh the
#	display of the node, since if textual content
#	is enabled the node's string value may have
#	changed.
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Tree nodes inserted or removed

proc domtree::_node_tree_modified {path evid} {

    set target [dom::event cget $evid -target]
    set children [dom::node children $target]
    set branch [Tree::nodes $path.tree [_dom_to_tree $target]]
    if {[llength $children] < [llength $branch]} {
	for {set idx 0} {$idx < [llength $branch]} {incr idx} {
	    if {![string length [lindex $children $idx]] || \
		    [_dom_to_tree [lindex $children $idx]] != [lindex $branch $idx]} {
		$path.tree delete [lindex $branch $idx]
		break
	    }
	}
    }

    _refresh $path [dom::event cget $evid -currentNode]

    return {}
}

# domtree::_node_inserted --
#
#	A node has been inserted.
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Insert tree node

proc domtree::_node_inserted {path evid} {

    # Find where the node was inserted into the child list
    set newnode [dom::event cget $evid -target]
    set parent [dom::node parent $newnode]
    set children [dom::node children $parent]
    set idx [lsearch $children $newnode]

    # Get old tree info
    set tparent [_dom_to_tree $parent]
    set branch [Tree::nodes $path.tree $tparent]

    if {$idx > [llength $branch]} {
	# Append the new node to the branch
	$path.tree insert end $tparent [_dom_to_tree $newnode]
    } else {
	# Insert the new node into the branch
	$path.tree insert $idx $tparent [_dom_to_tree $newnode]
    }

    _refresh $path $newnode
    _add_node $path [_dom_to_tree $newnode] $newnode

    return {}
}

# domtree::_node_removed --
#
#	A node has been removed.
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Remove tree node

proc domtree::_node_removed {path evid} {

    set oldnode [dom::event cget $evid -target]
    Tree::delete $path.tree [_dom_to_tree $oldnode]

    return {}
}

# domtree::_node_data_modified --
#
#	Character data has changed
#
# Arguments:
#	path	widget path
#	evid	DOM L2 event node
#
# Results:
#	Tree display updated

proc domtree::_node_data_modified {path evid} {
    _refresh $path [dom::event cget $evid -target] \
	    -label [dom::event cget $evid -newValue]
    return {}
}

# domtree::_node_attr_modified --
#
#	Attribute value modified
#
# Arguments:
#	path	widget path
#	evid	DOM L2 event node
#
# Results:
#	Display updated

proc domtree::_node_attr_modified {path evid} {
    _refresh $path [dom::event cget $evid -target]
    return {}
}

# domtree::_node_attr_removed --
#
#	Attribute removed
#
# Arguments:
#	path	widget path
#	evid	DOM L2 event node
#
# Results:
#	Display updated

proc domtree::_node_attr_removed {path evid} {
    _refresh $path [dom::event cget $evid -target]
    return {}
}

### Image data

image create photo ::domtree::element -data {R0lGODlhEAAQANX/AP7///3//vv+/vn+/fj+/ff9/fb9/fL8/PL6+e/8++78+u37+uz7+ur7
+ef6+d349tv49df18tT289P288718sPz773y7bPw6qzo4qTt5o3o4Ivn4IHb0nTj2XPj2Wrh
107bzzDVxxnQwRecjBCtmRCtmA+AdA6Hew21oQ2NgQq+rQqjlQpoXApmWgm/rQeekAXMuwXG
tQTMu////wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH/C0FET0JFOklS
MS4wAt7tACH5BAEAADMALAAAAAAQABAAAAaRwJlwCBgajZCKBeE4GhuXCCIinQycs+iUA8N4
HgHnZyuTcUoZAyAwIAgA5LKMNGIckksHgiOXoRAnElpUCCB8MlKEGolcGBsiMIwyGCFxHBMd
LpZxZQMPnDJ7fSVConIlKocyJSNChqcjKzEwqyMmQo+0rCYpLyUmwC1CmL/BLBQZIy3KQp7J
yy0KAgUJCwcCQQA7
}
image create photo ::domtree::textNode -data {R0lGODlhEAAOAJH/AP///39/fwAAAP///yH/C0FET0JFOklSMS4wAt7tACH5BAEAAAMALAAA
AAAQAA4AAAI0nIUpxi0AIWoOhAveouPFnHDPhV1CQHmfhFYkmbWMup6p9QbxvbJ3rrNVejuH
4ihjAF+GAgA7
}
image create photo ::domtree::PI -data {R0lGODdhEAAOAPEAALLA3AAAAAAA/////ywAAAAAEAAOAAACL4yPoBvi78Jio9oqJwh3oG90
DfSEF9dhKIioGFmqR4phFL3eaa6g+6ETaTYsw6IAADs=
}
image create photo ::domtree::DocType -data {R0lGODlhEAAQAKL/APfQ0MmZmYJfX2YAAEoBAf///wAAAAAAACH/C0FET0JFOklSMS4wAt7t
ACH5BAEAAAUALAAAAAAQABAAAAM7WDKyUjBGB8AaUl4RQFhZNIxM8D2hQJBNgIUKOZ5wsbJu
fcmNfrM1iEoWnIyKqRGqWHoFd0sdEOmAJAAAOw==
}
image create photo ::domtree::Comment -data {R0lGODlhEAAQAKL/AP///8fHx7CwsJ6enpycnHp6egAAAP///yH/C0FET0JFOklSMS4wAt7t
ACH5BAEAAAcALAAAAAAQABAAAANDeLrcazBGZ4C917CKTegPBnigwmVDJ5iikWbEelpuV8hi
bhTEMY8vGo+VE8Yeswhv1eDsCkuHb8Qj9KSRo9RniDG3CQA7
}
image create photo ::domtree::EntityReference -data {R0lGODlhEAAQALP/AP7+/vfQ0NOsrMmZmci5uYMwMIJfX2YAAEoBAf///wAAAAAAAAAAAAAA
AAAAAAAAACH/C0FET0JFOklSMS4wAt7tACH5BAEAAAkALAAAAAAQABAAAARPMEl5jAlhzJ2O
r1WmbV+CfEdGVtzJTp4xwq90XuvBmZVAeTtbxVAK0nQTYg11mKUGyJJL8ykQOiwAr1nsyDau
mgn3+8xsFwuzeUYopR5NBAA7
}
image create photo ::domtree::other -data {R0lGODlhEAAOAKL/AP///39/fxAQEAAAAP///wAAAAAAAAAAACH/C0FET0JFOklSMS4wAt7t
ACH5BAEAAAQALAAAAAAQAA4AAAM4SDSj/m8E0ByrdtI1wI4aFV6ZR5kiJpmrJ6kj+pbuGMCs
fIO1O/MdhmcHeUkCSGJEIriQIByoIwEAOw==
}

