
source ./ctext.tcl
pack [ctext {.t blah}]

ctext::addHighlightClass {.t blah} c blue [list bat ball boot cat hat]
ctext::addHighlightClass {.t blah} c2 red [list bozo bull bongo]
{.t blah} highlight 1.0 end


