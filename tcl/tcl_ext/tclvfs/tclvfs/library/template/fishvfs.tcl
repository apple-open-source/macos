#! /usr/bin/env tclsh

if 0 {
########################

fishvfs.tcl --

 A "FIles transferred over SHell" virtual filesystem
 This is not an official "FISH" protocol client as described at:
	http://mini.net/tcl/12792
 but it utilizes the same concept of turning any computer that offers
 access via ssh, rsh or similar shell into a file server.
 
	Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
	License: Tcl license
	Version 1.5.2
 
 Usage: mount ?-volume? \
 	?-cache <number>? \		# cache retention seconds
 	?-exec? \				# location of executable
 	?-transport <protocol>? \	# can be ssh, rsh or plink
 	?-user <username>? \		# remote computer login name
 	?-password <password>? \	# remote computer login password
 	?-host <remote hostname>? \	# remote computer domain name
 	?-port <port number>? \		# override default port
	?<option> <value>?
 	<remote directory> \		# an existing directory on the remote filesystem
 	<virtual mount directory or URL>
 
Options:

-cache
Sets number of seconds file information will dwell in cache after being retrieved.
Default is 2.  This value is viewable and editable after mount by calling 
"file attributes <virtual directory> -cache ?value?"

-volume
Volume specified in virtual directory pathname will be mounted as a virtual volume.

-exec
Full pathname of ssh or equivalent program.  Default is name of the -transport option,
which is assumed to be the name of the executable program findable in the PATH.

-transport
Protocol used to transport commands to remote computer.  Built-in allowable values are
ssh, rsh or plink.  Extensible to new protocols with addition of a single command line
formatting proc.

The ssh option assumes rsa login protocol is set up so no interactive password entry
is necessary.

-user 
Login name at remote computer if necessary.

-password
Password for remote login name if necessary.

-host
Hostname of remote computer.  Only necessary if not specified in virtual mount URL.

-port
Override default port if necessary.

Arbitrary option/value pairs can be included in the command line; they may be useful if 
a custom new transport protocol handler is added which requires info not included in the
provided set.

The vfs can be mounted as a local directory, or as a URL in conjunction with 
the "-volume" option.
 
The URL can be of the form:
 
transport://[user[:password]@]host[:port][/filename]
 
Option switches can be used in conjunction with a URL to specify connection 
information; the option switch values will override the URL values.


Examples:
 
 mount -transport ssh -user root -host tcl.tk / /mnt/vfs/tcl
 
 mount -volume /home/foo rsh://foo@localcomp
 
 mount -volume -password foopass /home/foo plink://foo@bar.org:2323/remotemount
 
 mount -cache 60 -transport plink -user foo -password foopass -host bar.org /home/foo C:/Tcl/mount/foo
 

Client configuration:
 
 If the -exec option is not used, the shell client must be in the PATH; it must be
 configured for non-interactive (no password prompt) use.
 
 The value of the -transport option is used to load an appropriate handler 
 procedure which is called to handle the specifics of the particular client.
 Handlers for the supported transports (ssh, rsh, plink) already exist.
 New clients can be added simply by providing a suitable handler procedure.
 
 server configuration:
 
 The remote computer is assumed to be running an SSH server, have a sh-type shell and 
 the standard GNU fileutils, but otherwise no configuration is needed. 

########################
}

package require vfs::template 1.5
package provide vfs::template::fish 1.5.2

namespace eval ::vfs::template::fish {

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

proc close_ {channelID} {
	upvar root root path path relative relative
	set fileName [file join $path $relative]

	fconfigure $channelID -translation binary
	seek $channelID 0 end
	set channelSize [tell $channelID]

# use cat to pump channel contents to target file:
	set command "cat>'$fileName'\;cat>/dev/null"
	Transport $root $command stdin $channelID

# check file size to ensure proper transmission:
	set command "ls -l '$fileName' | ( read a b c d x e\; echo \$x )"
	set fileSize [Transport $root $command]
	if {$channelSize != $fileSize} {error "couldn't save \"$fileName\": Input/output error" "Input/output error" {POSIX EIO {Input/output error}}}
	return
}

proc file_atime {file time} {
	upvar root root
	set atime [clock format $time -format %Y%m%d%H%M.%S -gmt 1]
	set command "TZ=UTC\; export TZ\; touch -a -c -t $atime '$file'"
	Transport $root $command
	return $time
}

proc file_mtime {file time} {
	upvar root root
	set mtime [clock format $time -format %Y%m%d%H%M.%S -gmt 1]
	set command "TZ=UTC\; export TZ\; touch -c -m -t $mtime '$file'"
	Transport $root $command
	return $time
}

proc file_attributes {file {attribute {}} args} {
	upvar root root
	set tail [file tail $file]
	set value $args

# retrive info option:
	if {([string equal $attribute {}]) || ([string equal $value {}])} {
		set command "find '$file' -maxdepth 1 -name '$tail' -printf '%u %g %m\\n'"

# set info option:
	} elseif ![string first $attribute "-group"] {
		set command "chgrp $value '$file'"
	} elseif ![string first $attribute "-owner"] {
		set command "chown $value '$file'"
	} elseif ![string first $attribute "-permissions"] {
		set command "chmod $value '$file'"
	}

	set returnValue [Transport $root $command]

# format retrieved info:
	if [string equal $attribute {}] {
		return "-group [lindex $returnValue 1] -owner [lindex $returnValue 0] -permissions [lindex $returnValue 2]"
	}
	if [string equal $value {}] {
		if ![string first $attribute "-group"] {
			return [lindex $returnValue 1]
		} elseif ![string first $attribute "-owner"] {
			return [lindex $returnValue 0]
		} elseif ![string first $attribute "-permissions"] {
			return [lindex $returnValue 2]
		}
	}
	return
}

proc file_delete {file} {
	upvar root root			
	set command "rm -rf '$file'"
	Transport $root $command
}
proc file_executable {file} {	
	file_access $file executable
}
proc file_exists {file} {
	file_access $file exists
}
proc file_mkdir {file} {
	upvar root root			
	set  command "mkdir -p '$file'"
	Transport $root $command
}
proc file_readable {file} {
	file_access $file readable
}

if 0 {
###
In the interest of efficiency, the stat call grabs a lot of info.
Since many operations require a stat call and then an access call, this proc
grabs the file's access info as well as the stat info and caches it.  Stat info
for every file in the target directory is grabbed in one call and cached for
possible future use.
###
}
proc file_stat {file arrayName} {
	upvar $arrayName array
	upvar path path root root relative relative
	set secs [clock seconds]
	set cache $::vfs::template::fish::cache($root)

# combined command retrieves access and stat info:
	set command "if \[ -r '$file' \]\; then echo 1\; else echo 0\; fi \; if \[ -w '$file' \]\; then echo 1\; else echo 0\; fi \; if \[ -x '$file' \]\; then echo 1\; else echo 0\; fi \;  if \[ -e '$file' \]\; then echo 1\; else echo 0\; fi \; find '[::file dirname $file]' -maxdepth 1 -xtype d -printf '%A@ %C@ %G %i %m %T@ %n %s %U  \{%f\}\\n' \; echo / \; find '[::file dirname $file]' -maxdepth 1 -xtype f -printf '%A@ %C@ %G %i %m %T@ %n %s %U  \{%f\}\\n'"

# see if info is in cache:
	set returnValue [CacheGet ::vfs::template::fish::stat [::file join $root $relative] $cache $secs]

#if not, retrieve it:
	if [string equal $returnValue {}] {
		set returnValue [Transport $root $command]

		set dir 1
		set returnValue [split $returnValue \n]

# split off access info and cache it:
		set access [lrange $returnValue 0 3]
		set returnValue [lrange $returnValue 4 end]
		CacheSet ::vfs::template::fish::readable [file join $root $relative] [lindex $access 0] $secs
		CacheSet ::vfs::template::fish::writable [file join $root $relative] [lindex $access 1] $secs
		CacheSet ::vfs::template::fish::executable [file join $root $relative] [lindex $access 2] $secs
		CacheSet ::vfs::template::fish::exists [file join $root $relative] [lindex $access 3] $secs

# current dir info is first entry, discard it if file is not root:
		if ![string equal $file "/"] {set returnValue [lrange $returnValue 1 end]}

# format and cache info for each file in dir containing target file:
		set pathLength [llength [file split $path]]
		foreach rV $returnValue {
			if [string equal $rV "/"] {set dir 0 ; continue}
			set fileTail [lindex $rV end]
			set fN [::file join $root [join [lrange [file split [file join [file dirname $file] $fileTail]] $pathLength end] /]]

			set value "mtime [lindex $rV 5] gid [lindex $rV 2] nlink [lindex $rV 6] atime [lindex $rV 0] mode [lindex $rV 4] type [if $dir {set type directory} else {set type file}] ctime [lindex $rV 1] uid [lindex $rV 8] ino [lindex $rV 3] size [lindex $rV 7] dev -1"
			CacheSet ::vfs::template::fish::stat $fN $value $secs

		}
# grab info for target file from cache:
		set returnValue $::vfs::template::fish::stat([file join $root $relative],value)
	}
# feed info into upvar'd array:
	array set array $returnValue
	return
}

proc file_writable {file} {
	file_access $file writable
}

if 0 {
###
glob call aims to increase efficiency by grabbing stat info of listed files, under
assumption that a file listing is likely to be followed by an operation on one
of the listed files:
###
}
proc glob_ {d directory nocomplain tails types typeString dashes pattern} {

	upvar 1 path path root root relative relative

# list files along with their stat info:
	set command "find '$directory' -maxdepth 1 -mindepth 1 -xtype d -printf '%A@ %C@ %G %i %m %T@ %n %s %U  \{%f\}\\n' \; echo / \; find '$directory' -maxdepth 1 -mindepth 1 -xtype f -printf '%A@ %C@ %G %i %m %T@ %n %s %U  \{%f\}\\n'"

	set returnValue [Transport $root $command]
	set secs [clock seconds]
	set virtualName [file join $root $relative]

	set dirs {}
	set files {}
	set dir 1

# loop through file list and cache stat info:
	foreach rV [split $returnValue \n] {
		if [string equal $rV "/"] {set dir 0 ; continue}
	
		set fileTail [lindex $rV end]
		set fN [file join $virtualName $fileTail]

		set value "mtime [lindex $rV 5] gid [lindex $rV 2] nlink [lindex $rV 6] atime [lindex $rV 0] mode [lindex $rV 4] type [if $dir {set type directory} else {set type file}] ctime [lindex $rV 1] uid [lindex $rV 8] ino [lindex $rV 3] size [lindex $rV 7] dev -1"
		CacheSet ::vfs::template::fish::stat $fN $value $secs

		if $dir {lappend dirs $fileTail} else {lappend files $fileTail}
	}

# decide to return dirs, files or both:
	set dir [lsearch $typeString "d"]
	set file [lsearch $typeString "f"]
	incr dir ; incr file

	if $dir {set values $dirs}
	if $file {set values $files}
	if {$dir && $file} {set values [concat $dirs $files]}

# give filenames virtual paths:
	set fileNames {}
	foreach fileName $values {
		if [string equal $fileName "."] {continue}
		if [string equal $fileName ".."] {continue}
		if ![string match $pattern $fileName] {continue}
		lappend fileNames $fileName
	}
	return $fileNames
}

proc open_ {file mode} {
	upvar root root

# check existence and file size before retrieval:
	set command "ls -l '$file' | ( read a b c d x e\; echo \$x )"
	if {([catch {set fileSize [Transport $root $command]}]) && ($mode == "r")} {error "couldn't open \"$file\": no such file or directory" "no such file or directory" {POSIX ENOENT {no such file or directory}}}

	set channelID [memchan]

# file must exist after open procedure, ensure it:
	set command "touch -a '$file'"
	Transport $root $command

# if write mode, don't need to retrieve contents:
	if [string match w* $mode] {return $channelID}

# cat file contents to stdout and transfer to channelID:
	fconfigure $channelID -translation binary
	set command "cat '$file'"
	Transport $root $command stdout $channelID

# check if entire file contents transported:
	seek $channelID 0 end
	set channelSize [tell $channelID]
	if {[info exists fileSize] && ($channelSize != $fileSize)} {error "Input/output error" "Input/output error" {POSIX EIO {Input/output error}}}
	return $channelID
}

# all file access procs are redirected here for ease of programming:
proc file_access {file type} {
	upvar 2 root root relative relative

	set command "if \[ -r '$file' \]\; then echo 1\; else echo 0\; fi \; if \[ -w '$file' \]\; then echo 1\; else echo 0\; fi \; if \[ -x '$file' \]\; then echo 1\; else echo 0\; fi \; if \[ -e '$file' \]\; then echo 1\; else echo 0\; fi"
	set returnValue [Transport $root $command]
	set access [split $returnValue \n]
	set secs [clock seconds]

	CacheSet ::vfs::template::fish::readable [file join $root $relative] [lindex $access 0] $secs
	CacheSet ::vfs::template::fish::writable [file join $root $relative] [lindex $access 1] $secs
	CacheSet ::vfs::template::fish::executable [file join $root $relative] [lindex $access 2] $secs
	CacheSet ::vfs::template::fish::exists [file join $root $relative] [lindex $access 3] $secs

	eval return \$::vfs::template::fish::${type}(\[file join \$root \$relative\],value)
}

proc MountProcedure {args} {
	upvar volume volume

	set to [lindex $args end]
	set path [lindex $args end-1]
	if [string equal $volume {}] {set to [file normalize $to]}

# if virtual mount contains mount info, retrieve it:
	array set params [FileTransport $to]

# retrieve all option/value pairs from args list:
	if {[llength $args] > 2} {
		set args [lrange $args 0 end-2]
		set argsIndex [llength $args]
		for {set i 0} {$i < $argsIndex} {incr i} {
			set arg [lindex $args $i]
			if {[string index $arg 0] == "-"} {
				set arg [string range $arg 1 end]
				set params($arg) [lindex $args [incr i]]
			}
		}
	}

# local option if no other transport given, useful for testing:
	if [string equal $params(transport) {}] {set params(transport) local}

# default executable name is transport name:
	if ![info exists params(exec)] {set params(exec) $params(transport)}

# store parameters:
	set ::vfs::template::fish::params($to) [array get params]
	set ::vfs::template::fish::transport($to) $params(transport)

# rewrite template vfshandler so appropriate transport proc is imported with each file operation:
	set body "set trans \$::vfs::template::fish::transport(\$root) \; namespace import -force ::vfs::template::fish::\$\{trans\}::Transport \n"	
	append body [info body handler]
	proc handler [info args handler] $body
	
	lappend pathto $path
	lappend pathto $to
	return $pathto
}

proc UnmountProcedure {path to} {
	unset ::vfs::template::fish::params($to)
	unset ::vfs::template::fish::transport($to)
	return
}

# execute commands, handle stdin/stdout if necessary:
proc ExecCommand {root command args} {
	array set params [lindex $args 0]
	if [info exists params(stdin)] {
		set execID [eval ::open \"|$command\" w]
		fconfigure $execID -translation binary
		seek $params(stdin) 0
		puts -nonewline $execID [read $params(stdin)]
		::close $execID
		return
	}

	if [info exists params(stdout)] {
		set execID [eval ::open \"|$command\" r]
		fconfigure $execID -translation binary
		seek $params(stdout) 0
		puts -nonewline $params(stdout) [read $execID]
		::close $execID
		return
	}
	eval exec $command
}
# analyze virtual URL for mount information:
proc FileTransport {filename} {
	if {[string first : $filename] < 0} {return [list transport {} user {} password {} host {} port {} filename [file normalize $filename]]}
	if {[string first [string range $filename 0 [string first : $filename]] [file volume]] > -1} {return [list transport {} user {} password {} host {} port {} filename [file normalize $filename]]}

	set filename $filename/f
	set transport {} ; set user {} ; set password {} ; set host {} ; set port {}
	
	regexp {(^[^:]+)://} $filename trash transport
	regsub {(^[^:]+://)} $filename "" userpasshost
	set userpass [lindex [split $userpasshost @] 0]
	set user $userpass
	regexp {(^[^:]+):(.+)$} $userpass trash user password

	if {[string first @ $userpasshost] == -1} {set user {} ; set password {}}

	regsub {([^/]+)(:[^/]+)(@[^/]+)} $filename \\1\\3 filename

	if [regexp {(^[^:]+)://([^/:]+)(:[^/:]*)*(.+$)} $filename trash transport host port filename] {
		regexp {([0-9]+)} $port trash port
		if {[string first [lindex [file split $filename] 1] [file volume]] > -1} {set filename [string range $filename 1 end]}
	} else {
		set host [lindex [split $filename /] 0]
		set filename [string range $filename [string length $host] end]
		set port [lindex [split $host :] 1]
		set host [lindex [split $host :] 0]
	}
	regexp {^.+@(.+)} $host trash host
	set filename [string range $filename 0 end-2]
	return [list transport $transport user $user password $password host $host port $port filename $filename ]
}


}
# end namespace ::vfs::template::fish


# Each transport procedure has its own namespace and Transport proc.
# Copy and customize for new transport methods:

namespace eval ::vfs::template::fish::local {
	proc Transport {root command {std none} {chan none}} {
		array set params "$std $chan"
		return [::vfs::template::fish::ExecCommand $root $command [array get params]]
	}
	namespace export *
}

namespace eval ::vfs::template::fish::plink {
	proc Transport {root command {std none} {chan none}} {
		array set params $::vfs::template::fish::params($root)
		array set params "$std $chan"

		set port {}
		if ![string equal $params(port) {}] {set port "-P $params(port)"}
		set commandLine "[list $params(exec)] -ssh $port -l $params(user) -batch -pw $params(password) $params(host) [list $command]"

		return [::vfs::template::fish::ExecCommand $root $commandLine [array get params]]
	}
	namespace export *
}

namespace eval ::vfs::template::fish::rsh {
	proc Transport {root command {std none} {chan none}} {

		array set params $::vfs::template::fish::params($root)
		array set params "$std $chan"

		set user {}
		if ![string equal $params(user) {}] {set user "-l $params(user)"}
		set commandLine "[list $params(exec)] $user $params(host) [list ${command}]"
		return [::vfs::template::fish::ExecCommand $root $commandLine [array get params]]
	}
	namespace export *
}

namespace eval ::vfs::template::fish::ssh {
	proc Transport {root command {std none} {chan none}} {

		array set params $::vfs::template::fish::params($root)
		array set params "$std $chan"

		set port {}
		if ![string equal $params(port) {}] {set port "-D $params(port)"}
		set user {}
		if ![string equal $params(user) {}] {set user "-l $params(user)"}
		set commandLine "[list $params(exec)] $port $user $params(host) [list ${command}]"
		return [::vfs::template::fish::ExecCommand $root $commandLine [array get params]]
	}
	namespace export *
}

