if 0 {
########################

globfind.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5

The proc globfind is a replacement for tcllib's fileutil::find

Usage: globfind ?basedir ?filtercmd? ?switches??

Options:

basedir - the directory from which to start the search.  Defaults to current directory.

filtercmd - Tcl command; for each file found in the basedir, the filename will be
appended to filtercmd and the result will be evaluated.  The evaluation should return
0 or 1; only files whose return code is 1 will be included in the final return result.

switches - The switches will "prefilter" the results before the filtercmd is applied.  The
available switches are:

	-depth   - sets the number of levels down from the basedir into which the 
		     filesystem hierarchy will be searched. A value of zero is interpreted
		     as infinite depth.

	-pattern - a glob-style filename-matching wildcard. ex: -pattern *.pdf

	-types   - any value acceptable to the "types" switch of the glob command.
		     ex: -types {d hidden}

Side effects:

If somewhere within the search space a directory is a link to another directory within
the search space, then the variable ::globfind::REDUNDANCY will be set to 1 (otherwise
it will be set to 0).  The name of the redundant directory will be appended to the
variable ::globfind::redundant_files.  This may be used to help track down and eliminate
infinitely looping links in the search space.

Unlike fileutil::find, the name of the basedir will be included in the results if it fits
the prefilter and filtercmd criteria (thus emulating the behavior of the standard Unix 
GNU find utility).

----

globfind is designed as a fast and simple alternative to fileutil::find.  It takes 
advantage of glob's ability to use multiple patterns to scan deeply into a directory 
structure in a single command, hence the name.

It reports symbolic links along with other files by default, but checks for nesting of
links which might otherwise lead to infinite search loops.  It reports hidden files by
default unless the -types switch is used to specify exactly what is wanted.

globfind may be used with Tcl versions earlier than 8.4, but emulation of missing
features of the glob command in those versions will result in slower performance.

globfind is generally two to three times faster than fileutil::find, and fractionally
faster than perl's File::Find function for comparable searches.

The filtercmd may be omitted if only prefiltering is desired; in this case it may be a 
bit faster to use the proc globtraverse, which uses the same basedir value and 
command-line switches as globfind, but does not take a filtercmd value.

If one wanted to search for pdf files for example, one could use the command:

	globfind $basedir {string match -nocase *.pdf}

It would, however, in this case be much faster to use:

	globtraverse $basedir -pattern *.pdf

########################
}


package provide fileutil::globfind 1.5

namespace eval ::fileutil::globfind {

namespace export globfind globtraverse

proc globfind {{basedir .} {filtercmd {}} args} {
	set returnFiles {}
	set types {}
	set basedir [file normalize $basedir]

	# account for possibility that filtercmd is missing by 
	# reformatting $args variable: 
	set args [concat [list $filtercmd] $args]
	if [expr fmod([llength $args],2)] {
		set filtercmd [lindex $args 0]
		set args [lrange $args 1 end]
	} else {
		set filtercmd {}
	}

	set filt [string length $filtercmd]
	set 83ok [package vsatisfies [package present Tcl] 8.3]

	# process command-line switches:
	foreach {flag value} $args {
		if {[string first $flag -types] >= 0} {set flag "-types"}
		if {[string first $flag -pattern] >= 0} {set flag "-pattern"}
		if {[string first $flag -depth] >= 0} {set flag "-depth"}

		switch -- $flag {
			-types {
				set types [list $value]

				# can't use -types pre-8.3, because it doesn't exist in glob command.
				# thus if it is specified, error out:
				if !$83ok {error {error: "-types" flag not supported in version 8.2 and earlier}}

				# bug in 8.3, if -types {hidden f} is used, possible crash.  So disallow:
				if {(![package vcompare [package present Tcl] 8.3]) && ($tcl_platform(platform) == "unix") && ([lsearch $types "hidden"] >= 0) && ([lsearch $types "f"] >= 0)} {
					error {Tcl 8.3 bug: potential program crash if "-types {hidden f}" used}
				}

				set types "-types $types"
			}
			-pattern {set pattern [list $value]}
			-depth {set depth [expr [list $value]]}
			default {error "$flag: incorrect flag value"}
		}
	}

	# add basedir to result if it satisfies prefilter conditions:
	set returnFiles [eval [eval list globtraverse [list [file dirname $basedir]] $args -depth 1]]
	if {[lsearch -exact $returnFiles $basedir] >= 0} {set returnFiles [list $basedir]} else {set returnFiles {}}
	# get all files in basedir that satisfy prefilter conditions:
	set returnFiles [concat $returnFiles [eval [eval list globtraverse \$basedir $args]]]

	# get hidden files if no specific types requested:
	if {$types == {}} {
		
		# save redundant file values already gathered:
		set redundant_files {}
		if [set REDUNDANCY $[namespace current]::REDUNDANCY] {
			set redundant_files $[namespace current]::redundant_files
		}

		# get hidden files:
		set returnFiles [concat $returnFiles [eval [eval list globtraverse \$basedir $args -type hidden]]]

		# virtual filesystems ignore hidden tag, so just in case, filter out duplicates:
		set returnFiles [lsort -unique $returnFiles]

		# collate redundant file info:
		if $[namespace current]::REDUNDANCY {
			set [namespace current]::redundant_files [concat $[namespace current]::redundant_files $redundant_files]
		} else {
			set [namespace current]::redundant_files $redundant_files
		}
		set [namespace current]::REDUNDANCY [expr ($[namespace current]::REDUNDANCY || $REDUNDANCY)]
	}

	# apply filtercmd to prefiltered results if one is specified:
	if $filt {
		set filterFiles {}
		foreach filename $returnFiles {
			if [uplevel $filtercmd [list $filename]] {
				lappend filterFiles $filename
			}
		}
	} else {
		set filterFiles $returnFiles
	}

	return $filterFiles
}

proc globtraverse {{basedir .} args} {
	set [namespace current]::REDUNDANCY 0
	unset -nocomplain [namespace current]::redundant_files
	set depth 0

	# search 16 directory levels per iteration, glob can't handle more patterns than that at once.
	set maxDepth 16

	set pattern *
	set types {}
	set resultList {}

	set basedir [file normalize $basedir]
	if ![file isdirectory $basedir] {return}

	set baseDepth [llength [file split $basedir]] ; # calculate starting depth

	lappend checkDirs $basedir ; # initialize list of dirs to check

	# format basedir variable for later infinite loop checking:
	set basedir $basedir/
	set basedir [string map {// /} $basedir]

	set 83ok [package vsatisfies [package present Tcl] 8.3]

	# process command-line switches:
	foreach {flag value} $args {
		if {[string first $flag -types] >= 0} {set flag "-types"}
		if {[string first $flag -pattern] >= 0} {set flag "-pattern"}
		if {[string first $flag -depth] >= 0} {set flag "-depth"}

		switch -- $flag {
			-types {
				set types [list $value]
				if !$83ok {error {error: "-types" flag not supported in version 8.2 and earlier}}
				if {(![package vcompare [package present Tcl] 8.3]) && ($tcl_platform(platform) == "unix") && ([lsearch $types "hidden"] >= 0) && ([lsearch $types "f"] >= 0)} {
					error {Tcl 8.3 bug: potential program crash if "-types {hidden f}" used}
				}
				set types "-types $types"
			}
			-pattern {set pattern [list $value]}
			-depth {set depth [expr [list $value]]}
			default {error "$flag: incorrect flag value"}
		}
	}

	# Main result-gathering loop:
	while {[llength $checkDirs]} {
		set currentDir [lindex $checkDirs 0]

		set currentDepth [expr [llength [file split $currentDir]] - $baseDepth] ; # distance from start depth

		set searchDepth [expr $depth - $currentDepth] ; # distance from max depth to search to

		# build multi-pattern argument to feed to glob command:
		set globPatternTotal {}
		set globPattern *
		set incrPattern /*
		for {set i 1} {$i <= $maxDepth} {incr i} {
			set customPattern [string range $globPattern 0 end-1]
			append customPattern $pattern
			lappend globPatternTotal $customPattern
			append globPattern $incrPattern
			incr searchDepth -1
			if {$searchDepth == 0} {break}
		}

		# save pattern to use for iterative dir search later:
		set dirPattern [string range $globPattern 0 end-2]

		# glob pre-8.3 doesn't support -directory switch; emulate it if necessary:
		if $83ok {
			set contents [eval glob -nocomplain -directory \$currentDir $types -- $globPatternTotal]
		} else {
			set wd [pwd]
			set newContents {}
			cd $currentDir
			if [catch {set contents [eval glob -nocomplain -- $globPatternTotal]} err] {
				cd $wd
				error $err
			}
			cd $wd
			foreach item $contents {
				set item [file join $currentDir $item]
				lappend newContents $item
			}
			set contents $newContents
			unset newContents
		}
		set resultList [concat $resultList $contents]

		# check if iterative dir search is necessary (if specified depth not yet reached):
		set contents {}
		set findDirs 1
		if {([expr $currentDepth + [llength [file split $dirPattern]]] >= $depth) && ($depth > 0)} {set findDirs 0}

		# find dirs at current depth boundary to prime iterative search.
		# Pre-8.3 glob doesn't support -type or -dir switches; emulate if necessary:
		if {$83ok && $findDirs} {
			set contents [glob -nocomplain -directory $currentDir -type d -- $dirPattern]
		} elseif $findDirs {
			set wd [pwd]
			set newContents {}
			cd $currentDir
			if [catch {set contents [glob -nocomplain -- $dirPattern/]} err] {
				cd $wd
				error $err
			}
			cd $wd
			foreach item $contents {
				set item [file join $currentDir [string range $item 0 end-1]]
				lappend newContents $item
			}
			set contents $newContents
			unset newContents
		}

		# check for redundant links in dir list:
		set contentLength [llength $contents]
		set i 0
		while {$i < $contentLength} {
			set item [lindex $contents end-$i]
			incr i
			
			# kludge to fully resolve link to native name:
			set linkValue [file dirname [file normalize [file join $item __dummy__]]]

			# if item is a link, and native name is already in the search space, skip it:
			if {($linkValue != $item) && (![string first $basedir $linkValue])} {
				set [namespace current]::REDUNDANCY 1
				lappend [namespace current]::redundant_files $item
				continue
			}

			lappend checkDirs $item			
		}

		# remove current search dir from search list to prime for next iteration:
		set checkDirs [lrange $checkDirs 1 end]
	}	
	return $resultList
}

# Tcl pre-8.4 lacks [file normalize] command; emulate it if necessary:
proc ::fileutil::globfind::file {args} {
	if {[lindex $args 0] == "normalize"} {
		set filename [lindex $args 1]
		set tail [file tail $filename]
		set filename [::fileutil::fullnormalize [file dirname $filename]]
		set filename [file join $filename $tail]
		return $filename
	} else {
		return [uplevel ::file $args]
	}
}

# Eliminate emulation of [file normalize] if version 8.4 or better:
if [package vsatisfies [package present Tcl] 8.4] {
	rename ::fileutil::globfind::file {}
} else {
	package require fileutil 1.13
}

}
# end namespace ::fileutil::globfind
