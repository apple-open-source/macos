if 0 {
########################

deltavfs.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5.2

A delta virtual filesystem.  Requires the template vfs in templatevfs.tcl.

Mount the delta vfs first, then mount the versioning vfs using the virtual location created by the 
delta vfs as its existing directory.

As the versioning filesystem generates a new separate file for every file edit, this filesystem will 
invisibly generate and manage deltas of the separate versions to save space.


Usage: mount <existing directory> <virtual directory>


The delta vfs inherits the -cache and -volume options of the template vfs.

########################
}

package require vfs::template 1.5
package require vfs::template::version 1.5

package provide vfs::template::version::delta 1.5.2

namespace eval ::vfs::template::version::delta {

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

# edit following procedures:
proc close_ {channel} {
	upvar path path relative relative
	set file [file join $path $relative]
	set fileName $file
	set f [open $fileName w]
	fconfigure $f -translation binary
	seek $f 0
	seek $channel 0
	fcopy $channel $f
	close $f
	Delta $fileName
	return
}
proc file_atime {file time} {
	set file [GetFileName $file]
	file atime $file $time
}
proc file_mtime {file time} {
	set file [GetFileName $file]
	file mtime $file $time
}
proc file_attributes {file {attribute {}} args} {
	set file [GetFileName $file]
	eval file attributes \$file $attribute $args
}
proc file_delete {file} {
	if [file isdirectory $file] {catch {file delete $file}}

	set fileName [GetFileName $file]
	set timeStamp [lindex [split [file tail $fileName] \;] 1]
	if [string equal $timeStamp {}] {
		catch {file delete $fileName} result
		return
	}
	set targetFile [Reconstitute $fileName]
	set referenceFiles [glob -directory [file dirname $fileName] -nocomplain *vfs&delta$timeStamp]
	if {[lindex [file system $fileName] 0] != "tclvfs"} {append referenceFiles " [glob -directory [file dirname $fileName] -nocomplain -type hidden *vfs&delta$timeStamp]"}
	foreach referenceFile $referenceFiles {
		regsub {\;vfs&delta[0-9]*$} $referenceFile "" reconFile]
		set f [open $referenceFile r]
		fconfigure $f -translation binary
		set signature [read $f]
		close $f
		tpatch $targetFile $signature $reconFile
		file delete $referenceFile
	}
	close $targetFile

	file delete -force -- $fileName
}
proc file_executable {file} {
	set file [GetFileName $file]
	file executable $file
}
proc file_exists {file} {
	set file [GetFileName $file]
	file exists $file
}
proc file_mkdir {file} {file mkdir $file}
proc file_readable {file} {
	set file [GetFileName $file]
	file readable $file
}
proc file_stat {file array} {
	upvar $array fs
	set fileName [GetFileName $file]

	set endtag [lindex [split $fileName \;] end]
	if {[string first "vfs&delta" $endtag] || [string equal "vfs&delta" $endtag]} {file stat $fileName fs ; return}
	set f [open $fileName r]
	fconfigure $f -translation binary
	set copyinstructions [read $f]
	close $f
	array set fileStats [lindex $copyinstructions 3]
	unset copyinstructions
	set size $fileStats(size)
	file stat $fileName fs
	set fs(size) $size
	return 
}
proc file_writable {file} {
	set file [GetFileName $file]
	file writable $file
}
proc glob_ {directory dir nocomplain tails types typeString dashes pattern} {
	set globList [glob -directory $dir -nocomplain -tails -types $typeString -- $pattern]
	set newGlobList {}
	foreach gL $globList {
		regsub {\;vfs&delta.*$} $gL "" gL
		lappend newGlobList $gL
	}
	return $newGlobList
}
proc open_ {file mode} {
	set fileName [GetFileName $file]

	set newFile 0
	if ![file exists $fileName] {set newFile 1}
	set fileName $file
	set channelID [Reconstitute $fileName]
	if [string equal $channelID {}] {set channelID [open $fileName $mode] ; close $channelID ; set channelID [memchan]}
	if $newFile {catch {file attributes $fileName -permissions $permissions}}
	return $channelID
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
	package require trsync
	namespace import -force ::trsync::tdelta ::trsync::tpatch

# return two-item list consisting of real and virtual locations.
	lappend pathto $path
	lappend pathto $to
	return $pathto
}


proc UnmountProcedure {path to} {
# add custom unmount handling of new vfs elements here.

	return
}

proc Delta {filename} {
	set fileRoot [lindex [split [file tail $filename] \;] 0]
	set fileNames [glob -nocomplain -path [file join [file dirname $filename] $fileRoot] *]
	if {[lindex [file system $filename] 0] != "tclvfs"} {append fileNames " [glob -nocomplain -path [file join [file dirname $filename] $fileRoot] -type hidden *]"}
	set nonDeltas {}
	foreach fn $fileNames {
		set endtag [lindex [split $fn \;] end]
		if ![string first "vfs&delta" $endtag] {continue}
		lappend nonDeltas $fn
		set atimes($fn) [file atime $fn]
	}
	if {[set deltaIndex [llength $nonDeltas]] < 2} {return}
	set nonDeltas [lsort -dictionary $nonDeltas]
	incr deltaIndex -1
	set i 0
	while {$i < $deltaIndex} {
		set referenceFile [lindex $nonDeltas $i]
		set targetFile [lindex $nonDeltas [incr i]]
		set signature [tdelta $referenceFile $targetFile $::trsync::blockSize 1 1]
		set targetTimeStamp [lindex [split $targetFile \;] 1]

		file stat $referenceFile fileStats
		set signatureSize [string length $signature]
		if {$signatureSize > $fileStats(size)} {
			set fileName $referenceFile\;vfs&delta
			file rename $referenceFile $fileName
			continue
		}

		array set fileStats [file attributes $referenceFile]

		set fileName $referenceFile\;vfs&delta$targetTimeStamp
		set f [open $fileName w]
		fconfigure $f -translation binary
		puts -nonewline $f $signature
		close $f
		file delete $referenceFile
		array set fileAttributes [file attributes $fileName]
		if [info exists fileAttributes(-readonly)] {catch {file attributes $fileName -readonly 0}}
		if [info exists fileAttributes(-permissions)] {catch {file attributes $fileName -permissions rw-rw-rw-}}
		catch {file attributes $fileName -owner $fileStats(uid)}
		catch {file attributes $fileName -group $fileStats(gid)}
		
		catch {file mtime $fileName $fileStats(mtime)}
		catch {file atime $fileName $fileStats(atime)}

		foreach attr [array names fileStats] {
			if [string first "-" $attr] {continue}
			if [string equal [array get fileStats $attr] [array get fileAttributes $attr]] {continue}
			if [string equal "-permissions" $attr] {continue}
			catch {file attributes $fileName $attr $fileStats($attr)}
		}
		catch {file attributes $fileName -permissions $fileStats(mode)}
		catch {file attributes $fileName -readonly $fileStats(-readonly)}
	}
	foreach fn [array names atimes] {
		if ![file exists $fn] {continue}
		file atime $fn $atimes($fn)
	}
}

proc GetFileName {file} {
	set isdir 0
	if {([string first \; $file] == -1) && ![set isdir [file isdirectory $file]]} {return {}}
	if $isdir {return $file}
	set fileNames [glob -nocomplain -path $file *]
	if {[lindex [file system $file] 0] != "tclvfs"} {append fileNames " [glob -nocomplain -path $file -type hidden *]"}
	set fileName [lindex $fileNames 0]
	if [set i [expr [lsearch -exact $fileNames $file] + 1]] {set fileName [lindex $fileNames [incr i -1]]}
	return $fileName
}

proc Reconstitute {fileName} {
	if ![catch {set channelID [open $fileName r]}] {return $channelID}
	if ![catch {set channelID [open $fileName\;vfs&delta r]}] {return $channelID}
	set targetFiles [glob -nocomplain -path $fileName *]
	if {[lindex [file system $fileName] 0] != "tclvfs"} {append targetFiles " [glob -nocomplain -path $fileName -type hidden *]"}
	set targetFile [lindex $targetFiles 0]

	set targetFile [string trim $targetFile]
	if [string equal $targetFile {}] {return}
 	set fileStack {}
	while {[string first "\;vfs&delta" $targetFile] > -1} {
		if ![regexp {\;vfs&delta([0-9]+)$} $targetFile trash targetTime] {break}
		set fileStack "[list $targetFile] $fileStack"
		set targetFiles [glob -directory [file dirname $fileName] *\;$targetTime*]
		if {[lindex [file system $fileName] 0] != "tclvfs"} {append targetFiles " [glob -directory [file dirname $fileName] -nocomplain -type hidden *\;$targetTime*]"}
		set targetFile [lindex $targetFiles 0]

		set atimes($targetFile) [file atime $targetFile]
	}
	set targetFile [open $targetFile r]
	foreach fs $fileStack {
		set f [open $fs r]
		fconfigure $f -translation binary
		set copyInstructions [read $f]
		close $f
		set fileToConstruct [memchan]
		tpatch $targetFile $copyInstructions $fileToConstruct
		catch {close $targetFile}
		set targetFile $fileToConstruct
	}
	foreach fn [array names atimes] {
		file atime $fn $atimes($fn)
	}
	fconfigure $targetFile -translation auto
	seek $targetFile 0
	return $targetFile
}

}
# end namespace ::vfs::template::version::delta

