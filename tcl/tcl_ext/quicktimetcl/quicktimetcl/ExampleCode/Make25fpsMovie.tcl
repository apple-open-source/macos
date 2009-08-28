package require QuickTimeTcl 3.0

set dir [tk_chooseDirectory -title "Pick dir with TIFFs" -mustexist 1]
set lfile [ glob -type f [file join $dir *.tiff] ]
foreach f $lfile {
    lappend imageList [image create photo -file $f]
}

movie .m
.m new zzz.mov
.m tracks new video 360 288
pack .m
update
set frameDuration [expr 600 / 25]
.m tracks add picture 1 0 $frameDuration $imageList -dialog 0 -keyframerate 25
.m save
