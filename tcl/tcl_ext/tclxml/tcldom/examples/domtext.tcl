# domtext.tcl --
#
#	Megawidget to display a DOM document in a Text widget.
#
#	This widget both generates and reacts to DOM Events.
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
# $Id: domtext.tcl,v 1.4 2003/01/17 23:43:29 balls Exp $

package provide domtext 2.5

# We need BWidgets

package require BWidget 1.4

# We need the DOM
# V2.0 gives us Level 2 Events

package require dom 2.5

# Configuration options:
#
#	-elementbgcolorlist {colour1 colour2 ...}
#		Specifies a list of colours to cycle through for
#		backgrounds of sucessive element content.
#
#	-showtag text|tab|<empty>
#		"text" denotes that start and end tags are shown
#		as their XML text.  "tab" denotes that start and
#		end tags are shown as an image.  Empty value
#		denotes that start and end tags are not shown.

namespace eval domtext {
    Widget::tkinclude domtext text .text \
	    remove {-command -state}

    Widget::declare domtext {
	{-highlightcolor	String	"#d9ffff"	0}
	{-rootnode		String	""		0}
	{-state			String	"normal"	0}
	{-tagcolor		String	"#18605a"	0}
	{-commentcolor		String	"#660f91"	0}
	{-entityrefcolor	String	"#0080c0"	0}
	{-elementbgcolorlist	String	""		0}
	{-showxmldecl		Boolean	1		0}
	{-showdoctypedecl	Boolean	1		0}
	{-showtag		String	"text"		0}
    }

    proc ::domtext { path args } { return [eval domtext::create $path $args] }
    proc use {} {}

    # Define bindings for domtext widget class

    # Certain mouse event bindings for the Text widget class must be overridden

    bind domtext <Button-1> [namespace code [list _tkevent_override %W %x %y]]
    bind domtext <Double-Button-1> [namespace code [list _tkevent_override %W %x %y]]

    # All of these bindings for the Text widget class cause characters
    # to be inserted or deleted.  These must be caught and prevented if the
    # characters are part of markup, otherwise the node value must be
    # updated
    # TODO: update with all bindings for Text widget

    foreach spec {
	<Meta-Key-d> <Meta-Key-Delete> <Meta-Key-BackSpace>
	<Control-Key-h> <Control-Key-t> <Control-Key-k> <Control-Key-d>
	<Control-Key-i> <Key>
	<<Cut>> <<Paste>> <<PasteSelection>> <<Clear>>
	<Key-BackSpace> <Key-Delete> <Key-Return>
    } {
	bind domtext $spec [list domtext::_tkevent_filter_$spec %W %A]
    }
    foreach spec {
	<Key-Up> <Key-Down> <Key-Left> <Key-Right>
    } {
	bind domtext $spec [list domtext::_key_select %W $spec]
    }
    foreach spec {
	<Meta-Key> <Control-Key>
    } {
	bind domtext $spec {# Do nothing - allow the normal Text class binding to take effect}
    }

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

# domtext::create --
#
#	Widget class creation command
#
# Arguments:
#	path	widget path
#	args	configuration options
#
# Results:
#	Widget created, returns path

proc domtext::create {path args} {
    upvar #0 [namespace current]::$path data
    array set maps [list Text {} :text {} .text {}]

    eval frame $path $maps(:text) -bd 0 -relief flat -takefocus 0 \
	    -class domtext -highlightthickness 0

    Widget::initFromODB domtext $path $maps(Text)

    # Setup event bindings for generating DOM events
    bindtags $path [list $path Bwdomtext [winfo toplevel $path] all]

    set text [eval text $path.text $maps(.text) \
	    -state [Widget::getMegawidgetOption $path -state] -wrap none \
	    -takefocus 1]
    $text tag configure starttab -elide 1
    $text tag configure endtab -elide 1
    $text tag configure xmldecl -elide 1
    $text tag configure doctypedecl -elide 1

    bindtags $path [list $path domtext [winfo toplevel $path] all]

    grid $text -sticky news
    grid rowconfigure $path 0 -weight 1
    grid columnconfigure $path 0 -weight 1

    # Certain class bindings must be overridden
    bindtags $text [list $path domtext [winfo class $text] [winfo toplevel $path] all]

    rename $path ::$path:cmd
    proc ::$path { cmd args } "return \[eval domtext::\$cmd $path \$args\]"

    set root [Widget::getMegawidgetOption $path -rootnode]
    if {[string length $root]} {
	_refresh $path $root
    }

    set data(insert) end
    set data(nextElemBgColor) 0

    configure $path \
	    -showtag [Widget::getMegawidgetOption $path -showtag] \
	    -showxmldecl [Widget::getMegawidgetOption $path -showxmldecl] \
	    -showdoctypedecl [Widget::getMegawidgetOption $path -showdoctypedecl]

    return $path
}

# domtext::cget --
#
#	Implements the cget method
#
# Arguments:
#	path	widget path
#	option	configuration option
#
# Results:
#	Returns value of option

proc domtext::cget {path option} {
    return [Widget::getoption $path $option]
}

# domtext::configure --
#
#	Implements the configure method
#
# Arguments:
#	path	widget path
#	args	configuration options
#
# Results:
#	Sets values of options

proc domtext::configure {path args} {
    upvar #0 [namespace current]::$path data

    set res [Widget::configure $path $args]

    set rn [Widget::hasChanged $path -rootnode root]
    if {$rn} {

	$path.text delete 1.0 end
	# Delete all marks and tags
	# This doesn't delete the standard marks and tags
	eval $path.text tag delete [$path.text tag names]
	eval $path.text mark unset [$path.text mark names]
	# Remove event listeners from previous DOM tree

	set data(insert) 1.0

	if {[string length $root]} {
	    set docel [dom::document cget $root -documentElement]

	    if {[string length $docel]} {
		# Listen for UI events
		dom::node addEventListener $root DOMActivate [namespace code [list _node_selected $path]] -usecapture 1

		# Listen for mutation events
		dom::node addEventListener $root DOMNodeInserted [namespace code [list _node_inserted $path]] -usecapture 1
		dom::node addEventListener $root DOMNodeRemoved [namespace code [list _node_removed $path]] -usecapture 1
		dom::node addEventListener $root DOMCharacterDataModified [namespace code [list _node_pcdata_modified $path]] -usecapture 1
		dom::node addEventListener $root DOMAttrModified [namespace code [list _node_attr_modified $path]] -usecapture 1
		dom::node addEventListener $root DOMAttrRemoved [namespace code [list _node_attr_removed $path]] -usecapture 1

		_refresh $path $root
	    }
	}
    }

    set tc [Widget::hasChanged $path -tagcolor tagcolor]
    set hc [Widget::hasChanged $path -highlightcolor hlcolor]
    set cc [Widget::hasChanged $path -commentcolor commcolor]
    set ec [Widget::hasChanged $path -entityrefcolor ercolor]
    set ebg [Widget::hasChanged $path -elementbgcolorlist ebgcolor]
    if {($rn && [string length $root]) || $tc} {
	$path.text tag configure tags -foreground $tagcolor
    }
    if {($rn && [string length $root]) || $cc} {
	$path.text tag configure comment -foreground $commcolor
    }
    if {($rn && [string length $root]) || $ec} {
	$path.text tag configure entityreference -foreground $ercolor
    }
    if {($rn && [string length $root]) || $hc} {
	$path.text tag configure highlight -background $hlcolor
    }
    if {($rn && [string length $root]) || $ebg} {
	set data(nextElemBgColor) 0
	_elementbg_setall $path $root
    }

    if {[Widget::hasChanged $path -showtag showtag]} {
	switch -- $showtag {
	    text {
		$path.text tag configure starttab -elide 1
		$path.text tag configure endtab -elide 1
		$path.text tag configure tags -elide 0
	    }
	    tab {
		$path.text tag configure tags -elide 1
		$path.text tag configure starttab -elide 0
		$path.text tag configure endtab -elide 0
	    }
	    {} {
		$path.text tag configure tags -elide 1
		$path.text tag configure starttab -elide 1
		$path.text tag configure endtab -elide 1
	    }
	    default {
		return -code error "invalid value \"$showtag\""
	    }
	}
    }

    if {[Widget::hasChanged $path -showxmldecl showxmldecl]} {
	$path.text tag configure xmldecl -elide [expr !$showxmldecl]
    }
    if {[Widget::hasChanged $path -showdoctypedecl showdoctypedecl]} {
	$path.text tag configure doctypedecl -elide [expr !$showdoctypedecl]
    }
    return $res
}

# domtext::xview --
#
#	Implements xview method
#
# Arguments:
#	path	widget path
#	args	additional arguments
#
# Results:
#	Depends on Text's xview method

proc domtext::xview {path args} {
    eval $path.text xview $args
}

# domtext::yview --
#
#	Implements yview method
#
# Arguments:
#	path	widget path
#	args	additional arguments
#
# Results:
#	Depends on Text's yview method

proc domtext::yview {path args} {
    eval $path.text yview $args
}

# domtext::_refresh --
#
#	Inserts serialized nodes into the Text widget,
#	while at the same time marking up the text to support
#	DOM-level editing functions.
#
#	This function is similar to the DOM package's
#	serialization feature.  The code started by being copied
#	from there.
#
#	Assumes that the widget is in normal state
#
# Arguments:
#	path	widget path
#	node	DOM node
#
# Results:
#	Text widget populated with serialized text.

proc domtext::_refresh {path node} {
    upvar #0 [namespace current]::$path data

    $path.text mark set $node $data(insert)
    $path.text mark gravity $node left

    set end $data(insert)

    # For all nodes we bind Tk events to be able to generate DOM events
    $path.text tag bind $node <1> [namespace code [list _tkevent_select $path $node %x %y]]
    $path.text tag bind $node <Double-1> [namespace code [list _tkevent_open $path $node]]

    $path.text tag configure $node -background [_elementbg_cycle $path]

    switch [::dom::node cget $node -nodeType] {
	document -
	documentFragment {

	    # Display the XML declaration
	    if {0} {
	    # OUCH!  Need an interface in the DOM package for this data
	    array set nodeInfo [set $node]
	    # XML Declaration attributes have a defined order, so can't use array directly
	    array set xmldecl $nodeInfo(document:xmldecl)
	    set xmldecllist [list version $xmldecl(version)]
	    catch {lappend xmldecllist standalone $xmldecl(standalone)}
	    catch {lappend xmldecllist encoding $xmldecl(encoding)}
	    $path.text insert $data(insert) "<?xml[dom::Serialize:attributeList $xmldecllist]?>\n" [list $node xmldecl]
	    set data(insert) [lindex [$path.text tag ranges $node] end]
	}
	    foreach childToken [::dom::node children $node] {
		set end [_refresh $path $childToken]
		set data(insert) $end
	    }

	    $path.text tag add $node $node $end
	    $path.text tag configure xmldecl -elide [expr ![Widget::cget $path -showxmldecl]]
	    $path.text tag raise xmldecl
	}

	element {

	    # Serialize the start tag
	    $path.text insert $data(insert) <[::dom::node cget $node -nodeName] [list tags tag:start:$node] [_serialize:attributeList [array get [::dom::node cget $node -attributes]]] [list tags attrs:$node] > [list tags tag:start:$node]

	    # Add the start tab icon
	    $path.text image create $data(insert) -image ::domtext::starttab -align center -name tab:start:$node
	    foreach t [list starttab tags tag:start:$node] {
		$path.text tag add $t tab:start:$node
	    }

	    set data(insert) [lindex [$path.text tag ranges tag:start:$node] end]

	    # Serialize the content
	    $path.text mark set content:$node $data(insert)
	    $path.text mark gravity content:$node left
	    foreach childToken [::dom::node children $node] {
		set end [_refresh $path $childToken]
		set data(insert) $end
	    }
	    $path.text tag add content:$node content:$node $end

	    # Serialize the end tag
	    $path.text insert $data(insert) </[::dom::node cget $node -nodeName]> [list tags tag:end:$node]
	    set end [lindex [$path.text tag ranges tag:end:$node] end]
	    # Add the end tab icon
	    $path.text image create $end -image ::domtext::endtab -align center -name tab:end:$node
	    foreach t [list endtab tags tag:end:$node] {
		$path.text tag add $t tab:end:$node
	    }
	    set end [lindex [$path.text tag ranges tag:end:$node] end]

	    set data(insert) $end
	    $path.text tag add $node $node $end

	    $path.text tag raise starttab
	    $path.text tag raise endtab
	    $path.text tag configure starttab -elide [expr {[Widget::cget $path -showtag] != "tab"}]
	    $path.text tag configure endtab -elide [expr {[Widget::cget $path -showtag] != "tab"}]

	}

	textNode {
	    set text [_encode [dom::node cget $node -nodeValue]]
	    if {[string length $text]} {
		$path.text insert $data(insert) $text $node
		set end [lindex [$path.text tag ranges $node] 1]
		set data(insert) $end
	    } else {
		set end $data(insert)
	    }
	}

	docType {
	    array set nodeInfo [set $node]
	    $path.text insert $data(insert) "<!DOCTYPE $nodeInfo(doctype:name)" [list $node doctypedecl]
	    set data(insert) [lindex [$path.text tag ranges $node] end]

	    if {[string length $nodeInfo(doctype:internaldtd)]} {
		$path.text insert $data(insert) " \[$nodeInfo(doctype:internaldtd)\]" [list $node doctypedecl]
		set data(insert) [lindex [$path.text tag ranges $node] end]
	    }

	    $path.text insert $data(insert) >\n [list $node doctypedecl]
	    set end [lindex [$path.text tag ranges $node] end]
	    set data(insert) $end
	    $path.text tag configure doctypedecl -elide [expr ![Widget::cget $path -showdoctypedecl]]
	    $path.text tag raise doctypedecl
	}

	comment {
	    set text [::dom::node cget $node -nodeValue]
	    $path.text insert $data(insert) <!-- [list comment markup $node] $text [list comment $node] --> [list comment markup $node]
	    set end [lindex [$path.text tag ranges $node] 1]
	    set data(insert) $end
	}

	entityReference {
	    set text [::dom::node cget $node -nodeName]
	    $path.text insert $data(insert) & [list entityreference markup $node] $text [list entityreference $node] \; [list entityreference markup $node]
	    set end [lindex [$path.text tag ranges $node] 1]
	    set data(insert) $end
	}

	processingInstruction {
	    set text [::dom::node cget $node -nodeValue]
	    if {[string length $text]} {
		set text " $text"
	    }
	    $path.text insert $data(insert) "<?[::dom::node cget $node -nodeName]$text?>" $node
	    set end [lindex [$path.text tag ranges $node] 1]
	    set data(insert) $end
	}

	default {
	    # Ignore it
	}

    }

    return $end
}

# domtext::_serialize:attributeList --
#
#	Produce textual representation of an attribute list.
#
#	NB. This is copied from TclDOM's domimpl.tcl,
#	but with the namespace handling removed.
#
# Arguments:
#	atlist	name/value list of attributes
#
# Results:
#	Returns string

proc domtext::_serialize:attributeList atlist {

    set result {}
    foreach {name value} $atlist {

	append result { } $name =

	# Handle special characters
	regsub -all & $value {\&amp;} value
	regsub -all < $value {\&lt;} value

	if {![string match *\"* $value]} {
	    append result \"$value\"
	} elseif {![string match *'* $value]} {
	    append result '$value'
	} else {
	    regsub -all \" $value {\&quot;} value
	    append result \"$value\"
	}

    }

    return $result
}

# domtext::_encode --
#
#	Protect XML special characters
#
#	NB. This is copied from TclDOM's domimpl.tcl.
#
# Arguments:
#	value	text
#
# Results:
#	Returns string

proc domtext::_encode value {
    array set Entity {
	$ $
	< &lt;
	> &gt;
	& &amp;
	\" &quot;
	' &apos;
    }

    regsub -all {([$<>&"'])} $value {$Entity(\1)} value

    return [subst -nocommand -nobackslash $value]
}

# domtext::_elementbg_setall --
#
#	Recurse node hierarchy setting element background color property
#
# Arguments:
#	path	widget path
#	node	DOM node
#
# Results:
#	Text widget tag configured

proc domtext::_elementbg_setall {path node} {

    $path.text tag configure $node -background [_elementbg_cycle $path]

    switch [dom::node cget $node -nodeType] {
	document -
	documentFragment -
	element {
	    foreach child [dom::node children $node] {
		_elementbg_setall $path $child
	    }
	}
	default {
	    # No more to do here
	}
    }

    return {}
}
proc domtext::_elementbg_cycle path {
    upvar #0 [namespace current]::$path data

    set list [Widget::cget $path -elementbgcolorlist]
    set colour [lindex $list $data(nextElemBgColor)]

    set data(nextElemBgColor) [expr [incr data(nextElemBgColor)] % [llength $$list]]

    return $colour
}

# domtext::_node_inserted --
#
#	React to addition of a node
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Display updated to reflect change to DOM structure

proc domtext::_node_inserted {path evid} {
    upvar #0 [namespace current]::$path data

    set node [dom::event cget $evid -target]

    # Remove parent's content and then render new content
    set parent [dom::node parent $node]
    set tags [$path.text tag ranges $parent]
    set start [lindex $tags 0]
    set end [lindex $tags end]
    if {[string length $start]} {
	$path.text delete $start $end
    } else {
	set start end
    }

    set data(insert) $start
    set end [_refresh $path $parent]

    # Restore grandparent element tags
    set parent [::dom::node parent $parent]
    while {[string length $parent]} {
	set ranges [$path.text tag ranges $parent]
	catch {eval [list $path.text] tag remove [list $parent] $ranges}
	catch {$path.text tag add $parent [lindex $ranges 0] [lindex $ranges end]}
	# Also do content tag for elements
	if {![string compare [::dom::node cget $parent -nodeType] "element"]} {
	    set ranges [$path.text tag ranges content:$parent]
	    catch {eval [list $path.text] tag remove [list $parent] $ranges}
	    catch {$path.text tag add content:$parent [lindex $ranges 0] [lindex $ranges end]}
	}

	set parent [::dom::node parent $parent]
    }

    return {}
}

# domtext::_node_removed --
#
#	React to removal of a node.
#	This is almost identical to node insertion,
#	except that we must get the parent from the event.
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Display updated to reflect change to DOM structure

proc domtext::_node_removed {path evid} {
    upvar #0 [namespace current]::selected$path selected

    set node [dom::event cget $evid -target]

    if {[info exists selected] && ![string compare $node $selected]} {
	unset selected
    }

    # Remove parent's content and then render new content
    set parent [dom::event cget $evid -relatedNode]
    set tags [$path.text tag ranges $parent]
    set start [lindex $tags 0]
    set end [lindex $tags end]
    if {[string length $start]} {
	$path.text delete $start $end
    } else {
	set start end
    }

    set data(insert) $start
    set end [_refresh $path $parent]

    # Restore grandparent element tags
    set parent [::dom::node parent $parent]
    while {[string length $parent]} {
	set ranges [$path.text tag ranges $parent]
	catch {eval [list $path.text] tag remove [list $parent] $ranges}
	catch {$path.text tag add $parent [lindex $ranges 0] [lindex $ranges end]}
	# Also do content tag for elements
	if {![string compare [::dom::node cget $parent -nodeType] "element"]} {
	    set ranges [$path.text tag ranges content:$parent]
	    catch {eval [list $path.text] tag remove [list $parent] $ranges}
	    catch {$path.text tag add content:$parent [lindex $ranges 0] [lindex $ranges end]}
	}

	set parent [::dom::node parent $parent]
    }

    return {}
}

# domtext::_node_attr_modified --
#
#	React to a change in the attribute list for a node
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Display updated to reflect change to DOM structure

proc domtext::_node_attr_modified {path evid} {

    set node [dom::event cget $evid -target]

    set tags [$path.text tag ranges attrs:$node]
    if {[llength $tags]} {

	# Remove previously defined attributes

	foreach {start end} $tags break
	set existingTags [$path.text tag names $start]
	$path.text delete $start $end
	$path.text tag delete attrs:$node

    } else {
	set tagStartEnd [lindex [$path.text tag ranges tag:start:$node] end]
	set start [$path.text index "$tagStartEnd - 1 char"]
	set existingTags [$path.text tag names $start]
    }

    # Replace with current attributes

    lappend existingTags attrs:$node
    $path.text insert $start [::dom::Serialize:attributeList [array get [::dom::node cget $node -attributes]]] $existingTags

    return {}
}

# domtext::_node_attr_removed --
#
#	React to a change in the attribute list for a node
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Display updated to reflect change to DOM structure

proc domtext::_node_attr_removed {path evid} {
    _node_attr_modified $path $evid
}

# domtext::_node_pcdata_modified --
#
#	React to a change in character data
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Display updated to reflect change to DOM structure

proc domtext::_node_pcdata_modified {path evid} {

    set node [dom::event cget $evid -target]

    if {[string compare [dom::node cget $node -nodeType] "textNode"]} {
	return -code error "node is not a text node"
    }

    # Remember where the insertion point is
    set insert [$path.text index insert]

    # Remove previous text
    set ranges [$path.text tag ranges $node]
    set tags [$path.text tag names [lindex $ranges 0]]
    eval [list $path.text] delete $ranges

    # Replace with new text
    $path.text insert [lindex $ranges 0] [dom::event cget $evid -newValue] $tags

    # Restore insertion point
    $path.text mark set insert $insert

    return {}
}

# domtext::_node_selected --
#
#	A node has been selected.
#
# Arguments:
#	path	widget path
#	evid	DOM event node
#
# Results:
#	Node's text is selected

proc domtext::_node_selected {path evid} {
    upvar #0 [namespace current]::selected$path selected

    set node [dom::event cget $evid -target]
    set selected $node

    catch {eval [list $path.text] tag remove sel [$path.text tag ranges sel]}

    set ranges [$path.text tag ranges $node]
    if {[llength $ranges]} {
	eval [list $path.text] tag add sel $ranges
    }

    $path.text mark set insert [lindex $ranges end]

    return {}
}

# domtext::_tkevent_override --
#
#	Certain Text widget class bindings must be prevented from firing
#
# Arguments:
#	path	widget path
#	x	x coord
#	y	y coord
#
# Results:
#	Return break error code

proc domtext::_tkevent_override {w x y} {
    return -code break
}

# domtext::_tkevent_select --
#
#	Single click.  We only want the highest priority tag to fire.
#
# Arguments:
#	path	widget path
#	node	DOM node
#	x
#	y	Coordinates
#
# Results:
#	DOM event posted

proc domtext::_tkevent_select {path node x y} {
    variable tkeventid

    catch {after cancel $tkeventid}
    set tkeventid [after idle "
    dom::event postUIEvent [list $node] DOMActivate -detail 1
    dom::event postMouseEvent [list $node] click -detail 1
    [namespace current]::_tkevent_select_setinsert [list $path] [list $node] [::tk::TextClosestGap $path.text $x $y]
"]
    return {}
}

# Helper routine for above proc

proc domtext::_tkevent_select_setinsert {path node idx} {
    switch [::dom::node cget $node -nodeType] {
	textNode {
	    # No need to change where the insertion point is going
	}
	element {
	    # Set the insertion point to the end of the first
	    # child textNode, or if none to immediately following
	    # the start tag.
	    set fc [::dom::node cget $node -firstChild]
	    if {[string length $fc] && [::dom::node cget $fc -nodeType] == "textNode"} {
		set idx [lindex [$path.text tag ranges $fc] end]
	    } else {
		set idx [lindex [$path.text tag ranges tag:start:$node] end]
	    }
	}
	default {
	    # Set the insertion point following the node
	    set idx [lindex [$path.text tag ranges $node] end]
	}
    }

    $path.text mark set insert $idx
    $path.text mark set anchor insert
    focus $path.text

    return {}
}

# domtext::_tkevent_open --
#
#	Double click
#
# Arguments:
#	path	widget path
#	node	DOM node
#
# Results:
#	DOM event posted

proc domtext::_tkevent_open {path node} {
    variable tkeventid

    catch {after cancel $tkeventid}
    set tkeventid [after idle "
    dom::event postUIEvent [list $node] DOMActivate -detail 2
    dom::event postMouseEvent [list $node] click -detail 2
"]
    return {}
}

# domtext::_key_select --
#
#	Select a node in which a key event has occurred.
#
# Arguments:
#	path	widget path
#	spec	the event specifier
#
# Results:
#	Appropriate node is selected.  Returns node id.

proc domtext::_key_select {path spec} {
    # Once the Text widget gets the focus, it receives the event.
    # We compensate for this here
    if {[winfo class $path] == "Text"} {
	set path [winfo parent $path]
    }
    upvar #0 [namespace current]::selected$path selected

    set root [Widget::cget $path -rootnode]

    # If selected node is a textNode move around the text itself
    # Otherwise markup has been selected.
    # Move around the nodes

    switch -glob [dom::node cget $selected -nodeType],$spec {
	textNode,<Key-Up> {
	    set ranges [$path.text tag ranges $selected]
	    foreach {line char} [split [lindex $ranges 0] .] break
	    set index [$path.text index insert]
	    foreach {iline ichar} [split [lindex $index 0] .] break
	    if {$line == $iline} {
		set new [dom::node parent $selected]
	    } else {
		::tk::TextSetCursor $path.text [::tk::TextUpDownLine $path.text -1]
		# The insertion point may now be in another node
		set newnode [_insert_to_node $path]
		if {[string compare $newnode $selected]} {
		    dom::event postUIEvent $newnode DOMActivate -detail 1
		}
		return -code break
	    }
	}
	textNode,<Key-Down> {
	    set ranges [$path.text tag ranges $selected]
	    foreach {line char} [split [lindex $ranges end] .] break
	    set index [$path.text index insert]
	    foreach {iline ichar} [split [lindex $index 0] .] break
	    if {$line == $iline} {
		bell
		return {}
	    } else {
		::tk::TextSetCursor $path.text [::tk::TextUpDownLine $path.text 1]
		# The insertion point may now be in another node
		set newnode [_insert_to_node $path]
		if {[string compare $newnode $selected]} {
		    dom::event postUIEvent $newnode DOMActivate -detail 1
		}
		return -code break
	    }
	}
	textNode,<Key-Left> {
	    set ranges [$path.text tag ranges $selected]
	    set index [$path.text index insert]
	    if {[$path.text compare $index == [lindex $ranges 0]]} {
		set new [dom::node cget $selected -previousSibling]
		if {![string length $new]} {
		    set new [dom::node parent $selected]
		}
	    } else {
		::tk::TextSetCursor $path.text insert-1c
		return -code break
	    }
	}
	textNode,<Key-Right> {
	    set ranges [$path.text tag ranges $selected]
	    set index [$path.text index insert]
	    if {[$path.text compare $index == [lindex $ranges end]]} {
		set new [dom::node cget $selected -nextSibling]
		if {![string length $new]} {
		    set new [dom::node parent $selected]
		}
	    } else {
		::tk::TextSetCursor $path.text insert+1c
		return -code break
	    }
	}

	*,<Key-Up>	{
	    set new [dom::node parent $selected]
	}
	*,<Key-Down>	{
	    set new [dom::node cget $selected -firstChild]
	    if {![string length $new]} {
		bell
		return {}
	    }
	}
	*,<Key-Left>	{
	    if {[dom::node parent $selected] == $root} {
		bell
		return {}
	    }
	    set new [dom::node cget $selected -previousSibling]
	    if {![string length $new]} {
		set new [dom::node parent $selected]
	    }
	}
	*,<Key-Right>	{
	    set new [dom::node cget $selected -nextSibling]
	    if {![string length $new]} {
		set new [dom::node parent $selected]
	    }
	}
    }
    if {![string length $new]} {
	bell
    }

    dom::event postUIEvent $new DOMActivate -detail 1

    return -code break
}

# domtext::_tkevent_filter_* --
#
#	React to editing events to keep the DOM structure
#	synchronised
#
# Arguments:
#	path	widget path
#	detail	key pressed
#
# Results:
#	Either event is blocked or passed through to the Text class binding
#	DOM events may be generated if text is inserted or deleted

proc domtext::_tkevent_filter_<Key> {path detail} {
    # Once the Text widget gets the focus, it receives the event.
    # We compensate for this here
    set code ok
    if {[winfo class $path] == "Text"} {
	set path [winfo parent $path]
	set code break
    }
    upvar #0 [namespace current]::selected$path selected

    set index [$path.text index insert]

    $path.text tag remove sel 0.0 end

    # Take action depending upon which node type the event has occurred.
    # Possibilities are:
    #	text node			insert the text, update node
    #	element				If a text node exists as first child,
    #					redirect event to it and make it active.
    #					Otherwise create a text node
    #	Document Type Declaration	ignore
    #	XML Declaration			ignore

    switch [dom::node cget $selected -nodeType] {
	element {
	    set child [dom::node cget $selected -firstChild]
	    if {[string length $child]} {
		if {[dom::node cget $child -nodeType] == "textNode"} {
		    dom::event postUIEvent $child DOMActivate -detail 1
		    dom::node configure $child -nodeValue [dom::node cget $child -nodeValue]$detail
		    ::tk::TextSetCursor $path.text insert+1c
		    focus $path.text
		    return -code $code {}
		} else {
		    bell
		    return -code $code {}
		}
	    } else {
		set child [dom::document createTextNode $selected $detail]
		dom::event postUIEvent $child DOMActivate -detail 1
		# When we return the new text node will have been
		# inserted into the Text widget
		set end [lindex [$path.text tag ranges $child] 1]
		$path.text mark set insert $end
		$path.text tag remove sel 0.0 end
		focus $path.text
		return -code $code {}
	    }
	}
	textNode {

	    # We need to know where in the character data to insert the
	    # character.  This is hard, so instead allow the Text widget
	    # to do the insertion then take all of the text and
	    # set that as the node's value

	    $path.text insert insert $detail $selected
	    $path.text see insert
	    focus $path.text
	    set ranges [$path.text tag ranges $selected]
	    set newvalue [$path.text get [lindex $ranges 0] [lindex $ranges end]]
	    dom::node configure $selected -nodeValue $newvalue
	    return -code $code {}

	}
	default {
	    bell
	    return -code $code {}
	}
    }

    return -code $code {}
}

proc domtext::_tkevent_filter_<Key-Return> {path detail} {
    set code [catch {_tkevent_filter_<Key> $path \n} msg]
    return -code $code $msg
}
proc domtext::_tkevent_filter_<Control-Key-i> {path detail} {
    set code [catch {_tkevent_filter_<Key> $path \t} msg]
    return -code $code $msg
}
# Don't support transposition (yet)
proc domtext::_tkevent_filter_<Control-Key-t> {path detail} {
    return -code break
}

proc domtext::_tkevent_filter_<Control-Key-h> {path detail} {
    set code [catch {_tkevent_filter_<Key-Backspace> $path $detail} msg]
    return -code $code $msg
}
proc domtext::_tkevent_filter_<Key-BackSpace> {path detail} {
    # Once the Text widget gets the focus, it receives the event.
    # We compensate for this here
    if {[winfo class $path] == "Text"} {
	set path [winfo parent $path]
    }
    upvar #0 [namespace current]::selected$path selected

    switch [dom::node cget $selected -nodeType] {
	textNode {
	    # If we're at the beginning of the text node stop here
	    set ranges [$path.text tag ranges $selected]
	    if {![llength $ranges] || [$path.text compare insert <= [lindex $ranges 0]]} {
		bell
		return -code break
	    }
	}
	default {
	    switch [tk_messageBox -parent [winfo toplevel $path] -title [mc {Confirm Delete Node}] -message [format [mc {Are you sure you want to delete a node of type %s?}] [dom::node cget $selected -nodeType]] -type okcancel] {
		ok {
		    dom::node removeNode [dom::node parent $selected] $selected
		}
		cancel {
		    return -code break
		}
	    }
	}
    }

    $path.text delete insert-1c
    $path.text see insert

    _tkevent_filter_update $path

    return -code break
}
proc domtext::_tkevent_filter_<Key-Delete> {path detail} {
    # Once the Text widget gets the focus, it receives the event.
    # We compensate for this here
    if {[winfo class $path] == "Text"} {
	set path [winfo parent $path]
    }
    upvar #0 [namespace current]::selected$path selected

    switch [dom::node cget $selected -nodeType] {
	textNode {
	    # If we're at the beginning of the text node stop here
	    set ranges [$path.text tag ranges $selected]
	    if {[$path.text compare insert >= [lindex $ranges end]]} {
		bell
		return -code break
	    }
	}
	default {
	    switch [tk_messageBox -parent [winfo toplevel $path] -title [mc {Confirm Delete Node}] -message [format [mc {Are you sure you want to delete a node of type %s?}] [dom::node cget $selected -nodeType]] -type okcancel] {
		ok {
		    dom::node removeNode [dom::node parent $selected] $selected
		}
		cancel {
		    return -code break
		}
	    }
	}
    }

    $path.text delete insert
    $path.text see insert

    _tkevent_filter_update $path

    return -code break
}
proc domtext::_tkevent_filter_update path {
    upvar #0 [namespace current]::selected$path selected

    # Now update the DOM node's value

    set ranges [$path.text tag ranges $selected]

    # If all text has been deleted then remove the node
    if {[llength $ranges]} {
	set newtext [$path.text get [lindex $ranges 0] [lindex $ranges end]]
	dom::node configure $selected -nodeValue $newtext
    } else {
	set parent [dom::node parent $selected]
	dom::node removeNode [dom::node parent $selected] $selected
	# Move selection to parent element, rather than removing selection
	#unset selected
	dom::event postUIEvent $parent DOMActivate -detail 1
    }

    return {}
}

# This will delete from the insertion point to the end of the line
# or text node, whichever is shorter
# TODO: implement this
proc domtext::_tkevent_filter_<Control-Key-k> {path detail} {
    return -code break
}
# TODO: this will delete the word to the left of the insertion point
# (only within the text node)
proc domtext::_tkevent_filter_<Meta-Key-Delete> {path detail} {
    return -code break
}
proc domtext::_tkevent_filter_<Meta-Key-BackSpace> {path detail} {
    _tkevent_filter_<Meta-Key-Delete> $path $detail
}

### Utilities

# domtext::_insert_to_node --
#
#	Finds the DOM node for the insertion point
#
# Arguments:
#	path	widget path
#
# Results:
#	Returns DOM token

proc domtext::_insert_to_node path {
    set tags [$path.text tag names insert]
    set newnode [lindex $tags end]
    while {![dom::DOMImplementation isNode $newnode]} {
	set tags [lreplace $tags end end]
	set newnode [lindex $tags end]
    }
    return $newnode
}

### Inlined images

image create photo ::domtext::starttab -data {
R0lGODlhEAAYAPcAAP//////zP//mf//Zv//M///AP/M///MzP/Mmf/MZv/M
M//MAP+Z//+ZzP+Zmf+ZZv+ZM/+ZAP9m//9mzP9mmf9mZv9mM/9mAP8z//8z
zP8zmf8zZv8zM/8zAP8A//8AzP8Amf8AZv8AM/8AAMz//8z/zMz/mcz/Zsz/
M8z/AMzM/8zMzMzMmczMZszMM8zMAMyZ/8yZzMyZmcyZZsyZM8yZAMxm/8xm
zMxmmcxmZsxmM8xmAMwz/8wzzMwzmcwzZswzM8wzAMwA/8wAzMwAmcwAZswA
M8wAAJn//5n/zJn/mZn/Zpn/M5n/AJnM/5nMzJnMmZnMZpnMM5nMAJmZ/5mZ
zJmZmZmZZpmZM5mZAJlm/5lmzJlmmZlmZplmM5lmAJkz/5kzzJkzmZkzZpkz
M5kzAJkA/5kAzJkAmZkAZpkAM5kAAGb//2b/zGb/mWb/Zmb/M2b/AGbM/2bM
zGbMmWbMZmbMM2bMAGaZ/2aZzGaZmWaZZmaZM2aZAGZm/2ZmzGZmmWZmZmZm
M2ZmAGYz/2YzzGYzmWYzZmYzM2YzAGYA/2YAzGYAmWYAZmYAM2YAADP//zP/
zDP/mTP/ZjP/MzP/ADPM/zPMzDPMmTPMZjPMMzPMADOZ/zOZzDOZmTOZZjOZ
MzOZADNm/zNmzDNmmTNmZjNmMzNmADMz/zMzzDMzmTMzZjMzMzMzADMA/zMA
zDMAmTMAZjMAMzMAAAD//wD/zAD/mQD/ZgD/MwD/AADM/wDMzADMmQDMZgDM
MwDMAACZ/wCZzACZmQCZZgCZMwCZAABm/wBmzABmmQBmZgBmMwBmAAAz/wAz
zAAzmQAzZgAzMwAzAAAA/wAAzAAAmQAAZgAAM+4AAN0AALsAAKoAAIgAAHcA
AFUAAEQAACIAABEAAADuAADdAAC7AACqAACIAAB3AABVAABEAAAiAAARAAAA
7gAA3QAAuwAAqgAAiAAAdwAAVQAARAAAIgAAEe7u7t3d3bu7u6qqqoiIiHd3
d1VVVURERCIiIhEREQAAACwAAAAAEAAYAAcIgwABCBxIsKBAfAjx2TNYMCHC
hQwPOrwHkaFDhRQjXtR3L6PBix3teSR4USRHexUlJuTY8WRFkBQ7dsQ3sOS9
kzNrOmR5M6dKhCFl3qP5EyPOoTpXymRJFABMkTKb2sSZL19ShDz1WSU5MeZW
rglNfgWL9d5YsvjMRgRQte3ZtXABAggIADs=
}
image create photo ::domtext::endtab -data {
R0lGODlhEAAYAPcAAP//////zP//mf//Zv//M///AP/M///MzP/Mmf/MZv/M
M//MAP+Z//+ZzP+Zmf+ZZv+ZM/+ZAP9m//9mzP9mmf9mZv9mM/9mAP8z//8z
zP8zmf8zZv8zM/8zAP8A//8AzP8Amf8AZv8AM/8AAMz//8z/zMz/mcz/Zsz/
M8z/AMzM/8zMzMzMmczMZszMM8zMAMyZ/8yZzMyZmcyZZsyZM8yZAMxm/8xm
zMxmmcxmZsxmM8xmAMwz/8wzzMwzmcwzZswzM8wzAMwA/8wAzMwAmcwAZswA
M8wAAJn//5n/zJn/mZn/Zpn/M5n/AJnM/5nMzJnMmZnMZpnMM5nMAJmZ/5mZ
zJmZmZmZZpmZM5mZAJlm/5lmzJlmmZlmZplmM5lmAJkz/5kzzJkzmZkzZpkz
M5kzAJkA/5kAzJkAmZkAZpkAM5kAAGb//2b/zGb/mWb/Zmb/M2b/AGbM/2bM
zGbMmWbMZmbMM2bMAGaZ/2aZzGaZmWaZZmaZM2aZAGZm/2ZmzGZmmWZmZmZm
M2ZmAGYz/2YzzGYzmWYzZmYzM2YzAGYA/2YAzGYAmWYAZmYAM2YAADP//zP/
zDP/mTP/ZjP/MzP/ADPM/zPMzDPMmTPMZjPMMzPMADOZ/zOZzDOZmTOZZjOZ
MzOZADNm/zNmzDNmmTNmZjNmMzNmADMz/zMzzDMzmTMzZjMzMzMzADMA/zMA
zDMAmTMAZjMAMzMAAAD//wD/zAD/mQD/ZgD/MwD/AADM/wDMzADMmQDMZgDM
MwDMAACZ/wCZzACZmQCZZgCZMwCZAABm/wBmzABmmQBmZgBmMwBmAAAz/wAz
zAAzmQAzZgAzMwAzAAAA/wAAzAAAmQAAZgAAM+4AAN0AALsAAKoAAIgAAHcA
AFUAAEQAACIAABEAAADuAADdAAC7AACqAACIAAB3AABVAABEAAAiAAARAAAA
7gAA3QAAuwAAqgAAiAAAdwAAVQAARAAAIgAAEe7u7t3d3bu7u6qqqoiIiHd3
d1VVVURERCIiIhEREQAAACwAAAAAEAAYAAcIgwABCBxIsKDBgvbwKcR3cGDC
hQwb2rsHMaLBiQ8XHpx4T1/Fi/c4fiRob6K+kCMBlOx4r6VHiAPxtWwpEqZA
mSFZZlQY0+XMlxpvzsxJ0SYAnCZRGsV50mVKnDRbpsyXL+fJnRYF5mvaMeXA
qjWDFtyqVOzYrkYNVvWqlqrbhg0BAggIADs=
}

