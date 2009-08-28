
package require QuickTimeTcl

set ans [tk_messageBox -type yesno -icon info -message  \
  {Open as many images you like, press cancel when you have opened\
  the last one. Proceed?}]
if {$ans == "no"} {
    return
}
set theFile [tk_getOpenFile -title {Pick Image}]
if {$theFile == ""} {
    return
}
set imageList [image create photo -file $theFile]
set width [image width $imageList]
set height [image height $imageList]
while {$theFile != ""} {
    set theFile [tk_getOpenFile -title {Pick Image}]
    if {$theFile != ""} {
	set newImage [image create photo -file $theFile]
	lappend imageList $newImage
    }
}
set theFile [tk_getSaveFile -title {Save File}  \
  -initialfile sequence.mov]
movie .m
.m new $theFile
.m tracks new video $width $height
pack .m
update
.m tracks add picture 1 0 300 $imageList -dialog 1 -compressor {smc } -colordepth 8
#.m tracks add picture 1 0 300 $imageList -dialog 1 -compressor jpeg
.m save
