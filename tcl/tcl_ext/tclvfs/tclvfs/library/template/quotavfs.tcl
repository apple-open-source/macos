if 0 {
########################

quotavfs.tcl --

Written by Stephen Huntley (stephen.huntley@alum.mit.edu)
License: Tcl license
Version 1.5.2

A quota-enforcing virtual filesystem.  Requires the template vfs in templatevfs.tcl.

Quotas can be set on any quantity returned by "file stat" or "file attributes",
plus the attribute "filename", which is the fully normalized pathname of the file.

Two types of quota can be set: an incremented count of files matching a certain criterion, and
a running total of a certain quantity.  Each quota is defined by a set of switches composing 
a "quota group," any number of quota groups can be defined.  A file must fit within all quotas defined
to avoid triggering quota enforcement.

The quotas are enforced as a FIFO stack of files; that is, if a new file is copied to the vfs whose
attributes exceed a quota, the file is not rejected, rather, the already present files with 
the oldest access times that contribute to the quota are deleted until there is room within 
the quota limit for the addition of the new file.

The exception for the running total variety is if the file's attribute is large enough to 
exceed the quota by itself, it is barred without first deleting all other files contributing to 
the quota.

At mount time, all files in the existing directory are examined and quotas calculated.  Files may be
deleted to keep quotas under their defined limits.  After mount, when a new file is moved into the 
virtual directory or an existing file edited, its properties are examined with respect to the defined 
quotas; if no room can be made for it, the move or edit is rejected.

Usage: mount <quota group> ?<quota group>... ? <existing directory> <virtual directory>

Quota group definition:

-<quantity> <rule> -[quota|ruletotal] <quota number>
or
-<quantity> -total <quota number>

Options:

-<quantity>
Where <quantity> is any item returned by the "file stat" or "file attributes" commands, with the dash
prepended as needed, for example: -archive, -permissions, -size, -mtime etc.  The attribute "filename"
is assumed to exist as well, defined as the file's full pathname.  The quantity need not exist, so the 
same command line could be used on Unix or Windows, for example.  Nonexistent quantities have no effect
and are ignored.

<rule>
The rule is the criterion a file must meet to have the quota applied to it.  It may take the form of a 
list of glob patterns as used by the "string match" command: if the quantity value matches all the 
patterns, the quota is applied.  The rule may be Tcl code, to which the quantity value will be 
appended and then evaluated.  The code should return 1 if the file is judged to meet the 
quota criterion, or 0 if not.  If glob patterns are used, each pattern in the list may, in 
addition to symbols used by "string match", have a "!" prepended to it, which will negate the 
sense of the match.

-quota
If the quota group contains this switch, then the vfs will keep a running count of all files that satisfy 
the quota group's rule.  It will not allow more than the number of files specified in <quota number> to 
exist in the virtual file space.

-total
If the quota group contains this switch, then the vfs will track the sum of the values of the specified
quantity of all files.  It will not allow the sum specified in <quota number> to 
be exceeded in the virtual file space.

-ruletotal
Like -total, but a rule is defined, and only files satisfying the rule have their values added to the quota sum.

The quota vfs inherits the -cache and -volume options of the template vfs.


Examples -- to set a 10 MB size limit on your ftp upload directory:
mount -size -total 10000000 C:/temp/upload C:/vfs/ftp/pub

To allow only PNG or JPEG files in a photo collection:
mount -filename {!*.png !*.jpg !*.jpeg} -quota 0 /home/shuntley/photos /vfs/photo

To ban GIF files from your web site images subdirectory:
mount -filename {C:/Program Files/Apache/htdocs/images/*.gif} -quota 0 {C:/Program Files/Apache/htdocs} /docroot

To disallow creation of subdirectories:
mount -type directory -quota 0 /ftp/upload /intake

Use a rule to allow only 1 MB of files greater than 10kB in size:
mount -size {expr 10000 <} -ruletotal 1000000 /tmp /vfs/dump

Use two quota groups to allow only log files and keep only 1 more than one week:
mount -filename !*.log -quota 0 -mtime {expr [clock scan {7 days ago}] >} -quota 1 /var/log /vfs/history

########################
}

package require vfs::template 1.5
package require fileutil::globfind

package provide vfs::template::quota 1.5.2

namespace eval ::vfs::template::quota {

# read template procedures into current namespace. Do not edit:
foreach templateProc [namespace eval ::vfs::template {info procs}] {
	set infoArgs [info args ::vfs::template::$templateProc]
	set infoBody [info body ::vfs::template::$templateProc]
	proc $templateProc $infoArgs $infoBody
}

# edit following procedures:
proc close_ {channel} {
	upvar path path root root relative relative
	fconfigure $channel -translation binary
	seek $channel 0 end
	set quotaSize [tell $channel]
	seek $channel 0
	set filechannel [lindex $::vfs::template::quota::channels($channel) 0]
	set newFile [lindex $::vfs::template::quota::channels($channel) 1]
	unset ::vfs::template::quota::channels($channel)
	set file [file join $path $relative]

# Check if edited size violates any size quotas before allowing commit:
	if [catch {QuotaAdd $file}] {
		close $filechannel
		if $newFile {catch {file delete -force $file}}
		error "Disk quota exceeded"
	}
	seek $filechannel 0
	fcopy $channel $filechannel
	close $filechannel
	return
}
proc file_atime {file time} {
	upvar root root
	file atime $file $time
	append ::vfs::template::quota::atimes($root) " $time [list $file]"
	if {$::vfs::template::quota::files($file) < $time} {set ::vfs::template::quota::files($file) $time ; return}
	set ::vfs::template::quota::files($file) $time
	set aList {}
	foreach {atime afile} $::vfs::template::quota::atimes($root) {
		lappend aList "$atime [list $afile]"
	}
	set atimes {}
	foreach aset [lsort -dictionary $aList] {
		set atime [lindex $aset 0]
		set afile [lindex $aset 1]
		append atimes " $atime [list $afile]"
	}
	set ::vfs::template::quota::atimes($root) $atimes
}
proc file_mtime {file time} {file mtime $file $time}
proc file_attributes {file {attribute {}} args} {eval file attributes \$file $attribute $args}
proc file_delete {file} {
	upvar root root
	array set quotaArray $::vfs::template::quota::quota($root)
	QuotaDelete $file
	set ::vfs::template::quota::quota($root) [array get quotaArray]
	return
}
proc file_executable {file} {file executable $file}
proc file_exists {file} {file exists $file}
proc file_mkdir {file} {
	upvar root root
	file mkdir $file
	globfind $file QuotaAdd
	return
}
proc file_readable {file} {file readable $file}
proc file_stat {file array} {upvar $array fs ; ::file stat $file fs}
proc file_writable {file} {file writable $file}
proc glob_ {directory dir nocomplain tails types typeString dashes pattern} {glob -directory $dir -nocomplain -tails -types $typeString -- $pattern}
proc open_ {file mode} {
	upvar root root permissions permissions
	upvar newFile newFile
	if {$mode == "r"} {
		set atime [clock seconds]
		append ::vfs::template::quota::atimes($root) " $atime [list $file]"
		set ::vfs::template::quota::files($file) $atime
		return [open $file r]
	}

if $newFile {
	set now [clock seconds]
	set fstat "mtime $now atime $now mode $permissions type file ctime $now size 0"
	QuotaAdd $file
}
	set channel [open $file $mode]

# Check if new file violates any quotas by adding it to quota tallies:
#	if $newFile {
#		set err [catch {QuotaAdd $file} result]
#		if $err {
#			close $channel
#			file delete -force -- $file
#			vfs::filesystem posixerror $::vfs::posix(EDQUOT)
#			error "Disk quota exceeded"
#		}
#	}
# remove file from quota tallies until channel is closed:
	array set quotaArray $::vfs::template::quota::quota($root)
	QuotaDelete $file 0
	set ::vfs::template::quota::quota($root) [array get quotaArray]

# Use memchan to store edits so edit can be rejected if it violates size quotas:
	set memchannel [memchan]
	fconfigure $channel -translation binary
	fconfigure $memchannel -translation binary
	seek $channel 0
	fcopy $channel $memchannel
	set [namespace current]::channels($memchannel) "$channel $newFile"
	return $memchannel
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
	set quotaArgs [lrange $args 0 end-2]

	ParseArgs ::vfs::template::quota::quota($to) $quotaArgs

# Initialize quotas:
	set root $to
	set aList {}
	foreach afile [globfind $path] {
		file stat $afile fs
		lappend aList "$fs(atime) [list $afile]"
	}
	set atimes {}
	foreach aset [lsort -dictionary $aList] {
		set atime [lindex $aset 0]
		set afile [lindex $aset 1]
		append atimes " $atime [list $afile]"
		set ::vfs::template::quota::files($afile) $atime
	}
	set ::vfs::template::quota::atimes($root) $atimes

	globfind $path QuotaAdd

	set ::vfs::template::quota::atimes($root) $atimes

# return two-item list consisting of real and virtual locations.
	lappend pathto $path
	lappend pathto $to
	return $pathto
}


proc UnmountProcedure {path to} {
# add custom unmount handling of new vfs elements here.

	unset -nocomplain ::vfs::template::quota::quota($to)
	unset -nocomplain ::vfs::template::quota::atimes($to)
	return
}

# Default rule for quotas with pattern specified:
proc CheckPattern {pattern value} {
	foreach ptn $pattern {
		set negate [string equal [string index $ptn 0] !]
		if $negate {set ptn [string range $ptn 1 end]}
		set match [string match $ptn $value]
		if $negate {set match [expr !$match]}
		if !$match {return 0}
	}
	return 1
}

# Used as argument to proc globfind to recurse down dir hierarchies and process each file and dir found:
proc QuotaAdd {fileName} {
	set caller [lindex [info level -1] 0]
	if {$caller == "MountProcedure"} {set init 1} else {set init 0}
	upvar path path root root quotaSize quotaSize fstat fstat
	if ![string first ".vfs_" [file tail $fileName]] {return 0}
	if {[info exists path] && ($fileName == $path)} {return 0}
	array set quotaArray $::vfs::template::quota::quota($root)
	set overLimit {}
	set items [lsort -unique [string map {",type " " " ",rule " " " ",quota " " " ",current " " "} " [array names quotaArray] "]]

	set delete 1
if [info exists fstat] {
	array set fs $fstat	
} else {
	set noexist [catch {file stat $fileName fs}]
	if $noexist {return 0}
}
	set fs(filename) $fileName

# if this call is being used to check edits, replace file size with channel size and don't delete file if edit too big:
	if [info exists quotaSize] {set fs(size) $quotaSize ; set delete 0 ; unset quotaSize}

# Update queue which tracks which files to try deleting first to make room for new files:
	append ::vfs::template::quota::atimes($root) " $fs(atime) [list $fileName]"
	set ::vfs::template::quota::files($fileName) $fs(atime)

# Check each defined quota to see if given file violates it:
	foreach item $items {
		regexp {([0-9]*),(.*)} $item trash groupCount item
		if ![info exists fs($item)] {if [file exists $fileName] {array set fs [file attributes $fileName]}}
		if ![info exists fs($item)] {continue}
		set contrib [eval $quotaArray($groupCount,$item,rule) [list $fs($item)]]
		if $contrib	{	
			if {$quotaArray($groupCount,$item,type) == "total"} {

				# If file quantity by itself would violate quota, reject immediately:
				if {$fs($item) > $quotaArray($groupCount,$item,quota)} {
					if $delete {catch {file delete -force -- $fileName} result}
if [info exists ::vfs::template::quota::debug] {
puts "\n$fileName violates quota by itself:
$item: $fs($item)
quota: $quotaArray($groupCount,$item,quota)"
if $delete {puts "$fileName deleted: $result"}
}
					if $init {return 0} else {vfs::filesystem posixerror $::vfs::posix(EDQUOT)}
				}
				set quotaArray($groupCount,$item,current) [expr $quotaArray($groupCount,$item,current) + $fs($item)]
			} else {
				if {$quotaArray($groupCount,$item,quota) == 0} {
					if $delete {catch {file delete -force -- $fileName} result}
if [info exists ::vfs::template::quota::debug] {
puts "\n$fileName violates quota by itself:
$item: $fs($item)
quota: $quotaArray($groupCount,$item,quota)"
if $delete {puts "$fileName deleted: $result"}
}
					if $init {return 0} else {vfs::filesystem posixerror $::vfs::posix(EDQUOT)}
				}
				incr quotaArray($groupCount,$item,current)
			}
			# If file violates quota, store quota to see if room can be made by deleting older files:
			if {$quotaArray($groupCount,$item,current) > $quotaArray($groupCount,$item,quota)} {lappend overLimit "$groupCount,$item"}
		}
	}
# if given file puts some quotas over limit, see if room can be made by deleting older files:

	foreach item $overLimit {
		set itm [lindex [split $item ,] 1]
		if {$quotaArray($item,current) <= $quotaArray($item,quota)} {continue}

		# examine queue of stored atimes to find older files:
		foreach {atime afile} $::vfs::template::quota::atimes($root) {

			# If stored atime doesn't match latest value, delete record and move on:
			if {($::vfs::template::quota::files($afile) != $atime) || ![file exists $afile]} {
				set deleteLoc [lsearch -exact $::vfs::template::quota::atimes($root) $afile]
				set ::vfs::template::quota::atimes($root) [lreplace $::vfs::template::quota::atimes($root) [expr $deleteLoc - 1] $deleteLoc]
				if {[lsearch -exact $::vfs::template::quota::atimes($root) $afile] < 0} {unset ::vfs::template::quota::files($afile)}
				continue
			}
			
			# if file from queue is in fact newer than given file, skip it:
			if {$atime > $fs(atime)} {continue}

			# if stored filename is same as given filename, given filename violates quota and must be rejected:
			if {$afile == $fileName} {
				if !$delete {set quotaSize $fs(size)}
				catch {QuotaDelete $fileName $delete}
				set ::vfs::template::quota::quota($root) [array get quotaArray]
				if $init {return 0} else {vfs::filesystem posixerror $::vfs::posix(EDQUOT)}
			}

			# If stored file contributes to quota, delete it and remove from quota tally:

			if {$itm == "filename"} {
				set itm_val $afile
			} elseif {[string index $itm 0] == "-"} {
				set itm_val [file attributes $afile $itm]
			} else {
				file stat $afile iv
				set itm_val $iv($itm)
			}
	
			set contrib [eval $quotaArray($item,rule) [list $itm_val]]
			if $contrib	{	
				if {$quotaArray($item,type) == "total"} {
					set itm [lindex [split $item ,] 1]
					if {[string index $itm 0] == "-"} {
						set itm_val [file attributes $afile $itm]
					} else {
						file stat $afile iv
						set itm_val $iv($itm)
					}
					if !$itm_val {continue}
				}
				set ::vfs::template::quota::quota($root) [array get quotaArray]
				QuotaDelete $afile
			}

			# If deletions make room for new file, then OK:
			if {$quotaArray($item,current) <= $quotaArray($item,quota)} {break}
		}
	}
	set ::vfs::template::quota::quota($root) [array get quotaArray]
	return 0
}

proc QuotaDelete {fileName {delete 1}} {
	upvar quotaArray quotaArray quotaSize quotaSize
	set items [lsort -unique [string map {",type " " " ",rule " " " ",quota " " " ",current " " "} " [array names quotaArray] "]]

# If given fileName is a dir, must remove all contents from quota tallies before removing dir itself:
	set files [lsort -decreasing [globfind $fileName]]
	set type file

# Must parse contents twice, eliminate files first, then dirs:
	foreach file [concat $files //// $files] {
		if {$file == "////"} {set type directory ; continue}
	
		# cache quantity info to save time on second pass:
		if ![info exists stat($file)] {
			file stat $file fs
			set fs(filename) $fileName
			if [info exists quotaSize] {set fs(size) $quotaSize}
			set stat($file) [array get fs]
		}
		array set fs $stat($file)

		# If file type is wrong for this pass, continue:
		if {($type == "file") && ($fs(type) == "directory")} {continue}
		if {($type == "directory") && ($fs(type) != "directory")} {continue}

		# Check each quota to see if current file contributes to it:
		foreach item $items {
		 	regexp {([0-9]*),(.*)} $item trash groupCount item
			if ![info exists fs($item)] {if [file exists $file] {array set fs [file attributes $file]} ; set stat($file) [array get fs]}
			if ![info exists fs($item)] {continue}
			set contrib [eval $quotaArray($groupCount,$item,rule) [list $fs($item)]]
			if $contrib	{
				if {$quotaArray($groupCount,$item,type) == "total"} {
					set quotaArray($groupCount,$item,current) [expr $quotaArray($groupCount,$item,current) - $fs($item)]
				} else {
					incr quotaArray($groupCount,$item,current) -1
				}
if [info exists ::vfs::template::quota::debug] {
puts "\n$file contributed to quota:
rule: $quotaArray($groupCount,$item,rule)
quota: $quotaArray($groupCount,$item,quota)
current: $quotaArray($groupCount,$item,current)"
}
			}
		}

		# After removing file from quota tallies, delete it:
		if $delete {file delete -force -- $file}
if {$delete && [info exists ::vfs::template::quota::debug]} {
puts "\n$file deleted"
}
	}
	return
}

# Decided on new command line syntax, rather than rewrite whole vfs,
# this proc casts new syntax into old format, then processes as before:
proc ParseArgs {argsStore args} {
	upvar path path
	set args [lindex $args 0]

	array set attrs [file attributes $path]
	set quotas {}
	set totals {}
	set rtotals {}
	set newArgs {}

# find location of each quota group:
	set qPosition [lsearch -all $args "-quota"]
	set tPosition [lsearch -all $args "-total"]
	set rPosition [lsearch -all $args "-ruletotal"]

# break group defs into separate categories:
	foreach qp $qPosition {
		incr qp
		append quotas " [lrange $args [expr $qp - 3] $qp]" 
	}

	foreach tp $tPosition {
		incr tp
		append totals " [lrange $args [expr $tp - 2] $tp]" 
	}

	foreach rp $rPosition {
		incr rp
		append rtotals " [lrange $args [expr $rp - 3] $rp]" 
	}

# cast each category into old syntax:
	foreach {type pr quota number} $quotas {
		set patrul "-pattern"
		if {[lsearch -exact [info commands [string trim [string range $pr 0 [string first { } $pr]]]] [string trim [string range $pr 0 [string first { } $pr]]]] > -1} {
			set patrul "-rule"
		}
		if ![info exists attrs($type)] {set type [string range $type 1 end]}
		append newArgs " -number: -item $type $patrul [list $pr] -quota $number"
	}

	foreach {type total number} $totals {
		if ![info exists attrs($type)] {set type [string range $type 1 end]}
		append newArgs " -total: -item $type -quota $number"
	}

	foreach {type pr rtotal number} $rtotals {
		set patrul "-pattern"
		if {[lsearch -exact [info commands [string trim [string range $pr 0 [string first { } $pr]]]] [string trim [string range $pr 0 [string first { } $pr]]]] > -1} {
			set patrul "-rule"
		}
		if ![info exists attrs($type)] {set type [string range $type 1 end]}
		append newArgs " -total: -item $type $patrul [list $pr] -quota $number"
	}

# process old syntax:
	unset args
	lappend args [string trim $newArgs]

	set groupCount 0
	set args [lindex $args 0]
	set argsIndex [llength $args]
	for {set i $argsIndex} {$i >= 0} {incr i -1} {
		switch -- [lindex $args $i] {
			-number: -
			-total: {
				set item $itemSet(item)
				if ![info exists itemSet(rule)] {set itemSet(rule) "CheckPattern *"}
				set argsArray($groupCount,$item,type) [string range [lindex $args $i] 1 end-1]
				set argsArray($groupCount,$item,rule) $itemSet(rule)
				set argsArray($groupCount,$item,quota) $itemSet(quota)
				set argsArray($groupCount,$item,current) 0
				array unset itemSet
				incr groupCount
			}
			-item {
				set itemSet(item) [lindex $args [expr $i + 1]]
			}
			-pattern {
				set itemSet(rule) "CheckPattern [list [lindex $args [expr $i + 1]]]"
			}
			-quota {
				set itemSet(quota) [lindex $args [expr $i + 1]]
			}
			-rule {
				set itemSet(rule) [lindex $args [expr $i + 1]]
			}
		}
	}
	set $argsStore [array get argsArray]
}

}
# end namespace ::vfs::template::quota

