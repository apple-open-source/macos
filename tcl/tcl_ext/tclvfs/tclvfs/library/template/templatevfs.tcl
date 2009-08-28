#/usr/bin/env tclsh

if 0 {
########################

templatevfs.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5.2

The template virtual filesystem is designed as a prototype on which to build new virtual 
filesystems.  Only a few simple, abstract procedures have to be overridden to produce a new
vfs, requiring no knowledge of the Tclvfs API. 

In addition, several behind-the-scenes functions are provided to make new vfs's more stable and
scalable, including file information caching and management of close callback errors. 

The template vfs provides a useful function of its own, it mirrors a real directory to a 
virtual location, analogous to a Unix-style link.

Usage: mount ?-cache <number>? ?-volume? <existing directory> <virtual directory>

Options:

-cache
Sets number of seconds file stat and attributes information will dwell in cache after 
being retrieved.  Default is 2.  Setting value of 0 will essentially disable caching.  This 
value is viewable and editable after mount by calling "file attributes <virtual directory> -cache ?value?"

-volume
Volume specified in virtual directory pathname will be mounted as a virtual volume.

The above options are inherited by all virtual filesystems built using the template.

Side effects: Files whose names begin with ".vfs_" will be ignored and thus invisible to the 
user unless the variable ::vfs::template::vfs_retrieve exists.

Sourcing this file will run code that overloads the exit command with
a procedure that ensures that all vfs's are explicitly unmounted before the
shell terminates.

When a vfs built on the template vfs is mounted, the mount command options are stored in an array named
vfs::template::mount with the virtual mount point as the array index name.  Thus a vfs can be re-mounted
by executing "eval" on the contents of the array element whose index is the vfs's virtual mount point.

########################
}

package require vfs 1.0

# force sourcing of vfsUtils.tcl: 
set vfs::posix(load) x
vfs::posixError load
unset vfs::posix(load)

package provide vfs::template 1.5.2

namespace eval ::vfs::template {

if 0 {
########################

In order to create a new virtual filesystem:

1. copy the contents of this namespace eval statement to a
new namespace eval statement with a unique new namespace defined

2. rewrite the copied procedures to retrieve and handle virtual filesystem 
information as desired and return it in the same format as the given native
file commands.

########################
}

package require vfs::template 1.5

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

# edit following procedures:

# Do not close channel within this procedure (will cause error).  Simply
# read info from channel as needed and return.
proc close_ {channel} {return}

# Variable $time is always defined.  These procs only set time values.
proc file_atime {file time} {file atime $file $time}
proc file_mtime {file time} {file mtime $file $time}

# Variables $attribute and $args may or may not be empty.
# If $attribute is empty so is $args (retrieve all attributes and values).
# If $args only is empty, retrieve value of specified attribute.
# If $args has a value, set it as value of specified attribute.
proc file_attributes {file {attribute {}} args} {eval file attributes \$file $attribute $args}

# Variable $file may be a file or directory.
# This proc only called if it is certain that deletion is the correct action.
proc file_delete {file} {file delete -force -- $file}

proc file_executable {file} {file executable $file}
proc file_exists {file} {file exists $file}
proc file_mkdir {file} {file mkdir $file}
proc file_readable {file} {file readable $file}
proc file_stat {file array} {upvar $array fs ; file stat $file fs}
proc file_writable {file} {file writable $file}

# All variables are always defined.
# Return list of filenames only, not full pathnames.
proc glob_ {directory dir nocomplain tails types typeString dashes pattern} {glob -directory $dir -nocomplain -tails -types $typeString -- $pattern}
proc open_ {file mode} {open $file $mode}


# MountProcedure is called once each time a vfs is newly mounted.
proc MountProcedure {args} {
	upvar volume volume

# take real and virtual directories from command line args.
	set to [lindex $args end]
	if [string equal $volume {}] {set to [::file normalize $to]}
	set path [::file normalize [lindex $args end-1]]

# make sure mount location exists:
	::file mkdir $path

# add custom handling for new vfs args here.

# return two-item list consisting of real and virtual locations.
	lappend pathto $path
	lappend pathto $to
	return $pathto
}


proc UnmountProcedure {path to} {
# add custom unmount handling of new vfs elements here.

	return
}

}
# end namespace ::vfs::template


# Below are template API procedures; there should be no need to edit them.

namespace eval ::vfs::template {

proc mount {args} {

# handle template command line args:
	set volume [lindex $args [lsearch $args "-volume"]]
	set cache 2
	if {[set cacheIndex [lsearch $args "-cache"]] != -1} {set cache [lindex $args [incr cacheIndex]]}
	set args [string map "\" -volume \" { } \" -cache $cache \" { }" " $args "]
# run unmount procedure if mount exists:
	set to [lindex $args end]
	if [info exists ::vfs::_unmountCmd($to)] {$::vfs::_unmountCmd($to) $to}

# call custom mount procedure:
	# ensure files named ".vfs_*" can be opened
	set ::vfs::template::vfs_retrieve 1

	set pathto [eval MountProcedure $args]

	# re-hide ".vfs_*" files
	unset -nocomplain ::vfs::template::vfs_retrieve

	set path [lindex $pathto 0]
	set to [lindex $pathto 1]
	if [string equal $volume {}] {set to [file normalize $to]}

# preserve mount info for later duplication if desired:
	set ::vfs::template::mount($to) "[namespace current]::mount $volume -cache $cache $args"

# if virtual location still mounted, unmount it by force:
	if {[lsearch [::vfs::filesystem info] $to] != -1} {::vfs::filesystem unmount $to}
	array unset ::vfs::_unmountCmd $to

# set file info cache dwell time value:
	set [namespace current]::cache($to) $cache

# register location with Tclvfs package:
	eval ::vfs::filesystem mount $volume \$to \[list [namespace current]::handler \$path\]
	::vfs::RegisterMount $to [list [namespace current]::unmount]

# ensure close callback background error appears at script execution level:
	trace remove execution ::close leave ::vfs::template::CloseTrace
	trace remove execution ::file leave ::vfs::template::FileTrace
	trace add execution ::close leave vfs::template::CloseTrace
	trace add execution ::file leave vfs::template::FileTrace

	return $to
}

# undo Tclvfs API hooks:
proc unmount {to} {
	set to [::file normalize $to]
	set path [lindex [::vfs::filesystem info $to] end]

# call custom unmount procedure:
	set ::vfs::template::vfs_retrieve 1
	UnmountProcedure $path $to
	unset -nocomplain ::vfs::template::vfs_retrieve

	::vfs::filesystem unmount $to
	array unset ::vfs::_unmountCmd [::file normalize $to]

# clear file info caches:
	CacheClear $to
}

# vfshandler command required by Tclvfs API:
proc handler {path cmd root relative actualpath args} {
# puts [list $path $root $relative $cmd $args [namespace current]]

	set fileName [::file join $path $relative]
	set virtualName [::file join $root $relative]
	switch -- $cmd {
		access {
			set mode [lindex $args 0]
			set error [catch {Access $path $root $relative $actualpath $mode}]
			if $error {::vfs::filesystem posixerror $::vfs::posix(EACCES) ; return -code error $::vfs::posix(EACCES)}
		}
		createdirectory {
			CreateDirectory $path $root $relative $actualpath
			CacheClear $virtualName
		}
		deletefile {
			DeleteFile $path $root $relative $actualpath
			CacheClear $virtualName
		}
		fileattributes {
			set index [lindex $args 0]
			if {[llength $args] > 1} {set value [lindex $args 1]}
			set extra {}
			if [string equal $relative {}] {eval set extra \"-cache \$[namespace current]::cache(\$root)\"}

			# try to get values from cache first:
			array set attributes [CacheGet [namespace current]::attributes $virtualName [set [namespace current]::cache($root)]]
			# if not in cache, get them from file:
			if [string equal [array get attributes] {}] {
				array set attributes "[FileAttributes $path $root $relative $actualpath] $extra"
				CacheSet [namespace current]::attributes $virtualName [array get attributes]
			}

			set attribute [lindex [lsort [array names attributes]] $index]

			# if value given in args, set it and return:
			if [info exists value] {
				if [string equal $attribute "-cache"] {
					set [namespace current]::cache($root) $value
				} else {
					FileAttributesSet $path $root $relative $actualpath $attribute $value
				}
				CacheClear $virtualName
				return
			}

			# if attribute given in args, return its value:
			if ![string equal $index {}] {
				return $attributes($attribute)
			}
			# otherwise, just return all attribute names
			return [lsort [array names attributes]]
		}
		matchindirectory {
			set pattern [lindex $args 0]
			set types [lindex $args 1]
			return [MatchInDirectory $path $root $relative $actualpath $pattern $types]
		} open {
			# ensure files named ".vfs_*" can't be opened ordinarily:
			if {![string first ".vfs_" [file tail $relative]] && ![info exists ::vfs::template::vfs_retrieve]} {vfs::filesystem posixerror $::vfs::posix(EACCES)}

			set mode [lindex $args 0]
			if {$mode == {}} {set mode r}

			# workaround: Tclvfs can't handle channels in write-only modes; see Tclvfs bug #1004273
			if {$mode == "w"} {set mode w+}
			if {$mode == "a"} {set mode a+}

			set permissions [lindex $args 1]
			set channelID [Open $path $root $relative $actualpath $mode $permissions]

			# ensure channel settings match file command defaults
			set eofChar {{} {}}
			if [string equal $::tcl_platform(platform) "windows"] {set eofChar "\x1a {}"}
			fconfigure $channelID -encoding [encoding system] -eofchar $eofChar -translation auto
			switch -glob -- $mode {
				"" -
				"r*" -
				"w*" {
					seek $channelID 0
				}
				"a*" {
	    				seek $channelID 0 end
				}
				default {
					::vfs::filesystem posixerror $::vfs::posix(EINVAL)
					return -code error $::vfs::posix(EINVAL)
				}
			}

			set result $channelID
			# designate handler as close callback command
			lappend result [list [namespace current]::handler $path close $root $relative $actualpath $channelID $mode]


			# make sure all interpreters can catch errors in close callback:
			foreach int [interp slaves] {
				InterpSeed $int
			}

			CacheClear $virtualName
			return $result
		} close {
			set channelID [lindex $args 0]
			set mode [lindex $args 1]
			if [string equal $mode "r"] {return}
			# never use real close command here, custom overloaded proc only.
			set err [catch {close_ $channelID} result]
			if $err {::vfs::template::closeerror $::errorInfo ; error $::errorInfo}
			return
		}
		removedirectory {
			set recursive [lindex $args 0]
			if !$recursive {
				if {[MatchInDirectory $path $root $relative $actualpath * 0] != {}} {
					::vfs::filesystem posixerror $::vfs::posix(EEXIST)
					return -code error $::vfs::posix(EEXIST)
				}
			}
			if {$relative == {}} {unmount $root ; return}
			RemoveDirectory $path $root $relative $actualpath
			CacheClear $virtualName
		}
		stat {
			set stat [CacheGet [namespace current]::stat $virtualName [set [namespace current]::cache($root)]]
			if ![string equal $stat ""] {
				return $stat
			}
			set stat [Stat $path $root $relative $actualpath]
			CacheSet [namespace current]::stat $virtualName $stat
			return $stat
		}
		utime {
			set atime [lindex $args 0]
			set mtime [lindex $args 1]
			Utime $path $root $relative $actualpath $atime $mtime
			array unset [namespace current]::stat $virtualName,time ; array unset [namespace current]::stat $virtualName,value
		}
	}
}

# following commands carry out information processing requirements for each vfshandler subcommand:
# note that all calls to file commands are redirected to simplified API procs at top of this script

proc Access {path root relative actualpath mode} {
	set fileName [::file join $path $relative]
	set virtualName [::file join $root $relative]
	set modeString [::vfs::accessMode $mode]
	set modeString [split $modeString {}]
	set modeString [string map "F exists R readable W writable X executable" $modeString]
	set secs [clock seconds]
	foreach mode $modeString {
		set result [CacheGet [namespace current]::$mode $virtualName [set [namespace current]::cache($root)] $secs]
		if [string equal $result ""] {
			set result [eval file_$mode \$fileName]
			CacheSet [namespace current]::$mode $virtualName $result $secs
		}
		if !$result {error error}
	}
	return
}

proc CreateDirectory {path root relative actualpath} {
	file_mkdir [::file join $path $relative]
}

proc DeleteFile {path root relative actualpath} {
	set fileName [::file join $path $relative]
#	file delete -force -- $fileName
	file_delete $fileName
}

proc FileAttributes {path root relative actualpath} {
	set fileName [::file join $path $relative]
	return [file_attributes $fileName]
}

proc FileAttributesSet {path root relative actualpath attribute value} {
	set fileName [::file join $path $relative]
	file_attributes $fileName $attribute $value
}

proc MatchInDirectory {path root relative actualpath pattern types} {
# special case: check for existence (see Tclvfs bug #1405317)
	if [string equal $pattern {}] {
		if ![::vfs::matchDirectories $types] {return {}}
		return [::file join $root $relative]
	}

# convert types bitstring back to human-readable alpha string:
	foreach {type shift} {b 0 c 1 d 2 p 3 f 4 l 5 s 6} {
		if [expr {$types == 0 ? 1 : $types & (1<<$shift)}] {lappend typeString $type}
	}	
	set pathName [::file join $path $relative]

# get non-hidden files:
	set globList [glob_ -directory $pathName -nocomplain -tails -types $typeString -- $pattern]
# if underlying location is not itself a vfs, get hidden files (Tclvfs doesn't pass "hidden" type to handler)
	if [catch {::vfs::filesystem info $path}] {set globList [concat $globList [glob_ -directory $pathName  -nocomplain -tails -types "$typeString hidden" -- $pattern]]}

# convert real path to virtual path:
	set newGlobList {}
	foreach gL $globList {
		if {![string first ".vfs_" $gL] && ![info exists ::vfs::template::vfs_retrieve]} {continue}
		set gL [::file join $root $relative $gL]
		lappend newGlobList $gL
	}
	set newGlobList [lsort -unique $newGlobList]
	return $newGlobList
}

proc Open {path root relative actualpath mode permissions} {
	set fileName [::file join $path $relative]
	set newFile 0
	if ![file exists $fileName] {set newFile 1}
	set channelID [open_ $fileName $mode]
	if $newFile {catch {file_attributes $fileName -permissions $permissions}}
	return $channelID
}

proc RemoveDirectory {path root relative actualpath} {
	set fileName [::file join $path $relative]
#	file delete -force -- $fileName
	file_delete $fileName
}

proc Stat {path root relative actualpath} {
	file_stat [::file join $path $relative] fs
	return [array get fs]
}

proc Utime {path root relative actualpath atime mtime} {
	set fileName [::file join $path $relative]
	file_atime $fileName $atime
	file_mtime $fileName $mtime
}

# check value of ::errorInfo to ensure close callback didn't generate background 
# error; if it did, force error break.
proc CloseTrace {commandString code result op} {
	if {[info exists ::vfs::template::vfs_error] && ($::vfs::template::vfs_error != {})} {
		set vfs_error $::vfs::template::vfs_error
		closeerror {}
		error $vfs_error
	}
	return
}

# file copy and file rename may trigger close callbacks internally, so check for close errors
# after these commands complete.
proc FileTrace {commandString code result op} {
	if {[string map {copy {} rename {}} [lindex $commandString 1]] != {}} {return}
	if {[info exists ::vfs::template::vfs_error] && ($::vfs::template::vfs_error != {})} {
		set vfs_error $::vfs::template::vfs_error
		closeerror {}
		error $vfs_error
	}
	return
}

# ensure ::errorInfo from background errors makes it into every child interpreter
# so CloseTrace and FileTrace can intercept it.

proc closeerror {errorInfo} {
	set ::vfs::template::vfs_error $errorInfo
	foreach int [interp slaves] {
		InterpSeed $int set ::vfs::template::vfs_error $::vfs::template::vfs_error
	}
}

# seed all interpreters with trace structures necessary to intercept close callback errors:
proc InterpSeed {interp args} {
	interp eval $interp {namespace eval ::vfs::template {}}
	$interp alias ::vfs::template::closeerror ::vfs::template::closeerror
	$interp alias ::vfs::template::FileTrace ::vfs::template::FileTrace
	$interp alias ::vfs::template::CloseTrace ::vfs::template::CloseTrace
	interp eval $interp trace remove execution ::file leave ::vfs::template::FileTrace 
	interp eval $interp trace remove execution ::close leave ::vfs::template::CloseTrace 

	interp eval $interp trace add execution ::close leave ::vfs::template::CloseTrace
	interp eval $interp trace add execution ::file leave ::vfs::template::FileTrace 

	interp eval $interp $args
	foreach int [interp slaves $interp] {
		InterpSeed $int $args
	}
}

# cache management functions:
proc CacheClear {file} {
	foreach arr {exists readable writable executable stat attributes} {
		array unset [namespace current]::$arr $file,time
		array unset [namespace current]::$arr $file,value
		array unset [namespace current]::$arr $file/*
	}
}

proc CacheGet {array file cache args} {
	if [string equal [array names $array $file,time] {}] {return}
	if ![string equal $args {}] {set secs $args} else {set secs [clock seconds]}
	set fileTime [lindex [array get $array $file,time] 1]
	if {[expr $secs - $fileTime] < $cache} {return [lindex [array get $array $file,value] 1]}
	array unset $array $file,time ; array unset $array $file,value
	return
}

proc CacheSet {array file value args} {
	if ![string equal $args {}] {set secs $args} else {set secs [clock seconds]}
	set fileTime $file,time
	array set $array [list $fileTime $secs]
	set fileValue $file,value
	array set $array [list $fileValue $value]
}

# map built-in file selection dialogs to pure Tk equivalents, so virtual
# filesystems can be browsed with same-looking code:
proc tk_getOpenFile {args} {
	eval [eval list ::tk::dialog::file:: open $args]
}

proc tk_getSaveFile {args} {
	eval [eval list ::tk::dialog::file:: save $args]
}

proc tk_chooseDirectory {args} {
	eval [eval list ::tk::dialog::file::chooseDir:: $args]
}

# workaround for bug in tclkit:
proc memchan {args} {
	if {$::tcl_platform(platform) == "windows"} {
		package require Memchan
		set chan [uplevel 1 ::memchan $args]
		return $chan
	} else {
		return ::vfs::memchan $args
	}
}

}
# end namespace eval ::vfs::template

# overload exit command so that all vfs's are explicitly 
# unmounted before program termination:
 
catch {rename ::exit ::vfs::template::exit}

proc ::exit {} {
	foreach vfs [::vfs::filesystem info] {
		if [catch {$::vfs::_unmountCmd($vfs) $vfs} result] {
			puts "$vfs: $result"
		}		
	}
	::vfs::template::exit
}

