# html.tcl --
#
#	Procedures to make generating HTML easier.
#
#	This module depends on the ncgi module for the procedures
#	that initialize form elements based on current CGI values.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# Originally by Brent Welch, with help from Dan Kuchler and Melissa Chawla

package require Tcl 8.2
package require ncgi
package provide html 1.2.2

namespace eval ::html {

    # State about the current page

    variable page

    # A simple set of global defaults for tag parameters is implemented
    # by storing into elements indexed by "key.param", where key is
    # often the name of an HTML tag (anything for scoping), and
    # param must be the name of the HTML tag parameter (e.g., "href" or "size")
    #	input.size
    #	body.bgcolor
    #	body.text
    #	font.face
    #	font.size
    #	font.color

    variable defaults
    array set defaults {
	input.size	45
	body.bgcolor	white
	body.text	black
    }

    # In order to nandle nested calls to redefined control structures,
    # we need a temporary variable that is known not to exist.  We keep this
    # counter to append to the varname.  Each time we need a temporary
    # variable, we increment this counter.

    variable randVar 0

    # No more export, because this defines things like
    # foreach and if that do HTML things, not Tcl control
    # namespace export *
}

# ::html::foreach
#
#	Rework the "foreach" command to blend into HTML template files.
#	Rather than evaluating the body, we return the subst'ed body.  Each
#	iteration of the loop causes another string to be concatenated to
#	the result value.  No error checking is done on any arguments.
#
# Arguments:
#	varlist	Variables to instantiate with values from the next argument.
#	list	Values to set variables in varlist to.
#	args	?varlist2 list2 ...? body, where body is the string to subst
#		during each iteration of the loop.
#
# Results:
#	Returns a string composed of multiple concatenations of the
#	substitued body.
#
# Side Effects:
#	None.

proc ::html::foreach {vars vals args} {
    variable randVar

    # The body of the foreach loop must be run in the stack frame
    # above this one in order to have access to local variable at that stack
    # level.

    # To support nested foreach loops, we use a uniquely named
    # variable to store incremental results.
    incr randVar
    ::set resultVar "result_$randVar"

    # Extract the body and any varlists and valuelists from the args.
    ::set body [lindex $args end]
    ::set varvals [linsert [lreplace $args end end] 0 $vars $vals]

    # Create the script to eval in the stack frame above this one.
    ::set script "::foreach"
    ::foreach {vars vals} $varvals {
        append script " [list $vars] [list $vals]"
    }
    append script " \{\n"
    append script "  append $resultVar \[subst \{$body\}\]\n"
    append script "\}\n"

    # Create a temporary variable in the stack frame above this one,
    # and use it to store the incremental resutls of the multiple loop
    # iterations.  Remove the temporary variable when we're done so there's
    # no trace of this loop left in that stack frame.

    upvar $resultVar tmp
    ::set tmp ""
    uplevel $script
    ::set result $tmp
    unset tmp
    return $result
}

# ::html::for
#
#	Rework the "for" command to blend into HTML template files.
#	Rather than evaluating the body, we return the subst'ed body.  Each
#	iteration of the loop causes another string to be concatenated to
#	the result value.  No error checking is done on any arguments.
#
# Arguments:
#	start	A script to evaluate once at the very beginning.
#	test	An expression to eval before each iteration of the loop.
#		Once the expression is false, the command returns.
#	next	A script to evaluate after each iteration of the loop.
#	body	The string to subst during each iteration of the loop.
#
# Results:
#	Returns a string composed of multiple concatenations of the
#	substitued body.
#
# Side Effects:
#	None.

proc ::html::for {start test next body} {
    variable randVar

    # The body of the for loop must be run in the stack frame
    # above this one in order to have access to local variable at that stack
    # level.

    # To support nested for loops, we use a uniquely named
    # variable to store incremental results.
    incr randVar
    ::set resultVar "result_$randVar"

    # Create the script to eval in the stack frame above this one.
    ::set script "::for [list $start] [list $test] [list $next] \{\n"
    append script "  append $resultVar \[subst \{$body\}\]\n"
    append script "\}\n"

    # Create a temporary variable in the stack frame above this one,
    # and use it to store the incremental resutls of the multiple loop
    # iterations.  Remove the temporary variable when we're done so there's
    # no trace of this loop left in that stack frame.

    upvar $resultVar tmp
    ::set tmp ""
    uplevel $script
    ::set result $tmp
    unset tmp
    return $result
}

# ::html::while
#
#	Rework the "while" command to blend into HTML template files.
#	Rather than evaluating the body, we return the subst'ed body.  Each
#	iteration of the loop causes another string to be concatenated to
#	the result value.  No error checking is done on any arguments.
#
# Arguments:
#	test	An expression to eval before each iteration of the loop.
#		Once the expression is false, the command returns.
#	body	The string to subst during each iteration of the loop.
#
# Results:
#	Returns a string composed of multiple concatenations of the
#	substitued body.
#
# Side Effects:
#	None.

proc ::html::while {test body} {
    variable randVar

    # The body of the while loop must be run in the stack frame
    # above this one in order to have access to local variable at that stack
    # level.

    # To support nested while loops, we use a uniquely named
    # variable to store incremental results.
    incr randVar
    ::set resultVar "result_$randVar"

    # Create the script to eval in the stack frame above this one.
    ::set script "::while [list $test] \{\n"
    append script "  append $resultVar \[subst \{$body\}\]\n"
    append script "\}\n"

    # Create a temporary variable in the stack frame above this one,
    # and use it to store the incremental resutls of the multiple loop
    # iterations.  Remove the temporary variable when we're done so there's
    # no trace of this loop left in that stack frame.

    upvar $resultVar tmp
    ::set tmp ""
    uplevel $script
    ::set result $tmp
    unset tmp
    return $result
}

# ::html::if
#
#	Rework the "if" command to blend into HTML template files.
#	Rather than evaluating a body clause, we return the subst'ed body.
#	No error checking is done on any arguments.
#
# Arguments:
#	test	An expression to eval to decide whether to use the then body.
#	body	The string to subst if the test case was true.
#	args	?elseif test body2 ...? ?else bodyn?, where bodyn is the string
#		to subst if none of the tests are true.
#
# Results:
#	Returns a string composed by substituting a body clause.
#
# Side Effects:
#	None.

proc ::html::if {test body args} {
    variable randVar

    # The body of the then/else clause must be run in the stack frame
    # above this one in order to have access to local variable at that stack
    # level.

    # To support nested if's, we use a uniquely named
    # variable to store incremental results.
    incr randVar
    ::set resultVar "result_$randVar"

    # Extract the elseif clauses and else clause if they exist.
    ::set cmd [linsert $args 0 "::if" $test $body]

    ::foreach {keyword test body} $cmd {
        ::if {[string equal $keyword "else"]} {
            append script " else \{\n"
            ::set body $test
        } else {
            append script " $keyword [list $test] \{\n"
        }
        append script "  append $resultVar \[subst \{$body\}\]\n"
        append script "\} "
    }

    # Create a temporary variable in the stack frame above this one,
    # and use it to store the incremental resutls of the multiple loop
    # iterations.  Remove the temporary variable when we're done so there's
    # no trace of this loop left in that stack frame.

    upvar $resultVar tmp
    ::set tmp ""
    uplevel $script
    ::set result $tmp
    unset tmp
    return $result
}

# ::html::set
#
#	Rework the "set" command to blend into HTML template files.
#	The return value is always "" so nothing is appended in the
#	template.  No error checking is done on any arguments.
#
# Arguments:
#	var	The variable to set.
#	val	The new value to give the variable.
#
# Results:
#	Returns "".
#
# Side Effects:
#	None.

proc ::html::set {var val} {

    # The variable must be set in the stack frame above this one.

    ::set cmd [list set $var $val]
    uplevel $cmd
    return ""
}

# ::html::eval
#
#	Rework the "eval" command to blend into HTML template files.
#	The return value is always "" so nothing is appended in the
#	template.  No error checking is done on any arguments.
#
# Arguments:
#	args	The args to evaluate.  At least one must be given.
#
# Results:
#	Returns "".
#
# Side Effects:
#	Throws an error if no arguments are given.

proc ::html::eval {args} {

    # The args must be evaluated in the stack frame above this one.
    ::eval [linsert $args 0 uplevel]
    return ""
}

# ::html::init
#
#	Reset state that gets accumulated for the current page.
#
# Arguments:
#	nvlist	Name, value list that is used to initialize default namespace
#		variables that set font, size, etc.
#
# Side Effects:
#	Wipes the page state array

proc ::html::init {{nvlist {}}} {
    variable page
    variable defaults
    ::if {[info exists page]} {
	unset page
    }
    ::if {[info exists defaults]} {
	unset defaults
    }
    array set defaults $nvlist
}

# ::html::head
#
#	Generate the <head> section.  There are a number of
#	optional calls you make *before* this to inject
#	meta tags - see everything between here and the bodyTag proc.
#
# Arguments:
#	title	The page title
#
# Results:
#	HTML for the <head> section

proc ::html::head {title} {
    variable page
    ::set html "[openTag html][openTag head]\n"
    append html "\t[title $title]"
    ::if {[info exists page(author)]} {
	append html "\t$page(author)"
    }
    ::if {[info exists page(meta)]} {
	::foreach line $page(meta) {
	    append html "\t$line\n"
	}
    }
    append html "[closeTag]\n"
}

# ::html::title
#
#	Wrap up the <title> and tuck it away for use in the page later.
#
# Arguments:
#	title	The page title
#
# Results:
#	HTML for the <title> section

proc ::html::title {title} {
    variable page
    ::set page(title) $title
    ::set html "<title>$title</title>\n"
    return $html
}

# ::html::getTitle
#
#	Return the title of the current page.
#
# Arguments:
#	None
#
# Results:
#	The title

proc ::html::getTitle {} {
    variable page
    ::if {[info exists page(title)]} {
	return $page(title)
    } else {
	return ""
    }
}

# ::html::meta
#
#	Generate a meta tag.  This tag gets bundled into the <head>
#	section generated by html::head
#
# Arguments:
#	args	A name-value list of meta tag names and values.
#
# Side Effects:
#	Stores HTML for the <meta> tag for use later by html::head

proc ::html::meta {args} {
    variable page
    ::set html ""
    ::foreach {name value} $args {
	append html "<meta name=\"$name\" value=\"[quoteFormValue $value]\">"
    }
    lappend page(meta) $html
    return ""
}

# ::html::refresh
#
#	Generate a meta refresh tag.  This tag gets bundled into the <head>
#	section generated by html::head
#
# Arguments:
#	content	Time period, in seconds, before the refresh
#	url	(option) new page to view. If not specified, then
#		the current page is reloaded.
#
# Side Effects:
#	Stores HTML for the <meta> tag for use later by html::head

proc ::html::refresh {content {url {}}} {
    variable page
    ::set html "<meta http-equiv=\"Refresh\" content=\"$content"
    ::if {[string length $url]} {
	append html "; url=$url"
    }
    append html "\">\n"
    lappend page(meta) $html
    return ""
}

# ::html::headTag
#
#	Embed a tag into the HEAD section
#	generated by html::head
#
# Arguments:
#	string	Everything but the < > for the tag.
#
# Side Effects:
#	Stores HTML for the tag for use later by html::head

proc ::html::headTag {string} {
    variable page
    lappend page(meta) <$string>
    return ""
}

# ::html::keywords
#
#	Add META tag keywords to the <head> section.
#	Call this before you call html::head
#
# Arguments:
#	args	The keywords
#
# Side Effects:
#	See html::meta

proc ::html::keywords {args} {
    html::meta keywords [join $args ", "]
}

# ::html::description
#
#	Add a description META tag to the <head> section.
#	Call this before you call html::head
#
# Arguments:
#	description	The description
#
# Side Effects:
#	See html::meta

proc ::html::description {description} {
    html::meta description $description
}

# ::html::author
#
#	Add an author comment to the <head> section.
#	Call this before you call html::head
#
# Arguments:
#	author	Author's name
#
# Side Effects:
#	sets page(author)

proc ::html::author {author} {
    variable page
    ::set page(author) "<!-- $author -->\n"
    return ""
}

# ::html::tagParam
#
#	Return a name, value string for the tag parameters.
#	The values come from "hard-wired" values in the 
#	param agrument, or from the defaults set with html::init.
#
# Arguments:
#	tag	Name of the HTML tag (case insensitive).
#	param	pname=value info that overrides any default values
#
# Results
#	A string of the form:
#		pname="keyvalue" name2="2nd value"

proc ::html::tagParam {tag {param {}}} {
    variable defaults

    ::set def ""
    ::foreach key [lsort [array names defaults $tag.*]] {
	append def [default $key $param]
    }
    return [string trimleft $param$def]
}

# ::html::default
#
#	Return a default value, if one has been registered
#	and an overriding value does not occur in the existing
#	tag parameters.
#
# Arguments:
#	key	Index into the defaults array defined by html::init
#		This is expected to be in the form tag.pname where
#		the pname part is used in the tag parameter name
#	param	pname=value info that overrides any default values
#
# Results
#	pname="keyvalue"

proc ::html::default {key {param {}}} {
    variable defaults
    ::set pname [string tolower [lindex [split $key .] 1]]
    ::set key [string tolower $key]
    ::if {![regexp -nocase "(\[ 	\]|^)$pname=" $param] &&
	    [info exists defaults($key)] &&
	    [string length $defaults($key)]} {
	return " $pname=\"$defaults($key)\""
    } else {
	return ""
    }
}

# ::html::bodyTag
#
#	Generate a body tag
#
# Arguments:
#	none
#
# Results
#	A body tag

proc ::html::bodyTag {args} {
    return [openTag body [join $args]]\n
}

# The following procedures are all related to generating form elements
# that are initialized to store the current value of the form element
# based on the CGI state.  These functions depend on the ncgi::value
# procedure and assume that the caller has called ncgi::parse and/or
# ncgi::init appropriately to initialize the ncgi module.

# ::html::formValue
#
#	Return a name and value pair, where the value is initialized
#	from existing form data, if any.
#
# Arguments:
#	name		The name of the form element
#	defvalue	A default value to use, if not appears in the CGI
#			inputs.  DEPRECATED - use ncgi::defValue instead.
#
# Retults:
#	A string like:
#	name="fred" value="freds value"

proc ::html::formValue {name {defvalue {}}} {
    ::set value [ncgi::value $name]
    ::if {[string length $value] == 0} {
	::set value $defvalue
    }
    return "name=\"$name\" value=\"[quoteFormValue $value]\""
}

# ::html::quoteFormValue
#
#	Quote a value for use in a value=\"$value\" fragment.
#
# Arguments:
#	value		The value to quote
#
# Retults:
#	A string like:
#	&#34;Hello, &lt;b&gt;World!&#34;

proc ::html::quoteFormValue {value} {
    return [string map [list "&" "&amp;" "\"" "&#34;" \
			    "'" "&#39;" "<" "&lt;" ">" "&gt;"] $value]
}

# ::html::textInput --
#
#	Return an <input type=text> element.  This uses the
#	input.size default falue.
#
# Arguments:
#	name		The form element name
#	args		Additional attributes for the INPUT tag
#
# Results:
#	The html fragment

proc ::html::textInput {name {value {}} args} {
    variable defaults
    ::set html "<input type=\"text\" "
    append html [formValue $name $value]
    append html [default input.size]
    ::if {[llength $args] != 0} then {
	append html " " [join $args]
    }
    append html ">\n"
    return $html
}

# ::html::textInputRow --
#
#	Format a table row containing a text input element and a label.
#
# Arguments:
#	label	Label to display next to the form element
#	name	The form element name
#	args	Additional attributes for the INPUT tag
#
# Results:
#	The html fragment

proc ::html::textInputRow {label name {value {}} args} {
    variable defaults
    ::set html [row $label [::eval [linsert $args 0 html::textInput $name $value]]]
    return $html
}

# ::html::passwordInputRow --
#
#	Format a table row containing a password input element and a label.
#
# Arguments:
#	label	Label to display next to the form element
#	name	The form element name
#
# Results:
#	The html fragment

proc ::html::passwordInputRow {label {name password}} {
    variable defaults
    ::set html [row $label [passwordInput $name]]
    return $html
}

# ::html::passwordInput --
#
#	Return an <input type=password> element.
#
# Arguments:
#	name	The form element name. Defaults to "password"
#
# Results:
#	The html fragment

proc ::html::passwordInput {{name password}} {
    ::set html "<input type=\"password\" name=\"$name\">\n"
    return $html
}

# ::html::checkbox --
#
#	Format a checkbox so that it retains its state based on
#	the current CGI values
#
# Arguments:
#	name		The form element name
#	value		The value associated with the checkbox
#
# Results:
#	The html fragment

proc ::html::checkbox {name value} {
    ::set html "<input type=\"checkbox\" [checkValue $name $value]>\n"
}

# ::html::checkValue
#
#	Like html::formalue, but for checkboxes that need CHECKED
#
# Arguments:
#	name		The name of the form element
#	defvalue	A default value to use, if not appears in the CGI
#			inputs
#
# Retults:
#	A string like:
#	name="fred" value="freds value" CHECKED


proc ::html::checkValue {name {value 1}} {
    variable page
    ::foreach v [ncgi::valueList $name] {
	::if {[string compare $value $v] == 0} {
	    return "name=\"$name\" value=\"[quoteFormValue $value]\" CHECKED"
	}
    }
    return "name=\"$name\" value=\"[quoteFormValue $value]\""
}

# ::html::radioValue
#
#	Like html::formValue, but for radioboxes that need CHECKED
#
# Arguments:
#	name	The name of the form element
#	value	The value associated with the radio button.
#
# Retults:
#	A string like:
#	name="fred" value="freds value" CHECKED

proc ::html::radioValue {name value {defaultSelection {}}} {
    ::if {[string equal $value [ncgi::value $name $defaultSelection]]} {
	return "name=\"$name\" value=\"[quoteFormValue $value]\" CHECKED"
    } else {
	return "name=\"$name\" value=\"[quoteFormValue $value]\""
    }
}

# ::html::radioSet --
#
#	Display a set of radio buttons while looking for an existing
#	value from the query data, if any.

proc ::html::radioSet {key sep list {defaultSelection {}}} {
    ::set html ""
    ::set s ""
    ::foreach {label v} $list {
	append html "$s<input type=\"radio\" [radioValue $key $v $defaultSelection]> $label"
	::set s $sep
    }
    return $html
}

# ::html::checkSet --
#
#	Display a set of check buttons while looking for an existing
#	value from the query data, if any.

proc ::html::checkSet {key sep list} {
    ::set s ""
    ::foreach {label v} $list {
	append html "$s<input type=\"checkbox\" [checkValue $key $v]> $label"
	::set s $sep
    }
    return $html
}

# ::html::select --
#
#	Format a <select> element that retains the state of the
#	current CGI values.
#
# Arguments:
#	name		The form element name
#	param		The various size, multiple parameters for the tag
#	choices		A simple list of choices
#	current		Value to assume if nothing is in CGI state
#
# Results:
#	The html fragment

proc ::html::select {name param choices {current {}}} {
    variable page

    ::set def [ncgi::valueList $name $current]
    ::set html "<select name=\"$name\"[string trimright  " $param"]>\n"
    ::foreach {label v} $choices {
	::if {[lsearch -exact $def $v] != -1} {
	    ::set SEL " SELECTED"
	} else {
	    ::set SEL ""
	}
	append html "<option value=\"$v\"$SEL>$label\n"
    }
    append html "</select>\n"
    return $html
}

# ::html::selectPlain --
#
#	Format a <select> element where the values are the same
#	as those that are displayed.
#
# Arguments:
#	name		The form element name
#	param		Tag parameters
#	choices		A simple list of choices
#
# Results:
#	The html fragment

proc ::html::selectPlain {name param choices {current {}}} {
    ::set namevalue {}
    ::foreach c $choices {
	lappend namevalue $c $c
    }
    return [select $name $param $namevalue $current]
}

# ::html::textarea --
#
#	Format a <textarea> element that retains the state of the
#	current CGI values.
#
# Arguments:
#	name		The form element name
#	param		The various size, multiple parameters for the tag
#	current		Value to assume if nothing is in CGI state
#
# Results:
#	The html fragment

proc ::html::textarea {name {param {}} {current {}}} {
    ::set value [ncgi::value $name $current]
    return "<[string trimright \
	"textarea name=\"$name\"\
		[tagParam textarea $param]"]>$value</textarea>\n"
}

# ::html::submit --
#
#	Format a submit button.
#
# Arguments:
#	label		The string to appear in the submit button.
#	name		The name for the submit button element
#
# Results:
#	The html fragment


proc ::html::submit {label {name submit}} {
    ::set html "<input type=\"submit\" name=\"$name\" value=\"$label\">\n"
}

# ::html::varEmpty --
#
#	Return true if the variable doesn't exist or is an empty string
#
# Arguments:
#	varname	Name of the variable
#
# Results:
#	1 if the variable doesn't exist or has the empty value

proc ::html::varEmpty {name} {
    upvar 1 $name var
    ::if {[info exists var]} {
	::set value $var
    } else {
	::set value ""
    }
    return [expr {[string length [string trim $value]] == 0}]
}

# ::html::getFormInfo --
#
#	Generate hidden fields to capture form values.
#
# Arguments:
#	args	List of elements to save.  If this is empty, everything is
#		saved in hidden fields.  This is a list of string match
#		patterns.
#
# Results:
#	A bunch of <input type=hidden> elements

proc ::html::getFormInfo {args} {
    ::if {[llength $args] == 0} {
	::set args *
    }
    ::set html ""
    ::foreach {n v} [ncgi::nvlist] {
	::foreach pat $args {
	    ::if {[string match $pat $n]} {
		append html "<input type=\"hidden\" name=\"$n\" \
				    value=\"[quoteFormValue $v]\">\n"
	    }
	}
    }
    return $html
}

# ::html::h1
#	Generate an H1 tag.
#
# Arguments:
#	string
#	param
#
# Results:
#	Formats the tag.

proc ::html::h1 {string {param {}}} {
    html::h 1 $string $param
}
proc ::html::h2 {string {param {}}} {
    html::h 2 $string $param
}
proc ::html::h3 {string {param {}}} {
    html::h 3 $string $param
}
proc ::html::h4 {string {param {}}} {
    html::h 4 $string $param
}
proc ::html::h5 {string {param {}}} {
    html::h 5 $string $param
}
proc ::html::h6 {string {param {}}} {
    html::h 6 $string $param
}
proc ::html::h {level string {param {}}} {
    return "<[string trimright "h$level [tagParam h$level $param]"]>$string</h$level>\n"
}

# ::html::openTag
#	Remember that a tag  is opened so it can be closed later.
#	This is used to automatically clean up at the end of a page.
#
# Arguments:
#	tag	The HTML tag name
#	param	Any parameters for the tag
#
# Results:
#	Formats the tag.  Also keeps it around in a per-page stack
#	of open tags.

proc ::html::openTag {tag {param {}}} {
    variable page
    lappend page(stack) $tag
    return "<[string trimright "$tag [tagParam $tag $param]"]>"
}

# ::html::closeTag
#	Pop a tag from the stack and close it.
#
# Arguments:
#	None
#
# Results:
#	A close tag.  Also pops the stack.

proc ::html::closeTag {} {
    variable page
    ::if {[info exists page(stack)]} {
	::set top [lindex $page(stack) end]
	::set page(stack) [lreplace $page(stack) end end]
    }
    ::if {[info exists top] && [string length $top]} {
	return </$top>
    } else {
	return ""
    }
}

# ::html::end
#
#	Close out all the open tags.  Especially useful for
#	Tables that do not display at all if they are unclosed.
#
# Arguments:
#	None
#
# Results:
#	Some number of close HTML tags.

proc ::html::end {} {
    variable page
    ::set html ""
    ::while {[llength $page(stack)]} {
	append html [closeTag]\n
    }
    return $html
}

# ::html::row
#
#	Format a table row.  If the default font has been set, this
#	takes care of wrapping the table cell contents in a font tag.
#
# Arguments:
#	args	Values to put into the row
#
# Results:
#	A <tr><td>...</tr> fragment

proc ::html::row {args} {
    ::set html <tr>\n
    ::foreach x $args {
	append html \t[cell "" $x td]\n
    }
    append html "</tr>\n"
    return $html
}

# ::html::hdrRow
#
#	Format a table row.  If the default font has been set, this
#	takes care of wrapping the table cell contents in a font tag.
#
# Arguments:
#	args	Values to put into the row
#
# Results:
#	A <tr><th>...</tr> fragment

proc ::html::hdrRow {args} {
    variable defaults
    ::set html <tr>\n
    ::foreach x $args {
	append html \t[cell "" $x th]\n
    }
    append html "</tr>\n"
    return $html
}

# ::html::paramRow
#
#	Format a table row.  If the default font has been set, this
#	takes care of wrapping the table cell contents in a font tag.
#
#       Based on html::row
#
# Arguments:
#	list	Values to put into the row
#       rparam   Parameters for row
#       cparam   Parameters for cells
#
# Results:
#	A <tr><td>...</tr> fragment

proc ::html::paramRow {list {rparam {}} {cparam {}}} {
    ::set html "<tr $rparam>\n"
    ::foreach x $list {
	append html \t[cell $cparam $x td]\n
    }
    append html "</tr>\n"
    return $html
}

# ::html::cell
#
#	Format a table cell.  If the default font has been set, this
#	takes care of wrapping the table cell contents in a font tag.
#
# Arguments:
#	param	Td tag parameters
#	value	The value to put into the cell
#	tag	(option) defaults to TD
#
# Results:
#	<td>...</td> fragment

proc ::html::cell {param value {tag td}} {
    ::set font [font]
    ::if {[string length $font]} {
	::set value $font$value</font>
    }
    return "<[string trimright "$tag $param"]>$value</$tag>"
}

# ::html::tableFromArray
#
#	Format a Tcl array into an HTML table
#
# Arguments:
#	arrname	The name of the array
#	param	The <table> tag parameters, if any.
#	pat	A string match pattern for the element keys
#
# Results:
#	A <table>

proc ::html::tableFromArray {arrname {param {}} {pat *}} {
    upvar 1 $arrname arr
    ::set html ""
    ::if {[info exists arr]} {
	append html "<TABLE $param>\n"
	append html "<TR><TH colspan=2>$arrname</TH></TR>\n"
	::foreach name [lsort [array names arr $pat]] {
	    append html [row $name $arr($name)]
	}
	append html </TABLE>\n
    }
    return $html
}

# ::html::tableFromList
#
#	Format a table from a name, value list
#
# Arguments:
#	querylist	A name, value list
#	param		The <table> tag parameters, if any.
#
# Results:
#	A <table>

proc ::html::tableFromList {querylist {param {}}} {
    ::set html ""
    ::if {[llength $querylist]} {
	append html "<TABLE $param>"
	::foreach {label value} $querylist {
	    append html [row $label $value]
	}
	append html </TABLE>
    }
    return $html
}

# ::html::mailto
#
#	Format a mailto: HREF tag
#
# Arguments:
#	email	The target
#	subject	The subject of the email, if any
#
# Results:
#	A <a href=mailto> tag </a>

proc ::html::mailto {email {subject {}}} {
    ::set html "<a href=\"mailto:$email"
    ::if {[string length $subject]} {
	append html ?subject=$subject
    }
    append html "\">$email</a>"
    return $html
}

# ::html::font
#
#	Generate a standard <font> tag.  This depends on defaults being
#	set via html::init
#
# Arguments:
#	args	Font parameters.  
#
# Results:
#	HTML

proc ::html::font {args} {
    variable defaults

    # e.g., font.face, font.size, font.color
    ::set param [tagParam font [join $args]][join $args]

    ::if {[string length $param]} {
	return "<[string trimright "font $param"]>"
    } else {
	return ""
    }
}

# ::html::minorMenu
#
#	Create a menu of links given a list of label, URL pairs.
#	If the URL is the current page, it is not highlighted.
#
# Arguments:
#
#	list	List that alternates label, url, label, url
#	sep	Separator between elements
#
# Results:
#	html

proc ::html::minorMenu {list {sep { | }}} {
    global page
    ::set s ""
    ::set html ""
    regsub -- {index.h?tml$} [ncgi::urlStub] {} this
    ::foreach {label url} $list {
	regsub -- {index.h?tml$} $url {} that
	::if {[string compare $this $that] == 0} {
	    append html "$s$label"
	} else {
	    append html "$s<a href=\"$url\">$label</a>"
	}
	::set s $sep
    }
    return $html
}

# ::html::minorList
#
#	Create a list of links given a list of label, URL pairs.
#	If the URL is the current page, it is not highlighted.
#
#       Based on html::minorMenu
#
# Arguments:
#
#	list	List that alternates label, url, label, url
#       ordered Boolean flag to choose between ordered and
#               unordered lists. Defaults to 0, i.e. unordered.
#
# Results:
#	A <ul><li><a...><\li>.....<\ul> fragment
#    or a <ol><li><a...><\li>.....<\ol> fragment

proc ::html::minorList {list {ordered 0}} {
    global page
    ::set s ""
    ::set html ""
    ::if { $ordered } {
	append html [openTag ol]
    } else {
	append html [openTag ul]
    }
    regsub -- {index.h?tml$} [ncgi::urlStub] {} this
    ::foreach {label url} $list {
	append html [openTag li]
	regsub -- {index.h?tml$} $url {} that
	::if {[string compare $this $that] == 0} {
	    append html "$s$label"
	} else {
	    append html "$s<a href=\"$url\">$label</a>"
	}
	append html [closeTag]
	append html \n
    }
    append html [closeTag]
    return $html
}

# ::html::extractParam
#
#	Extract a value from parameter list (this needs a re-do)
#
# Arguments:
#   param	A parameter list.  It should alredy have been processed to
#		remove any entity references
#   key		The parameter name
#   varName	The variable to put the value into (use key as default)
#
# Results:
#	returns "1" if the keyword is found, "0" otherwise

proc ::html::extractParam {param key {varName ""}} {
    ::if {$varName == ""} {
	upvar $key result
    } else {
	upvar $varName result
    }
    ::set ws " \t\n\r"
 
    # look for name=value combinations.  Either (') or (") are valid delimeters
    ::if {
      [regsub -nocase [format {.*%s[%s]*=[%s]*"([^"]*).*} $key $ws $ws] $param {\1} value] ||
      [regsub -nocase [format {.*%s[%s]*=[%s]*'([^']*).*} $key $ws $ws] $param {\1} value] ||
      [regsub -nocase [format {.*%s[%s]*=[%s]*([^%s]+).*} $key $ws $ws $ws] $param {\1} value] } {
        ::set result $value
        return 1
    }

    # now look for valueless names
    # I should strip out name=value pairs, so we don't end up with "name"
    # inside the "value" part of some other key word - some day
	
    ::set bad \[^a-zA-Z\]+
    ::if {[regexp -nocase  "$bad$key$bad" -$param-]} {
	return 1
    } else {
	return 0
    }
}

# ::html::urlParent --
#	This is like "file dirname", but doesn't screw with the slashes
#       (file dirname will collapse // into /)
#
# Arguments:
#	url	The URL
#
# Results:
#	The parent directory of the URL.

proc ::html::urlParent {url} {
    ::set url [string trimright $url /]
    regsub -- {[^/]+$} $url {} url
    return $url
}

