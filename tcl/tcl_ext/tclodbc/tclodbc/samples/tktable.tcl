####################################################################
#
# tktable.tcl
#
# View data in a table in tktable widget. 
# 
# Uses utility procedures TkTableInit TkTableRead in utilproc.tcl
#
# This file should be executed in wish shell, having tktable widget 
# installed
#

# Create the db, if not yet created.
set dir [file dirname [info script]]
source $dir/createdb.tcl

# Populate some data to tables
if {[db "select count(*) from table1"] == 0} {source $dir/populate.tcl}

####################################################################
#
# Execution starts here
#

package require Tktable
table .t -variable t -yscrollcommand {.scroll set} -height 10
scrollbar .scroll -command {.t yview}
pack .t .scroll -side left -fill y

wm resizable . 0 1

db statement stmt "select * from Table1 where IntData < 100"
stmt execute

tclodbc::TkTableInit .t stmt
tclodbc::TkTableRead .t stmt

