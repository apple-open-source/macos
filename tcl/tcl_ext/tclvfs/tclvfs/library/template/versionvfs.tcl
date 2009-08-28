if 0 {
########################

versionvfs.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5.2

A versioning virtual filesystem.  Requires the template vfs in templatevfs.tcl.

Similar to historical versioning filesystems, each edited version of a file is saved separately;
each version file is tagged with a timestamp, and optional project tags.
A deleted file is represented by a new zero-length file with timestamp and a tag reading "deleted".
By default only the latest version is visible.  If the latest version is marked deleted, it is invisible.

Directories are versioned and tagged in the same way as files.

Older versions can be retrieved by setting the -project and -time values appropriately.


Usage: mount ?-keep <number> -project <list of tags> -time <timestamp or "clock scan" suitable phrase>? <existing directory> <virtual directory>

Options:

-keep
maximum number of previous versions of a file to keep per project.

-project
a list of one or more version-identifying tags.  Any file created or edited in the virtual file space
will be given these tags.  File versions with a tag matching the ones given here will be visible in 
preference to other, possibly later versions without the tag.

If a file version has project tags but none of them is included in this project tag list, it will be 
invisible; the file itself will then be invisible unless a file version exists without any 
project tags, in which case it will be treated as the default.

In cases where several file versions each have multiple tags that match in the project
list, the version with the greatest number of matches will be visible.

Deleted files are marked with the tag "deleted".  If this value is in the project
name list, then deleted files will become visible again.

Newly-created and deleted directories are tagged and shown/hidden in the same way as files.

-time
A timestamp in the form returned by [clock seconds], or a string understandable by [clock scan].
Versions of files as they existed at the given time will be visible, rather than the
default latest version.  Version choices based on project tags will still be made, but
versions later than the timestamp will be ignored in the decision process.


The values of each option can be changed dynamically after mount by using the "file attributes" 
command on the mount virtual directory. Each option is editable as an attribute; 
i.e., "file attributes C:/version -project {version2 release}".  Display of visible files will 
change dynamically based on decisions with new attribute values.

In addition, new attributes are defined which can be queried for individual files:  

The command "file attributes $file -version_filename" will show the exact filename of the 
currently visible version of the given file as it is stored in the vfs, complete with 
timestamp and project tags if any.

The command "file attributes $file -versions" will return a list containing information on
all stored versions of the given file.  Each element of the list is itself a three-element 
list: 1) the unique millisecond-level timestamp of the version, 2) a human-readable date string
showing the time represented by the timestamp (can be used a a value for the -time attribute), 
and 3) a list of the project tags attached to the version, if any.

The versioning vfs inherits the -cache and -volume options of the template vfs.

########################
}

package require vfs::template 1.5
package require fileutil::globfind

package provide vfs::template::version 1.5.2

namespace eval ::vfs::template::version {

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

# edit following procedures:
proc close_ {channelID} {
	upvar path path root root relative relative

# get hash of file as it existed before edit:
	array set fileStats $::vfs::template::version::filestats($channelID)
	unset ::vfs::template::version::filestats($channelID)

# if new hash shows file is unchanged, return immediately:
	fconfigure $channelID -translation binary
	set hash [Hash $channelID]
	if [string equal -nocase $hash $fileStats(hash)] {return}

# create new unique filename for new version:
	set fileName [VFileNameEncode [file join $path $relative]]\;[VCreateTag $root]
	catch {array set oldAttributes [file attributes $fileStats(filename)]}

# delete file version created by open_ if it's a new file:
	if [string equal $fileStats(hash) {}] {
		file delete -- $fileStats(filename)
	}

# save new version:
	set f [open $fileName w]
	fconfigure $f -translation binary
	seek $channelID 0
	fcopy $channelID $f
	close $f

# ensure attributes are the same for new version:
	foreach {attr value} [file attributes $fileName] {
		if ![info exists oldAttributes($attr)] {continue}
		if {$oldAttributes($attr) != $value} {catch {file attributes $fileName $attr $oldAttributes($attr)}}
	}

	return
}
proc file_atime {file time} {	
	upvar path path root root relative relative
# check if current version is latest, if not disallow edit:
	set latest [lindex [lindex [lsort [VersionsAll $path $relative]] end] 0]
	set acquired [lindex [split [set fileName [VAcquireFile $path $root $relative]] \;] 1]
	if ![string first .&dir [file tail $fileName]] {::vfs::filesystem posixerror $::vfs::posix(ENOENT)}
	if {($acquired != {}) && ($latest != $acquired)} {::vfs::filesystem posixerror $::vfs::posix(EPERM)}

	file atime $fileName $time
}
proc file_mtime {file time} {
	upvar path path root root relative relative
# check if current version is latest, if not disallow edit:
	set latest [lindex [lindex [lsort [VersionsAll $path $relative]] end] 0]
	set acquired [lindex [split [set fileName [VAcquireFile $path $root $relative]] \;] 1]
	if ![string first .&dir [file tail $fileName]] {::vfs::filesystem posixerror $::vfs::posix(ENOENT)}
	if {($acquired != {}) && ($latest != $acquired)} {::vfs::filesystem posixerror $::vfs::posix(EPERM)}

	file mtime $fileName $time
}
proc file_attributes {file {attribute {}} args} {
	upvar path path root root relative relative
	set latest [lindex [lindex [lsort [set allVersions [VersionsAll $path $relative]]] end] 0]
	set acquired [lindex [split [set fileName [VAcquireFile $path $root $relative]] \;] 1]
	if ![string first .&dir [file tail $fileName]] {::vfs::filesystem posixerror $::vfs::posix(ENOENT)}
# process vfs-specific attributes:
	if {($relative == {}) && ([string map {-keep 1 -project 1 -time 1} $attribute] == 1)} {
		set attribute [string range $attribute 1 end]
		if {$args == {}} {
			if ![info exists ::vfs::template::version::${attribute}($root)] {return}
			eval return \$::vfs::template::version::${attribute}(\$root)
		}
		set ::vfs::template::version::${attribute}($root) [lindex $args 0]
		if {[lindex $args 0] == {}} {unset ::vfs::template::version::${attribute}($root)}
		return
	}
	# process read-only vfs-specific attributes:
	if {$attribute == "-versions"} {
		if {$args == {}} {return $allVersions}
		error "cannot set attribute \"-versions\" for file \"[file tail $relative]\": attribute is readonly"
	}
	if {$attribute == "-version_filename"} {
		if {$args == {}} {return [file tail $fileName]}
		error "cannot set attribute \"-version_filename\" for file \"[file tail $relative]\": attribute is readonly"
	}
# check if current version is latest, if not disallow edit:
	if {($args != {}) && ($acquired != {}) && ($latest != $acquired)} {::vfs::filesystem posixerror $::vfs::posix(EPERM)}
	set returnValue [eval file attributes \$fileName $attribute $args]
# collect values for vfs-specific attributes:
	if {$attribute == {}} {
		append returnValue " [list -versions $allVersions]"
		if {$file != $fileName} {append returnValue " [list -version_filename [file tail $fileName]]"}
		if {$relative != {}} {return $returnValue}
		foreach atr "keep project time" {
			set $atr "-$atr {}"
			if [info exists ::vfs::template::version::${atr}($root)] {eval set $atr \[list "-$atr" \$::vfs::template::version::${atr}(\$root)\]}
		}
		append returnValue " $keep $project $time"
	}
	return $returnValue
}

proc file_delete {file} {
	upvar path path root root relative relative
	set dir 0

# make sure subfiles of directory are deleted:
	if [file isdirectory $file] {
		set subfiles [globfind $file]
		set subfiles [lsort -decreasing $subfiles]
		set deleted {}
		foreach sf $subfiles {
			if [file isdirectory $sf] {continue}
			globdelete $sf
		}
		set dir 1
		return
	}
	set fileName [VAcquireFile $path $root $relative]

# allow straight deletion of new zero-length file:
	if {!$dir && ([llength [VersionsAll $path $relative]] == 1) && ![file size $fileName]} {
		file delete -force -- $fileName
		return
	}

# for all others, create new file with "deleted" tag
	set file [VFileNameEncode $file]
	set fileName $file\;[VCreateTag $root]
	if $dir {set fileName [file join $file .&dir[file tail $file]]\;[VCreateTag $root]}
	set fileName [split $fileName \;]
	set fileName [linsert $fileName 2 "deleted"]
	set fileName [join $fileName \;]
	close [open $fileName w]
	if $dir {catch {file attributes $fileName -hidden 1}}
}
proc file_executable {file} {
	upvar path path root root relative relative
	set fileName [VAcquireFile $path $root $relative]
	if ![string first .&dir [file tail $fileName]] {return 0}
	file executable $fileName
}
proc file_exists {file} {
	upvar path path root root relative relative
	set fileName [VAcquireFile $path $root $relative]
	if ![string first .&dir [file tail $fileName]] {return 0}
	if [file isdirectory $fileName] {return 1}
	expr ![string equal [file join $path $relative] $fileName]
}
proc file_mkdir {file} {
	upvar root root
	file mkdir $file

# create a file to store timestamps and tags for directory:
	set fileName [VFileNameEncode $file]
	set fileName [file join $fileName .&dir[file tail $fileName]]\;[VCreateTag $root]
	close [open $fileName w]
	catch {file attributes $fileName -hidden 1}
	return
}
proc file_readable {file} {
	upvar path path root root relative relative
	set fileName [VAcquireFile $path $root $relative]
	if ![string first .&dir [file tail $fileName]] {return 0}
	file readable $fileName
}
proc file_stat {file array} {
	upvar path path root root relative relative $array fs
	set fileName [VAcquireFile $path $root $relative]
	if ![string first .&dir [file tail $fileName]] {error "no such file or directory"}
	file stat $fileName fs
	if {$fs(type) == "directory"} {
		return
	}
	if {$fileName == [file join $path $relative]} {error "no such file or directory"}
	return
}
proc file_writable {file} {
	upvar path path root root relative relative
	set fileName [VAcquireFile $path $root $relative]
	if ![string first .&dir [file tail $fileName]] {return 0}
	file writable $fileName
}
proc glob_ {directory dir nocomplain tails types typeString dashes pattern} {
	upvar path path root root relative relative
	set globList [glob -directory $dir -nocomplain -types $typeString *]
	set newGlobList {}
	set acquireAttempts {}
	foreach gL $globList {
		if [file isdirectory $gL] {
			set acquiredFile [VAcquireFile $path $root [file join $relative [file tail $gL]]]
			if ![string equal $acquiredFile $gL] {continue}
		} else {
			if ![string first .&dir [file tail $gL]] {continue}
			set gL [VFileNameDecode $gL]
			if {[lsearch $acquireAttempts $gL] > -1} {continue}
			lappend acquireAttempts $gL
			set acquiredFile [VAcquireFile $path $root [file join $relative [file tail $gL]]]
			if [string equal $acquiredFile $gL] {continue}
		}
		if [string match $pattern [file tail $gL]] {lappend newGlobList [file tail $gL]}
	}
	return $newGlobList
}
proc open_ {file mode} {
	upvar path path root root relative relative 
	set fileName [VAcquireFile $path $root $relative]
	if {$mode == "r"} {return [open $fileName r]}
	set hash {}

# Use memchans so if file contents don't change we are free to delete file rather than commit to
# creating new version which is identical to last.
# If file is new, create new tag for it and return memchan:
	if {$fileName == [file join $path $relative]} {
		set fileName [VFileNameEncode [file join $path $relative]]\;[VCreateTag $root]
		close [open $fileName $mode]
		set channelID [memchan]
		set ::vfs::template::version::filestats($channelID) "filename [list $fileName] hash [list $hash]"
		return $channelID
	}

# otherwise, get hash of existing file and store it where close callback can grab it,
# then return memchan:
	set f [open $fileName r]
	fconfigure $f -translation binary
	set hash [Hash $f]
	close $f
	set filed [memchan]
	if {[string index $mode 0] == "a"} {
		set f [open $fileName r]
		fconfigure $f -translation binary
		fconfigure $filed -translation binary
		fcopy $f $filed
		close $f
		seek $filed 0
	}
	set fileStats(hash) $hash
	set fileStats(filename) $fileName
	set ::vfs::template::version::filestats($filed) [array get fileStats]
	return $filed
}


proc MountProcedure {args} {
	upvar volume volume

# take real and virtual directories from command line args.
	set to [lindex $args end]
	if [string equal $volume {}] {set to [::file normalize $to]}
	set path [::file normalize [lindex $args end-1]]

# make sure mount location exists:
	::file mkdir $path

# add custom handling for new vfs args here.

	namespace import -force ::fileutil::globfind::globfind

	set argsLength [llength $args]
	for {set i 0} {$i < $argsLength} {incr i} {
		switch -- [lindex $args $i] {
			-keep {
				set keep [lindex $args [incr i]]
				if ![string is digit -strict $keep] {continue}
				set ::vfs::template::version::keep($to) $keep
			}
			-project {
				set project [lindex $args [incr i]]
				set ::vfs::template::version::project($to) $project
			}
			-time {
				set time [lindex $args [incr i]]
				SetTime $time
				set ::vfs::template::version::time($to) $time
			}
		}
	}

	if [catch {glob -directory $path -type {f hidden} .&dir*}] {
		set root $to
		file_mkdir $path
	}


# return two-item list consisting of real and virtual locations.
	lappend pathto $path
	lappend pathto $to
	return $pathto
}


proc UnmountProcedure {path to} {
# add custom unmount handling of new vfs elements here.
	if [info exists ::vfs::template::version::keep($to)] {unset ::vfs::template::version::keep($to)}
	if [info exists ::vfs::template::version::project($to)] {unset ::vfs::template::version::project($to)}
	if [info exists ::vfs::template::version::time($to)] {unset ::vfs::template::version::time($to)}
	return
}

# utility proc called by file_delete for recursive deletion of dir contents:
proc globdelete {file} {
	upvar root root deleted deleted
	set file [file join [file dirname $file] [lindex [split [file tail $file] \;] 0]]
	if {[lsearch $deleted $file] > -1} {return}
	lappend deleted $file
	set fileName $file\;[VCreateTag $root]
	set fileName [split $fileName \;] 
	set fileName [linsert $fileName 2 "deleted"]
	set fileName [join $fileName \;]
	close [open $fileName w]
	if ![string first {.&} [file tail $fileName]] {catch {file attributes $fileName -hidden 1}}
}

# Can replace this proc with one that uses different hash function if preferred.
proc Hash {channel} {
	seek $channel 0
	if [catch {package present md5 2}] {package forget md5 ; package require md5 2}
	::md5::md5 -hex -- [read $channel]
}

# figure out if time is a string, milliseconds or seconds count, return seconds count
proc SetTime {time} {
	if ![string is digit -strict $time] {catch {set time [clock scan $time]}}
	if ![string is digit -strict $time] {error "invalid time value."}
	set time "[string range $time 0 [expr [string length [clock seconds]] - 1]]000"
}

# decide which version is preferred considering time and project settings:
proc VAcquireFile {path root relative {actualpath {}}} {
	set fileName [VFileNameEncode [file join $path $relative]]
	if [file isdirectory [file join $path $relative]] {
		set fileName [file join $fileName .&dir[file tail $fileName]]
		set relative [file join $relative .&dir[file tail $relative]]
	}

# grab all versions:
	set versions [glob -path $fileName -nocomplain -types f "\;*"]
	if [catch {::vfs::filesystem info $path}] {append versions " [glob -path $fileName -nocomplain -types "f hidden" "\;*"]"}

	set versions [string trim $versions]
	if {$versions == {}} {return [file join $path $relative]}

	set checkProject 0
	if [info exists ::vfs::template::version::project($root)] {
		set projects [string map {; &s} [string map {& &a} $::vfs::template::version::project($root)]]
		set checkProject 1
	}

# find versions that have current project tags to see if keep setting requires deleting any:
	foreach ver $versions {
		set ver $root/[file tail $ver]
		lappend versionFiles $ver
		if !$checkProject {continue}
		foreach project $projects {
			if {[lsearch [lrange [split $ver \;] 1 end] $project] > -1} {lappend projectFiles $ver}
		}
	}
	unset versions

# delete older versions if keep setting requires it:
	if ![catch {if {[llength $projectFiles] <= $::vfs::template::version::keep($root)} {error}}] {
		set keep $::vfs::template::version::keep($root)
		set projectFiles [lsort -decreasing -dictionary $projectFiles]
		set fileNumber [llength $projectFiles]
		for {set i [incr fileNumber -1]} {$i >= 0} {incr i -1} {
			if {[llength $projectFiles] <= $keep} {break}
			set delFile [file join [file dirname [file join $path $relative]] [file tail [lindex $projectFiles $i]]]
			if ![catch {file delete -- $delFile}] {set projectFiles [lreplace $projectFiles $i $i]}
		}
	}

# find version that's best match with time and project settings:
	set fileName [file tail [lindex [lsort -command VersionSort $versionFiles] 0]]

# if file version has "deleted" tag, return with no version info, indicating file doesn't exist:
	if !$checkProject {
		if {[lindex [split $fileName \;] 2] == "deleted"} {return [file join $path $relative]}
	}

# if project attribute is set, and version has project tags, ensure version belongs to one of the set projects,
# otherwise it will be invisible:
	if $checkProject {
		if {([lindex [split $fileName \;] 2] == "deleted") && ([lsearch $projects "deleted"] == -1)} {return [file join $path $relative]}
		set projectMember 0
		set tags [lrange [split $fileName \;] 1 end]
		if {[lindex $tags 1] == "deleted"} {set tags [lreplace $tags 1 1]}
		foreach project $projects {
				if {[lsearch $tags $project] > -1} {set projectMember 1}
		}
		set projectLength [llength $tags]
		if {($projectLength > 1) && !$projectMember} {return [file join $path $relative]}
	}

# if time tag value is before creation date of chosen version, make file invisible:
	if {[info exists ::vfs::template::version::time($root)] && ([lindex [split $fileName \;] 1] > "$::vfs::template::version::time($root)000")} {
		return [file join $path $relative]
	}

	if ![string first .&dir [file tail $relative]] {
		set fileName [file join $path [file dirname $relative]]
		return [file normalize $fileName]
	}
	return [file join [file dirname [file join $path $relative]] $fileName]
}

# create new version tag with millisecond-scale timestamp and curernt project tags:
proc VCreateTag {root} {
	set tag [clock seconds][string range [clock clicks -milliseconds] end-2 end]
	if [info exists ::vfs::template::version::project($root)] {
		set projects [string map {; &s} [string map {& &a} $::vfs::template::version::project($root)]]
		set projectTag [join $projects \;]
		set tag [join "$tag $projectTag" \;]
	}
	return $tag
}

# return info on all versions of a file:
proc VersionsAll {path relative} {
	set fileName [VFileNameEncode [file join $path $relative]]
	if [file isdirectory [file join $path $relative]] {
		set fileName [file join $fileName .&dir[file tail $fileName]]
		set relative [file join $relative .&dir[file tail $relative]]
	}
	set versions [glob -path $fileName -nocomplain -types f "\;*"]
	if [catch {::vfs::filesystem info $path}] {append versions " [glob -path $fileName -nocomplain -types "f hidden" "\;*"]"}

	set versions [string trim $versions]

	set newVersions {}
	foreach ver $versions {
		set ver [lrange [split $ver \;] 1 end]
		set ver [file tail [VFileNameDecode $ver]]
		set tag [lindex $ver 0]
		set time [string range $tag 0 end-3]
		set time [clock format $time -format "%Y%m%dT%H:%M:%S"]
		lappend newVersions "$tag $time [list [lrange $ver 1 end]]"
	}
	return $newVersions
}

# specialized command for lsort, decide which of two versions is preferred given 
# project and time settings:
proc VersionSort {element1 element2} {
	set root [file dirname $element1]
	set element1 [file tail $element1]
	set element2 [file tail $element2]
	if [string equal $element1 $element2] {return 0}
	set sorted [lsort -dictionary -decreasing "$element1 $element2"]

# decision 1: choose latest timestamp:
	if {[lindex $sorted 0] == $element1} {set returnValue -1}
	if {[lindex $sorted 0] == $element2} {set returnValue 1}

	set time1 [lindex [split $element1 \;] 1]
	set time2 [lindex [split $element2 \;] 1]
	set time $time1
	if {$time2 > $time1} {set time $time2}

# decision 2: if time setting exists, choose latest timestamp less than time setting:
	if [info exists ::vfs::template::version::time($root)] {
		set returnValue -1
		set time $::vfs::template::version::time($root)
		if {!([string is digit -strict $time] && ([string length $time] == [string length $time1]))} {
			set time [SetTime $::vfs::template::version::time($root)]
		}

		if {$time1 > $time} {set time1 [expr $time2 - 1]}
		if {($time2 <= $time) && ($time2 > $time1)} {set returnValue 1}
	}

# decision 3: choose version with greatest number of project tag matches with project setting:
	if [info exists ::vfs::template::version::project($root)] {
		set projects [string map {; &s} [string map {& &a} $::vfs::template::version::project($root)]]
		set project1 0
		set project2 0
		foreach project [lsort -unique $projects] {
			set sumproject1 [lsearch [lrange [split $element1 \;] 1 end] $project]
			set sumproject2 [lsearch [lrange [split $element2 \;] 1 end] $project]
			incr sumproject1 ; incr sumproject2
			if $sumproject1 {incr project1}
			if $sumproject2 {incr project2}
		}
		if {$project1 > $project2} {set project1 1 ; set project2 0}
		if {$project1 < $project2} {set project1 0 ; set project2 1}

		# don't count "deleted" as a project tag for purpose of choosing default:
		set tagEnd1 [lindex [split $element1 \;] 2]
		if {$tagEnd1 == "deleted"} {set tagEnd1 [lindex [split $element1 \;] 3]}
		set tagEnd2 [lindex [split $element2 \;] 2]
		if {$tagEnd2 == "deleted"} {set tagEnd2 [lindex [split $element2 \;] 3]}
	
		# set version with no project tags as default choice:
		if {($tagEnd1 == {}) && !($tagEnd2 == {})} {set returnValue -1}
		if {!($tagEnd1 == {}) && ($tagEnd2 == {})} {set returnValue 1}

		# if a project tag match exists, replace default choice with it:
		if {$project2 && !$project1 && ($time2 <= $time)} {set returnValue 1}
		if {$project1 && !$project2 && ($time1 <= $time)} {set returnValue -1}
	}
	return $returnValue
}

# ampersand and semicolon are privileged chars in tagging, 
# encode and decode filenames containing them:
proc VFileNameEncode {filename} {
	set filename [file dirname $filename]/[string map {& &a} [file tail $filename]]
	set filename [file dirname $filename]/[string map {; &s} [file tail $filename]]
}

proc VFileNameDecode {filename} {
	set filename [file dirname $filename]/[lindex [split [file tail $filename] \;] 0]
	set filename [file dirname $filename]/[string map {&s ;} [file tail $filename]]
	set filename [file dirname $filename]/[string map {&a &} [file tail $filename]]
}

}
# end namespace ::vfs::template::version

