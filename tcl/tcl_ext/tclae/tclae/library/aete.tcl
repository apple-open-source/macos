## -*- mode: Tcl; coding: utf-8; -*-
namespace eval tclAE::aete {}



tclAE::installEventHandler ascr gdte tclAE::handleGetAETE    

proc tclAE::handleGetAETE {theAppleEvent theReplyAE} {
    global tclAE::aete::gdteProcs
    
    if {![info exists tclAE::aete::gdteProcs]} {
        set tclAE::aete::gdteProcs {}
    }  
    
    set aetes {}
    catch {lappend aetes [tclAE::parseAETE [resource read aete 0]]}
    foreach gdteProc ${tclAE::aete::gdteProcs} {
        catch {lappend aetes [$gdteProc]}
    }

    set aeteList [tclAE::createList]
    foreach aete $aetes {
        tclAE::putData $aeteList -1 aete [eval tclAE::buildAETE $aete]
    }
    
    tclAE::putKeyDesc $theReplyAE ---- $aeteList
    
    tclAE::disposeDesc $aeteList
}

proc tclAE::aete::register {gdteProc} {
    global tclAE::aete::gdteProcs
    
    if {![info exists tclAE::aete::gdteProcs]} {
        set tclAE::aete::gdteProcs {}
    }  
    
    if {[lsearch -exact ${tclAE::aete::gdteProcs} $gdteProc] < 0} {
        lappend tclAE::aete::gdteProcs $gdteProc
    }
}

proc tclAE::aete::deregister {gdteProc} {
    global tclAE::aete::gdteProcs
    
    set ix [lsearch -exact ${tclAE::aete::gdteProcs} $gdteProc]
    if {$ix >= 0} {
        set tclAE::aete::gdteProcs [lreplace ${tclAE::aete::gdteProcs} $ix $ix]
    }
}



# ◊◊◊◊ build ◊◊◊◊ #

namespace eval tclAE::aete::build {
    namespace export buildAETE
}

proc tclAE::aete::build::buildAETE {major minor language script suites} {
    set aete [binary format cctt $major $minor $language $script]
    
    # array of suites
    append aete [tclAE::aete::build::buildList tclAE::aete::build::buildSuite $suites]

    # ensure that aggregate is treated as a byte array
    set aete [binary format a* $aete]
    
    return $aete
}

proc tclAE::aete::build::buildSuite {name description ID level version {events {}} {classes {}} {comparisons {}} {enumerations {}}} {
    
    set suite [tclAE::aete::build::buildIdentification $name $description]
    
    # suite ID
    append suite [tclAE::aete::build::buildOSType [encoding convertto macRoman $ID]]
 
    # suite level & version
    append suite [binary format tt $level $version]
    
    # array of events
    append suite [tclAE::aete::build::buildList tclAE::aete::build::buildEvent $events]

    # array of classes
    append suite [tclAE::aete::build::buildList tclAE::aete::build::buildClass $classes]

    # array of comparisons
    append suite [tclAE::aete::build::buildList tclAE::aete::build::buildComparison $comparisons]

    # array of enumerations
    append suite [tclAE::aete::build::buildList tclAE::aete::build::buildEnumeration $enumerations]
    
    return $suite
}

proc tclAE::aete::build::buildEvent {name description class ID reply direct {parameters {}}} {
    
    set event [tclAE::aete::build::buildIdentification $name $description]
    
    # event class & ID
    append event [tclAE::aete::build::buildOSType [encoding convertto macRoman $class]]
    append event [tclAE::aete::build::buildOSType [encoding convertto macRoman $ID]]

    append event [eval tclAE::aete::build::buildReplyParameter $reply]
    append event [eval tclAE::aete::build::buildDirectParameter $direct]
    
    # array of parameters
    append event [tclAE::aete::build::buildList tclAE::aete::build::buildParameter $parameters]
    
    return $event
}

proc tclAE::aete::build::buildParameter {name keyword type description flags} {
    set param [tclAE::aete::build::buildString $name]
    
    append param [tclAE::aete::build::buildOSType [encoding convertto macRoman $keyword]]

    append param [tclAE::aete::build::buildOSType [encoding convertto macRoman $type]]
    
    append param [tclAE::aete::build::buildString $description]
    
    append param [tclAE::aete::build::buildFlags $flags]
    
    return $param
}

proc tclAE::aete::build::buildEnumeration {ID enumerators} {
    set enumeration [tclAE::aete::build::buildOSType [encoding convertto macRoman $ID]]

    append enumeration [tclAE::aete::build::buildList tclAE::aete::build::buildEnumerator $enumerators]
    
    return $enumeration
}

proc tclAE::aete::build::buildEnumerator {name ID description} {
    set enumerator [tclAE::aete::build::buildString $name]
    
    append enumerator [tclAE::aete::build::buildOSType [encoding convertto macRoman $ID]]

    append enumerator [tclAE::aete::build::buildString $description]
    
    return $enumerator
}

proc tclAE::aete::build::buildList {command items} {
    # count of items
    set List [binary format t [llength $items]]
    # array of items
    foreach item $items {
        append List [eval $command $item]
    }
    
    return $List
}

proc tclAE::aete::build::buildDirectParameter {type description flags} {
    set param [tclAE::aete::build::buildOSType [encoding convertto macRoman $type]]

    set descLen [string length $description]
    
    # parameter description (Pascal string)
    append param [binary format ca* $descLen [encoding convertto macRoman $description]]
    
    # alignment byte
    if {[expr {$descLen % 2}] == 0} {
        append param [binary format x]
    } 
    
    append param [tclAE::aete::build::buildFlags $flags]
  
    return $param
}

proc tclAE::aete::build::buildReplyParameter {type description flags} {
    set param [tclAE::aete::build::buildOSType [encoding convertto macRoman $type]]

    set descLen [string length $description]
    
    # parameter description (Pascal string)
    append param [binary format ca* $descLen [encoding convertto macRoman $description]]
    
    # alignment byte
    if {[expr {$descLen % 2}] == 0} {
        append param [binary format x]
    } 
    
    append param [tclAE::aete::build::buildFlags $flags]

    return $param
}

proc tclAE::aete::build::buildString {str} {
    set strLen [string length $str]
    
    # Pascal string
    set String [binary format ca* $strLen [encoding convertto macRoman $str]]
    
    # alignment byte
    if {[expr {$strLen % 2}] == 0} {
        append String [binary format x]
    } 
    
    return $String    
}

proc tclAE::aete::build::buildIdentification {name description} {
    set nameLen [string length $name]
    set descLen [string length $description]
    
    # human-language name (Pascal string)
    set identification [binary format ca* $nameLen [encoding convertto macRoman $name]]
    
    # event description (Pascal string)
    append identification [binary format ca* $descLen [encoding convertto macRoman $description]]
    
    # alignment byte
    if {[expr {($nameLen + $descLen) % 2}] == 1} {
        append identification [binary format x]
    } 
    
    return $identification
}

proc tclAE::aete::build::buildOSType {str} {
    global tcl_platform
    
    if {$tcl_platform(byteOrder) eq "bigEndian"} {
        set String $str
    } else {
        set String [string reverse $str]
    } 
    
    return $String    
}

proc tclAE::aete::build::buildFlags {flags} {
    global tcl_platform

    set result [binary format B16 $flags]
    if {$tcl_platform(byteOrder) eq "littleEndian"} {
        set result [string reverse $result]
    } 
    
    return $result    
}




# ◊◊◊◊ parse ◊◊◊◊ #

namespace eval tclAE::aete::parse {
	namespace export parseAETE
}

proc tclAE::aete::parse::parseAETE {data} {
	binary scan $data cctta* major minor language script data
	
	lappend aete $major $minor $language $script
	
	set suites {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend suites [tclAE::aete::parse::parseSuite data]
	}
	lappend aete $suites
	
	return $aete
}

proc tclAE::aete::parse::parseSuite {stream} {
	upvar $stream data
	
	eval lappend suite [tclAE::aete::parse::parseIdentification data]
	
	# suite ID
	lappend suite [tclAE::aete::parse::parseOSType data]
	
	# suite level & version
	binary scan $data tta* level version data
	lappend suite $level $version
	
	set events {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend events [tclAE::aete::parse::parseEvent data]
	}
	lappend suite $events

	set classes {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend classes [tclAE::aete::parse::parseClass data]
	}
	lappend suite $classes

	set comparisons {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend comparisons [tclAE::aete::parse::parseCompEnum data]
	}
	lappend suite $comparisons
	
	set enumerations {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend enumerations [tclAE::aete::parse::parseEnumeration data]
	}
	lappend suite $enumerations
	
	return $suite
}

proc tclAE::aete::parse::parseEvent {stream} {
	upvar $stream data
	
	eval lappend event [tclAE::aete::parse::parseIdentification data]
	
	# event class & ID
	lappend event [tclAE::aete::parse::parseOSType data]
	lappend event [tclAE::aete::parse::parseOSType data]
	
	lappend event [tclAE::aete::parse::parseMainParameter data]
	lappend event [tclAE::aete::parse::parseMainParameter data]
	
	set parameters {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend parameters [tclAE::aete::parse::parseParameter data]
	}
	lappend event $parameters
	
	return $event
}

proc tclAE::aete::parse::parseClass {stream} {
	upvar $stream data
	
	lappend class [tclAE::aete::parse::parseString data]
	
	lappend class [tclAE::aete::parse::parseOSType data]
	
	lappend class [tclAE::aete::parse::parseString data]

	set properties {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		# class properties
		lappend properties [tclAE::aete::parse::parseParameter data]
	}
	lappend class $properties
	
	set elementClasses {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		# element classes
		lappend elementClasses [tclAE::aete::parse::parseElementClass data]
	}
	lappend class $elementClasses 
	
	return $class
}

proc tclAE::aete::parse::parseCompEnum {stream} {
	upvar $stream data
	
	lappend compEnum [tclAE::aete::parse::parseString data]
	
	lappend compEnum [tclAE::aete::parse::parseOSType data]
	
	lappend compEnum [tclAE::aete::parse::parseString data]
	
	return $compEnum
}

proc tclAE::aete::parse::parseEnumeration {stream} {
	upvar $stream data
	
	lappend enumeration [tclAE::aete::parse::parseOSType data]

	set enumerators {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		# class properties
		lappend enumerators [tclAE::aete::parse::parseCompEnum data]
	}
	lappend enumeration $enumerators
	
	return $enumeration
}

proc tclAE::aete::parse::parseIdentification {stream} {
	upvar $stream data
	
	# human-language name (Pascal string)
	binary scan $data ca* nameLen data
	set nameLen [expr {($nameLen + 0x100) % 0x100}]
	binary scan $data a${nameLen}a* name data
	lappend identification [encoding convertfrom macRoman $name]
	
	# event description (Pascal string)
	binary scan $data ca* descLen data
	set descLen [expr {($descLen + 0x100) % 0x100}]
	binary scan $data a${descLen}a* description data
	lappend identification [encoding convertfrom macRoman $description]
 
	# alignment byte
	if {[expr {($nameLen + $descLen) % 2}] == 1} {
		binary scan $data xa* data
	} 

	return $identification
}

proc tclAE::aete::parse::parseMainParameter {stream} {
	upvar $stream data
	
	lappend parameter [tclAE::aete::parse::parseOSType data]
	
	# parameter description (Pascal string)
	lappend parameter [tclAE::aete::parse::parseString data]

	lappend parameter [tclAE::aete::parse::parseFlags data]
	
	return $parameter
}

proc tclAE::aete::parse::parseParameter {stream} {
	upvar $stream data
	
	lappend parameter [tclAE::aete::parse::parseString data]
	
	lappend parameter [tclAE::aete::parse::parseOSType data]
	lappend parameter [tclAE::aete::parse::parseOSType data]

	lappend parameter [tclAE::aete::parse::parseString data]

	lappend parameter [tclAE::aete::parse::parseFlags data]
	
	return $parameter
}

proc tclAE::aete::parse::parseString {stream} {
	upvar $stream data
	
	binary scan $data ca* strLen data
	set strLen [expr {($strLen + 0x100) % 0x100}]
	binary scan $data a${strLen}a* String data
	
	# alignment byte
	if {[expr {$strLen % 2}] == 0} {
		binary scan $data xa* data
	} 
	
	return [encoding convertfrom macRoman $String]
}

proc tclAE::aete::parse::parseElementClass {stream} {
	upvar $stream data
	
	lappend elementClass [tclAE::aete::parse::parseOSType data]
	
	set keyForms {}
	binary scan $data ta* count data
	for {set i 0} {$i < $count} {incr i} {
		lappend keyForms [tclAE::aete::parse::parseOSType data]
	}
	lappend elementClass $keyForms
	
	return $elementClass
}

proc tclAE::aete::parse::parseOSType {stream} {
	global tcl_platform
	upvar $stream data

	binary scan $data a4a* ID data
	if {$tcl_platform(byteOrder) eq "bigEndian"} {
		set String $ID
	} else {
		set String [string reverse $ID]
	} 
	
	return $String    
}

proc tclAE::aete::parse::parseFlags {stream} {
	global tcl_platform
	upvar $stream data

	binary scan $data B16a* flags data
	if {$tcl_platform(byteOrder) eq "littleEndian"} {
		set flags [string reverse $flags]
	} 
	
	return $flags    
}


namespace eval tclAE {
	namespace import aete::build::*
	namespace import aete::parse::*
}
