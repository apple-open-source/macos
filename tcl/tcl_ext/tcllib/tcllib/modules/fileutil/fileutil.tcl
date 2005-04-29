# fileutil.tcl --
#
#	Tcl implementations of standard UNIX utilities.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# Copyright (c) 2002      by Phil Ehrens <phil@slug.org> (fileType)
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: fileutil.tcl,v 1.37.2.2 2004/05/27 02:47:39 andreas_kupries Exp $

package require Tcl 8.2
package require cmdline
package provide fileutil 1.6.1

namespace eval ::fileutil {
    namespace export grep find findByPattern cat foreachLine
}

# ::fileutil::grep --
#
#	Implementation of grep.  Adapted from the Tcler's Wiki.
#
# Arguments:
#	pattern		pattern to search for.
#	files		list of files to search; if NULL, uses stdin.
#
# Results:
#	results		list of matches

proc ::fileutil::grep {pattern {files {}}} {
    set result [list]
    if {[llength $files] == 0} {
	# read from stdin
	set lnum 0
	while {[gets stdin line] >= 0} {
	    incr lnum
	    if {[regexp -- $pattern $line]} {
		lappend result "${lnum}:${line}"
	    }
	}
    } else {
	foreach filename $files {
	    set file [open $filename r]
	    set lnum 0
	    while {[gets $file line] >= 0} {
		incr lnum
		if {[regexp -- $pattern $line]} {
		    lappend result "${filename}:${lnum}:${line}"
		}
	    }
	    close $file
	}
    }
    return $result
}

# ::fileutil::find ==
#
# Two different implementations of this command, one for Unix with its
# softlinks, the other for the Win* platform. The trouble with
# softlink is that they can generate circles in the directory and/or
# file structure, leading a simple recursion into infinity. So we
# record device/inode information for each file and directory we touch
# to be able to skip it should we happen to visit it again.

# Note about the general implementation: The tcl interpreter sets a
# tcl stack limit of 1000 levels to prevent infinite recursions from
# running out of bounds. As this command is implemented recursively it
# will fail for very deeply nested directory structures.

if {[string compare unix $tcl_platform(platform)]} {
    # Not a unix platform => Original implementation
    # Note: This may still fail for directories mounted via SAMBA,
    # i.e. coming from a unix server.

    # ::fileutil::find --
    #
    #	Implementation of find.  Adapted from the Tcler's Wiki.
    #
    # Arguments:
    #	basedir		directory to start searching from; default is .
    #	filtercmd	command to use to evaluate interest in each file.
    #			If NULL, all files are interesting.
    #
    # Results:
    #	files		a list of interesting files.

    proc ::fileutil::find {{basedir .} {filtercmd {}}} {

	# Instead of getting a directory, we have received one file
	# name.  Do not do directory operations.
	if { [file isfile $basedir] } {
	    set cwd "" ; # This variable is needed below.
	    set fileisbasedir 1
	    set filenames [list $basedir]
	} elseif { [file isdirectory $basedir] } {
	    set fileisbasedir 0
	    set oldwd [pwd]
	    cd $basedir
	    set cwd [pwd]
	    set filenames [glob -nocomplain * .*]
	} else {
	    return -code error "$basedir does not exist"
	}

	set files {}
	set filt [string length $filtercmd]
	# If we don't remove . and .. from the file list, we'll get stuck in
	# an infinite loop in an infinite loop in an infinite loop in an inf...
	foreach special [list "." ".."] {
	    set index [lsearch -exact $filenames $special]
	    set filenames [lreplace $filenames $index $index]
	}
	foreach filename $filenames {
	    # Use uplevel to eval the command, not eval, so that variable 
	    # substitutions occur in the right context.
	    if {!$filt || [uplevel $filtercmd [list $filename]]} {
		lappend files [file join $cwd $filename]
	    }
	    if {[file isdirectory $filename]} {
		set files [concat $files [find $filename $filtercmd]]
	    }
	}
	if { ! $fileisbasedir } {
	    cd $oldwd
	}
	return $files
    }
} else {
    # Unix, record dev/inode to detect and break circles

    # SF tcllib bug [784157], distinguish between pre and post Tcl
    # 8.4. In 8.4 and post 8.4. we have to conditionally exclude
    # dev/inode checking. This is not required for pre 8.4.

    if {[package vcompare [package present Tcl] 8.4] >= 0} {

	# ::fileutil::find --
	#
	#	Implementation of find.  Adapted from the Tcler's Wiki.
	#
	# Arguments:
	#	basedir		directory to start searching from; default is .
	#	filtercmd	command to use to evaluate interest in each file.
	#			If NULL, all files are interesting.
	#
	# Results:
	#	files		a list of interesting files.

	proc ::fileutil::find {{basedir .} {filtercmd {}} {nodeVar {}}} {
	    if {$nodeVar == {}} {
		# Main call, setup the device/inode structure
		array set inodes {}
	    } else {
		# Recursive call, import the device/inode record from the caller.
		upvar $nodeVar inodes
	    }

	    # Instead of getting a directory, we have received one file
	    # name.  Do not do directory operations.
	    if { [file isfile $basedir] } {
		set cwd "" ; # This variable is needed below.
		set fileisbasedir 1
		set filenames [list $basedir]
	    } elseif { [file isdirectory $basedir] } {
		set fileisbasedir 0
		set oldwd [pwd]
		cd $basedir
		set cwd [pwd]
		set filenames [glob -nocomplain * .*]
	    } else {
		return -code error "$basedir does not exist"
	    }

	    set files {}
	    set filt [string length $filtercmd]
	    # If we don't remove . and .. from the file list, we'll get stuck in
	    # an infinite loop in an infinite loop in an infinite loop in an inf...
	    foreach special [list "." ".."] {
		set index [lsearch -exact $filenames $special]
		set filenames [lreplace $filenames $index $index]
	    }
	    foreach filename $filenames {
		# Stat each file/directory get exact information about its identity
		# (device, inode). Non-'stat'able files are either junk (link to
		# non-existing target) or not readable, i.e. inaccessible. In both
		# cases it makes sense to ignore them.

		if {[catch {file stat [set full [file join $cwd $filename]] stat}]} {
		    continue
		}

		# SF [ 647974 ] find has problems recursing a metakit fs ...
		#
		# The following code is a HACK / workaround. We assume that virtual
		# FS's do not support links, and therefore there is no need for
		# keeping track of device/inode information. A good thing as the 
		# the virtual FS's usually give us bad data for these anyway, as
		# illustrated by the bug referenced above.

		if {[string equal native [lindex [file system $full] 0]]} {
		    # No skip over previously recorded files/directories and
		    # record the new files/directories.

		    set key "$stat(dev),$stat(ino)"
		    if {[info exists inodes($key)]} {
			continue
		    }
		    set inodes($key) 1
		}

		# Use uplevel to eval the command, not eval, so that variable 
		# substitutions occur in the right context.
		if {!$filt || [uplevel $filtercmd [list $filename]]} {
		    lappend files $full
		}
		if {[file isdirectory $filename]} {
		    set files [concat $files [find $filename $filtercmd inodes]]
		}
	    }
	    if { ! $fileisbasedir } {
		cd $oldwd
	    }
	    return $files
	}

    } else {
	# Unix, pre 8.4. No virtual file system is present, therefore there is no
	# need to conditionally exclude dev/inode checking.

	# ::fileutil::find --
	#
	#	Implementation of find.  Adapted from the Tcler's Wiki.
	#
	# Arguments:
	#	basedir		directory to start searching from; default is .
	#	filtercmd	command to use to evaluate interest in each file.
	#			If NULL, all files are interesting.
	#
	# Results:
	#	files		a list of interesting files.

	proc ::fileutil::find {{basedir .} {filtercmd {}} {nodeVar {}}} {
	    if {$nodeVar == {}} {
		# Main call, setup the device/inode structure
		array set inodes {}
	    } else {
		# Recursive call, import the device/inode record from the caller.
		upvar $nodeVar inodes
	    }

	    # Instead of getting a directory, we have received one file
	    # name.  Do not do directory operations.
	    if { [file isfile $basedir] } {
		set cwd "" ; # This variable is needed below.
		set fileisbasedir 1
		set filenames [list $basedir]
	    } elseif { [file isdirectory $basedir] } {
		set fileisbasedir 0
		set oldwd [pwd]
		cd $basedir
		set cwd [pwd]
		set filenames [glob -nocomplain * .*]
	    } else {
		return -code error "$basedir does not exist"
	    }

	    set files {}
	    set filt [string length $filtercmd]
	    # If we don't remove . and .. from the file list, we'll get stuck in
	    # an infinite loop in an infinite loop in an infinite loop in an inf...
	    foreach special [list "." ".."] {
		set index [lsearch -exact $filenames $special]
		set filenames [lreplace $filenames $index $index]
	    }
	    foreach filename $filenames {
		# Stat each file/directory get exact information about its identity
		# (device, inode). Non-'stat'able files are either junk (link to
		# non-existing target) or not readable, i.e. inaccessible. In both
		# cases it makes sense to ignore them.

		if {[catch {file stat [set full [file join $cwd $filename]] stat}]} {
		    continue
		}

		# No skip over previously recorded files/directories and
		# record the new files/directories.

		set key "$stat(dev),$stat(ino)"
		if {[info exists inodes($key)]} {
		    continue
		}
		set inodes($key) 1

		# Use uplevel to eval the command, not eval, so that variable 
		# substitutions occur in the right context.
		if {!$filt || [uplevel $filtercmd [list $filename]]} {
		    lappend files $full
		}
		if {[file isdirectory $filename]} {
		    set files [concat $files [find $filename $filtercmd inodes]]
		}
	    }
	    if { ! $fileisbasedir } {
		cd $oldwd
	    }
	    return $files
	}

    }
    # end if
}

# ::fileutil::findByPattern --
#
#	Specialization of find. Finds files based on their names,
#	which have to match the specified patterns. Options are used
#	to specify which type of patterns (regexp-, glob-style) is
#	used.
#
# Arguments:
#	basedir		Directory to start searching from.
#	args		Options (-glob, -regexp, --) followed by a
#			list of patterns to search for.
#
# Results:
#	files		a list of interesting files.

proc ::fileutil::findByPattern {basedir args} {
    set pos 0
    set cmd ::fileutil::FindGlob
    foreach a $args {
	incr pos
	switch -glob -- $a {
	    --      {break}
	    -regexp {set cmd ::fileutil::FindRegexp}
	    -glob   {set cmd ::fileutil::FindGlob}
	    -*      {return -code error "Unknown option $a"}
	    default {incr pos -1 ; break}
	}
    }

    set args [lrange $args $pos end]

    if {[llength $args] != 1} {
	set pname [lindex [info level 0] 0]
	return -code error \
		"wrong#args for \"$pname\", should be\
		\"$pname basedir ?-regexp|-glob? ?--? patterns\""
    }

    set patterns [lindex $args 0]
    return [find $basedir [list $cmd $patterns]]
}


# ::fileutil::FindRegexp --
#
#	Internal helper. Filter command used by 'findByPattern'
#	to match files based on regular expressions.
#
# Arguments:
#	patterns	List of regular expressions to match against.
#	filename	Name of the file to match against the patterns.
# Results:
#	interesting	A boolean flag. Set to true if the file
#			matches at least one of the patterns.

proc ::fileutil::FindRegexp {patterns filename} {
    foreach p $patterns {
	if {[regexp -- $p $filename]} {
	    return 1
	}
    }
    return 0
}

# ::fileutil::FindGlob --
#
#	Internal helper. Filter command used by 'findByPattern'
#	to match files based on glob expressions.
#
# Arguments:
#	patterns	List of glob expressions to match against.
#	filename	Name of the file to match against the patterns.
# Results:
#	interesting	A boolean flag. Set to true if the file
#			matches at least one of the patterns.

proc ::fileutil::FindGlob {patterns filename} {
    foreach p $patterns {
	if {[string match $p $filename]} {
	    return 1
	}
    }
    return 0
}

# ::fileutil::stripPwd --
#
#	If the specified path references is a path in [pwd] (or [pwd] itself) it
#	is made relative to [pwd]. Otherwise it is left unchanged.
#	In the case of [pwd] itself the result is the string '.'.
#
# Arguments:
#	path		path to modify
#
# Results:
#	path		The (possibly) modified path.

proc ::fileutil::stripPwd {path} {

    # [file split] is used to generate a canonical form for both
    # paths, for easy comparison, and also one which is easy to modify
    # using list commands.

    set pwd [pwd]
    if {[string equal $pwd $path]} {
	return "."
    }

    set pwd   [file split $pwd]
    set npath [file split $path]

    if {[string match ${pwd}* $npath]} {
	set path [eval [linsert [lrange $npath [llength $pwd] end] 0 file join ]]
    }
    return $path
}

# ::fileutil::stripN --
#
#	Removes N elements from the beginning of the path.
#
# Arguments:
#	path		path to modify
#	n		number of elements to strip
#
# Results:
#	path		The modified path

proc ::fileutil::stripN {path n} {
    set path [file split $path]
    if {$n >= [llength $path]} {
	return {}
    } else {
	return [eval [linsert [lrange $path $n end] 0 file join]]
    }
}

# ::fileutil::cat --
#
#	Tcl implementation of the UNIX "cat" command.  Returns the contents
#	of the specified file.
#
# Arguments:
#	args	name of the files to read.
#
# Results:
#	data		data read from the file.

proc ::fileutil::cat {args} {
    foreach filename $args {
	# Don't bother catching errors, just let them propagate up
	set fd [open $filename r]
	# Use the [file size] command to get the size, which preallocates memory,
	# rather than trying to grow it as the read progresses.
	set size [file size $filename]
	if {$size} {
	    append data [read $fd $size]
	} else {
	    # if the file has zero bytes it is either empty, or something 
	    # where [file size] reports 0 but the file actually has data (like
	    # the files in the /proc filesystem on Linux)
	    append data [read $fd]
	}
	close $fd
    }
    return $data
}

# ::fileutil::foreachLine --
#
#	Executes a script for every line in a file.
#
# Arguments:
#	var		name of the variable to contain the lines
#	filename	name of the file to read.
#	cmd		The script to execute.
#
# Results:
#	None.

proc ::fileutil::foreachLine {var filename cmd} {
    upvar 1 $var line
    set fp [open $filename r]

    # -future- Use try/eval from tcllib/control
    catch {
	set code 0
	set result {}
	while {[gets $fp line] >= 0} {
	    set code [catch {uplevel 1 $cmd} result]
	    if {($code != 0) && ($code != 4)} {break}
	}
    }
    close $fp

    if {($code == 0) || ($code == 3) || ($code == 4)} {
        return $result
    }
    if {$code == 1} {
        global errorCode errorInfo
        return \
		-code      $code      \
		-errorcode $errorCode \
		-errorinfo $errorInfo \
		$result
    }
    return -code $code $result
}

# ::fileutil::touch --
#
#	Tcl implementation of the UNIX "touch" command.
#
#	touch [-a] [-m] [-c] [-r ref_file] [-t time] filename ...
#
# Arguments:
#	-a		change the access time only, unless -m also specified
#	-m		change the modification time only, unless -a also specified
#	-c		silently prevent creating a file if it did not previously exist
#	-r ref_file	use the ref_file's time instead of the current time
#	-t time		use the specified time instead of the current time
#			("time" is an integer clock value, like [clock seconds])
#	filename ...	the files to modify
#
# Results
#	None.
#
# Errors:
#	Both of "-r" and "-t" cannot be specified.

if {[package vsatisfies [package provide Tcl] 8.3]} {
    namespace eval ::fileutil { namespace export touch }

    proc ::fileutil::touch {args} {
        # Don't bother catching errors, just let them propagate up
        
        set options {
            {a          "set the atime only"}
            {m          "set the mtime only"}
            {c          "do not create non-existant files"}
            {r.arg  ""  "use time from ref_file"}
            {t.arg  -1  "use specified time"}
        }
        set usage ": [lindex [info level 0] 0]\
                      \[options] filename ...\noptions:"
        array set params [::cmdline::getoptions args $options $usage]
        
        # process -a and -m options
        set set_atime [set set_mtime "true"]
        if {  $params(a) && ! $params(m)} {set set_mtime "false"}
        if {! $params(a) &&   $params(m)} {set set_atime "false"}
        
        # process -r and -t
        set has_t [expr {$params(t) != -1}]
        set has_r [expr {[string length $params(r)] > 0}]
        if {$has_t && $has_r} {
            return -code error "Cannot specify both -r and -t"
        } elseif {$has_t} {
            set atime [set mtime $params(t)]
        } elseif {$has_r} {
            file stat $params(r) stat
            set atime $stat(atime)
            set mtime $stat(mtime)
        } else {
            set atime [set mtime [clock seconds]]
        }

        # do it
        foreach filename $args {
            if {! [file exists $filename]} {
                if {$params(c)} {continue}
                close [open $filename w]
            }
            if {$set_atime} {file atime $filename $atime}
            if {$set_mtime} {file mtime $filename $mtime}
        }
        return
    }
}

# ::fileutil::fileType --
#
#	Do some simple heuristics to determine file type.
#
#
# Arguments:
#	filename        Name of the file to test.
#
# Results
#	type            Type of the file.  May be a list if multiple tests
#                       are positive (eg, a file could be both a directory 
#                       and a link).  In general, the list proceeds from most
#                       general (eg, binary) to most specific (eg, gif), so
#                       the full type for a GIF file would be 
#                       "binary graphic gif"
#
#                       At present, the following types can be detected:
#
#                       directory
#                       empty
#                       binary
#                       text
#                       script <interpreter>
#                       executable elf
#                       binary graphic [gif, jpeg, png, tiff]
#                       ps, eps, pdf
#                       html
#                       xml <doctype>
#                       message pgp
#                       bzip, gzip
#                       gravity_wave_data_frame
#                       link
#                  

proc ::fileutil::fileType {filename} {
    ;## existence test
    if { ! [ file exists $filename ] } {
        set err "file not found: '$filename'"
        return -code error $err
    }
    ;## directory test
    if { [ file isdirectory $filename ] } {
        set type directory
        if { ! [ catch {file readlink $filename} ] } {
            lappend type link
        }
        return $type
    }
    ;## empty file test
    if { ! [ file size $filename ] } {
        set type empty
        if { ! [ catch {file readlink $filename} ] } {
            lappend type link
        }
        return $type
    }
    set bin_rx {[\x00-\x08\x0b\x0e-\x1f]}

    if { [ catch {
        set fid [ open $filename r ]
        fconfigure $fid -translation binary
        fconfigure $fid -buffersize 1024
        fconfigure $fid -buffering full
        set test [ read $fid 1024 ]
        ::close $fid
    } err ] } {
        catch { ::close $fid }
        return -code error "::fileutil::fileType: $err"
    }

    if { [ regexp $bin_rx $test ] } {
        set type binary
        set binary 1
    } else {
        set type text
        set binary 0
    }

    # SF Tcllib bug [795585]. Allowing whitespace between #!
    # and path of script interpreter

    set metakit 0

    if { [ regexp {^\#\!\s*(\S+)} $test -> terp ] } {
        lappend type script $terp
    } elseif { $binary && [ regexp {^[\x7F]ELF} $test ] } {
        lappend type executable elf
    } elseif { $binary && [string match "BZh91AY\&SY*" $test] } {
        lappend type compressed bzip
    } elseif { $binary && [string match "\x1f\x8b*" $test] } {
        lappend type compressed gzip
    } elseif { $binary && [string match "GIF*" $test] } {
        lappend type graphic gif
    } elseif { $binary && [string match "\x89PNG*" $test] } {
        lappend type graphic png
    } elseif { $binary && [string match "\xFF\xD8\xFF*" $test] } {
        binary scan $test c3H2Sa5 id marker len txt 
        if {$marker == "e0" && $txt == "JFIF\x00"} {
            lappend type graphic jpeg jfif
        } elseif { $marker == "e1" && $txt == "Exif\x00" } {
            lappend type graphic jpeg exif
        }
    } elseif { $binary && [string match "MM\x00\**" $test] } {
        lappend type graphic tiff
    } elseif { $binary && [string match "\%PDF\-*" $test] } {
        lappend type pdf
    } elseif { ! $binary && [string match -nocase "*\<html\>*" $test] } {
        lappend type html
    } elseif { [string match "\%\!PS\-*" $test] } {
       lappend type ps
       if { [string match "* EPSF\-*" $test] } {
           lappend type eps
       }
    } elseif { [string match -nocase "*\<\?xml*" $test] } {
        lappend type xml
        if { [ regexp -nocase {\<\!DOCTYPE\s+(\S+)} $test -> doctype ] } {
            lappend type $doctype
        }
    } elseif { [string match {*BEGIN PGP MESSAGE*} $test] } {
        lappend type message pgp
    } elseif { $binary && [string match {IGWD*} $test] } {
        lappend type gravity_wave_data_frame
    } elseif {[string match "JL\x1a\x00*" $test] && ([file size $filename] >= 27)} {
	lappend type metakit smallendian
	set metakit 1
    } elseif {[string match "LJ\x1a\x00*" $test] && ([file size $filename] >= 27)} {
	lappend type metakit bigendian
	set metakit 1
    }

    # Additional checks of file contents at the end of the file,
    # possibly pointing into the middle too (attached metakit,
    # attached zip).

    ## Metakit File format: http://www.equi4.com/metakit/metakit-ff.html
    ## Metakit database attached ? ##

    if {!$metakit && ([file size $filename] >= 27)} {
	# The offsets in the footer are in always bigendian format

	if { [ catch {
	    set fid [ open $filename r ]
	    fconfigure $fid -translation binary
	    fconfigure $fid -buffersize 1024
	    fconfigure $fid -buffering full
	    seek $fid -16 end
	    set test [ read $fid 16 ]
	    ::close $fid
	} err ] } {
	    catch { ::close $fid }
	    return -code error "::fileutil::fileType: $err"
	}

	binary scan $test IIII __ hdroffset __ __
	set hdroffset [expr {[file size $filename] - 16 - $hdroffset}]

	# Further checks iff the offset is actually inside the file.

	if {($hdroffset >= 0) && ($hdroffset < [file size $filename])} {
	    # Seek to the specified location and try to match a metakit header
	    # at this location.

	    if { [ catch {
		set         fid [ open $filename r ]
		fconfigure $fid -translation binary
		fconfigure $fid -buffersize 1024
		fconfigure $fid -buffering full
		seek       $fid $hdroffset start
		set test [ read $fid 16 ]
		::close $fid
	    } err ] } {
		catch { ::close $fid }
		return -code error "::fileutil::fileType: $err"
	    }

	    if {[string match "JL\x1a\x00*" $test]} {
		lappend type attached metakit smallendian
		set metakit 1
	    } elseif {[string match "LJ\x1a\x00*" $test]} {
		lappend type attached metakit bigendian
		set metakit 1
	    }
	}
    }

    ## Zip File Format: http://zziplib.sourceforge.net/zzip-parse.html
    ## http://www.pkware.com/products/enterprise/white_papers/appnote.html


    ;## lastly, is it a link?
    if { ! [ catch {file readlink $filename} ] } {
        lappend type link
    }
    return $type
}

# ::fileutil::tempdir --
#
#	Return the correct directory to use for temporary files.
#	Python attempts this sequence, which seems logical:
#
#       1. The directory named by the `TMPDIR' environment variable.
#
#       2. The directory named by the `TEMP' environment variable.
#
#       3. The directory named by the `TMP' environment variable.
#
#       4. A platform-specific location:
#            * On Macintosh, the `Temporary Items' folder.
#
#            * On Windows, the directories `C:\\TEMP', `C:\\TMP',
#              `\\TEMP', and `\\TMP', in that order.
#
#            * On all other platforms, the directories `/tmp',
#              `/var/tmp', and `/usr/tmp', in that order.
#
#        5. As a last resort, the current working directory.
#
# Arguments:
#	None.
#
# Side Effects:
#	None.
#
# Results:
#	The directory for temporary files.

proc ::fileutil::TempDir {} {
    global tcl_platform env
    set attempdirs [list]

    foreach tmp {TMPDIR TEMP TMP} {
	if { [info exists env($tmp)] } {
	    lappend attempdirs $env($tmp)
	}
    }

    switch $tcl_platform(platform) {
	windows {
	    lappend attempdirs "C:\\TEMP" "C:\\TMP" "\\TEMP" "\\TMP"
	}
	macintosh {
	    set tmpdir $env(TRASH_FOLDER)  ;# a better place?
	}
	default {
	    lappend attempdirs [file join / tmp] \
		[file join / var tmp] [file join / usr tmp]
	}
    }

    foreach tmp $attempdirs {
	if { [file isdirectory $tmp] && [file writable $tmp] } {
	    return $tmp
	}
    }

    # If nothing else worked...
    return [pwd]
}

if { [package vcompare [package provide Tcl] 8.4] < 0 } {
    proc ::fileutil::tempdir {} {
	return [TempDir]
    }
} else {
    proc ::fileutil::tempdir {} {
	return [file normalize [TempDir]]
    }
}

# ::fileutil::tempfile --
#
#   generate a temporary file name suitable for writing to
#   the file name will be unique, writable and will be in the 
#   appropriate system specific temp directory
#   Code taken from http://mini.net/tcl/772 attributed to
#    Igor Volobouev and anon.
#
# Arguments:
#   prefix     - a prefix for the filename, p
# Results:
#   returns a file name
#

proc ::fileutil::TempFile {prefix} {
    set tmpdir [tempdir]

    set chars "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    set nrand_chars 10
    set maxtries 10
    set access [list RDWR CREAT EXCL TRUNC]
    set permission 0600
    set channel ""
    set checked_dir_writable 0
    set mypid [pid]
    for {set i 0} {$i < $maxtries} {incr i} {
 	set newname $prefix
 	for {set j 0} {$j < $nrand_chars} {incr j} {
 	    append newname [string index $chars \
		    [expr {([clock clicks] ^ $mypid) % 62}]]
 	}
	set newname [file join $tmpdir $newname]
 	if {[file exists $newname]} {
 	    after 1
 	} else {
 	    if {[catch {open $newname $access $permission} channel]} {
 		if {!$checked_dir_writable} {
 		    set dirname [file dirname $newname]
 		    if {![file writable $dirname]} {
 			return -code error "Directory $dirname is not writable"
 		    }
 		    set checked_dir_writable 1
 		}
 	    } else {
 		# Success
		close $channel
 		return $newname
 	    }
 	}
    }
    if {[string compare $channel ""]} {
 	return -code error "Failed to open a temporary file: $channel"
    } else {
 	return -code error "Failed to find an unused temporary file name"
    }
}

if { [package vcompare [package provide Tcl] 8.4] < 0 } {
    proc ::fileutil::tempfile {{prefix {}}} {
	return [TempFile $prefix]
    }
} else {
    proc ::fileutil::tempfile {{prefix {}}} {
	return [file normalize [TempFile $prefix]]
    }
}

# ::fileutil::install --
#
#	Tcl version of the 'install' command, which copies files from
#	one places to another and also optionally sets some attributes
#	such as group, owner, and permissions.
#
# Arguments:
#	-m		Change the file permissions to the specified
#                       value.  Valid arguments are those accepted by
#			file attributes -permissions
#
# Results:
#	None.

# TODO - add options for group/owner manipulation.

proc ::fileutil::install {args} {
    set options {
	{m.arg "" "Set permission mode"}
    }
    set usage ": [lindex [info level 0] 0]\
\[options] source destination \noptions:"
    array set params [::cmdline::getoptions args $options $usage]
    # Args should now just be the source and destination.
    if { [llength $args] < 2 } {
	return -code error $usage
    }
    set src [lindex $args 0]
    set dst [lindex $args 1]
    file copy -force $src $dst
    if { $params(m) != "" } {
	set targets [::fileutil::find $dst]
	foreach fl $targets {
	    file attributes $fl -permissions $params(m)
	}
    }
}
