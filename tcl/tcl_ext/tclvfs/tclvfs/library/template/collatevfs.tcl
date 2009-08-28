if 0 {
########################

collatevfs.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5

A collate/broadcast/collect/catchup virtual filesystem.  Requires the template vfs in templatevfs.tcl.

Collate: reads from multiple specified directories and presents the results as one at the mount location.

Broadcast: applies all writes in the mount location to multiple specified directories.

Collect: copies any file read from or written to any of the above locations to specified directories. 

Catchup: If any specified directory is not available during any write action, the action is recorded in 
a catchup queue.  With each subsequent write action, the queue is examined, and if any directory has 
become available, the action is performed, allowing offline directories to "catch up."

Usage: mount ?-read <directories> -write <directories> -collect <directories> -catchup <directories>? <virtual directory>

Each pathname in <directories> is meant to stand individually, the <directories> symbol is not meant to indicate a 
Tcl list.  The sets of specified locations are independent; they can overlap or not as desired.  Note each
option flag is optional, one could for example use only the -read flag to create a read-only directory.  Directories
do not have to exist and may go missing after mount, non-reachable locations will be ignored.

Options:

-read
When an individual file is opened for reading, each of the directories specified is searched in 
order for the file; the first file found with the appropriate name is opened.  When a subdirectory listing is 
generated, the combined files of the corresponding subdirectory of all specified directories are listed together.

-write
When an individual file is opened for writing, each of the directories specified is searched in 
order for the file; the first file found with the appropriate name is opened.  If the file doesn't exist, 
it is created in the first specified write location.  When the file is closed, a copy of it is distributed to 
each specified write directory.

-collect
Auto-generates one or more file caches; a copy of any file opened for reading or writing in any of the above 
specified directories is made to each directory specified with the -collect flag.  Collect locations are 
not included in file or directory listings, and are not searched for read access; so in order to make an 
active read cache, for example, one would have to include one directory location in both the -read and -collect sets.

-catchup
If this flag is included, the catchup function is activated, and a copy of the catchup queue is stored in a
file in each of the specified directories.  File writes, directory creations and file/directory deletes are
stored in the catchup queue if any write location is offline; at the next write/creation/delete the queue is 
examined, and if any skipped action can be completed due to a location becoming available again, it 
will be.  A catchup attempt will be made at mount time if this flag is included.

The values of each option can be changed dynamically after mount by using the "file attributes" command on the
mount virtual directory. Each option is editable as an attribute; i.e., "file attributes C:/collate -write C:/tmp"

The collate vfs inherits the -cache and -volume options of the template vfs.


Example use: specify parallel locations on a hard drive, on a CD-ROM mount and an ftp vfs as the read list.
Files will be read first from the hard drive, if not found there the CD-ROM and ftp site will be searched in turn.
The hard drive can be specified as the single write location, and no writes to the CD-ROM or 
ftp site will ever be attempted:

mount -read C:/install/package/docs CDROM:/package/docs FTP:/pub/releases/package/docs -write C:/install/package/docs C:/collate/docs


Example collect location use: specify a single hard drive location as a read and collect directory.  
Specify a ftp vfs as a secondary read directory.  As ftp files are downloaded they are copied to the 
collect directory; the local copies are accessed first on subsequent reads: hence the collect
specification produces a self-generating local cache:

mount -read C:/install/package/images FTP:/pub/releases/package/images -collect C:/install/package/images C:/collate/images


########################
}

package require vfs::template 1.5
package provide vfs::template::collate 1.5.2

namespace eval ::vfs::template::collate {

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

# edit following procedures:
proc close_ {channel} {
	upvar root root relative relative
	foreach file [lrange [WriteFile $root $relative close] 1 end] {
		set f [open $file w]
		seek $channel 0
		fcopy $channel $f
		close $f
	}
	return
}
proc file_atime {file time} {
	upvar root root relative relative
	set file [AcquireFile $root $relative]
	file atime $file $time
}
proc file_mtime {file time} {
	upvar root root relative relative
	set file [AcquireFile $root $relative]
	file mtime $file $time
}
proc file_attributes {file {attribute {}} args} {
	upvar root root relative relative
	set file [AcquireFile $root $relative]
	if {($relative == {}) && ([string map {-read 1 -write 1 -collect 1 -catchup 1} $attribute] == 1)} {
		set attribute [string range $attribute 1 end]
		if {$args == {}} {eval return \$::vfs::template::collate::${attribute}(\$root)}
		set ::vfs::template::collate::${attribute}($root) [lindex $args 0]
		set ::vfs::template::collate::catchup [file isdirectory [lindex $::vfs::template::collate::catchupstore 0]]
		return
	}
	set returnValue [eval file attributes \$file $attribute $args]
	if {($relative == {}) && ($attribute == {})} {set returnValue [concat $returnValue [list -read $::vfs::template::collate::read($root) -write $::vfs::template::collate::write($root) -collect $::vfs::template::collate::collect($root) -catchup $::vfs::template::collate::catchupstore($root)]]}
	return $returnValue
}
proc file_delete {file} {
	upvar root root relative relative
	foreach file [WriteFile $root $relative delete] {
		file delete -force -- $file
	}
}
proc file_executable {file} {
	upvar root root relative relative
	set file [AcquireFile $root $relative]
	file executable $file
}
proc file_exists {file} {
	upvar root root relative relative
	expr ![catch {AcquireFile $root $relative}]
}
proc file_mkdir {file} {
	upvar root root relative relative
	foreach file [WriteFile $root $relative mkdir] {
		file mkdir $file
	}
}
proc file_readable {file} {
	upvar root root relative relative
	set file [AcquireFile $root $relative]
	file readable $file
}
proc file_stat {file array} {
	upvar root root relative relative
	set file [AcquireFile $root $relative]
	upvar $array fs ; file stat $file fs
}
proc file_writable {file} {
	upvar root root relative relative
	expr ![catch {WriteFile $root $relative open}]
}
proc glob_ {directory dir nocomplain tails types typeString dashes pattern} {
	upvar root root relative relative
	set allFiles {}
	set newFiles {}
	foreach path $::vfs::template::collate::read($root) {
		if ![file exists $path] {continue}
		set allFiles [concat $allFiles [glob -directory [file join $path $relative] -nocomplain -tails -types $typeString -- $pattern]]
	}
	set allFiles [lsort -unique $allFiles]
	return $allFiles
}
proc open_ {file mode} {
	upvar root root relative relative
	if [string match w* $mode] {
		set file [lindex [WriteFile $root $relative open] 0]
		file mkdir [file dirname $file]
		return [open $file $mode]
	}
	if [string match r* $mode] {
		set file [AcquireFile $root $relative]
		if {$mode == "r"} {
			foreach cpath $::vfs::template::collate::collect($root) {
				set cfile [file join $cpath $relative]
				if {$file == $cfile} {continue}
				if ![file exists $cpath] {continue}
				file mkdir [::file dirname $cfile]
				file copy -force -- $file $cfile
			}
			return [open $file r]
		}
		set wfile [lindex [WriteFile $root $relative open] 0]
		file mkdir [file dirname $wfile]
		if {$wfile != $file} {file copy -force -- $file $wfile}
		return [open $wfile $mode]
	}
	if [string match a* $mode] {
		set wfile [lindex [WriteFile $root $relative open] 0]
		file mkdir [file dirname $wfile]
		if ![catch {set file [AcquireFile $root $relative]}] {
			if {$wfile != $file} {file copy -force -- $file $wfile}
		} 
		return [open $wfile $mode]
	}
}

proc MountProcedure {args} {
	upvar volume volume

# take real and virtual directories from command line args.
	set to [lindex $args end]
	if [string equal $volume {}] {set to [::file normalize $to]}

# add custom handling for new vfs args here.

	set ::vfs::template::collate::catchup($to) 0
	set ::vfs::template::collate::read($to) {}
	set ::vfs::template::collate::write($to) {}
	set ::vfs::template::collate::collect($to) {}
	set ::vfs::template::collate::catchupstore($to) {}

	set args [lrange $args 0 end-1]
	set argsIndex [llength $args]
	for {set i 0} {$i < $argsIndex} {incr i} {
		set arg [lindex $args $i]

		switch -- $arg {
			-read {
				set type read
			}
			-write {
				set type write
			}
			-collect {
				set type collect
			}
			-catchup {
				set ::vfs::template::collate::catchup($to) 1
				set type catchupstore
			}
			default {
				eval lappend ::vfs::template::collate::${type}(\$to) \[::file normalize \$arg\]
			}
		}
	}

	WriteFile $to {} mkdir

# return two-item list consisting of real and virtual locations.
	lappend pathto {}
	lappend pathto $to
	return $pathto
}

proc UnmountProcedure {path to} {
# add custom unmount handling of new vfs elements here.
	unset -nocomplain ::vfs::template::collate::read($to)
	unset -nocomplain ::vfs::template::collate::write($to)
	unset -nocomplain ::vfs::template::collate::collect($to)
	unset -nocomplain ::vfs::template::collate::catchup($to)
	unset -nocomplain ::vfs::template::collate::catchupstore($to)
	return
}

proc AcquireFile {root relative} {
	foreach path $::vfs::template::collate::read($root) {
		set file [::file join $path $relative]
		if [::file exists $file] {
			return $file
		}
	}
	vfs::filesystem posixerror $::vfs::posix(ENOENT) ; return -code error $::vfs::posix(ENOENT)
}

proc WriteFile {root relative action} {
	set allWriteLocations {}
	foreach awl [concat $::vfs::template::collate::write($root) $::vfs::template::collate::collect($root)] {
		if {[lsearch $allWriteLocations $awl] < 0} {lappend allWriteLocations $awl}
	}
	if ![llength $allWriteLocations] {
		vfs::filesystem posixerror $::vfs::posix(EROFS) ; return -code error $::vfs::posix(EROFS)
	}
	if {$vfs::template::collate::catchup($root) && ([file tail $relative] != ".vfs_catchup") && ($action != "open")} {
		set catchupActivate 1
		set addCatchup {}
		set newCatchup {}
	} else {
		set catchupActivate 0
	}
	set returnValue {}
	foreach path $allWriteLocations  {
		if {$catchupActivate && ![file exists $path]} {
			append addCatchup "[list $action $path $relative]\n"
			continue
		}
		set rvfile [file join $path $relative]
		if {[lsearch $returnValue $rvfile] == -1} {lappend returnValue $rvfile}
	}
	if {$returnValue == {}} {vfs::filesystem posixerror $::vfs::posix(EROFS) ; return -code error $::vfs::posix(EROFS)}
	if $catchupActivate {
		set catchup {}
		set ::vfs::template::vfs_retrieve 1

		foreach store $::vfs::template::collate::catchupstore($root) {
			set store [file join $store ".vfs_catchup"]
			if [file readable $store] {
				set f [open $store r]
				unset ::vfs::template::vfs_retrieve
				seek $f 0
				set catchup [read $f]
				close $f
				break
			}
		}
		catch {set currentRead [AcquireFile $root {}]} result
		foreach {action path rel} $catchup {
			if {$relative == $rel} {continue}
			if ![file exists $path] {append newCatchup "[list $action $path $rel]\n" ; continue}
			if {[lsearch $allWriteLocations  $path] < 0} {continue}
			switch -- $action {
				close {
					if {![info exists currentRead] || ([set source [file join $currentRead $rel]] == [set target [file join $path $rel]])} {
						append newCatchup "[list $action $path $rel]\n" ; continue
					}
					if ![file exists $source] {continue}
					file mkdir [file dirname $target]
					file copy -force -- $source $target
				}
				delete {
					file delete -force -- [file join $path $rel]
				}
				mkdir {
					file mkdir [file join $path $rel]
				}
			}
		}
		append newCatchup $addCatchup
		foreach path $::vfs::template::collate::catchupstore($root) {
			set vfscatchup [file join $path ".vfs_catchup"]
			set ::vfs::template::vfs_retrieve 1
			set err [catch {
				if {$newCatchup != {}} {
					set f [open $vfscatchup w]
					puts $f $newCatchup
					close $f
				} else {
					file delete $vfscatchup
				}
			} result]
			unset ::vfs::template::vfs_retrieve
		}
	}
	return $returnValue
}

}
# end namespace ::vfs::template::collate
