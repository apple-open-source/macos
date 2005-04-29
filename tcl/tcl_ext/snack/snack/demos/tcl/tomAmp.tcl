#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"
# derived from Snack snamp.tcl demo player by Tom Wilkason
#
global taFiles tomAmp
set tomAmp(settingsFile) "~/.tomAmpDefaults"
set tomAmp(cacheFile) "~/.tomAmpCache"
set tomAmp(defaultFolderFile) "~/default.tafs"
set tomAmp(Console) 0
set tomAmp(p) 0
set tomAmp(pause) 0
set tomAmp(elapsed) 0
set tomAmp(current) {}
set tomAmp(currentMP3) {}
;# Setup sound settings
package require -exact snack 2.2
# Buttons
option add *font {Helvetica 10 bold}
option add *Button.activeBackground yellow
option add *Button.borderwidth 1
option add *Button.relief raised
option add *Entry.background lightyellow
option add *Entry.foreground black
option add *Entry.font {Helvetica 9 bold}
option add *Entry.relief sunken
#option add *Entry.borderwidth 2
option add *Label.font {Helvetica 9 bold}
option add *Listbox.background black
option add *Listbox.foreground green
option add *Listbox.font {Helvetica 9 bold}
if {$::tcl_platform(os) == "Linux"} {
    option add *Scale.font {Helvetica 8}
} else {
    option add *Scale.font {Helvetica 7}
}
option add *Scale.activebackground blue
option add *troughcolor black

snack::debug 0
snack::sound s -debug 0
snack::audio playLatency 1200

###############################################################################
# Function   : buildGui
# Description: Build the player Gui
# Author     : Tom Wilkason
# Date       : 12/27/2000
###############################################################################
proc buildGui {args} {

   global tomAmp taFiles LB SS
   set tomAmp(timestr) "Snack MP3 Player"
   wm title . "SnackAmp MPEG Player"
   wm protocol . WM_DELETE_WINDOW exit
   ;##
   ;# Control Panel
   ;#
   pack [frame .panel -borderwidth 2 -relief ridge] -side top -fill x -expand false -anchor w
   ;# Display name and current status
   label .panel.l -background black -foreground green -borderwidth 2 -relief ridge \
   -font {Helvetica 12 bold} -height 4 -textvar tomAmp(timestr) -wraplength 550
   set tomAmp(Mute) 0
   bind .panel.l <ButtonPress-1> {toggleMute .panel.l}
   grid .panel.l -row 0 -column 1 -columnspan 2 -sticky ew

   snack::createIcons

   grid [frame .panel.player -borderwidth 2 -relief ridge] -row 1 -column 1 -sticky news
   ;# Amp controls
   pack [button .panel.player.prev -bitmap snackPlayPrev -command Prev] -side left
   balloon .panel.player.prev "Play previous in list"
   pack [button .panel.player.bp -bitmap snackPlay -command Play] -side left
   balloon .panel.player.bp "Play Selection"
   pack [button .panel.player.pause -bitmap snackPause -command {Pause}] -side left
   balloon .panel.player.pause "Pause/Resume playback"
   proc pauseRelief {down} {
      global tomAmp
      if {$down} {
         .panel.player.pause configure -relief sunken
      } else {
         .panel.player.pause configure -relief raised
      } ;# end if
      set tomAmp(pause) $down
   }
   pack [button .panel.player.bs -bitmap snackStop -command Stop] -side left
   balloon .panel.player.bs "Stop playing"
   pack [button .panel.player.next -bitmap snackPlayNext -command Next] -side left
   balloon .panel.player.next "Play next in list"
   pack [button .panel.player.bo -image snackOpen -command OpenFile] -side left
   balloon .panel.player.bo "Add a file to play list"

   ;# Offset within file
   set SS [scale .panel.ss -show yes -orient horiz -width 10 -len 250 -from 0. -to 60. \
      -var tomAmp(p) -tickinterval 30 -sliderlength 15 -relief ridge -borderwidth 2 \
      -activebackground blue -troughcolor black]
   balloon $SS "Show/Adjust playback location within file"
   grid $SS -column 2 -row 1 -sticky ew

   ;# Sound level
   set tomAmp(gain) [snack::audio play_gain]
   scale .panel.sv -show no -orient vert -width 10 -command {snack::audio play_gain}\
         -len 85 -var tomAmp(gain) -from 130. -to 0. -sliderlength 15 \
         -activebackground blue -troughcolor black

   grid .panel.sv -column 0 -row 0 -rowspan 3 -sticky ns
   balloon .panel.sv "Adjust the playback level"
   # Adjust grid resize behavior
   grid columnconfigure .panel 0 -weight 0
   grid columnconfigure .panel 1 -weight 0
   grid columnconfigure .panel 2 -weight 1

   ;# Allow time to be addjusted
   set tomAmp(setdrag) 1
   bind $SS <ButtonPress-1> {set tomAmp(setdrag) 0}
   bind $SS <ButtonRelease-1> {set tomAmp(setdrag) 1 ; PlayOffset}

   ;# Collapsable frame
   set CF [cframe .hull "MP3 Files"]
   pack .hull -side top -fill both -expand true
   cframe.close $CF

   ;# Filter box
   pack [frame $CF.filter -borderwidth 2 -relief ridge] -side top -expand no -fill x
   pack [label $CF.filter.label -text "Filter Selection"] -side left -expand no -fill x
   pack [entry $CF.filter.entry -textvariable tomAmp(filter) -width 50] -side left -expand no -fill x
   bind $CF.filter.entry <Return>   "filterMusic"
   bind $CF.filter.entry <Double-1>   {set tomAmp(filter) "";filterMusic}
   balloon $CF.filter.entry "Enter text to restrict files to play,\nEnter or 'Replace' to replace list\n'Append' to add to bottom of current list\nDouble-click to clear"

   ;# Append/Replace buttons for filter
   pack [button $CF.filter.btApp -text "Append" -command {filterMusic append} -pady 0] -side left
   balloon $CF.filter.btApp "Modify the filter box and then press this to append files to files below"
   pack [button $CF.filter.btRep -text "Replace" -command {filterMusic replace} -pady 0] -side left
   balloon $CF.filter.btApp "Modify the filter box and then press this to replace list below with newly filtered files"

   ;##
   ;# Scrolled Listbox
   ;#
   set LB [Scrolled_Listbox $CF.frame -selectmode extended -exportselection no -height 20 \
      -background black -foreground green -font {Helvetica 9 bold}]
   pack $CF.frame -side top -expand yes -fill both
   # set w $LB

   # Listbox popup menu
   set m $LB.popup
   menu $m -tearoff 0
   $m add command -label "Play"  -command {Play}
   $m add command -label "Remove"  -command {Cut}
   $m add command -label "Delete from Disk"  -command {Delete}
   $m add command -label "Append to Config File"  -command {}

   bind $LB <ButtonPress-3> {
      tk_popup %W.popup %X %Y
   }

   # Bindings
   bind $LB <Double-ButtonPress-1> Play
   bind $LB <B1-Motion> {Drag %y}
   bind $LB <ButtonPress-1> {focus %W;Select %y}
   bind $LB <BackSpace> Cut
   bind $LB <Delete> Cut

   ;# Entry box to allow renaming of current selection
   pack [frame $CF.fname] -side top -expand no -fill x
   pack [label $CF.fname.fname -textvariable tomAmp(fullName) -justify left -borderwidth 2 -relief ridge] -side top -expand no -fill x
   pack [entry $CF.fname.name -textvariable tomAmp(entryName) ] -side top -expand false -fill x
   balloon $CF.fname.name "Change the file name here and press enter when complete"
   bind $CF.fname.name <Return> {
      if {$tomAmp(entryName) != $tomAmp(origEntryName)} {
         if {$tomAmp(currentMP3)==$taFiles($tomAmp(origEntryName))} {
            Stop
            s destroy
            snack::sound s
         } ;# end if
         set newFileName [file join [file dirname $taFiles($tomAmp(origEntryName))] $tomAmp(entryName)]
         # puts "$tomAmp(entryName) vs $tomAmp(origEntryName) -> '$newFileName'"
         file rename -force $taFiles($tomAmp(origEntryName)) $newFileName
         unset taFiles($tomAmp(origEntryName))
         set tomAmp(origEntryName) $tomAmp(entryName)
         set taFiles($tomAmp(entryName))  $newFileName
         $LB delete $tomAmp(entryIndex)
         ;# Insert either with or without folder name
         $LB insert $tomAmp(entryIndex) $tomAmp(entryName)
         Highlight $tomAmp(entryIndex)
      } ;# end if
   }

   InstallMenu

}
proc Scrolled_Listbox { f args } {
   frame $f
   listbox $f.list \
      -xscrollcommand [list Scrolled_Listbox.set $f.xscroll  [list grid $f.xscroll -row 1 -column 0 -sticky news]] \
      -yscrollcommand [list Scrolled_Listbox.set $f.yscroll  [list grid $f.yscroll -row 0 -column 1 -sticky news]]
   eval {$f.list configure} $args
   scrollbar $f.xscroll -orient horizontal -command [list $f.list xview]
   scrollbar $f.yscroll -orient vertical   -command [list $f.list yview]
   grid $f.list -sticky news
   ;##
   ;# Only the data should expand, not the scroll bars
   ;#
   grid rowconfigure    $f 0 -weight 1
   grid columnconfigure $f 0 -weight 1
   ;##
   ;# Modify to not resize when the scroll bars go away (make a minimum size)
   ;#
   grid columnconfigure $f 1 -minsize 18
   grid rowconfigure    $f 1 -minsize 18
   return $f.list
}
#******************************************************#
# Function   : Scroll_set
# Description: callback for the scroll list box, hide
#              scroll bars if they aren't needed
# Author     : Tom Wilkason
# Date       : 6/4/1999
#*******************************************************#
proc Scrolled_Listbox.set {scrollbar geoCmd offset size} {
   if {$offset != 0.0 || $size != 1.0} {
      eval $geoCmd
      $scrollbar set $offset $size
   } else {
      set manager [lindex $geoCmd 0]
      $manager forget $scrollbar
   } ;# end if
}

;##
;# Build the menu bar at the top of the GUI
;#
proc InstallMenu {} {
   global taFiles tomAmp LB tomAmpSettings tcl_platform
   set M .bRmenubar
   catch {menu $M}
   # attach it to the main window
   . config -menu $M
   # Create more cascade menus
   foreach m {File Play-Lists Folders Settings Help} {
      if {$m == "Play-Lists"} {
         set $m [menu $M.m$m -postcommand [list configFiles $M.m$m]]
         $M.m$m add command -label "Clear" -command {Clear}
         $M.m$m add command -label "Save..." -command {SavePlaylistFile}
         $M.m$m add separator
         $M.m$m add cascade -label "Append" -menu [menu $M.m$m.mAppend -tearoff 0]
         $M.m$m add cascade -label "Replace" -menu [menu $M.m$m.mReplace -tearoff 0]

      } else {
         set $m [menu $M.m$m]
      }
      $M add cascade -label $m -menu $M.m$m
   }
   $Folders add command -label "Clear All" -command {
      global taFolders
      foreach {item} [array name taFolders *] {
         set taFolders($item) 0
      } ;# end foreach
      filterMusic
   }
   $Folders add command -label "Set All" -command {
      global taFolders
      foreach {item} [array name taFolders *] {
         set taFolders($item) 1
      } ;# end foreach
      filterMusic
   }
   $Folders add command -label "Save Folder List..." -command {SaveFolderFile}
   $Folders add cascade -label "Folder Lists..." -menu [menu $Folders.settings -tearoff 0 -postcommand [list folderFiles $Folders.settings]]

   $Folders add separator
   set tomAmp(FolderMenu) $Folders
   set tomAmp(FolderSettings) $Folders.settings


   $File add command -label "Add a file to playlist" -command {OpenFile}
   $File add command -label "Rescan Folders" -command {Rescan}
   $File add separator
   $File add command -label "Exit" -command {exit}

   if {$tcl_platform(platform) == "windows"} {
      $Settings add check -label "Show Console" -variable tomAmp(Console) -command Console
      $Settings add separator
   } ;# end if
   $Settings add check -label "Random Play" -variable tomAmpSettings(randomPlay) -command filterMusic
   $Settings add check -label "Continuous Play" -variable tomAmpSettings(ContinuousPlay)
   $Settings add check -label "Rescan Folders on Startup" -variable tomAmpSettings(AutoRescan)
   $Settings add check -label "Show folder names with files" -variable tomAmpSettings(ShowFolder) -command showFolders
   $Settings add check -label "Monitor base folders for new files" -variable tomAmpSettings(MonitorFolders) -command MonitorFolders
   $Settings add separator
   $Settings add command -label "Set Defaults"  -command settingsGui
}
###############################################################################
# Function   : Console
# Description: Toggle the console for debugging purposes (WIN32 only)
# Author     : Tom Wilkason
# Date       : 12/27/2000
###############################################################################
proc Console {args} {
   global taFiles tomAmp LB
   if {$tomAmp(Console)==0} {
      console hide
   } else {
      console show
   }
}
;##
;# Callback to show config files on menu, called from postcommand
;#
proc configFiles {menu} {
   global taFiles tomAmp LB
   # Rescan
   if {[catch {set Files [glob ~/*.taf]} result]} {
      return
   }

   $menu.mAppend delete 0 end
   $menu.mReplace delete 0 end
   ;##
   ;# Need to sort the list by decending date
   ;#
   foreach File $Files {
      set Date [file mtime $File]
      lappend FileList [list $File $Date]
   }
   set FileList [lsort -index 1 -decreasing $FileList]
   foreach {File} $FileList {
      ;##
      ;# Pull out only the file name and add it to the menu
      ;#
      set F [lindex $File 0]
      $menu.mAppend add command -label "[file tail $F]" -command [list ReadPlaylistFile $F "append"]
      $menu.mReplace add command -label "[file tail $F]" -command [list ReadPlaylistFile $F "replace"]
   }
}
;##
;# Callback to show folder setting files
;#
proc folderFiles {menu} {
   global taFiles tomAmp LB
   # Rescan
   if {[catch {set Files [glob ~/*.tafs]} result]} {
      return
   }

   $menu delete 0 end
   ;##
   ;# Need to sort the list by decending date
   ;#
   foreach File $Files {
      set Date [file mtime $File]
      lappend FileList [list $File $Date]
   }
   set FileList [lsort -index 1 -decreasing $FileList]
   foreach {File} $FileList {
      ;##
      ;# Pull out only the file name and add it to the menu
      ;#
      set F [lindex $File 0]
      $menu add command -label "[file tail $F]" -command [list ReadFolderFile $F]
   }
}
###############################################################################
# Function   : toggleMute
# Description: Toggle muting of sound, bound to click on main label
# Author     : Tom Wilkason
# Date       : 12/31/2000
###############################################################################
proc toggleMute {widget} {
   global tomAmp
   if {$tomAmp(Mute)} {
      set tomAmp(Mute) 0
      $widget configure -foreground green
      snack::audio play_gain $tomAmp(saveGain)
   } else {
      set tomAmp(Mute) 1
      set tomAmp(saveGain) [snack::audio play_gain]
      snack::audio play_gain 0
      $widget configure -foreground red
   } ;# end if
   set tomAmp(saveMute) $tomAmp(Mute)

}
###############################################################################
# Function   : settingsGui
# Description: Small GUI to allow the user to set the defaults
# Author     : Tom Wilkason
# Date       : 12/30/2000
###############################################################################
proc settingsGui {args} {
   global taFiles tomAmp LB tomAmpSettings
   set B .settings
   toplevel $B
   set i 0
   grid [label $B.[incr i] -text "Please set the defaults below and press OK to save\nRescan folders if you change them here" ]\
    -columnspan 2
   grid [frame $B.[incr i] -height 2 -borderwidth 2 -relief sunken] -columnspan 2 -sticky nsew
   #----------------------------------------------------------------------------------------------
   grid [label $B.[incr i] -text "List of base directories to scan" -justify left] \
      [entry $B.[incr i] -textvariable tomAmpSettings(mediaDirs) -width 50]  -sticky ew
   grid [label $B.[incr i] -text "Start with random play" -justify left] \
      [checkbutton $B.[incr i] -variable tomAmpSettings(randomPlay)] -sticky ew
   grid [label $B.[incr i] -text "Continuous Play Mode" -justify left] \
      [checkbutton $B.[incr i] -variable tomAmpSettings(ContinuousPlay)] -sticky ew
   grid [label $B.[incr i] -text "Rescan Folders on Startup" -justify left] \
      [checkbutton $B.[incr i] -variable tomAmpSettings(AutoRescan)] -sticky ew
   grid [label $B.[incr i] -text "Show folder names with song names" -justify left] \
      [checkbutton $B.[incr i] -variable tomAmpSettings(ShowFolder)] -sticky ew
   grid [label $B.[incr i] -text "Monitor base folders for changes" -justify left] \
      [checkbutton $B.[incr i] -variable tomAmpSettings(MonitorFolders)] -sticky ew
   grid [label $B.[incr i] -text "Folder level to show on Folder Menu" -justify left] \
      [entry $B.[incr i] -textvariable tomAmpSettings(FolderDepth)] -sticky ew
   grid [label $B.[incr i] -text "Subfolders to show in Song List" -justify left] \
      [entry $B.[incr i] -textvariable tomAmpSettings(ListDepth)] -sticky ew
   #----------------------------------------------------------------------------------------------
   grid [frame $B.[incr i] -height 2 -borderwidth 2 -relief sunken] -columnspan 2 -sticky nsew
   grid [button $B.[incr i] -text "OK" -command saveSettings] \
      [button $B.[incr i] -text "Cancel" -command {destroy .settings}]
   after idle raise $B
}
###############################################################################
# Function   : saveSettings
# Description: Save the settings from the settingsGui
# Author     : Tom Wilkason
# Date       : 12/30/2000
###############################################################################
proc saveSettings {args} {
   global tomAmpSettings tomAmp
   set fid [open $tomAmp(settingsFile) w]
   puts $fid [array get tomAmpSettings]
   close $fid
   destroy .settings
   MonitorFolders
}
###############################################################################
# Function   : MonitorFolders
# Description: Monitor the base folders for incoming data, useful
#              for napaster adding files.
# Author     : Tom Wilkason
# Date       : 12/30/2000
###############################################################################
proc MonitorFolders {args} {
   global tomAmpSettings tomAmp
   if {$tomAmpSettings(MonitorFolders)} {
      foreach {Folder} $tomAmpSettings(mediaDirs) {
         ;# First time will be empty
         set tomAmp($Folder,thisData) [glob -nocomplain [file join $Folder *.mp3]]
         if {![info exists tomAmp($Folder,lastData)]} {
            set tomAmp($Folder,lastData) $tomAmp($Folder,thisData)
         } elseif {$tomAmp($Folder,lastData) != $tomAmp($Folder,thisData)} {
            puts "Changes detected"
            foreach {item} $tomAmp($Folder,thisData) {
               if {[lsearch $tomAmp($Folder,lastData) $item] < 0} {
                  Append $item
                  puts "Detected new file $item"
               } ;# end if
            } ;# end foreach
            set tomAmp($Folder,lastData) $tomAmp($Folder,thisData)
         } ;# end if
      } ;# end foreach
      after cancel MonitorFolders
      after 5000 MonitorFolders
   }
}
;##
;# Read the default settings from the user home dir
;#
proc readSettings {args} {
   global tomAmpSettings tomAmp
   set tomAmpSettings(mediaDirs) ~
   set tomAmpSettings(randomPlay) 0
   set tomAmpSettings(ContinuousPlay) 1
   set tomAmpSettings(AutoRescan) 0
   set tomAmpSettings(ShowFolder) 0
   set tomAmpSettings(MonitorFolders) 0
   set tomAmpSettings(ListDepth) 10       ;# Will show last folder
   set tomAmpSettings(FolderDepth) 10     ;# Will show last folder
   if {[catch {
      set fid [open $tomAmp(settingsFile) r]
      array set tomAmpSettings [read $fid]
   } result]} {
      settingsGui
   } else {
      close $fid
   }
}
;##
;# Save the current folder settings to a file
;#
proc SaveFolderFile {args} {
   global taFiles tomAmp LB taFolders
   ;##
   ;# If a list for GlobData is passed in, the second item should be the name to save as (if different)
   ;#
   set types [list [list "Tom Amp Files" .tafs] [list "All Files" "*"]]
   set fName [tk_getSaveFile -title "Save Folder Setting File" -filetypes $types \
      -initialdir ~ -defaultextension tafs]
   catch {focus $ThisWin}
   if { $fName == "" } {
      return  ""
   }
   if {[catch {open $fName w} InFile]} {
      return -code error "Save Error: Unable to open '$fName' for writing\n"
   }

   if {$InFile!=0} {
      foreach {File} [array names taFolders] {
         puts $InFile [list $File $taFolders($File)]
      } ;# end foreach
      close $InFile
      return $fName
   } else {
      return -1
   }
}
;##
;# Read a file and change the favorate folders
;#
proc ReadFolderFile {fName} {
   global taFolders
   if {[file exists $fName]} {
      set fid [open $fName r]
      set full [split [read $fid] \n]
      ;# Insert the files if they are not already in the list
      foreach line $full {
         set taFolders([lindex $line 0]) [lindex $line 1]
      } ;# end foreach
      close $fid
   } ;# end if
   filterMusic
}
;##
;# Save current files in list
;#
proc SavePlaylistFile {args} {
   global taFiles tomAmp LB
   ;##
   ;# If a list for GlobData is passed in, the second item should be the name to save as (if different)
   ;#
   set types [list [list "Tom Amp Files" .taf] [list "All Files" "*"]]
   set fName [tk_getSaveFile -title "Save Configuration File" -filetypes $types \
      -initialdir ~ -defaultextension taf]
   catch {focus $ThisWin}
   if { $fName == "" } {
      return  ""
   }
   if {[catch {open $fName w} InFile]} {
      return -code error "Save Error: Unable to open '$fName' for writing\n"
   }

   if {$InFile!=0} {
      foreach {File} [getSelection 0 end] {
         puts $InFile $taFiles($File)
      } ;# end foreach
      close $InFile
      return $fName
   } else {
      return -1
   }
}
;##
;# Read a file and append/replace current list
;#
proc ReadPlaylistFile {fName {how "replace"}} {
   global taFiles tomAmp LB tomAmpSettings
   set fid [open $fName r]
   set full [split [read $fid] \n]
   if {$how == "replace"} {
      $LB delete 0 end
   } ;# end if
   ;# Insert the files if they are not already in the list
   foreach file $full {
      if {[file exists $file]} {
         set name [getListName $file]
         $LB insert end $name
         set taFiles($name) $file
      } ;# end if
   } ;# end foreach
   close $fid
}
###############################################################################
# Function   : Clear
# Description: Clear the playlist
# Author     : Tom Wilkason
# Date       : 12/27/2000
###############################################################################
proc Clear {args} {
   global taFiles tomAmp LB
   # catch {unset taFiles} result
   $LB delete 0 end
}
;##
;# Open a single file for playing, add to play list (should we play immed?)
;#
proc OpenFile {} {
   global taFiles tomAmp LB
   set file [snack::getOpenFile -format MP3]
   if {$file != ""} {
      Append $file
   }
}
;# Play a list item.
proc Play {{offset 0}} {
   global taFiles tomAmp LB SS
   if {[$LB index active] == ""} {
      set i 0
   } else {
      set i [$LB index active]
   }
   ;# Note: Using current selection causes a problem if the song didn't change
   ;# but the selection did an time scale change that changes the time.
   # if no offset specified then see if random play
   Highlight $i
   Stop
   ;# Check file existance
   set Selection [getSelection $i]
   if {$Selection!={}} {
      if {! [file exists $taFiles($Selection)]} {
         after 10 Next
         return
      } ;# end if
      set errorInfo ""
      ;##
      ;# It seems faster on play -start to have the short file name and cd to that directory
      ;# Also, UNC names are really slow (on Win2K anyway)!
      ;# Strangly, network drive mappings are also fast (if not using UNC)
      ;#
      set tomAmp(currentMP3) $taFiles($Selection)
      cd [file dirname $taFiles($Selection)]
      s config -file $Selection
#     length samprate "maximum sample" "minimum sample" "STRencoding" "number of channels" "file format" "header size in bytes"
      set Info [s info]
      ;# This can check for an invalid header
      if {[lindex $Info 0] == -1} {
         puts stderr "Invalid file: $taFiles($Selection)\n $Info"
         set tomAmp(timestr) "Invalid header for:\n$Selection"
         update
         after 10 Next
         return
      } else {
         #puts "OK: $taFiles($Selection)\n $Info"
         #update
      }
      ;# See if user changed the slider to adjust where to play
      if {$offset == 0} {
         s play -command Next        ;# After complete call 'Next' function
      } else {
         ;# Fires Next command at end of this file
         s play -start $offset -command Next
      }
      ;# What ~10 ticks on 5 or multiple sec intervals
      set trackTime  [s length -units SECONDS]
      set Interval [expr {(round($trackTime/10)/5)*5}]
      if {$Interval == 0} {
         set interval 5
      } ;# end if
      $SS configure -to $trackTime -tickinterval $Interval

      set songTitle [file root $Selection]
      wm title . "$songTitle"
      set tomAmp(current) $songTitle

      file stat $tomAmp(currentMP3) Stat
      append tomAmp(current) " - ($Stat(size))"
      balloon .panel.l "$taFiles($Selection)"

      Timer
   }
}

;##
;# Play next sound file in list
;#
proc Next {} {
   global taFiles tomAmp LB tomAmpSettings
   catch {
#      puts "$tomAmp(elapsed) of [s length -units sec] - $tomAmp(current)"
   }
   set i [$LB index active]
   if {$i == ""} {
      set i 0
   } else {
      incr i
   }
   ;# check for continouos play
   if {$i >= [$LB index end]} {
      if {$tomAmpSettings(ContinuousPlay)} {
         set i 0
      }  else {
         return
      }
   } ;# end if
   Highlight $i
   after 10 Play
}
;##
;# Play previous sound file in list
;#
proc Prev {} {
   global taFiles tomAmp LB tomAmpSettings
   set i [$LB index active]
   if {$i == ""} {
      set i 0
   } elseif {$i==0} {
      set i [$LB index end]
   } else {
      if {$i > 0} {
         incr i -1
      } ;# end if
   }
   Highlight $i
   after 10 Play
}
;##
;# Play a mp3 at some offset, likely bound to slider control
;#
proc PlayOffset {{manual 0}} {
   global taFiles tomAmp LB
   ;# Make sure we have an open file
   if {[s length] > 0} {
      Stop
      ;# Some files are fast and some are slow to respond to -start = 0
      if {$manual == 0} {
         set offset [expr {int(double($tomAmp(p))/[s length -units sec]*[s length])}]
      } else {
         set offset $manual
      }
      s play -start $offset -command Next
      Timer
   } ;# end if
}
###############################################################################
# Function   : Highlight
# Description: Highlight only one selection
# Author     : Tom Wilkason
# Date       : 12/27/2000
###############################################################################
proc Highlight {i} {
   global taFiles tomAmp LB
   $LB selection clear 0 end
   $LB selection set $i
   $LB activate $i
   $LB see $i
   set Entry [getSelection $i]
   catch {
      set tomAmp(entryName) $Entry
      set tomAmp(fullName) [file dirname $taFiles($Entry)]
   }
}
###############################################################################
# Function   : getSelection
# Description: Return the selection(s) with no folder names (for index use)
# Author     : Tom Wilkason
# Date       : 1/1/2001
###############################################################################
proc getSelection {start {end ""}} {
   global LB
   if {$end==""} {
      return [file tail [$LB get $start]]
   } else {
      foreach {item} [$LB get $start $end] {
         lappend result [file tail $item]
      } ;# end foreach
      return $result
   } ;# end if
}
proc showFolders {args} {
   global LB tomAmpSettings taFiles
   foreach {item} [getSelection 0 end] {
      lappend insertList [getListName $taFiles($item)]
   }
   $LB delete 0 end
   if {[llength $insertList] > 0} {
      eval $LB insert end $insertList
   }
}

;##
;# Apend a file to the bottom of the play list
;#
proc Append {file} {
   global taFiles tomAmp LB
   set name [file tail $file]
   if {$file != "" && ![info exists taFiles($name)]} {
      $LB insert end $name
      set taFiles($name) $file
   } ;# end if
}
;##
;# Stop playback of a file and cancle the update timer
;#
proc Stop {} {
   global taFiles tomAmp LB
   pauseRelief 0
   s stop
   after cancel Timer
}
;##
;# Pause/resume playback
;#
proc Pause {args} {
   global taFiles tomAmp LB
   s pause
   if {$tomAmp(pause)} {
      pauseRelief 0
      Timer
   } else {
      pauseRelief 1
      after cancel Timer
   }
   set tomAmp(savePause) $tomAmp(pause)

}
;##
;# Timer to update the display data, active while MP3 playing
;#
proc Timer {} {
   global taFiles tomAmp LB SS
   # Time is curren pos/total len * total time
   # set elapsed [expr {round(double([s current_position])/[s length]*[s length -units sec])}]
   set elapsed [expr {round([s current_position -units sec])}]
   set tomAmp(elapsed) $elapsed
   set tomAmp(timestr) "$tomAmp(current)\n[clock format $elapsed -format %M:%S] of [clock format [expr {int([s length -units sec])}] -format %M:%S]"
   ;# If user dragging slider, don't also try to set slider
   if $tomAmp(setdrag) {
      catch {
         $SS set $elapsed
      }
   }
   #    Draw
   after 500 Timer
}
;##
;# GUI Bindings
;#
proc Cut {} {
   global taFiles tomAmp LB
   if {[$LB curselection] != ""} {
      set Index [$LB index active]
      if {$Index} {incr Index -1}
      foreach {ind} [lsort -decreasing [$LB curselection]] {
         set cut [getSelection $ind]
         unset taFiles($cut)
         $LB delete $ind
      } ;# end foreach
      Highlight $Index
   }
}
;##
;# Callback to remove the current selections from the display
;#
proc Delete {} {
   global taFiles tomAmp LB
   if {[$LB curselection] != ""} {
      set Index [$LB index active]
      if {$Index} {incr Index -1}
      foreach {ind} [lsort -decreasing [$LB curselection]] {
         set File [getSelection $ind]
         if {$tomAmp(currentMP3)==$taFiles($File)} {
            Stop
            s destroy
            snack::sound s
         } ;# end if
         file delete $taFiles($File)
         unset taFiles($File)
         $LB delete $ind
      } ;# end foreach
      Highlight $Index
   }
}
;##
;# Select/Drag
;#
proc Select y {
   global taFiles tomAmp LB
   set tomAmp(old) [$LB nearest $y]
   set Value [$LB get $tomAmp(old)]
   if {$Value != {}} {
      set tomAmp(entryName) [file tail $Value]
      set tomAmp(fullName) [file dirname $taFiles([file tail $Value])]
      set tomAmp(origEntryName) [file tail $Value] ;# for renaming
      set tomAmp(entryIndex) $tomAmp(old)
   } ;# end if

}

proc Drag y {
   global taFiles tomAmp LB
   set new [$LB nearest $y]
   if {$new == -1} return
   set tmp [$LB get $tomAmp(old)]
   $LB delete $tomAmp(old)
   $LB insert $new $tmp
   $LB selection set $new
   set tomAmp(old) $new
}
###############################################################################
# Function   : filterMusic
# Description: Restrict files shown based on the filter & folder settings
# Author     : Tom Wilkason
# Date       : 12/26/2000
###############################################################################
proc filterMusic {{how replace}} {
   global taFiles tomAmp LB tomAmpSettings taFolders
   if {$how == "replace"} {
      $LB delete 0 end
   } ;# end if
   set insertList [list]

   ;# Below will prevent duplicates due to hashing
   foreach {File} [array names taFiles *] {
      if {$File != "" && [string match -nocase "*$tomAmp(filter)*" $taFiles($File)]} {
         ;# Verify this file is in a checked folder
         if {[catch {
            # See if this file matches folder menu entry
            if {$taFolders([getFolderName [file dirname $taFiles($File)]])} {
               set File [getListName $taFiles($File)]
               lappend insertList $File
            } ;# end if
         } result]} {
            puts stderr $result
         }
      } ;# end if
   } ;# end foreach
   ;# Shuffle if needed
   if {$tomAmpSettings(randomPlay)} {
      set insertList [shuffle $insertList]
   } else {
      set insertList [lsort -dictionary $insertList]
   }
   if {[llength $insertList] > 0} {
      eval $LB insert end $insertList
   } ;# end if
   Highlight 0
}
;##
;# Shuffle a list, used to randomize play list
;#
proc shuffle {list} {
   set tmp [list]
   foreach item $list {
      lappend tmp [list $item [expr {rand()}]]
   }
   set res [list]
   foreach item [lsort -real -index 1 $tmp] {
      foreach {orig num} $item {
         lappend res $orig
      } ;# end foreach
   }
   return $res
}
###############################################################################
# Function   : getListName
# Description: Return a song name with the appropriate folders prepended
# Author     : Tom Wilkason
# Date       : 1/3/2001
###############################################################################
proc getListName {File} {
   global tomAmpSettings
   set File [string map {// /} $File]      ;# remove UNC stuff
   if {$tomAmpSettings(ShowFolder)} {
      set parts [lrange [file split $File] end-$tomAmpSettings(ListDepth) end]
      set name [join $parts /]
   } else {
      set name [file tail $File]
   }
   return $name
}
###############################################################################
# Function   : getListName
# Description: Return a folder name with the appropriate depth away from base
# Author     : Tom Wilkason
# Date       : 1/3/2001
###############################################################################
proc getFolderName {File} {
   global tomAmpSettings
   set File [string map {// /} $File]      ;# remove UNC stuff
   set parts [file split $File]
   set len [llength $parts]
   if {$tomAmpSettings(FolderDepth) >= $len} {
      set name [lindex $parts end]
   }  else {
      set name [lindex $parts $tomAmpSettings(FolderDepth)]
   }
   # puts "getFolderName:($tomAmpSettings(FolderDepth)) $name - $File"
   return $name
}
###############################################################################
# Function   : buildFolderMenu
# Description: Build the folder menu base on folder names in taFolders
# Author     : Tom Wilkason
# Date       : 12/26/2000
###############################################################################
proc buildFolderMenu {args} {
   global taFiles tomAmp LB tomAmpSettings taFolders
   $LB delete 0 end
   # Remove after the last fixed menu
   $tomAmp(FolderMenu) delete 5 end
   $tomAmp(FolderMenu) add separator
   foreach {folder} [lsort -dictionary [array names taFolders *]] {
      $tomAmp(FolderMenu) add check -label "$folder" -variable taFolders($folder) -command filterMusic
   } ;# end foreach
}
###############################################################################
# Function   : Rescan
# Description: Reread the directories for files, include subdirectories
#              For each folder, add a menu entry
# Author     : Tom Wilkason
# Date       : 12/26/2000
###############################################################################
proc Rescan {args} {
   global taFiles tomAmp LB tomAmpSettings taFolders
   set here [pwd]
   catch {unset taFiles} result
   ;# save folder settings
   catch {array set saveTaFolders [array get taFolders]}
   catch {unset taFolders} result
   foreach dir $tomAmpSettings(mediaDirs) {
      # set taFolders([file tail $dir]) 1
      set taFolders([getFolderName $dir]) 1
      Rescan.getRecurseFiles $dir
   } ;# end foreach
   ;# Restore folder settings
   foreach {index dir} [array get taFolders] {
      if {[info exists saveTaFolders($index)]} {
            set taFolders($index) $saveTaFolders($index)
      }
   } ;# end foreach
   ;##
   ;# Build the folder list for the menu
   ;#
   buildFolderMenu
   filterMusic
   ;##
   ;# Save the settings
   ;#
   set fid [open $tomAmp(cacheFile) w]
   puts $fid [array get taFiles]
   puts $fid [array get taFolders]
   close $fid

}
;##
;# Recursively scan a directory and build a list of all files
;#
proc Rescan.getRecurseFiles {dir} {
   global taFiles tomAmp LB taFolders
   set dirList [list]
   set fileCount 0

   foreach file [glob -nocomplain [file join $dir *]] {
      if {[file isdirectory $file]} {
         lappend dirList $file
      } else {
         if {[string match -nocase "*.mp3" $file]} {
            set name [file tail $file]
            set taFiles($name) [file join $dir $file]
            incr fileCount
         } ;# end if
      }
   }
   ;# If this dir contained files, the create a menu entry for it
   if {$fileCount} {
      set taFolders([getFolderName $dir]) 1    ;# checkbox on menu
   } ;# end if
   ;# process each directory recursively
   foreach {file} $dirList {
      Rescan.getRecurseFiles [file join $dir $file]
   } ;# end foreach
}
;####################################################################################
;#                   C O L L A P S A B L E    F R A M E
;####################################################################################
;# cframe: Collapsable frame
;# base: name of outer collapsable frame, user is responsible for packing
;# text: title for the window frame, next to collapse button
;# args: additional user settings passed to inner frame
;# returns hull: user can pack into this frame
;#
proc cframe {base text args} {
   #-----------------------------------------------------------
   # Add the widget to the parent.
   #-----------------------------------------------------------
   # create button icons
   image create photo im_Close -data {
      R0lGODlhEAAQAKIAAP///9TQyICAgEBAQAAAAAAAAAAAAAAAACwAAAAAEAAQAAADNhi63BMg
      yinFAy0HC3Xj2EJoIEOM32WeaSeeqFK+say+2azUi+5ttx/QJeQIjshkcsBsOp/MBAA7
   }

   image create photo im_Open -data {
      R0lGODlhEAAQAKIAAP///9TQyICAgEBAQAAAAAAAAAAAAAAAACwAAAAAEAAQAAADMxi63BMg
      yinFAy0HC3XjmLeA4ngpRKoSZoeuDLmo38mwtVvKu93rIo5gSCwWB8ikcolMAAA7
   }

   frame $base
   # let the user pack the outside base frame
   frame $base.ohull  -borderwidth 2 -height 16 -relief ridge

   label $base.button  -borderwidth 0 -image im_Open -relief raised -text "101"

   bind $base.button <Button-1> {cframe.toggle [winfo parent %W].ohull.ihull}
   label $base.label  -anchor w -borderwidth 1 -text $text

   #
   # pack special frame/button
   #
   pack $base.ohull -anchor nw -expand 1 -fill both -side left  -ipadx 2 -ipady 8
   place $base.button  -x 5 -y -1 -width 21 -height 21 -anchor nw -bordermode ignore
   place $base.label  -x 23 -y 3  -height 13 -anchor nw -bordermode ignore

   #
   # Create a label underneath the "title" to prevent overlap
   # then pack a frame in below that which will be the hull i.e. childsite for users
   #
   label $base.ohull.padder -text ""
   pack $base.ohull.padder -side top
   eval set hull $base.ohull.ihull
   ;# Create the inner frame using any additional arguments passed in
   eval frame $hull $args
   pack $hull -expand true -fill both
   return $hull
}
#-----------------------------------------------------------
# Toggle the widget, toggle the 'can be opened' icon.
# can also called as cframe.toggle $cframeWidget
#-----------------------------------------------------------
proc cframe.toggle {ihull} {
   set button "[winfo parent [winfo parent $ihull]].button"
   set a [$button cget -image]
   if { $a == "im_Open" } {
      cframe.close $ihull
   } else {
      cframe.open $ihull
   }
}
#-----------------------------------------------------------
# Collapse the widget, display the 'can be opened' icon.
# can also called as cframe.close $cframeWidget
#-----------------------------------------------------------
proc cframe.close {ihull} {
   set base [winfo parent [winfo parent $ihull]]
   $base.button configure -image im_Close
   pack forget $base.ohull
   ;# Make outer frame just small enough for button and title
   $base configure -height 16 -width 50
   # Force the screen back to its requested size in case the
   # user manually changed it's size.
   wm geometry [winfo toplevel $base] {}
}

#-----------------------------------------------------------
# Open the widget, display the 'can be closed' icon.
# cal also called as cframe.open $cframeWidget
#-----------------------------------------------------------
proc cframe.open {ihull} {
   set base [winfo parent [winfo parent $ihull]]
   $base.button configure -image im_Open
   pack $base.ohull -anchor nw -expand 1 -fill both -side left -ipadx 2 -ipady 10
   ;# Below will force the toplevel to refresh it's size incase the user
   ;# Manually resized the toplevel. In those cases the packing areas
   ;# won't automatically contract unless you tell the toplevel to
   ;# use it's requested size.
   wm geometry [winfo toplevel $base] {}
}
#//////////////////////////////////////////////////////////////////////////////
#                   B A L L O O N    H E L P
#//////////////////////////////////////////////////////////////////////////////
###############################################################################
# Function   : balloon
# Description: Bind ballon help to some widget
# Author     : Tom Wilkason
# Date       : 1/2/2000
###############################################################################
proc balloon {w help} {
    bind $w <Any-Enter> "after 300 [list balloon:show %W [list $help]]"
    bind $w <Any-Leave> "destroy %W.balloon"
}
###############################################################################
# Function   : balloon:show
# Description: Pop-up the ballon help based on a callback for the widget enter
# Author     : Tom Wilkason
# Date       : 1/2/2000
###############################################################################
proc balloon:show {w arg} {
   if {[eval winfo containing  [winfo pointerxy .]]!=$w} {return}
   set top $w.balloon
   catch {destroy $top}
   toplevel $top -bd 1 -bg black
   wm overrideredirect $top 1
   pack [message $top.txt -aspect 10000 -bg lightyellow \
         -font {Arial 8} -text $arg]
   set wmx [winfo rootx $w]
   set wmy [expr [winfo rooty $w]+[winfo height $w]]
   wm geometry $top \
      [winfo reqwidth $top.txt]x[winfo reqheight $top.txt]+$wmx+$wmy
   raise $top
}
#
###############################################################################
# Fire it up
###############################################################################
#
buildGui
readSettings
if {$tomAmpSettings(AutoRescan)} {
   Rescan
   ;##
   ;# Apply the default folder settings
   ;#
   ReadFolderFile $tomAmp(defaultFolderFile)

} else {
   if {[catch {set fid [open $tomAmp(cacheFile) r]} result]} {
      Rescan
   } else {
      ;##
      ;# Read the cached data
      ;#
      gets $fid TaFiles
      array set taFiles $TaFiles
      gets $fid TaFolders
      array set taFolders $TaFolders
      close $fid

      buildFolderMenu
      ReadFolderFile $tomAmp(defaultFolderFile)
   }
}
MonitorFolders

###############################################################################
# Function   : tst
# Description:
# Author     : Tom Wilkason
# Date       : 12/31/2000
###############################################################################
proc tst {{offset 0}} {
   set file "d:/media/incoming/Classic Rock/Crosby, Stills, Nash & Young - Everybody I Love You.mp3"
   set file "//Toms/K6 D/Media/Incoming/Classic Rock/Crosby, Stills, Nash & Young - Everybody I Love You.mp3"
   Stop
   s config -file $file
   set now [clock seconds]
   set xxx [expr {int(double(100.)/[s length -units sec]*[s length])}]
   s play -start $offset -command Next
   puts "Time:[expr {[clock seconds] - $now}]"
   puts [s info]
   Timer
   after 3000 s stop
}

