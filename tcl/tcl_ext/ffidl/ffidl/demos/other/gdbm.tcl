#
# Ffidl interface to GNU dbm database library version 1.8
#
# design, and documentation, taken from Tclgdbm0.6
#
# gdbm open <file> [r|rw|rwc|rwn]
#
# Opens a gdbm database <file> with an optional mode. If the mode is not
# given it is opened for reading (r). The mode can be (r) (read only),
# (rw) (read,write), (rwc) (read,write and create if not already
# existent), and (rwn) (read,write and create a new database regardless
# if one exists). The command returns a handle <name> which is used to
# refer to the open database.
#  
# gdbm close <name>  
# 
# Close a gdbm database with the name <name>.
#  
# gdbm insert <name> <key> <content>  
# 
# <name> is the name of a gdbm database previously opened with gdbm
# open.  Inserts the data <content> giving it the key <key>.  If data
# with <key> is already in the database an error is generated. Nothing
# returned.
#  
# gdbm store  <name> <key> <content>  
# 
# <name> is the name of a gdbm database previously opened with gdbm
# open.  Inserts <content> to the database. If <key> already exists
# the new <content> replaces the old. Nothing returned.
#  
# gdbm fetch  <name> <key>  
#  
# <name> is the name of a gdbm database previously opened with gdbm
# open.  Searches for <key> in the database and returns the associated
# contents, or returns a tcl error if the key is not found.
#  
# gdbm delete  <name> <key> 
#  
# <name> is the name of a gdbm database previously opened with gdbm
# open.  Searches for <key> and deletes it in the database.  If <key> is
# not found an error is generated.  Nothing returned.
#  
# gdbm list  <name>  
# 
# <name> is the name of a gdbm database previously opened with gdbm
# open.  Returns a list of all keys in the database.
#  
# gdbm reorganize  <name>  
# 
# <name> is the name of a gdbm database previously opened with gdbm
# open.  This routine can be used to shrink the size of the database
# file if there have been a lot of deletions.  Nothing returned.
# 
# gdbm exists <name> <key>
# 
# Returns "0" if <key> is not found within the previously opened
# database <name>, "1" otherwise.
# 
# gdbm firstkey <name> 
# gdbm nextkey <name> <lastkey>
# 
# A first/next scheme permits retrieving all keys from a database in
# sequential (but unsorted!) order. gdbm firstkey <name> returns a
# starting key, which may be used to retrieve the following key with
# nextkey. nextkey returns the next key to a given previous key. When no
# next key is available, the empty string is returned.
# 

package provide Gdbm 1.8
package require Ffidl 0.1
package require Ffidlrt 0.1

namespace eval ::gdbm:: {
    set lib gdbm
    set nhandle 0
    array set handles {
    }
    array set writemodes {
    }
    array set symbols {
    }
    array set modes {
	r GDBM_READER
	rw GDBM_WRITER
	rwc GDBM_WRCREAT
	rwn GDBM_NEWDB
    }
    array set constants {
	GDBM_INSERT 0
	GDBM_REPLACE 1
	GDBM_READER 0
	GDBM_WRITER 1
	GDBM_WRCREAT 2
	GDBM_NEWDB 3
    }
}

#
# find library
#
set ::gdbm::lib [::ffidl::find-lib gdbm]

#
# typedefs
#
::ffidl::typedef GDBM_FILE pointer
::ffidl::typedef gdbm_datum pointer int

#
# symbols
#
set ::gdbm::symbols(gdbm_errno) [::ffidl::symbol $::gdbm::lib gdbm_errno]

#
# bindings
#
::ffidl::callout ::gdbm::gdbm_open {pointer-utf8 int int int pointer} GDBM_FILE [::ffidl::symbol $::gdbm::lib gdbm_open]
::ffidl::callout ::gdbm::gdbm_close {GDBM_FILE} void [::ffidl::symbol $::gdbm::lib gdbm_close]
::ffidl::callout ::gdbm::gdbm_store {GDBM_FILE gdbm_datum gdbm_datum int} int [::ffidl::symbol $::gdbm::lib gdbm_store]
::ffidl::callout ::gdbm::gdbm_fetch {GDBM_FILE gdbm_datum} gdbm_datum [::ffidl::symbol $::gdbm::lib gdbm_fetch]
::ffidl::callout ::gdbm::gdbm_delete {GDBM_FILE gdbm_datum} int [::ffidl::symbol $::gdbm::lib gdbm_delete]
::ffidl::callout ::gdbm::gdbm_firstkey {GDBM_FILE} gdbm_datum [::ffidl::symbol $::gdbm::lib gdbm_firstkey]
::ffidl::callout ::gdbm::gdbm_nextkey {GDBM_FILE gdbm_datum} gdbm_datum [::ffidl::symbol $::gdbm::lib gdbm_nextkey]
::ffidl::callout ::gdbm::gdbm_reorganize {GDBM_FILE} int [::ffidl::symbol $::gdbm::lib gdbm_reorganize]
::ffidl::callout ::gdbm::gdbm_sync {GDBM_FILE} void [::ffidl::symbol $::gdbm::lib gdbm_sync]
::ffidl::callout ::gdbm::gdbm_exists {GDBM_FILE gdbm_datum} int [::ffidl::symbol $::gdbm::lib gdbm_exists]
::ffidl::callout ::gdbm::gdbm_setopt {GDBM_FILE int pointer-byte int} int [::ffidl::symbol $::gdbm::lib gdbm_setopt]
::ffidl::callout ::gdbm::gdbm_strerror {int} pointer-utf8 [::ffidl::symbol $::gdbm::lib gdbm_strerror]

#
# helpers, create or extract the gdbm_datum structure
# note, these are currently built for string, ie utf8,
# data, so its important that each datum created has
# the nul terminator included.
#
# this could be rewritten to allow binary data.
#
proc make-datum {string} {
    if {[string length $string] == 0} {
	binary format [::ffidl::info format gdbm_datum] 0 0
    } else {
	binary format [::ffidl::info format gdbm_datum] [::ffidl::get-string $string] [expr {1+[string length $string]}]
    }
}
proc extract-datum {datum} {
    binary scan $datum [::ffidl::info format gdbm_datum] string length
    if {$string == 0} {
	set result {}
    } else {
	set result [::ffidl::pointer-into-string $string]
	::ffidl::free $string
    }
    set result
}

#
# commands
#
proc ::gdbm::cmd-open {file {mode r}} {
    variable nhandle
    variable handles
    variable writemodes
    variable modes
    variable constants
    set h [gdbm_open $file 0 $constants($modes($mode)) 0644 0]
    if {$h == 0} { error "could not open: $file" }
    set name gdbm[incr nhandle]
    set handles($name) $h
    set writemodes($name) GDBM_REPLACE
    set name
}
proc ::gdbm::cmd-close {name} {
    variable handles
    variable writemodes
    set h $handles($name)
    unset handles($name)
    unset writemodes($name)
    gdbm_close $h
}
proc ::gdbm::cmd-insert {name key content} {
    variable handles
    variable constants
    switch [gdbm_store $handles($name) [make-datum $key] [make-datum $content] $constants(GDBM_INSERT)] {
	0 { return }
	1 { error "cannot insert \"$key\" into database, key already exists" }
	-1 { error "cannot insert \"$key\" into database, not opened for writing or invalid data" }
    }
}
proc ::gdbm::cmd-store {name key content} {
    variable handles
    variable writemodes
    variable constants
    switch [gdbm_store $handles($name) [make-datum $key] [make-datum $content] $constants($writemodes($name))] {
	0 { return }
	1 { error "cannot insert \"$key\" into database, key already exists" }
	-1 { error "cannot insert \"$key\" into database, not opened for writing or invalid data" }
    }
}
proc ::gdbm::cmd-fetch {name key} {
    variable handles
    extract-datum [gdbm_fetch $handles($name) [make-datum $key]]
}
proc ::gdbm::cmd-delete {name key} {
    variable handles
    gdbm_delete $handles($name) [make-datum $key]
}
proc ::gdbm::cmd-exists {name key} {
    variable handles
    gdbm_exists $handles($name) [make-datum $key]
}
proc ::gdbm::cmd-list {name} {
    variable handles
    set format [::ffidl::info format gdbm_datum]
    set list {}
    set key [gdbm_firstkey $handles($name)]
    binary scan $key $format string length
    while {$string != 0} {
	lappend list [::ffidl::pointer-into-string $string]
	set key [gdbm_nextkey $handles($name) $key]
	::ffidl::free $string
	binary scan $key $format string length
    }
    set list
}
proc ::gdbm::cmd-reorganize {name} {
    variable handles
    gdbm_reorganize $handles($name)
}
proc ::gdbm::cmd-firstkey {name} {
    variable handles
    extract-datum [gdbm_firstkey $handles($name)]
}
proc ::gdbm::cmd-nextkey {name lastkey} {
    variable handles
    extract-datum [gdbm_nextkey $handles($name) [make-datum $lastkey]]
}
proc ::gdbm::cmd-error {which} {
    variable symbols
    switch $which {
	number { peek-int $symbols(gdbm_errno) }
	text { gdbm_strerror [peek-int gdbm_errno] }
	default { error "usage: gdbm error number|text" }
    }
}
proc ::gdbm::cmd-writemode {name writemode} {
    variable handles
    variable writemodes
    switch $which {
	replace { set writemodes($name) GDBM_REPLACE }
	insert { set writemodes($name) GDBM_INSERT }
	default { error "usage: gdbm writemode name replace|insert" }
    }
}

proc gdbm args {
    if {[llength $args] == 0} {
	error "usage: gdbm open|close|insert|store|fetch|delete|exists|list|reorganize|firstkey|nextkey|error|writemode"
    }
    eval ::gdbm::cmd-[lindex $args 0] [lrange $args 1 end]
}
