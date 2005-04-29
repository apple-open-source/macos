#!/bin/sh
# the next line restarts using wish \
exec wish8.3 "$0" "$@"

package require -exact snack 2.2

snack::sound s

set v(0) 1
set v(1) 0
set v(2) 0
set v(3) 1
set f [snack::filter map $v(0) $v(1) $v(2) $v(3)]

pack [label .l -text "In (L,R)"] -anchor e
pack [frame .f]
pack [label .f.l -text Out] -side left
pack [frame .f.f] -side left
pack [frame .f.f.t]
pack [frame .f.f.b]
pack [label .f.f.t.l -text L] -side left
pack [checkbutton .f.f.t.a -var v(0) -command Config] -side left
pack [checkbutton .f.f.t.b -var v(1) -command Config] -side left
pack [label .f.f.b.l -text R] -side left
pack [checkbutton .f.f.b.a -var v(2) -command Config] -side left
pack [checkbutton .f.f.b.b -var v(3) -command Config] -side left
pack [frame .fb]
snack::createIcons
pack [button .fb.a -image snackOpen -command Load] -side left
pack [button .fb.b -bitmap snackPlay -command Play] -side left
pack [button .fb.c -bitmap snackStop -command "s stop"] -side left

proc Config {} {
    global f v
    $f configure $v(0) $v(1) $v(2) $v(3)
}

proc Play {} {
    global f
    s stop
    s play -devicechannels 2 -filter $f
}

proc Load {} {
 set file [snack::getOpenFile -initialdir [file dirname [s cget -file]]]
 if {$file == ""} return
 s config -file $file
}
