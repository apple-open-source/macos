proc P { str } {
    catch {puts $str ; flush stdout}
}

proc PN { str } {
    catch {puts -nonewline $str ; flush stdout}
}

proc PP { str } {
    P ""
    P $str
    P ""
}

proc PS { } {
    P ""
    P "------------------------------------------------------------"
    P ""
}

proc PH { str } {
    P ""
    P "Test: $str"
    PS
}

proc PF { floatVal { prec 4 } } {
    set fmtStr "%.${prec}f"
    return [format $fmtStr $floatVal]
}

proc PSec { msg sec } {
    P [format "%s: %.4f seconds" $msg $sec]
}

proc PrintMachineInfo {} {
    global tcl_platform

    P "Machine specific information:"
    P  "platform    : $tcl_platform(platform)"
    P  "os          : $tcl_platform(os)"
    P  "osVersion   : $tcl_platform(osVersion)"
    P  "machine     : $tcl_platform(machine)"
    P  "byteOrder   : $tcl_platform(byteOrder)"
    P  "wordSize    : $tcl_platform(wordSize)"
    P  "user        : $tcl_platform(user)"
    P  "hostname    : [info hostname]"
    P  "Tcl version : [info patchlevel]"
    P  "Visuals     : [winfo visualsavailable .]"
}

proc SetFileTypes { } {
    global fInfo env

    set fInfo(suf) ".tga"
    set fInfo(fmt) "targa"
    set fInfo(vsn) "int"
    set fInfo(modfmt) 0
}
