# -*- tcl -*-
#
# $Id: xmlrules.tcl,v 1.1 2001/09/11 05:25:51 andreas_kupries Exp $
#
# [expand] utilities for generating XML.
#
# Copyright (C) 2001 Joe English <jenglish@sourceforge.net>.
# Freely redistributable.
#
######################################################################

# xmlEscape text --
#	Replaces XML markup characters in $text with the
#	appropriate entity references.
#
variable textMap 	{ & &amp;  < &lt;  > &gt; }
variable attvalMap	{ & &amp;  < &lt;  > &gt; {"} &quot; } ; # "

proc xmlEscape {text} {
    variable textMap
    string map $textMap $text
}

# startTag gi ?attname attval ... ? --
#	Return start-tag for element $gi with specified attributes.
#
proc startTag {gi args} {
    variable attvalMap
    if {[llength $args] == 1} { set args [lindex $args 0] }
    set tag "<$gi"
    foreach {name value} $args {
    	append tag " $name='[string map $attvalMap $value]'"
    }
    return [append tag ">"]
}

# endTag gi --
#	Return end-tag for element $gi.
#
proc endTag {gi} {
    return "</$gi>"
}

# emptyElement gi ?attribute  value ... ?
#	Return empty-element tag.
#
proc emptyElement {gi args} {
    variable attvalMap
    if {[llength $args] == 1} { set args [lindex $args 0] }
    set tag "<$gi"
    foreach {name value} $args {
    	append tag " $name='[string map $attvalMap $value]'"
    }
    return [append tag "/>"]
}

# xmlComment text --
#	Return XML comment declaration containing $text.
#	NB: if $text includes the sequence "--", it will be mangled.
#
proc xmlComment {text} {
    return "<!-- [string map {-- { - - }} $text] -->"
}

# wrap content gi --
#	Returns $content wrapped inside <$gi> ... </$gi> tags.
#
proc wrap {content gi} {
    return "<$gi>$content</$gi>"
}

# wrap? content gi --
#	Same as [wrap], but returns an empty string if $content is empty.
#
proc wrap? {content gi} {
    if {![string length [string trim $content]]} { return "" }
    return "<$gi>$content</$gi>"
}

# wrapLines? content gi ? gi... ?
#	Same as [wrap?], but separates entries with newlines
#       and supports multiple nesting levels.
#
proc wrapLines? {content args} {
    if {![string length $content]} { return "" }
    foreach gi $args {
	set content [join [list <$gi> $content </$gi>] "\n"]
    }
    return $content
}

# sequence args --
#	Handy combinator.
#
proc sequence {args} { join $args "\n" }

######################################################################
# XML context management.
#

variable elementStack [list]

# start gi ?attribute value ... ? --
#	Return start-tag for element $gi
#	As a side-effect, pushes $gi onto the element stack.
#
proc start {gi args} {
    variable elementStack
    lappend elementStack $gi
    return [startTag $gi $args]
}

# xmlContext {gi1 ... giN}  --
#	Pops elements off the element stack until one of
#	the specified element types is found.
#
#	Returns: sequence of end-tags for each element popped.
#
proc xmlContext {gis} {
    variable elementStack
    set endTags [list]
    while {    [llength $elementStack]
            && [lsearch $gis [set current [lindex $elementStack end]]] < 0
    } {
	lappend endTags "</$current>"
	set elementStack [lreplace $elementStack end end]
    }
    return [join $endTags \n]
}

# end ? gi ? --
#	Generate markup to close element $gi, including end-tags
#	for any elements above it on the element stack.
#
#	If element name is omitted, closes the current element.
#
proc end {{gi {}}} {
    variable elementStack
    if {![string length $gi]} {
    	set gi [lindex $elementStack end]
    }
    set prefix [xmlContext $gi]
    set elementStack [lreplace $elementStack end end]
    return [join [list $prefix </$gi>] "\n"]
}

######################################################################
# Utilities for multi-pass processing.
#
# Not really XML-related, but I find them handy.
#

variable PassProcs
variable Buffers

# pass $passNo procName procArgs { body  } --
#	Specifies procedure definition for pass $n.
#
proc pass {pass proc args body} {
    variable PassProcs
    lappend PassProcs($pass) $proc $args $body
}

proc setPassProcs {pass} {
    variable PassProcs
    foreach {proc args body} $PassProcs($pass) {
	proc $proc $args $body
    }
}

# holdBuffers buffer ? buffer ...? --
#	Declare a list of hold buffers, 
#	to collect data in one pass and output it later.
#
proc holdBuffers {args} {
    variable Buffers
    foreach arg $args {
	set Buffers($arg) [list]
    }
}

# hold buffer text --
#	Append text to named buffer
#
proc hold {buffer entry} {
    variable Buffers
    lappend Buffers($buffer) $entry
    return
}

# held buffer --
#	Returns current contents of named buffer and empty the buffer.
#
proc held {buffer} {
    variable Buffers
    set content [join $Buffers($buffer) "\n"]
    set Buffers($buffer) [list]
    return $content
}

#*EOF*
