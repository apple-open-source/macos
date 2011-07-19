package provide sdx 1.0

namespace eval sdx {
    namespace export sdx
    proc sdx args {
        set s [interp create [namespace current]::s]
        $s eval {
            proc exit {{returnCode 0}} {
                if {$returnCode} {
                    error "sdx slave interpreter exited abnormally: 
$returnCode"
                } else {
                    return $returnCode
                }
            }
        }
        $s eval set argv [list $args]
        $s eval set argc [llength $args]
        $s eval set argv0 [list $::argv0]
        $s eval set auto_path [list $::auto_path]
        set error [catch {$s eval package require app-sdx} return]
        if { $error } { set code error } { set code ok }
        interp delete $s
        return -code $code $return
    }
}
