#----------------------------------------------------------------------------
#   Copyright (c) 1999 Jochen Loewer (loewerj@hotmail.com)
#----------------------------------------------------------------------------
#
#   $Id: tdom.tcl,v 1.20 2008/05/24 21:59:58 rolf Exp $
#
#
#   The higher level functions of tDOM written in plain Tcl.
#
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
#
#   The Initial Developer of the Original Code is Jochen Loewer
#   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
#   Jochen Loewer. All Rights Reserved.
#
#   Contributor(s):
#       Rolf Ade (rolf@pointsman.de):   'fake' nodelists/live childNodes
#
#   written by Jochen Loewer
#   April, 1999
#
#----------------------------------------------------------------------------

package require tdom 

#----------------------------------------------------------------------------
#   setup namespaces for additional Tcl level methods, etc.
#
#----------------------------------------------------------------------------
namespace eval ::dom {
    namespace eval  domDoc {
    }
    namespace eval  domNode {
    }
    namespace eval  DOMImplementation {
    }
    namespace eval  xpathFunc {
    }
    namespace eval  xpathFuncHelper {
    }
}

namespace eval ::tDOM { 
    variable extRefHandlerDebug 0
    variable useForeignDTD ""

    namespace export xmlOpenFile xmlReadFile extRefHandler baseURL
}

#----------------------------------------------------------------------------
#   hasFeature (DOMImplementation method)
#
#
#   @in  url    the URL, where to get the XML document
#
#   @return     document object
#   @exception  XML parse errors, ...
#
#----------------------------------------------------------------------------
proc ::dom::DOMImplementation::hasFeature { dom feature {version ""} } {

    switch $feature {
        xml -
        XML {
            if {($version == "") || ($version == "1.0")} {
                return 1
            }
        }
    }
    return 0

}

#----------------------------------------------------------------------------
#   load (DOMImplementation method)
#
#       requests a XML document via http using the given URL and
#       builds up a DOM tree in memory returning the document object
#
#
#   @in  url    the URL, where to get the XML document
#
#   @return     document object
#   @exception  XML parse errors, ...
#
#----------------------------------------------------------------------------
proc ::dom::DOMImplementation::load { dom url } {

    error "Sorry, load method not implemented yet!"

}

#----------------------------------------------------------------------------
#   isa (docDoc method, for [incr tcl] compatibility)
#
#
#   @in  className
#
#   @return         1 iff inherits from the given class
#
#----------------------------------------------------------------------------
proc ::dom::domDoc::isa { doc className } {

    if {$className == "domDoc"} {
        return 1
    }
    return 0
}

#----------------------------------------------------------------------------
#   info (domDoc method, for [incr tcl] compatibility)
#
#
#   @in  subcommand
#   @in  args
#
#----------------------------------------------------------------------------
proc ::dom::domDoc::info { doc subcommand args } {

    switch $subcommand {
        class {
            return "domDoc"
        }
        inherit {
            return ""
        }
        heritage {
            return "domDoc {}"
        }
        default {
            error "domDoc::info subcommand $subcommand not yet implemented!"
        }
    }
}

#----------------------------------------------------------------------------
#   importNode (domDoc method)
#
#       Document Object Model (Core) Level 2 method
#
#
#   @in  subcommand
#   @in  args
#
#----------------------------------------------------------------------------
proc ::dom::domDoc::importNode { doc importedNode deep } {

    if {$deep || ($deep == "-deep")} {
        set node [$importedNode cloneNode -deep]
    } else {
        set node [$importedNode cloneNode]
    }
    return $node
}

#----------------------------------------------------------------------------
#   isa (domNode method, for [incr tcl] compatibility)
#
#
#   @in  className
#
#   @return         1 iff inherits from the given class
#
#----------------------------------------------------------------------------
proc ::dom::domNode::isa { doc className } {

    if {$className == "domNode"} {
        return 1
    }
    return 0
}

#----------------------------------------------------------------------------
#   info (domNode method, for [incr tcl] compatibility)
#
#
#   @in  subcommand
#   @in  args
#
#----------------------------------------------------------------------------
proc ::dom::domNode::info { doc subcommand args } {

    switch $subcommand {
        class {
            return "domNode"
        }
        inherit {
            return ""
        }
        heritage {
            return "domNode {}"
        }
        default {
            error "domNode::info subcommand $subcommand not yet implemented!"
        }
    }
}

#----------------------------------------------------------------------------
#   isWithin (domNode method)
#
#       tests, whether a node object is nested below another tag
#
#
#   @in  tagName  the nodeName of an elment node
#
#   @return       1 iff node is nested below a element with nodeName tagName
#                 0 otherwise
#
#----------------------------------------------------------------------------
proc ::dom::domNode::isWithin { node tagName } {

    while {[$node parentNode] != ""} {
        set node [$node parentNode]
        if {[$node nodeName] == $tagName} {
            return 1
        }
    }
    return 0
}

#----------------------------------------------------------------------------
#   tagName (domNode method)
#
#       same a nodeName for element interface
#
#----------------------------------------------------------------------------
proc ::dom::domNode::tagName { node } {

    if {[$node nodeType] == "ELEMENT_NODE"} {
        return [$node nodeName]
    }
    return -code error "NOT_SUPPORTED_ERR not an element!"
}

#----------------------------------------------------------------------------
#   simpleTranslate (domNode method)
#
#       applies simple translation rules similar to Cost's simple
#       translations to a node
#
#
#   @in  output_var
#   @in  trans_specs
#
#----------------------------------------------------------------------------
proc ::dom::domNode::simpleTranslate { node output_var trans_specs } {

    upvar $output_var output

    if {[$node nodeType] == "TEXT_NODE"} {
        append output [cgiQuote [$node nodeValue]]
        return
    }
    set found 0

    foreach {match action} $trans_specs {

        if {[catch {
            if {!$found && ([$node selectNode self::$match] != "") } {
              set found 1
            }
        } err]} {
            if {![string match "NodeSet expected for parent axis!" $err]} {
                error $err
            }
        }
        if {$found && ($action != "-")} {
            set stop 0
            foreach {type value} $action {
                switch $type {
                    prefix { append output [subst $value] }
                    tag    { append output <$value>       }
                    start  { append output [eval $value]  }
                    stop   { set stop 1                   }
                }
            }
            if {!$stop} {
                foreach child [$node childNodes] {
                    simpleTranslate  $child output $trans_specs
                }
            }
            foreach {type value} $action {
                switch $type {
                    suffix { append output [subst $value] }
                    end    { append output [eval $value]  }
                    tag    { append output </$value>      }
                }
            }
            return
        }
    }
    foreach child [$node childNodes] {
        simpleTranslate $child output $trans_specs
    }
}

#----------------------------------------------------------------------------
#   a DOM conformant 'live' childNodes
#
#   @return   a 'nodelist' object (it is just the normal node)
#
#----------------------------------------------------------------------------
proc ::dom::domNode::childNodesLive { node } {

    return $node
}

#----------------------------------------------------------------------------
#   item method on a 'nodelist' object
#
#   @return   a 'nodelist' object (it is just a normal
#
#----------------------------------------------------------------------------
proc ::dom::domNode::item { nodeListNode index } {

    return [lindex [$nodeListNode childNodes] $index]
}

#----------------------------------------------------------------------------
#   length method on a 'nodelist' object
#
#   @return   a 'nodelist' object (it is just a normal
#
#----------------------------------------------------------------------------
proc ::dom::domNode::length { nodeListNode } {

    return [llength [$nodeListNode childNodes]]
}

#----------------------------------------------------------------------------
#   appendData on a 'CharacterData' object
#
#----------------------------------------------------------------------------
proc ::dom::domNode::appendData { node  arg } {

    set type [$node nodeType]
    if {($type != "TEXT_NODE") && ($type != "CDATA_SECTION_NODE") &&
        ($type != "COMMENT_NODE")
    } {
        return -code error "NOT_SUPPORTED_ERR: node is not a cdata node"
    }
    set oldValue [$node nodeValue]
    $node nodeValue [append oldValue $arg]
}

#----------------------------------------------------------------------------
#   deleteData on a 'CharacterData' object
#
#----------------------------------------------------------------------------
proc ::dom::domNode::deleteData { node offset count } {

    set type [$node nodeType]
    if {($type != "TEXT_NODE") && ($type != "CDATA_SECTION_NODE") &&
        ($type != "COMMENT_NODE")
    } {
        return -code error "NOT_SUPPORTED_ERR: node is not a cdata node"
    }
    incr offset -1
    set before [string range [$node nodeValue] 0 $offset]
    incr offset
    incr offset $count
    set after  [string range [$node nodeValue] $offset end]
    $node nodeValue [append before $after]
}

#----------------------------------------------------------------------------
#   insertData on a 'CharacterData' object
#
#----------------------------------------------------------------------------
proc ::dom::domNode::insertData { node  offset arg } {

    set type [$node nodeType]
    if {($type != "TEXT_NODE") && ($type != "CDATA_SECTION_NODE") &&
        ($type != "COMMENT_NODE")
    } {
        return -code error "NOT_SUPPORTED_ERR: node is not a cdata node"
    }
    incr offset -1
    set before [string range [$node nodeValue] 0 $offset]
    incr offset
    set after  [string range [$node nodeValue] $offset end]
    $node nodeValue [append before $arg $after]
}

#----------------------------------------------------------------------------
#   replaceData on a 'CharacterData' object
#
#----------------------------------------------------------------------------
proc ::dom::domNode::replaceData { node offset count arg } {

    set type [$node nodeType]
    if {($type != "TEXT_NODE") && ($type != "CDATA_SECTION_NODE") &&
        ($type != "COMMENT_NODE")
    } {
        return -code error "NOT_SUPPORTED_ERR: node is not a cdata node"
    }
    incr offset -1
    set before [string range [$node nodeValue] 0 $offset]
    incr offset
    incr offset $count
    set after  [string range [$node nodeValue] $offset end]
    $node nodeValue [append before $arg $after]
}

#----------------------------------------------------------------------------
#   substringData on a 'CharacterData' object
#
#   @return   part of the node value (text)
#
#----------------------------------------------------------------------------
proc ::dom::domNode::substringData { node offset count } {

    set type [$node nodeType]
    if {($type != "TEXT_NODE") && ($type != "CDATA_SECTION_NODE") &&
        ($type != "COMMENT_NODE")
    } {
        return -code error "NOT_SUPPORTED_ERR: node is not a cdata node"
    }
    set endOffset [expr $offset + $count - 1]
    return [string range [$node nodeValue] $offset $endOffset]
}

#----------------------------------------------------------------------------
#   coerce2number
#
#----------------------------------------------------------------------------
proc ::dom::xpathFuncHelper::coerce2number { type value } {
    switch $type {
        empty      { return 0 }
        number -
        string     { return $value }
        attrvalues { return [lindex $value 0] }
        nodes      { return [[lindex $value 0] selectNodes number()] }
        attrnodes  { return [lindex $value 1] }
    }
}

#----------------------------------------------------------------------------
#   coerce2string
#
#----------------------------------------------------------------------------
proc ::dom::xpathFuncHelper::coerce2string { type value } {
    switch $type {
        empty      { return "" }
        number -
        string     { return $value }
        attrvalues { return [lindex $value 0] }
        nodes      { return [[lindex $value 0] selectNodes string()] }
        attrnodes  { return [lindex $value 1] }
    }
}

#----------------------------------------------------------------------------
#   function-available
#
#----------------------------------------------------------------------------
proc ::dom::xpathFunc::function-available { ctxNode pos
                                            nodeListType nodeList args} {

    if {[llength $args] != 2} {
        error "function-available(): wrong # of args!"
    }
    foreach { arg1Typ arg1Value } $args break
    set str [::dom::xpathFuncHelper::coerce2string $arg1Typ $arg1Value ]
    switch $str {
        boolean -
        ceiling -
        concat -
        contains -
        count -
        current -
        document -
        element-available -
        false -
        floor -
        format-number -
        generate-id -
        id -
        key -
        last -
        lang -
        local-name -
        name -
        namespace-uri -
        normalize-space -
        not -
        number -
        position -
        round -
        starts-with -
        string -
        string-length -
        substring -
        substring-after -
        substring-before -
        sum -
        translate -
        true -
        unparsed-entity-uri {
            return [list bool true]
        }
        default {
            set TclXpathFuncs [info procs ::dom::xpathFunc::*]
            if {[lsearch -exact $TclXpathFuncs $str] != -1} {
                return [list bool true]
            } else {
                return [list bool false]
            }
        }
    }
}

#----------------------------------------------------------------------------
#   element-available
#
#   This is not strictly correct. The XSLT namespace may be bound
#   to another prefix (and the prefix 'xsl' may be bound to another
#   namespace). Since the expression context isn't available at the
#   moment at tcl coded XPath functions, this couldn't be done better
#   than this "works in the 'normal' cases" version.
#----------------------------------------------------------------------------
proc ::dom::xpathFunc::element-available { ctxNode pos
                                            nodeListType nodeList args} {

    if {[llength $args] != 2} {
        error "element-available(): wrong # of args!"
    }
    foreach { arg1Typ arg1Value } $args break
    set str [::dom::xpathFuncHelper::coerce2string $arg1Typ $arg1Value ]
    # The XSLT recommendation says: "The element-available
    # function returns true if and only if the expanded-name
    # is the name of an instruction." The following xsl
    # elements are not in the category instruction.
    # xsl:attribute-set 
    # xsl:decimal-format 
    # xsl:include
    # xsl:key 
    # xsl:namespace-alias
    # xsl:output
    # xsl:param
    # xsl:strip-space
    # xsl:preserve-space
    # xsl:template
    # xsl:import
    # xsl:otherwise
    # xsl:sort
    # xsl:stylesheet
    # xsl:transform
    # xsl:with-param
    # xsl:when
    switch $str {
        xsl:apply-templates -
        xsl:apply-imports -
        xsl:call-template -
        xsl:element -
        xsl:attribute -
        xsl:text -
        xsl:processing-instruction -
        xsl:comment -
        xsl:copy -
        xsl:value-of -
        xsl:number -
        xsl:for-each -
        xsl:if -
        xsl:choose -
        xsl:variable -
        xsl:copy-of -
        xsl:message -
        xsl:fallback {
            return [list bool true]
        }
        default {
            return [list bool false]
        }
    }
}

#----------------------------------------------------------------------------
#   system-property
#
#   This is not strictly correct. The XSLT namespace may be bound
#   to another prefix (and the prefix 'xsl' may be bound to another
#   namespace). Since the expression context isn't available at the
#   moment at tcl coded XPath functions, this couldn't be done better
#   than this "works in the 'normal' cases" version.
#----------------------------------------------------------------------------
proc ::dom::xpathFunc::system-property { ctxNode pos
                                         nodeListType nodeList args } {

    if {[llength $args] != 2} {
        error "system-property(): wrong # of args!"
    }
    foreach { arg1Typ arg1Value } $args break
    set str [::dom::xpathFuncHelper::coerce2string $arg1Typ $arg1Value ]
    switch $str {
        xsl:version {
            return [list number 1.0]
        }
        xsl:vendor {
            return [list string "Jochen Loewer (loewerj@hotmail.com), Rolf Ade (rolf@pointsman.de) et. al."]
        }
        xsl:vendor-url {
            return [list string "http://www.tdom.org"]
        }
        default {
            return [list string ""]
        }
    }
}

#----------------------------------------------------------------------------
#   IANAEncoding2TclEncoding
#
#----------------------------------------------------------------------------

# As of version 8.3.4 tcl supports 
# cp860 cp861 cp862 cp863 tis-620 cp864 cp865 cp866 gb12345 cp949
# cp950 cp869 dingbats ksc5601 macCentEuro cp874 macUkraine jis0201
# gb2312 euc-cn euc-jp iso8859-10 macThai jis0208 iso2022-jp
# macIceland iso2022 iso8859-13 iso8859-14 jis0212 iso8859-15 cp737
# iso8859-16 big5 euc-kr macRomania macTurkish gb1988 iso2022-kr
# macGreek ascii cp437 macRoman iso8859-1 iso8859-2 iso8859-3 ebcdic
# macCroatian koi8-r iso8859-4 iso8859-5 cp1250 macCyrillic iso8859-6
# cp1251 koi8-u macDingbats iso8859-7 cp1252 iso8859-8 cp1253
# iso8859-9 cp1254 cp1255 cp850 cp1256 cp932 identity cp1257 cp852
# macJapan cp1258 shiftjis utf-8 cp855 cp936 symbol cp775 unicode
# cp857
# 
# Just add more mappings (and mail them to the tDOM mailing list, please).

proc tDOM::IANAEncoding2TclEncoding {IANAName} {
    
    # First the most widespread encodings with there
    # preferred MIME name, to speed lookup in this
    # usual cases. Later the official names and the
    # aliases.
    #
    # For "official names for character sets that may be
    # used in the Internet" see 
    # http://www.iana.org/assignments/character-sets
    # (that's the source for the encoding names below)
    # 
    # Matching is case-insensitive

    switch [string tolower $IANAName] {
        "us-ascii"    {return ascii}
        "utf-8"       {return utf-8}
        "utf-16"      {return unicode; # not sure about this}
        "iso-8859-1"  {return iso8859-1}
        "iso-8859-2"  {return iso8859-2}
        "iso-8859-3"  {return iso8859-3}
        "iso-8859-4"  {return iso8859-4}
        "iso-8859-5"  {return iso8859-5}
        "iso-8859-6"  {return iso8859-6}
        "iso-8859-7"  {return iso8859-7}
        "iso-8859-8"  {return iso8859-8}
        "iso-8859-9"  {return iso8859-9}
        "iso-8859-10" {return iso8859-10}
        "iso-8859-13" {return iso8859-13}
        "iso-8859-14" {return iso8859-14}
        "iso-8859-15" {return iso8859-15}
        "iso-8859-16" {return iso8859-16}
        "iso-2022-kr" {return iso2022-kr}
        "euc-kr"      {return euc-kr}
        "iso-2022-jp" {return iso2022-jp}
        "koi8-r"      {return koi8-r}
        "shift_jis"   {return shiftjis}
        "euc-jp"      {return euc-jp}
        "gb2312"      {return gb2312}
        "big5"        {return big5}
        "cp866"       {return cp866}
        "cp1250"      {return cp1250}
        "cp1253"      {return cp1253}
        "cp1254"      {return cp1254}
        "cp1255"      {return cp1255}
        "cp1256"      {return cp1256}
        "cp1257"      {return cp1257}

        "windows-1251" -
        "cp1251"      {return cp1251}

        "windows-1252" -
        "cp1252"      {return cp1252}    

        "iso_8859-1:1987" -
        "iso-ir-100" -
        "iso_8859-1" -
        "latin1" -
        "l1" -
        "ibm819" -
        "cp819" -
        "csisolatin1" {return iso8859-1}
        
        "iso_8859-2:1987" -
        "iso-ir-101" -
        "iso_8859-2" -
        "iso-8859-2" -
        "latin2" -
        "l2" -
        "csisolatin2" {return iso8859-2}

        "iso_8859-5:1988" -
        "iso-ir-144" -
        "iso_8859-5" -
        "iso-8859-5" -
        "cyrillic" -
        "csisolatincyrillic" {return iso8859-5}

        "ms_kanji" -
        "csshiftjis"  {return shiftjis}
        
        "csiso2022kr" {return iso2022-kr}

        "ibm866" -
        "csibm866"    {return cp866}
        
        default {
            # There are much more encoding names out there
            # It's only laziness, that let me stop here.
            error "Unrecognized encoding name '$IANAName'"
        }
    }
}

#----------------------------------------------------------------------------
#   xmlOpenFile
#
#----------------------------------------------------------------------------
proc tDOM::xmlOpenFile {filename {encodingString {}}} {

    set fd [open $filename]

    if {$encodingString != {}} {
        upvar $encodingString encString
    }

    # The autodetection of the encoding follows
    # XML Recomendation, Appendix F

    fconfigure $fd -encoding binary
    if {![binary scan [read $fd 4] "H8" firstBytes]} {
        # very short (< 4 Bytes) file
        seek $fd 0 start
        set encString UTF-8
        return $fd
    }
    
    # First check for BOM
    switch [string range $firstBytes 0 3] {
        "feff" -
        "fffe" {
            # feff: UTF-16, big-endian BOM
            # ffef: UTF-16, little-endian BOM
            seek $fd 0 start
            set encString UTF-16            
            fconfigure $fd -encoding identity
            return $fd
        }
    }

    # If the entity has a XML Declaration, the first four characters
    # must be "<?xm".
    switch $firstBytes {
        "3c3f786d" {
            # UTF-8, ISO 646, ASCII, some part of ISO 8859, Shift-JIS,
            # EUC, or any other 7-bit, 8-bit, or mixed-width encoding which 
            # ensures that the characters of ASCII have their normal positions,
            # width and values; the actual encoding declaration must be read to
            # detect which of these applies, but since all of these encodings
            # use the same bit patterns for the ASCII characters, the encoding
            # declaration itself be read reliably.

            # First 300 bytes should be enough for a XML Declaration
            # This is of course not 100 percent bullet-proof.
            set head [read $fd 296]

            # Try to find the end of the XML Declaration
            set closeIndex [string first ">" $head]
            if {$closeIndex == -1} {
                error "Weird XML data or not XML data at all"
            }

            seek $fd 0 start
            set xmlDeclaration [read $fd [expr {$closeIndex + 5}]]
            # extract the encoding information
            set pattern {^[^>]+encoding=[\x20\x9\xd\xa]*["']([^ "']+)['"]}
            # emacs: "
            if {![regexp $pattern $head - encStr]} {
                # Probably something like <?xml version="1.0"?>. 
                # Without encoding declaration this must be UTF-8
                set encoding utf-8
                set encString UTF-8
            } else {
                set encoding [IANAEncoding2TclEncoding $encStr]
                set encString $encStr
            }
        }
        "0000003c" -
        "0000003c" -
        "3c000000" -
        "00003c00" {
            # UCS-4
            error "UCS-4 not supported"
        }
        "003c003f" -
        "3c003f00" {
            # UTF-16, big-endian, no BOM
            # UTF-16, little-endian, no BOM
            seek $fd 0 start
            set encoding identity
            set encString UTF-16
        }
        "4c6fa794" {
            # EBCDIC in some flavor
            error "EBCDIC not supported"
        }
        default {
            # UTF-8 without an encoding declaration
            seek $fd 0 start
            set encoding identity
            set encString "UTF-8"
        }
    }
    fconfigure $fd -encoding $encoding
    return $fd
}

#----------------------------------------------------------------------------
#   xmlReadFile
#
#----------------------------------------------------------------------------
proc tDOM::xmlReadFile {filename {encodingString {}}} {

    if {$encodingString != {}} {
        upvar $encodingString encString
    }
    
    set fd [xmlOpenFile $filename encString]
    set data [read $fd [file size $filename]]
    close $fd 
    return $data
}

#----------------------------------------------------------------------------
#   extRefHandler
#   
#   A very simple external entity resolver, included for convenience.
#   Depends on the tcllib package uri and resolves only file URLs. 
#
#----------------------------------------------------------------------------

if {![catch {package require uri}]} {
    proc tDOM::extRefHandler {base systemId publicId} {
        variable extRefHandlerDebug
        variable useForeignDTD

        if {$extRefHandlerDebug} {
            puts stderr "tDOM::extRefHandler called with:"
            puts stderr "\tbase:     '$base'"
            puts stderr "\tsystemId: '$systemId'"
            puts stderr "\tpublicId: '$publicId'"
        }
        if {$systemId == ""} {
            if {$useForeignDTD != ""} {
                set systemId $useForeignDTD
            } else {
                error "::tDOM::useForeignDTD does\
                        not point to the foreign DTD"
            }
        }
        set absolutURI [uri::resolve $base $systemId]
        array set uriData [uri::split $absolutURI]
        switch $uriData(scheme) {
            file {
                return [list string $absolutURI [xmlReadFile $uriData(path)]]
            }
            default {
                error "can only handle file URI's"
            }
        }
    }
}

#----------------------------------------------------------------------------
#   baseURL
#   
#   A simple convenience proc which returns an absolute URL for a given
#   filename.
#
#----------------------------------------------------------------------------

proc tDOM::baseURL {path} {
    switch [file pathtype $path] {
        "relative" {
            return "file://[pwd]/$path"
        }
        default {
            return "file://$path"
        }
    }
}

# EOF
