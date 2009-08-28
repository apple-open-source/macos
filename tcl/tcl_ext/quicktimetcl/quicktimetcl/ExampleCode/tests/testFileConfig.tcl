package require QuickTimeTcl

set fn1 [tk_getOpenFile]
set fn2 [tk_getOpenFile]
pack [button .bt -text {Hit Me} -command   \
  {.top.m configure -file $fn2; .top.m play}]
toplevel .top
pack [movie .top.m -file $fn1]
