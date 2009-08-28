
package require QuickTimeTcl
wm title . {Count Up}
movie .m
.m new CountUp.mov
.m tracks new text 100 100
pack .m
update
.m tracks add text 1 0 600 One
.m tracks add text 1 600 600 Two
.m tracks add text 1 1200 600 Three
.m tracks add text 1 1800 600 Four
.m tracks add text 1 2400 600 Five
.m tracks add text 1 3000 600 Six
.m tracks add text 1 3600 600 Seven
.m tracks add text 1 4200 600 Eight
.m tracks add text 1 4800 600 Nine
.m tracks add text 1 5400 600 Ten
update
.m save
