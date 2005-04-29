set f [open tiff.def1]
set g [open libtiff.def2 w]

while {![eof $f]} {
    set line [gets $f]
    puts $g "\t[lindex $line 2]"
}

close $g
close $f