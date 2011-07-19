package provide app-sdx 2.0

  # adjust, so things make more sense from Tclkit on Windows/Mac

catch {
  #package require Tk
  console show
  if {[llength $argv] == 0} {
    pack [label .l -text "Command line:"] -anchor w
    pack [entry .e -width 50]
    bind .e <Return> { set argv [.e get] }
    vwait argv
    set argc [llength $argv]
    destroy .l .e
  }
  wm withdraw .
  puts "sdx [join $argv { }]\n"
  update
  # catch exit so errors stay visible
  rename exit _exit
  proc exit {{n 0}} {
    if {$n ne "0"} { starkit::panic "SDX cannot continue ($::errorInfo)" }
    _exit $n
  }
}

  # fix bug in two mk4vfs revs (needed when "mkfile" and "local" differ)
  switch [package require vfs::mk4] 1.0 - 1.1 {
    proc vfs::mk4::Mount {mkfile local args} {
      set db [eval [list ::mk4vfs::_mount $local $mkfile] $args]
      ::vfs::filesystem mount $local [list ::vfs::mk4::handler $db]
      ::vfs::RegisterMount $local [list ::vfs::mk4::Unmount $db]
      return $db
    }
    proc mk4vfs::mount {local mkfile args} {
      uplevel [list ::vfs::mk4::Mount $mkfile $local] $args
    }
  }

proc run_sdx {} {
    global argv0 argv argc

    set a [lindex $argv 0]
    set b [file dirname [info script]]

    if {$a eq "" || $a eq "sdx"} {
	set a help
    } elseif {[file exists [file join $b $a.tcl]]} {
	set argv0 $a
	set argv [lrange $argv 1 end]
	incr argc -1
    } else {
	set a help
    }

    uplevel #0 [list source [file join $b $a.tcl]]
}

run_sdx
