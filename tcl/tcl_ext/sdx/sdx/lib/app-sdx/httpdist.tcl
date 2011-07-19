#!/bin/sh
#
# httpdist.tcl -- Software packing lists, archives, and distribution
#
# Copyright (c) 1999 Jean-Claude Wippler and Equi4 Software.
# Inspired by http://www.mibsoftware.com/httpsync/
# \
exec tclkitsh "$0" ${1+"$@"}

set PINFO Package.info  ;# not in packing list, can be edited before sending
set PLIST Packing.list  ;# do not touch the contents of this file in any way
set PSEND Packurl.send  ;# list of settings used before to send packages out
set PTEMP Httpdist.tmp  ;# temporary file in $env(TEMP) - used during upload
set PVERS 101           ;# file version, not changed to allow using original

package require md5

proc calcDigest {file} {
    set fd [open $file]
    fconfigure $fd -trans binary
    set sum [md5::md5 [read $fd]]
    close $fd
    return $sum
}

proc clockDisplay {{s ""}} {
    if {$s == ""} { set s [clock seconds] }
    clock format $s -format {%a, %m %b %Y %H:%M:%S} -gmt 1
}

proc packedItem {file} {
    set s [file size $file]
    set d [clockDisplay [file mtime $file]]
    if {[catch {regexp {(...)$} [file attr $file -perm]} m]} {
        set m 644
    }
    return "$file $s $d GMT $m [calcDigest $file]"
}

proc walker {{omit {}} {dirs .}} {
    array set skips {. 1 .. 1}
    foreach x $omit {
        set skips($x) 1
    }

    set result {}

    while {[llength $dirs] > 0} {
        set d [lindex $dirs 0]
        set dirs [lrange $dirs 1 end]
        foreach f [glob -nocomplain [file join $d *] [file join $d .*]] {
            set t [file tail $f]
            if {![info exists skips($f)] && ![info exists skips($t)]} {
                if {[file isdirectory $f]} {
                    lappend dirs $f
                } elseif {[file isfile $f]} {
                    lappend result $f
                }
            }
        }
    }

    return $result
}

proc localPackingList {a {omit {}}} {
    upvar $a result

    foreach f [walker $omit] {
        set result($f) [packedItem $f]
    }
}

proc splitList {a data {omit {}}} {
    upvar $a result

    foreach line [split $data \n] {
        if {[regexp {^# Ignore: (.*)$} $line - omit]} continue
        if {[string match #* $line]} continue

        if {[regexp {^(\./[^ ]+) } $line - name]} {
            set result($name) $line
        }
    }

    return $omit
}

proc compareLists {a1 a2} {
    upvar $a1 from $a2 to

    set matches {}
    set additions {}
    set changes {}

    array set unseen [lsort [array get to]]

    foreach f [array names from] {
        if {[info exists to($f)]} {         # in both
            set fsz [lindex $from($f) 1]
            set tsz [lindex $to($f) 1]
            set fmd [lindex $from($f) 9]
            set tmd [lindex $to($f) 9]

            if {$fsz != $tsz} {             #   different size
                lappend changes $f
            } elseif {$fmd == $tmd || $fmd == "" || $tmd == ""} {
                # compare digest if available at both ends
                lappend matches $f
            } else {                        #   different
                lappend changes $f
            }
            unset unseen($f)
        } else {                            # in from, not in to
            lappend additions $f
        }
    }

    set deletions [lsort [array names unseen]]

    return [list $matches $additions $changes $deletions]
}

proc httpFetch {url {fd ""}} {
    if {$fd != ""} {
        set token [http::geturl $url -channel $fd]
    } else {
        set token [http::geturl $url]
    }

    upvar #0 $token state

    if {$state(status) != "ok"} { error $state(error) }
    if {[lindex $state(http) 1] != 200} { error $state(http) }

    return $state(body)
}

proc makeDirsForFTP {f} {
    set d {}
    foreach s [file split [file dirname $f]] {
        set d [file join $d $s]
        FTP::MkDir $d
    }
}

proc stowFileAway {fd f md5 odir} {
    global failures

    if {$odir == "" || [string length $md5] != 32} {
        return 0
    }

    regsub {^(..)} $md5 {\1/} newf
    set newf [file join $odir $newf]

    if {![FTP::Rename $f $newf]} {
        makeDirsForFTP $newf
        if {![FTP::Rename $f $newf]} {
            puts -nonewline { (NOT SAVED) }
            incr failures
            return 0
        }
    }

    puts -nonewline { (saved) }
    puts $fd "  $md5 -"
    return 1
}

proc sendFiles {site dir user pw odir} {
    global temp ignores failures
    set failures 0

    set fd [open $::PLIST]
    set myPack [read $fd]
    close $fd

    set hisPack ""
    set packSum ""
    if {[FTP::Get ./$::PLIST $temp/$::PTEMP]} {
        set fd [open $temp/$::PTEMP]
        set hisPack [read $fd]
        close $fd
        set packSum [calcDigest $temp/$::PTEMP]
    }

    set infoSum ""
    if {[FTP::Get ./$::PINFO $temp/$::PTEMP]} {
        set infoSum [calcDigest $temp/$::PTEMP]
    }

    file delete $temp/$::PTEMP

    array set here {}
    set ignores [splitList here $myPack $ignores]

    array set there {}
    set ignores [splitList there $hisPack $ignores]

    set diffs [compareLists here there]
    foreach {matches additions changes deletions} $diffs break

    set nm [llength $matches]
    set na [llength $additions]
    set nc [llength $changes]
    set nd [llength $deletions]
    set stats "$na additions, $nc changes, $nd deletions"

    puts stderr " $nm matches, $stats"

    set log [open $::PINFO a]
    puts $log "\nHTTPDIST - [clockDisplay] - $stats\n"

    if {$na + $nc + $nd > 0} {
        puts -nonewline "* ./$::PLIST "
        puts $log "* ./$::PLIST"
        stowFileAway $log ./$::PLIST $packSum $odir
        puts ""
        if {$packSum != ""} {
            puts $log "  [calcDigest $::PLIST] +"
        }

        foreach {v t} {additions a changes r deletions d} {
            foreach x [set $v] {
                set mods($x) $t
            }
        }

        foreach f [lsort [array names mods]] {
            puts -nonewline "$mods($f) $f "
            flush stdout
            puts $log "$mods($f) $f"

            switch $mods($f) {
                r {
                    stowFileAway $log $f [lindex $there($f) 9] $odir
                }
                d {
                    if {[stowFileAway $log $f [lindex $there($f) 9] $odir]} {
                        set mods($f) x ;# it was moved away, prevent deletion
                    }
                }
            }

            switch $mods($f) {
                a -
                r {
                    if {![FTP::Put $f]} {
                        makeDirsForFTP $f
                        if {![FTP::Put $f]} {
                            puts -nonewline { (PUT?) }
                            incr failures
                        }
                    }
                }
                d {
                    if {![FTP::Delete $f]} {
                        puts -nonewline { (DEL?) }
                        incr failures
                    }
                    #!! should clean up empty dirs ...
                }
            }
            puts ""
        }

        FTP::Put ./$::PLIST
    } elseif {$packSum != ""} {
        puts $log "* ./$::PLIST"
        puts $log "  [calcDigest $::PLIST] +"
    }

    puts -nonewline "* ./$::PINFO "
    puts $log "* ./$::PINFO"
    stowFileAway $log ./$::PINFO $infoSum $odir
    close $log

    FTP::Put ./$::PINFO
}

proc usage {} {
    puts stderr "  Usage: httpdist ?-proxy host? ?-dir path? command ?arg?

    @?url?      Fetch packing list and update in and below current dir.
                Looks for url in '$::PINFO' file if arg is just '@'.
                Prefixes with 'http://purl.org/' if arg is not an URL.
                WARNING: can alter (and delete) any files inside curr dir!

    pack ?...?  Scan current directory and create a '$::PLIST' file.
                Only file '$::PINFO' may be edited after this step.
                Any remaining args are used as filenames to ignore.

    send ftp://user?:pw?@site/dir ?archive?
                Send out changed files as specified in the packing list.
                Optional: archive old files out of the way to remote dir.
                Send log is added to '$::PINFO' before sending it last.
                Tip: use 'send <site>' to resend with its last settings.
"
if 0 {# not yet
    test x      Compare packing list against the current set of files.
                Values for x:   files   reports only files not listed
                                sums    only files listed and different
                                match   only files which are the same
                                all     all differences (default)
}
    exit 1
}

    # strip off command line options
array set opts {-dir . -proxy ""}

set skip 0
foreach {a b} $argv {
    if {![info exists opts($a)]} break
    set opts($a) $b
    incr skip 2
}
set argv [lrange $argv $skip end]

if {[llength $argv] < 1} usage

    # change into the distribution directory
    # this is very useful in combination with VFS automounting
catch {
    package require vfs
    vfs::auto $opts(-dir)
}
cd $opts(-dir)

set ignores "CVS .cvsignore core"

if {[catch {set env(TEMP)} temp] && [catch {set env(TMP)} temp]} {
    set temp .
}

switch -glob -- [lindex $argv 0] {
    @*
    {
            # don't update an *outgoing* distribution area without asking
        if {[file exists $::PSEND]} {
            puts -nonewline stderr "Found a '$::PSEND' file,\
                            do you really want to overwrite files here? "
            if {![string match {[yY]*} [gets stdin]]} {
                exit 1
            }
        }

        regsub {^@} $argv {} argv

            # when no url is specified, try to find one in $::PINFO
        if {$argv == ""} {
            if {![file exists $::PINFO]} {
                puts stderr "There is no '$::PINFO' file here."
                exit 1
            }
            set fd [open $::PINFO]
            while {[gets $fd line] >= 0} {
                if {[regexp {[Hh]ttpdist: ([^ ]+)} $line - argv]} break
            }
            close $fd

            if {$argv == ""} {
                puts stderr "No package distribution URL found in '$::PINFO'."
                exit 1
            }

            puts stderr "Fetching updates from $argv ..."
        }

            # expand possible shorthand using a Persistent URL
        regsub {^([^/:][^:]*[^/:])$} $argv {http://purl.org/&/} argv

        if {![regexp -nocase {^(http://.+/)(.*)} $argv - url file]} usage

        if {$file == ""} {
            set file $::PLIST
        }

		if {[catch {package require nhttp}]} {
        	package require http
        }

            # fetch/http "-proxy" setting is: <host>?:<port>?
        set o [split "$opts(-proxy):80" :]
        if {[llength $o] >= 2} {
            http::config -proxyhost [lindex $o 0] -proxyport [lindex $o 1]
        }

        if {[catch {httpFetch $url$file} hisPack]} {
            puts stderr "Cannot open packing list: $hisPack"
            exit 1
        }
        puts stderr ""

			# treat ./$::PINFO separately
        set fd [open $::PINFO w+]
        puts $fd [httpFetch $url$::PINFO]
        puts $fd "Httpdist: $argv - [clockDisplay]"

            # show the first three lines of the package info file
        seek $fd 0
        foreach x {1 2 3} {
            set s [gets $fd]
            if {$s == ""} break
            puts stderr "  [string range $s 0 77]"
        }
        close $fd
		
        array set there {}
        set ignores [splitList there $hisPack $ignores]

        array set here {}
        lappend ignores ./$::PINFO ./$::PLIST ./$::PSEND
        localPackingList here $ignores ;# uses ignore list from remote site

        set diffs [compareLists there here]
        foreach {matches additions changes deletions} $diffs break

        set nm [llength $matches]
        set na [llength $additions]
        set nc [llength $changes]
        set nd [llength $deletions]

        if {$na + $nc + $nd > 0} {
            puts stderr "\n$nm matches, $na additions, $nc changes, $nd deletions"
            puts -nonewline stderr "Apply these changes to [pwd] ? "

            if {[string match {[yY]*} [gets stdin]]} {
                puts stderr ""

		        foreach {v t} {additions a changes r deletions d} {
		            foreach x [set $v] {
		                set mods($x) $t
		            }
		        }
		
		        foreach f [lsort [array names mods]] {
		            puts -nonewline "$mods($f) $f "
		            flush stdout
		            
		            switch $mods($f) {
		                a -
		                r {
		                    set t $url$f
		                    regsub -all {/\./} $t {/} t
		
		                    file mkdir [file dirname $f]
		                    set fd [open $f w]
		
		                    httpFetch $t $fd
		                    
		                	set size [tell $fd]
		                    close $fd
		
		                	set want [lindex $there($f) 1]
		                    if {$size != $want} {
		                        puts -nonewline " (SIZE IS $size INSTEAD OF $want) "
		                    }
							
		                }
		                d {
		                    file delete $f
		                }
		            }
		            puts ""
		        }

	            set fd [open $::PLIST w]
	            puts -nonewline $fd $hisPack
	            close $fd
	        }
        }
    }

    pack
    {
        if {[llength $argv] > 1} { set ignores [lrange $argv 1 end] }

        set fd [open $::PLIST w]
        puts $fd "#-#httpsync $::PVERS Packing List for httpdist (with MD5)"
        puts $fd "# Ignore: [list $ignores]"
        puts $fd "# For details, see: http://www.equi4.com/httpdist/"

        set count 0
        set size 0

        lappend ignores ./$::PINFO ./$::PLIST ./$::PSEND
        foreach f [lsort [walker $ignores]] {
            set item [packedItem $f]
            puts $fd $item
            incr count
            incr size [expr {([lindex $item 1]+1023)/1024}]
        }
        close $fd

        puts " File '$::PLIST' created ($count files, total $size Kb)"
    }

    send
    {
        if {![file isfile $::PLIST]} {
            puts stderr "There is no '$::PLIST', you must create it first."
            exit 1
        }

        foreach {x url odir} $argv break
        if {[llength $argv] > 3 || $url == ""} usage

        set re {^ftp://([^:/@]+):?([^/@]*)?@([^/]+)/(.*)}
        if {![regexp $re $url - user pw site dir]} {
                # look for a site abbreviation
            set site ""
            if {![catch {open $::PSEND r} fd]} {
                while {[gets $fd line] >= 0} {
                    foreach {site user pw dir odir} $line break
                    if {$url == $site} break
                }
                close $fd
            }
            if {$url != $site} usage
        } else { # save settings for later
            set fd [open $::PSEND a]
            puts $fd [list $site $user $pw $dir $odir]
            close $fd
        }

		if {[catch {package require FTP}]} {
			package require ftp_lib
        }

        set FTP::VERBOSE 0
        set FTP::DEBUG 0

        if {$pw == ""} {
            puts -nonewline stderr "Password: "
            set pw [gets stdin]
        }

            # send/ftp "-proxy" setting is: (active|passive):<port>
        set o [split $opts(-proxy) :]
        if {[llength $o] < 2} {
            set o {active 21}
        }

        if {![FTP::Open $site $user $pw -mode [lindex $o 0] -port [lindex $o 1]]
                || ![FTP::Cd $dir]} {
            exit 1
        }

        proc FTP::DisplayMsg {args} {} ;# turn off all FTP error output

        sendFiles $site $dir $user $pw $odir

        FTP::Close

        if {$failures > 0} {
            puts stderr "\nThere were $failures errors."
            exit $failures
        }
    }

    test
    {
        if {![file isfile $::PLIST]} {
            puts stderr "There is no '$::PLIST' file here to verify."
            exit 1
        }

        puts stderr "Sorry, not yet implemented..."
        exit 1
    }

    default
        usage
}

exit
