
package require QuickTimeTcl
set myFile [tk_getOpenFile]
if {$myFile != ""} {
    
    toplevel .top
    wm overrideredirect .top 1
    wm geometry .top +100+100
    wm geometry . +400+200
    movie .top.m -file $myFile -controller 0
    pack .top.m
    console hide
    update
    .top.m play
}
