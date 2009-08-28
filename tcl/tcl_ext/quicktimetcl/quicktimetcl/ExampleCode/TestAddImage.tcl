
package require QuickTimeTcl
set firstImage [image create photo -file [tk_getOpenFile -title {Pick Image}]]
set width [image width $firstImage]
set height [image height $firstImage]

set theFile [tk_getSaveFile -title {Save File}  \
  -initialfile slask.mov]
movie .m
.m new $theFile
.m tracks new video $width $height
pack .m
update
.m tracks add picture 1 0 600 $firstImage -compressor jpeg -dialog 0 \
  -colordepth 0
.m save
