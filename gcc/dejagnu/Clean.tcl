#!/usr/bin/tclsh

# Clean.tcl
#	This script is used to remove all unwanted files from a
#	directory not in the .clean file list. This should only
#	be used by maintainers when producing a release.

# Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Please email any bugs, comments, and/or additions to this file to:
# bug-dejagnu@gnu.org

# This file was written by Rob Savoye. (rob@welcomehome.org)

# default to no verbosity
if ![info exists verbose] {
    set verbose 0
}

proc usage { } {
    puts "USAGE: Clean.tcl \[options...\]"
    puts "\t--verbose (-v)\t\tPrint all test output to screen"
}

# print a message if it's verbosity is greater than the default
proc verbose { args } {
    global verbose
    set newline 1

    set i 0
    if { [string index [lindex $args 0] 0] == "-" } {
	for { set i 0 } { $i < [llength $args] } { incr i } {
	    if { [lindex $args $i] == "--" } {
		incr i
		break
	    } elseif { [lindex $args $i] == "-n" } {
		set newline 0
	    } elseif { [string index [lindex $args $i] 0] == "-" } {
		puts "ERROR: verbose: illegal argument: [lindex $args $i]"
		return
	    } else {
		break
	    }
	}
	if { [llength $args] == $i } {
	    puts "ERROR: verbose: nothing to print"
	    return
	}
    }

    set level 1
    if { [llength $args] > $i + 1 } {
	set level [lindex $args [expr $i+1]]
    }
    set message [lindex $args $i]
    
    if { $verbose >= $level } {
	# There is no need for the "--" argument here, but play it safe.
	# We assume send_user also sends the text to the log file (which
	# appears to be the case though the docs aren't clear on this).
	if { $newline } {
	    puts -nonewline "$message\n"
	} else {
	    puts -nonewline "$message"
	}
    }
}

# process the command line arguments
for { set i 0 } { $i < $argc } { incr i } {
    set option [lindex $argv $i]

    # make all options have two hyphens
    switch -glob -- $option {
        "--*" {
        }
        "-*" {
	    set option "-$option"
        }
    }


    switch -glob -- $option {
	"--he*" {			# (--help) help text
	    usage;
	    exit 0	
	}

	"--v" -
	"--verb*" {			# (--verbose) verbose output
	    incr verbose
	    continue
	}
    }
}
verbose "Verbose level is $verbose" 2

proc cleanfiles { directory } {
    set filelist ""

    # get a list of all the files in this directory
    set allfiles [glob -nocomplain "$directory/*"]
    regsub -all "$directory/" $allfiles "" allfiles
    
    # open the .clean file, which has the list of stuff we
    # want to save
    catch "set cleanfile [open "$directory/.clean" r]"
    if ![info exists cleanfile] {
	verbose "WARNING: no .clean file in $directory, removing the default set of \"*! core CVS RCS\"" 3
	set allfiles [glob -nocomplain "$directory/*~"]
	append allfiles " [glob -nocomplain "$directory/core*"]"
	append allfiles " [glob -nocomplain "$directory/CVS"]"
	append allfiles " [glob -nocomplain "$directory/Cvs.*"]"
	append allfiles " [glob -nocomplain "$directory/*.out"]"
	append allfiles " [glob -nocomplain "$directory/*.dvi"]"
	append allfiles " [glob -nocomplain "$directory/*.rej"]"
	append allfiles " [glob -nocomplain "$directory/*.orig"]"
	append allfiles " [glob -nocomplain "$directory/*.log"]"
	append allfiles " [glob -nocomplain "$directory/*.cvsignore"]"
	append allfiles " [glob -nocomplain "$directory/*.tgz"]"
	append allfiles " [glob -nocomplain "$directory/*.tar.gz"]"
	append allfiles " [glob -nocomplain "$directory/autom4te.cache"]"
	append allfiles " [glob -nocomplain "$directory/RCS"]"
	append allfiles " [glob -nocomplain "$directory/.\#*"]"
    } else {
	# read in the .clean file, line by line
	while { [gets $cleanfile cur_line]>=0 } {
	    # ignore comments
	    if { [string index $cur_line 0] == "\#" } {
		verbose "Ignoring comment" 2
		continue
	    } 
	    # ignore blank lines
	    if { [string length $cur_line]<=0 } {
		verbose "Ignoring blank line" 2
		continue
	    } 
	    regsub -all "\[\+\]" $cur_line "\\+" cur_line
	    # remove the filename from the list
	    regsub -all " $cur_line " $allfiles " " allfiles
	    # also match the name if it's the last one in the file
	    regsub -all " $cur_line$" $allfiles " " allfiles
	    # also match if it's the only name in the list
	    regsub -all "^$cur_line" $allfiles " " allfiles
	}
    }
    
    # remove the leading and trailing blank spaces for cleanliness sake
    set allfiles [string trimleft $allfiles]
    set allfiles [string trimright $allfiles]
    # nuke the files
    if { [string length $allfiles] > 0 } {
	verbose "Removing \"$allfiles\" from $directory"
	catch "exec rm -fr $allfiles"
    } else {
	verbose "Nothing to remove from $directory" 2
    }

    # close the .clean file
    catch "close $cleanfile"
}


# now that we've removed everything we don't want from this
# directory, recur into the directories that are left to clean
# those as well.

proc getdirs { directory } {
    set dirs ""
    set files [glob -nocomplain "$directory/*"]
    if { [llength $files] != 0 } {
	foreach j $files {
	    if [file isdirectory $j] {
		append dirs " $j"
		append dirs " [getdirs $j]"
	    }
	}
    }
    return $dirs
}

cleanfiles .
# now we actually do it all
foreach i [getdirs .] {
    cleanfiles $i
}




