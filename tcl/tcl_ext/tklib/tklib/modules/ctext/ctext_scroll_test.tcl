
source ./ctext.tcl

scrollbar .y -orient vertical -command {.t yview}
ctext .t -xscrollcommand {.x set} -yscrollcommand {.y set} -wrap none
scrollbar .x -orient horizontal -command {.t xview}


grid .y -sticky ns
grid .t -row 0 -column 1
grid .x -column 1 -sticky we
