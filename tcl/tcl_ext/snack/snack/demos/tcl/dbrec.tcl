#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

# Prompted sentence recording application
 
package require -exact snack 2.2

set rate 16000
snack::sound t -rate $rate
snack::sound s -rate $rate


# dbrec.tcl menus

set m [menu .menu]
$m add cascade -label File -menu $m.file -underline 0
menu $m.file -tearoff 0
$m.file add command -label "New session..." -command [list NewSession]
$m.file add command -label "Open script..." -command [list OpenScriptFile]
$m.file add command -label "Database browser..." -command [list OpenBrowser]
$m.file add command -label "Show speaker info..." -command OpenSpeakerDialog
$m.file add command -label "Exit" -command exit
$m add cascade -label Audio -menu $m.audio -underline 0
menu $m.audio -tearoff 0
$m.audio add command -label "Mixer..." -command snack::mixerDialog
. config -menu $m


# Initialize some global variables

set needsave 0
set replay 0
set feedback 1
set fontsize 20
set prompt "Please load a recording script and start a new session"
set ::name ""
set ::imax 0


# Draw waveform and prompt boxes

pack [canvas .c -height 80 -width 1000 -relief sunken -bd 3]
.c create waveform 0 0 -sound s -height 80 -width 1000 -limit 32768 -tags wave
pack [frame .f2 -relief sunken -bd 3] -pady 15
pack [label .f2.l1 -text Prompt: -anchor w] -fill x
pack [label .f2.l2 -textvar prompt -font "Helvetica $fontsize bold"] \
	-expand yes -fill x


# Buttons, time, and level meter

snack::createIcons
pack [frame .f1] -pady 5
button .f1.bp -bitmap snackPlay -width 40 -command Play -state disabled
#button .f1.bu -bitmap snackPause -command Pause
#button .f1.bs -bitmap snackStop -command Stop
button .f1.br -bitmap snackRecord -width 40 -fg red -state disabled
button .f1.pr -text Prev -command Prev -state disabled
button .f1.ne -text Next -command Next -state disabled
frame .f1.cbf
checkbutton .f1.cbf.be -text replay -variable replay -anchor w
checkbutton .f1.cbf.bf -text feedback -variable feedback -command ToggleGraphics\
 -anchor w
label .f1.time -text "00:00.0" -width 10
snack::levelMeter .f1.lm -width 20 -length 200
label .f1.level -textvariable level

# Arrow key descriptions

frame .f1.f
grid [frame .f1.f.g]
grid [label .f1.f.g.lc -text <Space>=Play -relief raised -bd 3] -row 2 \
  -column 1 -padx 20
grid [label .f1.f.g.lu -text <Up>=Record -relief raised -bd 3] -row 1 -column 3
grid [label .f1.f.g.ll -text <Left>=Prev -relief raised -bd 3] -row 2 -column 2
grid [label .f1.f.g.ld -text <Down>=Stop -relief raised -bd 3] -row 2 -column 3
grid [label .f1.f.g.lr -text <Right>=Next -relief raised -bd 3] -row 2 \
  -column 4

pack .f1.cbf.be .f1.cbf.bf -fill x
pack .f1.bp .f1.br .f1.pr .f1.ne .f1.cbf .f1.time .f1.lm .f1.level \
    .f1.f -side left
bind .f1.br <ButtonPress-1>   Record
bind .f1.br <ButtonRelease-1> Stop


# Database browser

frame .db -relief raised -bd 3
pack [label .db.l -text "Note! Recording is disabled when the database browser is displayed."]
pack [frame .db.f0] -expand true -fill x
pack [label .db.f0.l1 -text Session: -anchor w] -side left -fill x \
    -expand true
pack [label .db.f0.l2 -text Sentence: -anchor w] -side left -fill x \
    -expand true
pack [frame .db.f1] -expand true -fill both
pack [listbox .db.f1.l1 -yscrollcommand [list .db.f1.s1 set]] \
	-side left -fill both -expand true
pack [scrollbar .db.f1.s1 -orient vertical -command [list .db.f1.l1 yview]] \
	-side left -fill y
pack [listbox .db.f1.l2 -yscrollcommand [list .db.f1.s2 set]] \
	-side left -fill both -expand true
pack [scrollbar .db.f1.s2 -orient vertical -command [list .db.f1.l2 yview]] \
	-side left -fill y
bind .db.f1.l1 <ButtonRelease-1> BrowseSession
bind .db.f1.l2 <ButtonRelease-1> BrowseSentence
pack [button .db.f1.b -text Goto -command Goto] -side left

pack [frame .db.f2]
pack [button .db.f2.b -text Save -command SaveTrans] -side right
pack [entry .db.f2.e -width 100 -textvariable ::editprompt] -side right
pack [button .db.b -text "Hide" -command CloseBrowser]


# Message bar

pack [frame .bf] -side bottom -fill x
entry .bf.lab -font {Helvetica 18 bold} -textvar msg -width 1 \
    -relief sunken -bd 1 -state disabled
pack .bf.lab -side left -expand yes -fill x

wm protocol . WM_DELETE_WINDOW exit

proc OpenBrowser {} {
  wm geometry . {}
  pack .db -before .bf -expand true -fill both
  .f1.br configure -state disabled
  bind . <KeyRelease-Up> {}
  bind . <KeyPress-Down> {}
}

proc CloseBrowser {} {
  wm geometry . {}
  pack forget .db
  .f1.br configure -state normal
  bind . <KeyRelease-Up> Record
  bind . <KeyPress-Down> Stop
}

proc BrowseSession {} {
  set cur [.db.f1.l1 curselection]
  if {$cur != ""} {
    set ::bsession [lindex [split [.db.f1.l1 get $cur] :] 0]
    set dir [format "sn%04d" $::bsession]
    set filelist [lsort [glob -nocomplain [file join $dir sent???.wav]]]
    .db.f1.l2 delete 0 end
    foreach file $filelist {
      .db.f1.l2 insert end $file
    }
    set ::msg "Recorded [llength $filelist]/$::imax"
  }
}

proc BrowseSentence {} {
  set cur [.db.f1.l2 curselection]
  if {$cur != ""} {
    s read [.db.f1.l2 get $cur]
    SetTime [s length -unit sec]
    if [catch {open [file rootname [.db.f1.l2 get $cur]].txt} in] {
      set msg $in
    } else {
      set ::editprompt [lindex [split [read $in] \n] 0]
      close $in
    }
    Play
  }
}

proc SaveTrans {} {
  set cur [.db.f1.l2 curselection]
  if {$cur != ""} {
    if [catch {open [file rootname [.db.f1.l2 get $cur]].txt w} out] {
      error $out
    } else {
      puts $out $::editprompt
      close $out
    }
  }
}

proc Goto {} {
  CloseBrowser
  if {![info exists ::bsession]} return
  set ::session $::bsession
  GetSpeakerInfo $::session
  DoOpenScriptFile $::script
  set ::dir [format "sn%04d" $::session]
  set cur [.db.f1.l2 curselection]
  if {$cur != ""} {
    scan [.db.f1.l2 get $cur] "sn%d/sent%d" dummy n
    set ::sentence $n
  } else {
    set ::sentence 1
  }
  set ::prompt $::prompts($::sentence)
  GetSentence
  if {$::sentence == $::imax} {
    ConfigPrev normal
    ConfigNext disabled
  } elseif {$::sentence == 1} {
    ConfigPrev disabled
    ConfigNext normal
  } else {
    ConfigPrev normal
    ConfigNext normal
  }
  wm title . "Session $::session ($::script)"
  set ::msg "Session $::session, sentence 1/$::imax"
}

proc OpenSpeakerDialog {} {
  set w .si
  catch {destroy $w}
  toplevel $w -class Dialog
  GetSpeakerInfo $::session
  pack [label $w.nl -text Name:]
  pack [entry $w.ne -textvariable ::name -width 40]
  pack [label $w.al -text Age:]
  pack [entry $w.ae -textvariable ::age -width 4]
  pack [label $w.rl -text Region:]
  pack [entry $w.re -textvariable ::region -width 40]
  pack [radiobutton $w.gf -text Female -value Female -variable ::gender] \
      -anchor w
  pack [radiobutton $w.gm -text Male -value Male -variable ::gender] \
      -anchor w
  pack [label $w.ol -text Other:]
  pack [entry $w.oe -textvariable ::other -width 40]
  pack [frame $w.bf -relief raised -bd 1] -expand yes -fill x
  snack::makeDialogBox $w -title "Speaker information" -type ok
  SaveSpeakerInfo
}

proc GetSpeakerInfo {n} {
  set ::name ""
  set ::age ""
  set ::region ""
  set ::gender Female
  set ::other ""
  set dir [format "sn%04d" $n]
  catch {source [file join $dir info.txt]}
}

proc SaveSpeakerInfo {} {
  set dir [format "sn%04d" $::session]
  if {[catch {open [file join $dir info.txt] w} out]} {
    error $out
  } else {
    puts $out "set ::name   \"$::name\""
    puts $out "set ::age    \"$::age\""
    puts $out "set ::region \"$::region\""
    puts $out "set ::gender \"$::gender\""
    puts $out "set ::other  \"$::other\""
    puts $out "set ::script \"$::script\""
    close $out
  }
  catch {destroy .si}
  set i 0
  while {[lindex [split [.db.f1.l1 get $i] :] 0] < $::session} {
    if {[.db.f1.l1 get $i] == ""} break
    incr i
  }
  .db.f1.l1 delete $i
  .db.f1.l1 insert $i "$::session: $::name, d $::script"
}

proc OpenScriptFile {} {
  set types {
    {{Script Files} {.scr}}
    {{All Files}    *  }
  }
  set file [tk_getOpenFile -title "Open prompt file" -filetypes $types]
  if {$file == ""} return
  set ::script $file
  if {$::name != ""} SaveSpeakerInfo
  DoOpenScriptFile $file
  wm title . "Session $::session ($::script)"
  set msg "Session $::session, sentence 1/$::imax"
  set ::sentence 1
  GetSentence
  ConfigNext normal
  ConfigPrev disabled
}

proc SetTime {t} {
  set mmss [clock format [expr int($t)] -format "%M:%S"]
  .f1.time config -text $mmss.[format "%d" [expr int(10*($t-int($t)))]]
}

proc Update {} {
  if {$::op == "p"} {
    set t [audio elapsed]
    set end   [expr int([s cget -rate] * $t)]
    set start [expr $end - [s cget -rate] / 10]
    if {$start < 0} { set start 0}
    if {$end >= [s length]} { set end -1 }
    if {[s length] > 0 && $start < [s length]} {
      if [catch {set l [s max -start $start -end $end]}] {
	puts [s length],$start,$end
      }
    } else {
      set l 0
    }
  } else {
    set l [t max]
    t length 0
    set t [s length -unit sec]
    SetTime $t
  }
  if {$::feedback} {
   .f1.lm configure -level $l
  }
  
  after 100 Update
}

proc ToggleGraphics {} {
 if {$::feedback} {
  .c create waveform 0 0 -sound s -height 80 -width 1000 -limit 32768 -tags wave
 } else {
  .c delete wave
 }
}

proc Record {} {
  if {$::op == "r"} return
  ConfigPrev disabled
  ConfigNext disabled
  s stop
  s record
  t record
  set ::op r
  set ::needsave 1
  .f1.bp configure -relief raised
#  .f1.br configure -relief groove
  .c itemconfig wave -fill darkgreen
  if {$::feedback == 0} {
   .c delete wave
  }

}

proc Play {} {
  t stop
  s stop
  s play -command Stop
  set ::op p
  .f1.bp configure -relief groove
#  .f1.br configure -relief raised
  ConfigPrev disabled
  ConfigNext disabled
  # .f1.bu configure -relief raised
}

proc Stop {} {
  if {$::op == "s"} return
  s stop
  t record
  .f1.bp configure -relief raised
#  .f1.br configure -relief raised

  if {[winfo ismapped .db] == 0} {
    if {[info exists ::sentence] && $::sentence > 1} {
      ConfigPrev normal
    }
    if {[info exists ::sentence] && $::sentence < $::imax} {
      ConfigNext normal
    }
  }
  if {$::op == "p"} {
    set ::op s
    if {[info exists ::sentence] && $::sentence == $::imax} {
      tk_messageBox -message "The script is finished"
    }
    return
  }
  set ::op s
  # .f1.bu configure -relief raised
  if {[s length -unit sec] < 0.8} {
    tk_messageBox -message "Note! Pressing the record button starts recording. Releasing it stops recording. You can not just click on it." -icon warning
    return
  }  
  set arg [expr {[s max] / 32767.0}]
  if {$arg < 0.00001} { set arg 0.00001 }
  set ::level [format "%.1fdB" [expr {20.0 * log($arg)}]]
  if {[s max] < 10000} {
    .c itemconfig wave -fill red
    tk_messageBox -message "Low volume!" -icon warning
  }
  if {[s max] == 32767 || [s min] == -32768} {
    .c itemconfig wave -fill red
    tk_messageBox -message "Signal clipped!" -icon warning
  }
  if {$::feedback == 0} {
   .c create waveform 0 0 -sound s -height 80 -width 1000 -limit 32768 -tags wave
  }
  if {$::needsave && [info exists ::dir]} {
    s write [file join $::dir [format "sent%03d" $::sentence].wav]
    if {[catch {open [file join $::dir [format "sent%03d" $::sentence].txt] \
	w} out]} {
      error $out
    } else {
      puts $out $::prompt
      close $out
    }
    set ::needsave 0
    if {$::replay} {
      Play
    } else {
      if {$::sentence == $::imax} {
	tk_messageBox -message "The script is finished"
      }
    }
    .menu.file entryconfigure "Open script..." -state disabled
  }
}

proc Pause {} {
  s pause
  if {$::op != "s"} {
    if {[.f1.bu cget -relief] == "raised"} {
      .f1.bu configure -relief groove
    } else {
      .f1.bu configure -relief raised
    }
  }
}

proc GetSentence {} {
  if {[info exists ::dir]} {
    if {[file exists [file join $::dir [format "sent%03d" $::sentence].wav]]} {
      s read [file join $::dir [format "sent%03d" $::sentence].wav]
      SetTime [s length -unit sec]
    }
  }
  set ::prompt $::prompts($::sentence)
  set ::msg "Session $::session, sentence $::sentence/$::imax"
  
  set size 20
  while {[font measure "Helvetica $size bold" $::prompt] > 1024} {
    incr size -2
  }    
  .f2.l2 configure -font "Helvetica $size bold"
}

proc Next {} {
  incr ::sentence
  s flush
  GetSentence
  if {$::sentence == $::imax} {
    ConfigNext disabled
  }
  ConfigPrev normal
}

proc Prev {} {
  incr ::sentence -1
  s flush
  GetSentence
  if {$::sentence == 1} {
    ConfigPrev disabled
  }
  ConfigNext normal
}

proc DoOpenScriptFile {script} {
  set i 1
  if [catch {open $script} in] {
    set ::msg $in
  } else {
    set promptfile [read $in]
    close $in
    foreach row [split $promptfile \n] {
      if {$row != ""} {
	set ::prompts($i) $row
	incr i
      }
    }
    set ::imax [expr $i - 1]
  }
  .f1.bp configure -state normal
  bind . <space> Play
  .f1.br configure -state normal
  bind . <KeyRelease-Up> Record
  bind . <KeyPress-Down> Stop
}

proc FirstSession {} {
  set declist [lsort -decreasing $::dirlist]
  if {$::dirlist != ""} {
    set lastdir [lindex $declist 0]
    set lastsession [string trimleft $lastdir sn0]
    if {[llength [glob -nocomplain [file join $lastdir sent???.wav]]] > 0} {
      incr lastsession
    }
    set ::session $lastsession
  } else {
    set ::session 1
  }
  incr ::session -1
  # Uncomment to make Speaker window pop-op immediately
  #    NewSession
}

set ::next(normal)   Next
set ::next(disabled) ""
set ::prev(normal)   Prev
set ::prev(disabled) ""

proc ConfigNext { arg } {
  .f1.ne configure -state $arg
  bind . <Key-Right> $::next($arg)
}

proc ConfigPrev { arg } {
  .f1.pr configure -state $arg
  bind . <Key-Left> $::prev($arg)
}

proc NewSession {} {
  set ::name ""
  set ::age ""
  set ::region ""
  set ::gender Female
  set ::other ""
  incr ::session
  set ::dir [format "sn%04d" $::session]
  file mkdir $::dir
  if {$::script != ""} {
    set ::sentence 1
    set ::prompt $::prompts($::sentence)
    GetSentence
    ConfigNext normal
    ConfigPrev disabled
  }
  .menu.file entryconfigure "Open script..." -state normal
  wm title . "Session $::session ($::script)"
  set msg "Session $::session, sentence 1/$::imax"
  update
  OpenSpeakerDialog
  #    while {$::name == ""} OpenSpeakerDialog
}

# Create a list with all sessions so far

set ::script ""
set dirlist [lsort [glob -type d -nocomplain {sn[0-9][0-9][0-9][0-9]}]]
foreach sn $dirlist {
  set n [string trimleft $sn sn0]
  GetSpeakerInfo $n
  set l $script
  if {[string length $l] > 30} {
    set l ...[string range $l [expr {[string length $l]-30}] end]
  }
  .db.f1.l1 insert end "$n: $::name, $::l"
}


# Uncomment these lines to open default script at start-up
#set script tests2.txt
#DoOpenScriptFile $script


# Uncomment these line to use built-in script
#set script "Built-in"
#set sentlist [list \
#    "This is sentence one" \
#    "This is sentence two" \
#    "This is sentence three" \
#    "This is sentence four"
#]
#set i 0
#foreach sent $sentlist { set prompts([incr i]) $sent }
#set ::imax $i
#.f1.bp configure -state normal
#.f1.br configure -state normal
#bind . <KeyRelease-Up> Record
#bind . <KeyPress-Down> Stop


# Use session number specified on command line, otherwise use next slot

if {[info exists argv] && $argv != ""} {
  if {[string match "-b" [lindex $argv 0]]} {
    OpenBrowser
    set argv [lreplace $argv 0 0]
  }
  set session [lindex $argv end]
  if {$session != ""} {
    set ::dir [format "sn%04d" $session]
    file mkdir $::dir
  }
} else {
  FirstSession
}

t record
set op s
Update
