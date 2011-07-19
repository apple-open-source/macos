# Synchronization of starkits across a communications channel
# Dec 2001, Jean-Claude Wippler <jcw@equi4.com>

package provide starsync 1.0

namespace eval starsync {
  namespace export request summary reply

# private state
  namespace eval v {
    variable seqn	0	;# used to generate db names
    variable vers	1	;# protocol format
  }

# update local starkit, using differences obtained from server
  proc request {url path {fake 0}} {
    set id [string tolower [file root [file tail $path]]]
    # if file is new and we're doing a real sync, fetch full copy
    if {![file exists $path] && !$fake} {
      set data [rpc $url [vfs::zip -mode c [kit2cat $path <F/$v::vers> $id]]]
      if {$data eq ""} return
      # create file from scratch
      set fd [open $path w]
      fconfigure $fd -translation binary
      puts -nonewline $fd $data
      close $fd
      # return full catalog made from local copy
      return [kit2cat $path]
    }
    set data \
      [rpc $url [vfs::zip -mode c [kit2cat $path <D/$v::vers> $id $fake]]]
    if {$data eq "" || $fake} { return $data }
    # apply the differences to local starkit
    vfs::mk4::Mount $path $path -nocommit
    set r [applydiffs $data $path]
    vfs::unmount $path
    return $r
  }

# apply differences to starkit, return stripped catalog
  proc applydiffs {data path} {
    set db [memvfs <diff> $data]
    for {lappend dirs {}} {[llength $dirs]} {set dirs [lrange $dirs 1 end]} {
      set curr [lindex $dirs 0]
      foreach x [glob -nocomplain -tails -directory <diff> -join $curr *] {
	set t [file join <diff> $x]
	set h [file join $path $x]
	if {[file isdir $t]} {
	  file mkdir $h
	  lappend dirs $x
	} else {
	  file delete -force $h
	  set m [file mtime $t]
	  if {$m} {
	    file copy $t $h
	    file mtime $h $m
	  }
	}
      }
    }
    stripdata $db
    set r [mk2str $db]
    vfs::unmount <diff>
    return $r
  }

# generate a list of file and size entries from a catalog string
  proc summary {data} {
    set xfers {}
    if {$data ne ""} {
      memvfs <diff> $data
      for {lappend dirs {}} {[llength $dirs]} {set dirs [lrange $dirs 1 end]} {
	set curr [lindex $dirs 0]
	foreach x [glob -nocomplain -tails -directory <diff> -join $curr *] {
	  set t [file join <diff> $x]
	  if {[file isdir $t]} {
	    lappend dirs $x
	  } elseif {[file mtime $t]} {
	    lappend xfers $x [file size $t]
	  } else {
	    lappend xfers $x -
	  }
	}
      }
      vfs::unmount <diff>
    }
    return $xfers
  }

# remote procedure call, wraps a request/response as HTTP
  proc rpc {url data} {
    #puts "sent [string length $data] bytes"
    package require http
    set t [http::geturl $url -query $data -binary 1 \
			-type "application/octet-stream"]
    if {[http::status $t] ne "ok" || [http::ncode $t] != 200} {
      set r "unexpected reply: [http::code $t]"
      http::cleanup $t
      error $r
    }
    set r [http::data $t]
    http::cleanup $t
    #puts "got: [string length $r] bytes"
    return $r
  }

# the main starsync trick: strip contents of all files
  proc stripdata {db} {
    mk::view layout $db.dirs {name parent:I {files {name size:I date:I}}}
  }

# take starkit as input, produce MK catalog as result (args are passed along)
  proc kit2cat {path args} {
    set db db[incr v::seqn]
    if {[file exists $path]} {
      mk::file open $db $path -readonly
    } else {
      mk::file open $db
    }
    stripdata $db
    mk::view layout $db.sync s
    foreach x $args { mk::row append $db.sync s $x }
    set r [mk2str $db]
    mk::file close $db
    return $r
  }

# convert an open MK datafile to a serialized string representation
  proc mk2str {db} {
    set fd [vfs::memchan]
    mk::file save $db $fd
    seek $fd 0
    set r [read $fd]
    close $fd
    return $r
  }

# open an in-memory MK VFS, contents given as string, returns db name
  proc memvfs {path data} {
    # set up in-mem channel with result
    set fd [vfs::memchan]
    fconfigure $fd -translation binary
    puts -nonewline $fd $data
    flush $fd
    seek $fd 0
    # open result as MK datafile
    set db db[incr v::seqn]
    mk::file open $db
    mk::file load $db $fd
    close $fd
    # mount from a MK datafile, bypass usual logic
    vfs::filesystem mount $path [list vfs::mk4::handler $db]
    vfs::RegisterMount $path [list ::vfs::mk4::Unmount $db]
    return $db
  }

# extract auxiliary info added by kit2cat
  proc catinfo {db} {
    set r {}
    mk::loop c $db.sync { lappend r [mk::get $c s] }
    return $r
  }

# difference logic, leaves $here with only diffs from $there
  proc calcdiff {here there} {
    set hasmods 0
    set delpend {}
    foreach x [glob -nocomplain -tails -directory $there *] {
      set remote($x) ""
    }
    foreach x [glob -nocomplain -tails -directory $here *] {
      if {[info exists remote($x)]} {
	set h [file join $here $x]
	set t [file join $there $x]
	if {[file isfile $h] && [file isfile $t] &&
		  [file size $h] == [file size $t] &&
		  [file mtime $h] == [file mtime $t]} {
	  lappend delpend $h
        } elseif {[file isdir $h] && [file isdir $t]} {
	  if {[calcdiff $h $t]} {
	    incr hasmods
	  } else {
	    lappend delpend $h
	  }
	} else {
	  incr hasmods
	}
	array unset remote $x
      } else {
        incr hasmods
      }
    }
    foreach x [array names remote] {
      set h [file join $here $x]
      close [open $h w]
      file mtime $h 0 ;# this flags entry as being a deletion
      incr hasmods
    }
    if {$hasmods} {
      foreach x $delpend { file delete -force $x }
    }
    return $hasmods
  }

# take incoming catalog and return difference starkit
  proc <D/1> {tid path fake} {
    set r ""
    if {[file exists $path]} {
      #2003-02-01 expand symlinks because mk4vfs has trouble with it
      catch { set path [file readlink $path] }
      set db [vfs::mk4::Mount $path $path -readonly]
      catch { vfs::attributes $path -state translucent }
      if {[calcdiff $path $tid]} {
	if {$fake} { stripdata $db }
	set r [mk2str $db]
      }
      vfs::unmount $path
    }
    return $r
  }

# return full starkit for initial download (including header)
  proc <F/1> {tid path} {
    set fd [open $path]
    fconfigure $fd -translation binary
    set r [read $fd]
    close $fd
    return $r
  }

# request handler, takes a request and dispatches as needed
  proc reqhandler {data} {
    if {[string length $data] == 0} { return - }
    set info [catinfo [set db [memvfs <temp> [vfs::zip -mode d $data]]]]
    lassign $info cmd id
    if {[string match <?/?> $cmd] && [string is wordchar -strict $id]} {
      set out [eval [lreplace $info 1 1 <temp> $id.kit]]
    } else {
      set out ?
    }
    vfs::unmount <temp>
    list $info $out
  }

# call request handler with input data as arg, send result back to rpc client
  proc reply {} {
    fconfigure stdin -translation binary
    set in [read stdin]
    lassign [reqhandler $in] info out
    puts "Content-type: application/octet-stream"
    puts "Content-length: [string length $out]\n"
    fconfigure stdout -translation binary
    puts -nonewline $out
    flush stdout
    list [string length $in] [string length $out] $info
  }
}
