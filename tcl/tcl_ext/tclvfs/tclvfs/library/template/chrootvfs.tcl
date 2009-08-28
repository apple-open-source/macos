#/usr/bin/env tclsh

if 0 {
########################

chrootvfs.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5

A chroot virtual filesystem.

This virual filesystem has an effect similar to a "chroot" command; it makes the named existing directory appear
to be the top of the filesystem and makes the rest of the real filesystem invisible.

This vfs does not block access by the "exec" command to the real filesystem outside the chroot directory,
or that of the "open" command when its command pipeline syntax is used.

At the end of this file is example code showing one way to set up a safe slave interpreter suitable for
running a process safely with limited filesystem access: its file access commands are re-enabled, the exec
command remains disabled, the open command is aliased so that it can only open files and can't spawn new 
processes, and mounted volumes besides the volume on which the chroot directory resides are aliased so 
that they act as mirrors of the chroot directory.

Such an interpreter should be advantageous for applications such as a web server: which requires some 
filesystem access but presents security threats that make access limitations desirable.

 Install: This code requires the vfs::template package included in the Tclvfs distribution.

 Usage: mount ?-volume? <existing "chroot" directory>  <virtual directory>

 examples:

	mount $::env(HOME) /

	mount {C:\My Music} C:/

	mount -volume /var/www/htdocs chroot://

########################
}

namespace eval ::vfs::template::chroot {

package require vfs::template 1.5
package provide vfs::template::chroot 1.5.2

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

proc file_attributes {file {attribute {}} args} {eval file attributes \$file $attribute $args}

catch {rename redirect_handler {}}
catch {rename handler redirect_handler}

proc handler args {
	set path [lindex $args 0]
	set to [lindex $args 2]
	set volume [lindex $::vfs::template::mount($to) 1]
	if {$volume != "-volume"} {set volume {}}
	set startDir [pwd]

	::vfs::filesystem unmount $to

	set err [catch {set rv [uplevel ::vfs::template::chroot::redirect_handler $args]} result] ; set errorCode $::errorCode

	eval ::vfs::filesystem mount $volume [list $to] \[list [namespace current]::handler \[file normalize \$path\]\]
	if {[pwd] != $startDir} {catch {cd $startDir}}
	if {$err && ([lindex $errorCode 0] == "POSIX")} {vfs::filesystem posixerror $::vfs::posix([lindex $errorCode 1])}
	if $err {return -code $err $result}
	return $rv
}


# Example code to set up a safe interpreter with limited filesystem access:
proc chroot_slave {} {
	file mkdir /tmp
	package require vfs::template
	::vfs::template::chroot::mount -volume /tmp C:/
	set vols [lsort -unique [file volumes]]
	foreach vol $vols {
		if {$vol == "C:/"} {continue}
		::vfs::template::mount C:/ $vol
	}
	set slave [interp create -safe]
	$slave expose cd  
	$slave expose encoding
	$slave expose fconfigure
	$slave expose file
	$slave expose glob
	$slave expose load
	$slave expose pwd
	$slave expose socket
	$slave expose source

	$slave alias exit exit_safe $slave
	$slave alias open open_safe $slave

	interp share {} stdin $slave
	interp share {} stdout $slave
	interp share {} stderr $slave
}

proc exit_safe {slave} {
	interp delete $slave
}

proc open_safe {args} {
	set slave [lindex $args 0]
	set handle [lindex $args 1]
	set args [lrange $args 1 end]
	if {[string index $handle 0] != "|"} {
		eval [eval list interp invokehidden $slave open $args]
	} else {
		error "permission denied"
	}
}


}
# end namespace ::vfs::template::chroot

