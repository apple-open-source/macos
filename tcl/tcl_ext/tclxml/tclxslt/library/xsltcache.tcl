# xsltcache.tcl --
#
#	Handles performing XSLT transformations,
#	caching documents and results.
#
# Copyright (c) 2002 Zveno Pty Ltd
# http://www.zveno.com/
#
# $Id: xsltcache.tcl,v 1.6 2003/03/09 11:30:42 balls Exp $

package require xslt 2.5
package require uri

package provide xslt::cache 2.5

namespace eval xslt::cache {
    namespace export transform transformdoc flush
    namespace export parse_depend

    variable sources
    array set sources {}
    variable stylesheets
    array set stylesheets {}
    variable results
    array set results {}
}

# xslt::cache::transform --
#
#	Perform an XSLT transformation.
#
# Arguments:
#	src	Filename of source document
#	ssheet	Filename of stylesheet document
#	args	Configuration options, stylesheet parameters
#
# Results:
#	Result document token

proc xslt::cache::transform {src ssheet args} {
    variable sources
    variable stylesheets
    variable results

    # Separate parameters from options
    set parameters {}
    set options {}
    foreach {key value} $args {
	switch -glob -- $key {
	    -* {
		lappend options $key $value
	    }
	    default {
		lappend parameters $key $value
	    }
	}
    }

    # Normalize the parameter list
    array set paramArray $parameters
    set parameters {}
    foreach name [lsort [array names paramArray]] {
	lappend parameters $name $paramArray($name)
    }

    set hash $src.$ssheet.$parameters

    array set opts {
	-xmlinclude 1
    }
    array set opts $options

    set readSource [ReadXML $src -xmlinclude $opts(-xmlinclude)]

    set readStylesheet 1
    if {[info exists stylesheets($ssheet)]} {
	if {[file mtime $ssheet] < $stylesheets($ssheet,time)} {
	    set readStylesheet 0
	}
    }
    if {$readStylesheet} {
	catch {rename $stylesheets($ssheet) {}}
	ReadXML $ssheet -xmlinclude $opts(-xmlinclude)

	set stylesheets($ssheet) [xslt::compile $sources($ssheet)]
	set stylesheets($ssheet,time) [clock seconds]
    }

    if {$readSource || $readStylesheet || ![info exists results($hash)]} {

	set results($hash) [eval [list $stylesheets($ssheet)] transform [list $sources($src)] $parameters]
	set results($hash,time) [clock seconds]
    }

    return $results($hash)
}

# xslt::cache::ReadXML --
#
#	Internal proc to manage parsing a document.
#	Used for both source and stylesheet documents.
#
# Arguments:
#	src	Filename of source document
#	args	Configuration options
#
# Results:
#	Returns 1 if document was read.  Returns 0 if document is cached.

proc xslt::cache::ReadXML {src args} {
    variable sources
    array set opts {
	-xmlinclude 1
    }
    array set opts $args

    set readSource 1
    if {[info exists sources($src)]} {
	if {[file mtime $src] < $sources($src,time)} {
	    set readSource 0
	}
    }
    if {$readSource} {
	catch {dom::destroy $sources($src)}
	set ch [open $src]
	set sources($src) [dom::parse [read $ch] -baseuri file://$src]
	close $ch
	if {$opts(-xmlinclude)} {
	    dom::xinclude $sources($src)
	}
	set sources($src,time) [clock seconds]
    }

    return $readSource
}

# xslt::cache::transformdoc --
#
#	Perform an XSLT transformation on a DOM document.
#
# Arguments:
#	src	DOM token of source document
#	ssheet	Filename of stylesheet document
#	args	Configuration options, stylesheet parameters
#
# Results:
#	Result document token

proc xslt::cache::transformdoc {src ssheet args} {
    variable sources
    variable stylesheets

    # Separate parameters from options
    set parameters {}
    set options {}
    foreach {key value} $args {
	switch -glob -- $key {
	    -* {
		lappend options $key $value
	    }
	    default {
		lappend parameters $key $value
	    }
	}
    }

    # Normalize the parameter list
    array set paramArray $parameters
    set parameters {}
    foreach name [lsort [array names paramArray]] {
	lappend parameters $name $paramArray($name)
    }

    array set opts {
	-xmlinclude 1
    }
    array set opts $options

    set readStylesheet 1
    if {[info exists stylesheets($ssheet)]} {
	if {[file mtime $ssheet] < $stylesheets($ssheet,time)} {
	    set readStylesheet 0
	}
    }
    if {$readStylesheet} {
	catch {rename $stylesheets($ssheet) {}}
	ReadXML $ssheet -xmlinclude $opts(-xmlinclude)

	set stylesheets($ssheet) [xslt::compile $sources($ssheet)]
	set stylesheets($ssheet,time) [clock seconds]
    }

    set result [eval [list $stylesheets($ssheet)] transform [list $src] $parameters]

    return $result
}

# ::xslt::cache::parse_depend --
#
#	Parse a document while determining its dependencies.
#
# Arguments:
#	uri	Document's URI
#	depVar	Global variable name for dependency document
#
# Results:
#	Returns parsed document token.
#	Document token for dependency document is stored in depVar.

proc xslt::cache::parse_depend {uri depVar} {
    upvar #0 $depVar dep

    set dep [dom::create]
    dom::document createElement $dep dependencies

    array set uriParsed [uri::split $uri]

    switch -- $uriParsed(scheme) {
	file {
	    set ch [open $uriParsed(path)]
	    set doc [dom::parse [read $ch] -baseuri $uri -externalentitycommand [namespace code [list ParseDepend_Entity $depVar]]]
	    close $ch

	    ParseDepend_XInclude $doc $depVar
	    ParseDepend_XSLT $doc $depVar
	}
	http {
	    return -code error "URI scheme \"http\" not yet implemented"
	}
	dom {
	    set doc $uriParsed(dom)

	    # Can't determine external entities, but can find XInclude
	    # and XSL stylesheet includes/imports.
	    ParseDepend_XInclude $uriParsed(dom) $depVar
	    ParseDepend_XSLT $uriParsed(dom) $depVar
	}
	default {
	    return -code error "URI scheme \"$uriParsed(scheme)\" not supported"
	}
    }

    return $doc
}

# xslt::cache::ParseDepend_Entity --
#
#	Callback for external entity inclusion.
#
# Arguments:
#	depVar	Global variable of dependency document
#	pubId	Public identifier
#	sysId	System identifier
#
# Results:
#	Dependency added to dependency document

proc xslt::cache::ParseDepend_Entity {depVar pubId sysId} {
    upvar #0 $depVar dep

    dom::document createNode $dep /dependencies/external-entities/entity
}

# ::xslt::cache::flush --
#
#	Flush the cache
#
# Arguments:
#	src	source document filename
#	ssheet	stylesheet document filename
#	args	parameters
#
# Results:
#	Returns the empty string.
#	If all arguments are given then all entries corresponding
#	to that transformation are destroyed.
#	If the source and/or stylesheet are given then all
#	entries corresponding to those documents are destroyed.

proc xslt::cache::flush {src ssheet args} {
    variable sources
    variable stylesheets
    variable results

    # Normalize parameter list
    array set paramArray $args
    set parameters {}
    foreach name [lsort [array names paramArray]] {
	lappend parameters $name $paramArray($name)
    }

    set hash $src.$ssheet.$parameters

    switch -glob [string length $src],[string length $ssheet],[llength $args] {
	0,0,* {
	    # Special case: flush all
	    unset sources
	    array set sources {}
	    unset stylesheets
	    array set stylesheets {}
	    unset results
	    array set results {}
	}

	0,*,0 {
	    # Flush all entries for the given stylesheet
	    catch {rename $stylesheets($ssheet) {}}
	    catch {unset stylesheets($ssheet)}
	    catch {unset stylesheets($ssheet,time)}

	    foreach entry [array names results *.$ssheet.*] {
		catch {dom::destroy $results($entry)}
		catch {unset results($entry)}
		catch {unset results($entry,time)}
	    }
	}

	*,0,0 {
	    # Flush all entries for the given source document
	    catch {dom::destroy $sources($src)}
	    catch {unset sources($src)}
	    catch {unset sources($src,time)}
	    foreach entry [array names results $src.*] {
		catch {dom::destroy $results($entry)}
		catch {unset results($entry)}
		catch {unset results($entry,time)}
	    }
	}

	default {
	    # Flush specific entry
	    catch {dom::destroy $results($hash)}
	    catch {unset results($hash)}
	    catch {unset results($hash,time)}
	}
    }
}
