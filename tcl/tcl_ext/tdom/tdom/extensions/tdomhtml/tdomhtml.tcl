#----------------------------------------------------------------------------
#   Copyright (c) 1999,2000 Jochen Loewer (loewerj@hotmail.com) et al.   
#----------------------------------------------------------------------------
#
#   Rcsid: @(#)$Id: tdomhtml.tcl,v 1.2 2003/04/20 10:50:00 rolf Exp $
#
#   Implements simple HTML layer on top of core DOM Level-1 specification,
#   as implemented in tDOM package.
#
#   The contents of this file are subject to the Mozilla Public License
#   Version 1.1 (the "License"); you may not use this file except in
#   compliance with the License. You may obtain a copy of the License at
#   http://www.mozilla.org/MPL/
#
#   Software distributed under the License is distributed on an "AS IS"
#   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
#   License for the specific language governing rights and limitations
#   under the License.
#
#   The Original Code is tDOM.
#   The Initial Developer of the Original Code is Jochen Loewer.
#
#   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
#   Jochen Loewer. All Rights Reserved.
#
#   Portions created by Zoran Vasiljevic are Copyright (C) 2000-2002
#   Zoran Vasiljevic. All Rights Reserved.
#
#   Portions created by Rolf Ade are Copyright (C) 1999-2002
#   Rolf Ade. All Rights Reserved.

#   Contributor(s):
#
#       3 Apr 2000   Zoran Vasiljevic (zoran@v-connect.com)
#                    Initial idea
#
#      20 Oct 2002   Rolf Ade (rolf@pointsman.de)
#                    Suggestion to rewrite with new tdom :)
#
#      23 Oct 2002   Zoran Vasiljevic (zoran@archiware.com)
#                    Rewritten from scratch using new tdom.
#
#   Written by Zoran Vasiljevic
#   April, 2000      
#
#----------------------------------------------------------------------------

#
# This package requires the loaded tdom
# so bark early if we can't find it.
#

package require tdom

#
# Caller (usually our pkgIndex.tcl loader) will supply the package
# version by defining the _V_ variable before sourcing this file.
# For all other cases, we just provide the no-version package.
#

if {[info exists _V_] == 0} {
    package provide tdomhtml
} else {
    package provide tdomhtml $_V_
}

#
# Declare HTML generating commands
#

namespace eval ::dom::domHTML {
    
    #
    # Create commands for generating HTML tags. This is a complete 
    # set taken from http://www.w3.org/TR/html4/index/elements.html
    #
    
    variable elementNodeCmd {
        a
        abbr
        acronym
        address
        applet
        area
        b
        base
        basefont
        bdo
        big
        blockquote
        body
        br
        button
        caption
        center
        cite
        code
        col
        colgroup
        dd
        del
        dfn
        dir
        div
        dl
        dt
        em
        fieldset
        font
        form
        frame
        frameset
        h1
        h2 
        h3 
        h4 
        h5 
        h6
        head
        hr
        html
        i
        iframe
        img
        input
        ins
        isindex
        kbd
        label
        legend
        li
        link
        map
        menu
        meta 
        noframes
        noscript
        object 
        ol
        optgroup
        option
        p
        param
        pre
        q
        s
        samp
        script
        select
        small
        span
        strike
        strong
        style
        sub
        sup
        table
        tbody
        td
        textarea
        tfoot
        th
        thead
        title
        tr
        tt
        u
        ul
        var
    }

    foreach nodecmd $elementNodeCmd {
        dom createNodeCmd elementNode $nodecmd
    }

    #
    # Miscelaneous commands. Not part of HTML specs but needed 
    # for generation of special DOM nodes.
    #

    variable textNodeCmd t
    dom createNodeCmd textNode $textNodeCmd

    variable commentNodeCmd c
    dom createNodeCmd commentNode $commentNodeCmd
}

#-----------------------------------------------------------------------------
# ::dom::domHTML::newdoc --  
#
# Creates the HTML document and fils it with content. 
# Note: script is evaluated in the context of ::dom::domHTML namespace.
#-----------------------------------------------------------------------------

proc ::dom::domHTML::newdoc {script {upvars {}}} {

    foreach name $upvars { upvar $name $name }

    set doc [dom createDocument html]
    [$doc documentElement] appendFromScript $script

    return $doc
}

#-----------------------------------------------------------------------------
# ::dom::domHTML::putdoc --  
#
# Convenience wrapper to serialize the document to the output channel
#-----------------------------------------------------------------------------

proc ::dom::domHTML::putdoc {doc chan} {

    [$doc documentElement] asHTML -channel $chan
}

#-----------------------------------------------------------------------------
# ::dom::domHTML::deldoc --
#
# Convenience wrapper to dispose the html document 
#-----------------------------------------------------------------------------

proc ::dom::domHTML::deldoc {doc} {

    $doc delete
}

#-----------------------------------------------------------------------------
# ::dom::domHTML::html2tcl --  
#
# Parses the html file and creates a Tcl script usable for passing
# to the ::dom::domHTML::newdoc command.
#-----------------------------------------------------------------------------

proc ::dom::domHTML::html2tcl {htmlfile {outfile ""}} {

    #
    # Slurp-in the entire html file
    #

    set ichan [open $htmlfile]
    set html  [read $ichan]
    close $ichan

    #
    # Create in-memory DOM tree by parsing
    # the html content with the built-in
    # tdom html parser.
    #

    dom parse -html $html doc

    #
    # Open output file and recursively
    # format all elements found there.
    #

    if {$outfile == ""} {
        set outfile [file root $htmlfile].tcl
    }

    set ochan [open $outfile w]
    _2tcl [$doc documentElement] $ochan
    close $ochan
}

#-----------------------------------------------------------------------------
# ::dom::domHTML::_2tcl --  
#
# Helper procedure for recursively parsing the html tag
#-----------------------------------------------------------------------------

proc ::dom::domHTML::_2tcl {top ochan {indent 2} {offset 0}} {

    variable commentNodeCmd
    variable textNodeCmd
    variable elementNodeCmd

    set space [string repeat " " $offset]

    foreach child [$top childNodes] {
        switch -- [$child nodeType] {
            ELEMENT_NODE {    
          
                # Emit the nodename as html command
                # and create node command if missing
                set nodecmd [string tolower [$child nodeName]]
                if {[lsearch $elementNodeCmd $name] == -1} {
                    dom createNodeCmd elementNode $nodecmd
                }
                puts -nonewline $ochan $space
                puts -nonewline $ochan $nodecmd

                # Emit node attributes as key/value pairs
                foreach att [$child attributes] {
                    puts -nonewline $ochan " "
                    puts -nonewline $ochan [string tolower $att]
                    puts -nonewline $ochan " "
                    set val [_entityesc [$child getAttribute $att]]
                    if {[regexp { } $val]} {
                        puts -nonewline $ochan \"$val\"
                    } else {
                        puts -nonewline $ochan $val
                    }
                }

                # Recurse to child nodes
                if {[llength [$child childNodes]]} {
                    puts $ochan " {"
                    _2tcl $child $ochan $indent [expr {$offset+$indent}]
                    puts -nonewline $ochan $space
                    puts $ochan "}"
                } else {
                    puts $ochan ""
                }
            }
            TEXT_NODE - CDATA_SECTION_NODE {

                # Escape contents of text nodes
                puts -nonewline $ochan $space
                puts -nonewline $ochan "$textNodeCmd {"
                puts -nonewline $ochan [_entityesc [$child nodeValue]]
                puts $ochan "}"
            }
            COMMENT_NODE {

                # Pass contents of comment nodes as-is
                puts -nonewline $ochan $space
                puts -nonewline $ochan "$commentNodeCmd {"
                puts -nonewline $ochan [$child nodeValue]
                puts $ochan "}"
            }
        }
    }
}

#-----------------------------------------------------------------------------
# ::dom::domHTML::_entityesc --  
#
# Helper procedure for entity escaping
#-----------------------------------------------------------------------------

proc ::dom::domHTML::_entityesc {string} {

    regsub -all {(&[^;]+;)}  $string {\\\1} string
    regsub -all {([\#\[\]])} $string {\\\1} string

    return $string
}

#-----------------------------------------------------------------------------
# Short usage example.
#
#-----------------------------------------------------------------------------

if {0} {
    set doc [dom::domHTML::newdoc {
        title {t "Test document generated with tDOM"}
        body {
            table border 1 width 100 {
                for {set i 0} {$i < 5} {incr i} {
                    tr {
                        td {
                            i {
                                t "italic $i and "
                                b {t "italic-bold $i"}
                            }
                        }
                    }
                }
            }
        }
    }]

    dom::domHTML::putdoc $doc stdout
    dom::domHTML::deldoc $doc
}

# - EOF -
