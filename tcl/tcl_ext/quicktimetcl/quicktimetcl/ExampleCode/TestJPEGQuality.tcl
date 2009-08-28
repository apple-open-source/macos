
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
.m tracks add picture 1 0 600 $firstImage -compressor jpeg -spatialquality max
.m tracks add picture 1 600 600 $firstImage -compressor jpeg -spatialquality high
.m tracks add picture 1 1200 600 $firstImage -compressor jpeg -spatialquality normal
.m tracks add picture 1 1800 600 $firstImage -compressor jpeg -spatialquality low
.m tracks add picture 1 2400 600 $firstImage -compressor jpeg -spatialquality min
.m tracks add picture 1 3000 600 $firstImage -compressor jpeg -colordepth 40 -spatialquality max
.m tracks add picture 1 3600 600 $firstImage -compressor jpeg -colordepth 40 -spatialquality high
.m tracks add picture 1 4200 600 $firstImage -compressor jpeg -colordepth 40 -spatialquality normal
.m tracks add picture 1 4800 600 $firstImage -compressor jpeg -colordepth 40 -spatialquality low
.m tracks add picture 1 5400 600 $firstImage -compressor jpeg -colordepth 40 -spatialquality min
.m tracks add picture 1 6000 600 $firstImage -compressor jpeg -colordepth 16 -spatialquality normal
.m tracks add picture 1 6600 600 $firstImage -compressor jpeg -colordepth 8 -spatialquality normal
.m tracks add picture 1 7200 600 $firstImage -compressor jpeg -colordepth 4 -spatialquality normal
.m tracks add picture 1 7800 600 $firstImage -compressor jpeg -colordepth 2 -spatialquality normal
.m save
