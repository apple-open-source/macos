#
# Ffidl interface to Tcl8.2
#
# Run time support for Ffidl.
#
package provide Ffidlrt 0.1
package require Ffidl

namespace eval ::ffidl:: {}

proc ::ffidl::find-pkg-lib {pkg} {
    package require $pkg
    foreach i [::info loaded {}] {
        foreach {l p} $i {}
        if {$p eq "$pkg"} {
            return $l
        }
    }
    # ignore errors when running under pkg_mkIndex:
    if {![llength [info commands __package_orig]] } {
        return -code error "Library for package $pkg not found"
    }
}

namespace eval ::ffidl:: {
    set ffidl_lib [find-pkg-lib Ffidl]
    array set libs [list ffidl $ffidl_lib ffidl_test $ffidl_lib]
    unset ffidl_lib
    
    # 'libs' array is used by the ::ffidl::find-lib
    # abstraction to store the resolved lib paths
    #
    # 'types' and 'typedefs' arrays are used by the ::ffidl::find-type
    # abstraction to store resolved system types
    # and whether they have already been defined
    # with ::ffidl::typedef
    array set typedefs {}
    switch -exact $tcl_platform(platform) {
	unix {
	    switch -glob $tcl_platform(os) {
                Darwin {
                    array set libs {
                        c libSystem.dylib
                        m libSystem.dylib
                        gdbm {}
                        gmp {}
                        mathswig libmathswig0.5.dylib
                    }
                    array set types {
                        size_t {{unsigned long}}
                        clock_t {{unsigned long}}
                        time_t long
                        timeval {uint32 uint32}
                    }
                }
	    }
	}
    }
}

#
# find a shared library given a root name
# this is an abstraction in search of a
# solution.
#
proc ::ffidl::find-lib {root} {
    upvar \#0 ::ffidl::libs libs
    if { ! [::info exists libs($root)] || [llength libs($root)] == 0} {
	error "::ffidl::find-lib $root - no mapping defined for $root"
    }
    if {[llength $libs($root)] > 1} {
	foreach l $libs($root) {
	    if {[file exists $l]} {
		set libs($root) $l
		break
	    }
	}
    }
    lindex $libs($root) 0
}

#
# find a typedef for a standard type
# and define it with ::ffidl::typedef
# if not already done
#
# currently wired for my linux box
#
proc ::ffidl::find-type {type} {
    upvar \#0 ::ffidl::types types
    upvar \#0 ::ffidl::typedefs typedefs
    if { ! [::info exists types($type)]} {
	error "::ffidl::find-type $type - no mapping defined for $type"
    }
    if { ! [::info exists typedefs($type)]} {
	eval ::ffidl::typedef $type $types($type)
	set typedefs($type) 1
    }
}
    
#
# get the address of the string rep of a Tcl_Obj
# get the address of the unicode rep of a Tcl_Obj
# get the address of the bytearray rep of a Tcl_Obj
#
# CAUTION - anything which alters the Tcl_Obj may
# invalidate the results of this function.  Use
# only in circumstances where the Tcl_Obj will not
# be modified in any way.
#
# CAUTION - the memory pointed to by the addresses
# returned by ::ffidl::get-string and ::ffidl::get-unicode
# is managed by Tcl, the contents should never be
# modified.
#
# The memory pointed to by ::ffidl::get-bytearray may
# be modified if care is taken to respect its size,
# and if shared references to the bytearray object
# are known to be compatible with the modification.
#

::ffidl::callout ::ffidl::get-string {pointer-obj} pointer [::ffidl::stubsymbol tcl stubs 340]; #Tcl_GetString
::ffidl::callout ::ffidl::get-unicode {pointer-obj} pointer [::ffidl::stubsymbol tcl stubs 382]; #Tcl_GetUnicode
::ffidl::callout ::ffidl::get-bytearray-from-obj {pointer-obj pointer-var} pointer [::ffidl::stubsymbol tcl stubs 33]; #Tcl_GetByteArrayFromObj

proc ::ffidl::get-bytearray {obj} {
    set len [binary format [::ffidl::info format int] 0]
    ::ffidl::get-bytearray-from-obj $obj len
}

#
# create a new string Tcl_Obj
# create a new unicode Tcl_Obj
# create a new bytearray Tcl_Obj
#
# I'm not sure if these are actually useful
#

::ffidl::callout ::ffidl::new-string {pointer int} pointer-obj [::ffidl::stubsymbol tcl stubs 56]; #Tcl_NewStringObj
::ffidl::callout ::ffidl::new-unicode {pointer int} pointer-obj [::ffidl::stubsymbol tcl stubs 378]; #Tcl_NewUnicodeObj
::ffidl::callout ::ffidl::new-bytearray {pointer int} pointer-obj [::ffidl::stubsymbol tcl stubs 50]; #Tcl_NewByteArrayObj

#
# access the standard allocator, malloc, free, realloc
#
::ffidl::find-type size_t
::ffidl::callout ::ffidl::malloc {size_t} pointer [::ffidl::symbol [::ffidl::find-lib c] malloc]
::ffidl::callout ::ffidl::realloc {pointer size_t} pointer [::ffidl::symbol [::ffidl::find-lib c] realloc]
::ffidl::callout ::ffidl::free {pointer} void [::ffidl::symbol [::ffidl::find-lib c] free]

#
# Copy some memory at some location into a Tcl bytearray.
#
# Needless to say, this can be very hazardous to your
# program's health if things aren't sized correctly.
#

::ffidl::callout ::ffidl::memcpy {pointer-var pointer int} pointer [::ffidl::symbol [::ffidl::find-lib c] memcpy]

proc ::ffidl::peek {address nbytes} {
    set dst [binary format x$nbytes]
    ::ffidl::memcpy dst $address $nbytes
    set dst
}

#
# convert raw pointers, as integers, into Tcl_Obj's
#
::ffidl::callout ::ffidl::pointer-into-string {pointer} pointer-utf8 [::ffidl::symbol [::ffidl::find-lib ffidl] ffidl_pointer_pun]
::ffidl::callout ::ffidl::pointer-into-unicode {pointer} pointer-utf16 [::ffidl::symbol [::ffidl::find-lib ffidl] ffidl_pointer_pun]
proc ::ffidl::pointer-into-bytearray {pointer length} {
    set bytes [binary format x$length]
    ::ffidl::memcpy [::ffidl::get-bytearray $bytes] $pointer $length
    set bytes
}
