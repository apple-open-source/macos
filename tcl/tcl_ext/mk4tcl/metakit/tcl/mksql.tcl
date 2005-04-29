# mksql.tcl --
# $Id: mksql.tcl,v 1.8 2003/11/23 01:42:51 wcvs Exp $
# This is part of Metakit, see http://www.equi4.com/metakit/
# Copyright (C) 2000-2003 by Matt Newman and Jean-Claude Wippler.
#
# This is an experimental wrapper around the new MkSQL engine.
# It tries to conform to the Tclodbc API.
#
# Examples of usage:
#
# set db [mk::sql #auto data.mk -readonly]
#
# foreach row [$db "select * from serves"] {
#   puts $row
# }
#
# OR using read:
#
# $db read likes "select * from likes"
# puts $likes(joe,beer)
# puts $likes(joe,perday)

package provide mk.sql 0.1

# Public name
proc mk::sql {args} {
  # invoke private namespace - retain relative stack level
  uplevel 1 [linsert $args 0 mksql::new db]
}
#
# Private
#
namespace eval mksql {
  variable prefix   [namespace current]
  variable debug    1
  variable uid    0

  variable options      ;# maps $this,option -> value
  variable sto        ;# maps $this -> c4_Storage name
  variable sqlcmd       ;# maps $this -> SQL processor
  variable dbfile       ;# maps $this -> open file
  variable stmt       ;# maps $this -> SQL Query
  variable results      ;# maps $this -> SQL Results
  variable columns      ;# maps $this -> SQL Columns
  #
  #
  # colinfo maps c4_Property name ->
  # DATA_TYPE TYPE_NAME PRECISION LENGTH SCALE RADIX NULLABLE REMARKS
  #
  # Should really have this contained inside mk4tcl as a fake (in-memory)
  # c4_View called systypes or some-such....
  variable typeinfo
  array set typeinfo {
    M {-4 image 2147483647 2147483647 {} {} 1 {}}
    B {-3 varbinary 255 255 {} {} 1 {}}
    I {4 int 10 4 0 10 0 {}}
    F {6 float 10 8 0 10 0 {}}
    D {7 real 10 8 0 10 0 {}}
    D {8 double 10 8 0 10 0 {}}
    T {11 datetime 23 16 3 10 1 {unsupported}}
    S {12 varchar 255 255 {} {} 1 {}}
  }
  # Note: actually metakit doesn't have a 'T' (datetime)
}
#
#
# Private Module
#
proc mksql::log {this msg {lvl 1}} {
  variable debug
  if {$lvl <= $debug} {
    tclLog "$this: $msg"
  }
}
#
# Object Constructor/Destructor - a bit overblow, but
# new to share alot of stuff between "statement" and "database"
# "classes"....
# public methods are class.name
# protected methods are class-name
#
proc mksql::new {Class this args} {
  variable prefix
  variable uid

  if {$this == "#auto"} {
    while 1 {
      set this ${prefix}[incr uid]
      if {[info commands $this] == ""} break
    }
  } else {
    #
    # this may contain namespace components
    #
    if {![string match ::* $this]} {
      set ns [uplevel 1 namespace current]
      if {$ns == "::"} {
        set this ::$this
      } else {
        set this ${ns}::$this
      }
    }
    incr uid
  }
  # simple OO framework, shared between "database" and "statement" commands
  variable usage
  variable class

  set class($this) $Class

  # set usage string...
  set tmp [lsort [info commands ${prefix}::$class($this).*]]
  regsub -all "${prefix}::$class($this)." $tmp {} tmp 
  set usage($this) "ambiguous option \"%s\": must be [join $tmp ", "]"

  # execute constructor...
  eval [linsert $args 0 $class($this)-constructor $this]

  proc $this {option args} [format {
    set this %s
    set prefix %s
    set class %s

    # support extending the "methods"
    set match [info commands ${prefix}::${class}.${option}*]

    if {[llength $match] == 1} {

      return [uplevel 1 [linsert $args 0 [lindex $match 0] $this]]

    } else {
      variable ${prefix}::unknown
      variable ${prefix}::usage

      if {[llength $match] == 0 && [info exists unknown($this)]} {
        uplevel 1 [linsert $args 0 $unknown($this) $this $option]
      } else {
        return -code error [format $usage($this) $option]
      }
    }
  } [list $this] [list $prefix] [list $class($this)]]

  return $this
}

proc mksql::destroy {this} {
  variable prefix
  variable options
  variable class

  $class($this)-destructor $this

  foreach v {class usage unknown} {
    variable $v
    if {[info exists ${v}($this)]} {
      unset ${v}($this)
    }
  }
  foreach key [array names options $this,*] {
    unset options($key)
  }
  rename $this ""
}
#
# Database class - call-compatibable with TclODBC
#
proc mksql::db-constructor {this {file ""} args} {
  # core vars
  variable prefix
  variable uid
  variable unknown
  variable usage

  eval [linsert $args 0 mk::file open sql$uid $file]

  stmt-constructor $this sql$uid $file ""

  # override...
  set unknown($this) ${prefix}::db-execute
  append usage($this) ", or an sql statement"
}

proc mksql::db-destructor {this} {
  variable sto

  set db $sto($this)

  stmt-destructor $this

  mk::file close $db
}

#
# SQL Operations
#
proc mksql::db.disconnect {this} {
  destroy $this
}

proc mksql::db.eval {this command sql {argspec ""} args} {
  variable prefix

  stmt-compile $this $sql $argspec

  # Execute the "Compiled" statement
  eval [linsert $args 0 stmt.execute $this]

  uplevel 1 [linsert $args 0 ${prefix}::stmt.eval $this $command]
}
#
# called as default method for "database" instances
#
proc mksql::db-execute {this sql {argspec ""} args} {

  stmt-compile $this $sql $argspec

  eval [linsert $args 0 stmt.run $this]
}

proc mksql::db.read {this arrspec sql {argspec ""} args} {
  variable prefix

  stmt-compile $this $sql $argspec

  # Execute the "Compiled" statement
  eval [linsert $args 0 stmt.execute $this]

  # Need uplevel due to array scoping
  return [uplevel 1 [linsert $args 0 ${prefix}::stmt.read $this $arrspec]]
}

proc mksql::db.statement {this name sql {argspec ""}} {
#  return -code error "method \"statement\" not implemented"
  variable dbfile
  variable sto

  set cmd [list mksql::new stmt $name $sto($this) $dbfile($this) $sql $argspec]
  set name [uplevel 1 $cmd]
}

#
# Introspection
#
proc mksql::db.columns {this {pattern *}} {

  stmt-compile $this columns

  return [stmt.run $this $pattern]
}

proc mksql::db.indexes {this table} {
  # Who needs them <grin>
  return {}
}

proc mksql::db.tables {this {pattern *}} {

  stmt-compile $this tables

  return [stmt.run $this $pattern]
}

proc mksql::db.typeinfo {this {typeid ignored}} {

  stmt-compile $this typeinfo

  return [stmt.run $this $typeid]
}
#
# Options
#
proc mksql::db.get {this option} {
  return [stmt.get $this $option]
}

proc mksql::db.set {this option value} {
  return [stmt.set $this $option $value]
}
#
# Commit/Rollback
#
proc mksql::db.commit {this} {
  variable sto

  mk::file commit $sto($this)
  return
}

proc mksql::db.rollback {this} {
  variable sto

  mk::file rollback $sto($this)
  return
}
#
# Statement Support - should fix the above code to work
# like this and turn eveything back-to-front
#
proc mksql::stmt-constructor {this db file sql {argspec ""}} {
  # core vars
  variable prefix
  variable options
  variable uid

  # initialize variables...
  foreach v {columns dbfile results stmt sqlcmd sto} {
    variable $v
    set ${v}($this) ""
  }
  set options($this,autocommit) 0
  set options($this,maxrows) 0

  set dbfile($this) $file
  set sqlcmd($this) ${prefix}::Q$uid
  set sto($this)    $db

log $this "db=$db, file=$file, sql=$sql, argspec=$argspec"
  mk::Q $sto($this) $sqlcmd($this)

  if {$sql != ""} {
    stmt-compile $this $sql $argspec
  }
}

proc mksql::stmt-destructor {this} {
  variable prefix
  variable class
  variable sqlcmd

  log $this "rename $sqlcmd($this) {}"
# Fails when the mk::Q command is deleted, needs to be fixed, JCW
# rename $sqlcmd($this) ""

  foreach v {columns dbfile results stmt sqlcmd sto} {
    variable $v
    unset ${v}($this)
  }
}

proc mksql::stmt-compile {this sql {argspec ""}} {
  variable columns
  variable results
  variable stmt

  # Discard old information
  set columns($this) {}
  set results($this) {}

  # "Compile" the statement
  set stmt($this) $sql

  log $this "compiled $sql"
}

proc mksql::stmt.run {this args} {
  variable results

  eval [linsert $args 0 stmt.execute $this]
  set data $results($this)
  set results($this) {}
  return $data
}

proc mksql::stmt.execute {this args} {
  variable options
  variable columns
  variable results
  variable sqlcmd
  variable stmt
  variable sto

  # Physically "Execute" the "Compiled" statement...
  log $this "executing ..." 2

  switch -- $stmt($this) {
  columns   { set data [eval stmt-columns $this $args] ; set hdr 1 }
  tables    { set data [eval stmt-tables $this $args] ; set hdr 1 }
  typeinfo  { set data [eval stmt-typeinfo $this $args] ; set hdr 1 }
  default   { set data [$sqlcmd($this) $stmt($this)] ;set hdr 0 }
  }

  if {$options($this,autocommit)} {
    # Yuck - should fix metakit to optimize the commit - i.e. 
    # have it know if anything changed.....
    if {![regexp -nocase "^\[ \t\]*select" $stmt($this)]} {
      log $this "commiting ..."

      mk::file commit $sto($this)
    }
  }
  #
  # Setup column and typeinfo for result set....
  #
  set row0 [lindex $data 0]

  if {$hdr} {# XXX - NEED TO CHANGE mk::Q to return props ...Soon...
    set columns($this) $row0
    set results($this) [lrange $data 1 end]
  } else {# Fake...
    set columns($this) {}
    for {set i 0} {$i < [llength $row0]} {incr i} {
      lappend columns($this) noname${i}
    }
    set results($this) $data
  }
  # Row LIMIT?
  if {$options($this,maxrows) > 0} {
    set results($this) [lrange $results($this) 0 [expr {$options($this,maxrows)-1}]]
  }
  log $this "returned [llength $results($this)] rows, [llength $columns($this)] cols"
  log $this "columns: $columns($this)" 2

  return OK
}

proc mksql::stmt.fetch {this {arr ""} {cols ""}} {
  variable results

  set row [lindex $results($this) 0]
  set results($this) [lrange $results($this) 1 end]

  if {$arr != ""} {
    return $row
  } else {
    if {[llength $row] == 0} {
      return 0
    }
    upvar 1 $arr a

    if {[llength $cols] == 0} {
      variable columns
      set cols $columns($this)
    }
    foreach val $row fld $cols {
      set a($fld) $val
    }
    return 1
  }
}

proc mksql::stmt.rowcount {this} {
  # this doesn't seem to work for MS Access or SqlServer
  # Returns the rowcount for thw number of rows effected by
  # the last insert/update/delete/select
  return -1
}

# XXX WRONG
proc mksql::stmt.columns {this} {
  variable columns
  variable dbfile
  variable typeinfo

  # Should return:
  # (TABLE_QUALIFIER TABLE_OWNER TABLE_NAME COLUMN_NAME DATA_TYPE
  # TYPE_NAME PRECISION LENGTH SCALE RADIX NULLABLE REMARKS ORDINAL)*

  set result {}
  set ord 0
  foreach prop $columns($this) {
    set sp [split $prop :]
    if {[llength $sp] == 1} {
      set type S
    } else {
      set type [lindex $sp 1]
    }
    set col [lindex $sp 0]
    # XXX - no way of knowing the "table name"
    set row [list $dbfile($this) {} ??? $col]
  
    lappend result [concat $row $typeinfo($type) [incr ord]]
  }
  return $result
}

proc mksql::stmt.set {this option value} {
  variable options

  if {![info exists options($this,$option)]} {
    set tmp [array names options $this,*]
    regsub -all "$this," $tmp {} tmp
    set opts [join [lsort [split $tmp]] ", "]
    return -code error "bad option \"$option\": should be $opts"
  }
  #
  # XXX - not strictly correct - autocommit it not a valid
  # option for a "statement" since it is really a "property" of
  # the underlying "database" object.
  #
  switch -- $option {
  autocommit {
    switch -- $value {
    1 - true - yes - on {
      return [set options($this,autocommit) 1]
    }
    0 - false - no - off {
      return [set options($this,autocommit) 0]
    }
    default {
      return -code error "bad boolean \"$value\""
    }
    };#sw
  }
  default { set options($this,$option) $value }
  };#sw
}

proc mksql::stmt.get {this option} {
  variable options

  if {![info exists options($this,$option)]} {
    set tmp [array names options $this,*]
    regsub -all "$this," $tmp {} tmp
    set opts [join [lsort [split $tmp]] ", "]
    return -code error "bad option \"$option\": should be $opts"
  }
  return $options($this,$option)
}

proc mksql::stmt.drop {this} {
  destroy $this
}

proc mksql::stmt.eval {this command} {
  variable results
  #
  # This would be a good example of the power of mk if
  # the sql layer returned a c4_View.... because it would look
  # like:
  # set view [...sql2view $sql]
  # mk::loop cur $view {
  #  uplevel 1 [concat $command [mk::get $cur]]
  # }
  # $view release (or drop or destory etc. )
  # Well almost, we would need mk::get to take an
  # option to return a positional list instead of
  # key value, but you get the point....
  #
  foreach row $results($this) {
    tclLog [concat $command $row]
    uplevel 1 [concat $command $row]
  }
  set results($this) {}
}

proc mksql::stmt.read {this arrspec args} {
  variable columns
  variable results

  if {[llength $results($this)] == 0} {
    return ""
  }
  set row0 [lindex $results($this) 0]
  if {[llength $row0] < 2} {
    return -code error "read is undefined when query contains only one column"
  }
  set arrlen [llength $arrspec]

  if {($arrlen + 1) == [llength $row0]} {
    # multiple arrays
    set i 1
    foreach arr $arrspec {
      upvar 1 $arr a$i
      incr i
    }
    foreach row $results($this) {
      set key [lindex $row 0]

      set i 0
      foreach val $row {
        set a${i}($key) $val
        incr i
      }
    }
  } elseif {$arrlen == 1} {
    upvar 1 $arrspec a

    set i 0 
    foreach col $columns($this) {
      set c($i) $col
      incr i
    }

    foreach row $results($this) {
      set key [lindex $row 0]

      set i 0
      foreach val $row {
        set a($key,$c($i)) $val
        incr i
      }
    }
  } else {
    return -code error "mismatch between arrspec and query results"
  }
  set results($this) {}
}
#
# dynamic views
#
proc mksql::stmt-columns {this {pattern *}} {
  variable dbfile
  variable sto
  variable typeinfo

  set cols "TABLE_QUALIFIER TABLE_OWNER TABLE_NAME COLUMN_NAME DATA_TYPE \
    TYPE_NAME PRECISION LENGTH SCALE RADIX NULLABLE REMARKS ORDINAL"

  set result [list $cols]
  foreach view [mk::file views $sto($this)] {
    if {![string match $pattern $view]} { continue }

    set ord 0
    foreach prop [mk::view info $sto($this).$view] {
      set sp [split $prop :]
      if {[llength $sp] == 1} {
        set type S
      } else {
        set type [lindex $sp 1]
      }
      set col [lindex $sp 0]
      set row [list $dbfile($this) {} $view $col]
    
      lappend result [concat $row $typeinfo($type) [incr ord]]
    }
  }
  return $result
}

proc mksql::stmt-tables {this {pattern *}} {
  variable sto
  variable dbfile

  # Returns:
  set cols {TABLE_QUALIFIER TABLE_OWNER TABLE_NAME TABLE_TYPE REMARKS}

  set result [list $cols]
  foreach view [mk::file views $sto($this)] {
    if {![string match $pattern $view]} { continue }

    set layout [mk::view layout $sto($this).$view]

    lappend result [list $dbfile($this) {} $view table $layout]
  }
  return $result
}

proc mksql::stmt-typeinfo {this {typeid ignored}} {
  # Does the phrase YUCK ring able bells?
  #
  # Should really have this contained inside mk4tcl as a fake (in-memory)
  # c4_View called systypes or some-such....
  # This data was captured from SqlServer and represents static
  # type info usually supplied via ODBC
  # 
  # Given Metakit's adaptive integer support it can claim to support all
  # of the int variations....
  #
  return {
{TYPE_NAME DATA_TYPE COLUMN_SIZE LITERAL_PREFIX LITERAL_SUFFIX CREATE_PARAMS NULLABLE CASE_SENSITIVE SEARCHABLE UNSIGNED_ATTRIBUTE FIXED_PREC_SCALE AUTO_UNIQUE_VALUE LOCAL_TYPE_NAME MINIMUM_SCALE MAXIMUM_SCALE SQL_DATA_TYPE SQL_DATETIME_SUB NUM_PREC_RADIX INTERVAL_PRECISION USERTYPE}
{bit -7 1 {} {} {} 0 0 2 {} 0 {} bit 0 0 -7 {} 2 {} 16}
{tinyint -6 3 {} {} {} 1 0 2 1 0 0 tinyint 0 0 -6 {} 10 {} 5}
{image -4 2147483647 0x {} {} 1 0 0 {} 0 {} image {} {} -4 {} {} {} 20}
{varbinary -3 255 0x {} {} 1 0 2 {} 0 {} varbinary {} {} -3 {} {} {} 4}
{binary -2 255 0x {} {} 1 0 2 {} 0 {} binary {} {} -2 {} {} {} 3}
{timestamp -2 8 0x {} {} 0 0 2 {} 0 {} timestamp {} {} -2 {} {} {} 80}
{text -1 2147483647 ' ' {} 1 0 1 {} 0 {} text {} {} -1 {} {} {} 19}
{char 1 255 ' ' length 1 0 3 {} 0 {} char {} {} 1 {} {} {} 1}
{numeric 2 28 {} {} precision,scale 1 0 2 0 0 0 numeric 0 28 2 {} 10 {} 25}
{decimal 3 28 {} {} precision,scale 1 0 2 0 0 0 decimal 0 28 3 {} 10 {} 26}
{money 3 19 {$} {} {} 1 0 2 0 1 0 money 4 4 3 {} 10 {} 11}
{smallmoney 3 10 {$} {} {} 1 0 2 0 1 0 smallmoney 4 4 3 {} 10 {} 21}
{int 4 10 {} {} {} 1 0 2 0 0 0 int 0 0 4 {} 10 {} 7}
{{int identity} 4 10 {} {} {} 0 0 2 0 0 1 {int identity} 0 0 4 {} 10 {} 7}
{smallint 5 5 {} {} {} 1 0 2 0 0 0 smallint 0 0 5 {} 10 {} 6}
{float 6 15 {} {} {} 1 0 2 0 0 0 float {} {} 6 {} 10 {} 8}
{real 7 7 {} {} {} 1 0 2 0 0 0 real {} {} 7 {} 10 {} 23}
{datetime 11 23 ' ' {} 1 0 3 {} 0 {} datetime 3 3 9 3 10 {} 12}
{smalldatetime 11 16 ' ' {} 1 0 3 {} 0 {} smalldatetime 0 0 9 3 10 {} 22}
{varchar 12 255 ' ' {max length} 1 0 3 {} 0 {} varchar {} {} 12 {} {} {} 2}
   }
}
