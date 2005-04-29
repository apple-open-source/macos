# pooltest.tcl

source [file join [file dirname [info script]] pool.tcl]
namespace import pool::*
pool CarPool

CarPool add Toyota Volkswagen Chrysler Trabant

CarPool request item -prefer Trabant -allocID me


proc poolinfo {} {
    puts "Current pool size: [CarPool info cursize]"
    puts "Maximum pool size: [CarPool info maxsize]"
    puts "Free items: [CarPool info freeitems]"
    if { [CarPool info cursize] > 0 } {
        set sep_line [string repeat - 40]
        puts "Allocation info:\
                \nnr.    item   allocID (-1 = free)"
        puts $sep_line
        set i 0
        foreach {item state} [CarPool info allocstate] {
            puts "[incr i]     $item        $state"
        }
        puts $sep_line
    }
    return
}
poolinfo
set failedtests {}

# Exercise all error cases

proc MatchErrMsg {errid errmsg} {
    global failedtests
    
    set pattern [format $::pool::Errors($errid) * *]
    if { ![string match $pattern $errmsg] } {
        puts "$errid: failed \
                \nPattern: $pattern \
                \nError message: $errmsg"
        lappend failedtests $errid
    } else  {
        puts "$errid: passed"
    }
}

proc VARNAME_EXISTS {} {
    set ::pool::existvar 1
    catch {pool::create existvar} errmsg
    MatchErrMsg [info level 0] $errmsg
    unset ::pool::existvar
}

proc DUPLICATE_POOLNAME {} {
    catch {pool::create CarPool} errmsg
    MatchErrMsg [info level 0] $errmsg
}

proc NONINT_REQSIZE {} {
    catch {pool::create CarPool noninteger} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool maxsize noninteger} errmsg
    MatchErrMsg [info level 0] $errmsg
}

proc UNKNOWN_POOL {} {
    catch {pool::destroy NonExistentPool} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc BAD_SUBCMD {} {
    catch {CarPool badsubcommand whateverargs} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc SOME_ITEMS_NOT_FREE {} {
    catch {CarPool clear} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool destroy} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc DUPLICATE_ITEM_IN_ARGS {} {
    catch {CarPool add Toyota duplicatecar someothercar somestrangecar duplicatecar} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc FORBIDDEN_ALLOCID {} {
    catch {CarPool request car -allocID -1} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc ITEM_ALREADY_IN_POOL {} {
    catch {CarPool add Toyota} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc ITEM_STILL_ALLOCATED {} {
    catch {CarPool remove Trabant} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc ITEM_NOT_ALLOCATED {} {
    catch {CarPool release Toyota} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc ITEM_NOT_IN_POOL {} {
    catch {CarPool info allocID Buggy} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool request item -prefer Buggy} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool release Buggy} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool remove Buggy} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc EXCEED_MAXSIZE {} {
    catch {CarPool add  1 2 3 4 5 6 7} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc INVALID_POOLSIZE {} {
    catch {CarPool maxsize [expr {[CarPool info cursize] - 1}] } errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc WRONG_INFO_TYPE {} {
    catch {CarPool info wronginfotype} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc UNKNOWN_ARG {} {
    catch {CarPool clear unknownarg} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool request item Toyota unknownarg} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool destroy unknownarg} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool remove Toyota unknownarg} errmsg
    MatchErrMsg [info level 0] $errmsg
}


proc WRONG_NARGS {} {
    catch {CarPool add} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool info cursize oneargtoomany} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool info allocID} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool info allocID Trabant oneargtoomany} errmsg
    MatchErrMsg [info level 0] $errmsg
    
    catch {CarPool request item Toyota -prefer me} errmsg
    MatchErrMsg [info level 0] $errmsg
}


puts "TESTING ERROR CASES:\n"

foreach errid [array names pool::Errors] {
    if { [llength [::info procs $errid]] } {
        eval $errid
    }
}

puts {}
if { [llength $failedtests] } {
    puts "The following tests failed:"
    foreach errid $failedtests {
        puts $errid
    }
} else  {
    puts "All tests passed."
}

# EOF pooltest.tcl
