# Synchronize two directory trees, VFS-aware
#
# Copyright (c) 1999 Matt Newman, Jean-Claude Wippler and Equi4 Software.

#
# Recursively sync two directory structures
#
proc rsync {arr src dest} {
    #tclLog "rsync $src $dest"
    upvar 1 $arr opts

    if {$opts(-auto)} {
	# Auto-mounter
	vfs::auto $src -readonly
	vfs::auto $dest
    }

    if {![file exists $src]} {
	return -code error "source \"$src\" does not exist"
    }
    if {[file isfile $src]} {
	#tclLog "copying file $src to $dest"
	return [rcopy opts $src $dest]
    }
    if {![file isdirectory $dest]} {
	#tclLog "copying non-file $src to $dest"
	return [rcopy opts $src $dest]
    }
    set contents {}
    eval lappend contents [glob -nocomplain -dir $src *]
    eval lappend contents [glob -nocomplain -dir $src .*]

    set count 0		;# How many changes were needed
    foreach file $contents {
	#tclLog "Examining $file"
	set tail [file tail $file]
	if {$tail == "." || $tail == ".."} {
	    continue
	}
	set target [file join $dest $tail]

	set seen($tail) 1

	if {[info exists opts(ignore,$file)] || \
	    [info exists opts(ignore,$tail)]} {
	    if {$opts(-verbose)} {
		tclLog "skipping $file (ignored)"
	    }
	    continue
	}
	if {[file isdirectory $file]} {
	    incr count [rsync opts $file $target]
	    continue
	}
	if {[file exists $target]} {
	    #tclLog "target $target exists"
	    # Verify
	    file stat $file sb
	    file stat $target nsb
	    #tclLog "$file size=$sb(size)/$nsb(size), mtime=$sb(mtime)/$nsb(mtime)"
	    if {$sb(size) == $nsb(size)} {
		# Copying across filesystems can yield a slight variance
		# in mtime's (typ 1 sec)
		if { ($sb(mtime) - $nsb(mtime)) < $opts(-mtime) } {
		    # Good
		    continue
		}
	    }
	    #tclLog "size=$sb(size)/$nsb(size), mtime=$sb(mtime)/$nsb(mtime)"
	}
	incr count [rcopy opts $file $target]
    }
    #
    # Handle stray files
    #
    if {$opts(-prune) == 0} {
	return $count
    }
    set contents {}
    eval lappend contents [glob -nocomplain -dir $dest *]
    eval lappend contents [glob -nocomplain -dir $dest .*]
    foreach file $contents {
	set tail [file tail $file]
	if {$tail == "." || $tail == ".."} {
	    continue
	}
	if {[info exists seen($tail)]} {
	    continue
	}
	rdelete opts $file
	incr count
    }
    return $count
}
proc _rsync {arr args} {
    upvar 1 $arr opts
    #tclLog "_rsync $args ([array get opts])"

    if {$opts(-show)} {
	# Just show me, don't do it.
	tclLog $args
	return
    }
    if {$opts(-verbose)} {
	tclLog $args
    }
    if {[catch {
	eval $args
    } err]} {
	if {$opts(-noerror)} {
	    tclLog "Warning: $err"
	} else {
	    return -code error -errorinfo ${::errorInfo} $err 
	}
    }
}

# This procedure is better than just 'file copy' on Windows,
# MacOS, where the source files probably have native eol's,
# but the destination should have Tcl/unix native '\n' eols.
# We therefore need to handle text vs non-text files differently.
proc file_copy {src dest {textmode 0}} {
    set mtime [file mtime $src]
    if {!$textmode} {
      file copy $src $dest
    } else {
      switch -- [file extension $src] {
	  ".tcl" -
	  ".txt" -
	  ".msg" -
	  ".test" -
	  ".itk" {
	  }
	  default {
	      if {[file tail $src] != "tclIndex"} {
		  # Other files are copied as binary
		  #return [file copy $src $dest]
		  file copy $src $dest
		  file mtime $dest $mtime
		  return
	      }
	  }
      }
      # These are all text files; make sure we get
      # the translation right.  Automatic eol 
      # translation should work fine.
      set fin [open $src r]
      set fout [open $dest w]
      fcopy $fin $fout
      close $fin
      close $fout
    }
    file mtime $dest $mtime
}

proc rcopy {arr path dest} {
    #tclLog "rcopy: $arr $path $dest"
    upvar 1 $arr opts
    # Recursive "file copy"

    set tail [file tail $dest]
    if {[info exists opts(ignore,$path)] || \
	[info exists opts(ignore,$tail)]} {
	if {$opts(-verbose)} {
	    tclLog "skipping $path (ignored)"
	}
	return 0
    }
    global rsync_globs
    foreach expr $rsync_globs {
        if {[string match $expr $path]} {
            if {$opts(-verbose)} {
                tclLog "skipping $path (matched $expr) (ignored)"
            }
            return 0
        }
    }
    if {![file isdirectory $path]} {
	if {[file exists $dest]} {
	    _rsync opts file delete $dest
	}
	_rsync opts file_copy $path $dest $opts(-text)
	return 1
    }
    set count 0
    if {![file exists $dest]} {
	_rsync opts file mkdir $dest
	set count 1
    }
    set contents {}
    eval lappend contents [glob -nocomplain -dir $path *]
    eval lappend contents [glob -nocomplain -dir $path .*]
    #tclLog "copying entire directory $path, containing $contents"
    foreach file $contents {
	set tail [file tail $file]
	if {$tail == "." || $tail == ".."} {
	    continue
	}
	set target [file join $dest $tail]
	incr count [rcopy opts $file $target]
    }
    return $count
}
proc rdelete {arr path} {
    upvar 1 $arr opts 
    # Recursive "file delete"
    if {![file isdirectory $path]} {
	_rsync opts file delete $path
	return
    }
    set contents {}
    eval lappend contents [glob -nocomplain -dir $path *]
    eval lappend contents [glob -nocomplain -dir $path .*]
    foreach file $contents {
	set tail [file tail $file]
	if {$tail == "." || $tail == ".."} {
	    continue
	}
	rdelete opts $file
    }
    _rsync opts file delete $path
}
proc rignore {arr args} {
    upvar 1 $arr opts 

    foreach file $args {
	set opts(ignore,$file) 1
    }
}
proc rpreserve {arr args} {
    upvar 1 $arr opts 

    foreach file $args {
	catch {unset opts(ignore,$file)}
    }
}
proc rignore_globs {args} {
    global rsync_globs
    set rsync_globs $args
}
    
# 28-01-2003: changed -text default to 0, i.e. copy binary mode
array set opts {
    -prune	0
    -verbose	1
    -show	0
    -ignore	""
    -mtime	1
    -compress	1
    -auto	1
    -noerror	1
    -text	0
}
# 2005-08-30 only ignore the CVS subdir
# 2007-03-29 added .svn as well
# 2009-02-02 added .git
#rignore opts CVS RCS core a.out
rignore opts CVS .svn .git
rignore_globs {}

set USAGE "[file tail $argv0] ?options? src dest

    Where options are:-

    -auto	0|1	Auto-mount starkits (default: $opts(-auto))
    -compress	0|1	Enable MetaKit compression (default: $opts(-compress))
    -mtime	n	Acceptable difference in mtimes (default: $opts(-mtime))
    -prune	0|1	Remove extra files in dest (default: $opts(-prune))
    -show	0|1	Show what would be done, but don't do it (default: $opts(-show))
    -verbose	0|1	Show each file being processed (default: $opts(-verbose))
    -noerror    0|1     Continue processing after errors (default: $opts(-noerror))
    -ignore     glob	Pattern of files to ignore (default: CVS RCS core a.out)
    -preserve	glob	Pattern of files not to ignore (i.e. to clear defaults)
    -text       0|1	Copy .txt/tcl/msg/test/itk files as text (default: $opts(-text))"

if {[llength $argv] < 2} {
    puts stderr $USAGE
    exit 1
}

while {[llength $argv] > 0} {
    set arg [lindex $argv 0]

    if {![string match -* $arg]} {
	break
    }
    if {![info exists opts($arg)]} {
	puts stderr "invalid option \"$arg\"\n$USAGE"
	exit 1
    }
    if {$arg eq "-ignore"} {
	rignore opts [lindex $argv 1]
    } elseif {$arg eq "-preserve"} {
	rpreserve opts [lindex $argv 1]
    } else {
	set opts($arg) [lindex $argv 1]
    }
    set argv [lrange $argv 2 end]
}
catch {
package require vfs::mk4
set vfs::mk4::compress $opts(-compress)
}
set src [lindex $argv 0]
set dest [lindex $argv 1]
#
# Load up sync params (tcl script)
#
if {[file exists $src/.rsync]} {
    upvar #0 opts cb
    source $src/.rsync
}
#
# Perform actual sync
#

set n [rsync opts $src $dest]

puts stdout "$n updates applied"
