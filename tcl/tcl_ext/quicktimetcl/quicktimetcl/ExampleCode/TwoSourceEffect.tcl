
package require QuickTimeTcl
wm title . {Two Source Effect}

set ans [tk_messageBox -type yesno -message  \
  {Two still images of the same size are required. Are they ready?}]
if {$ans == "no"} {
    return
}

# The track id's are 1 and 2.
# Make two images in tk from two images on disk. 
# Preferrably of the same size as the video track, else they are scaled automatically.
set firstImage [image create photo -file [tk_getOpenFile -title {First Image}]]
set secondImage [image create photo -file [tk_getOpenFile -title {Second Image}]]

set width [image width $firstImage]
set height [image height $firstImage]

if {$tcl_platform(platform) == "windows"} {
    set effectFile [tk_getSaveFile -title {Effect File}  \
      -initialfile VideoEffect.mov -initialdir [file dirname [info script]]]
} else {
    set effectFile [tk_getSaveFile -title {Effect File}  \
      -initialfile VideoEffect.mov]
}
movie .m
.m new $effectFile
.m tracks new video $width $height
.m tracks new video $width $height
pack .m
update
.m tracks add picture 1 0 6000 $firstImage
.m tracks add picture 2 0 6000 $secondImage

# Shift the second track to make the desired overlap.
.m tracks configure 2 -offset 3000

# Pick the wanted effect in the dialog. Choose the overlap for the duration of
.m effect 3000 3000 1 2
tkwait variable quicktimetcl::effectfinished
.m save

#update
#return

set ans [.m tracks new text]
set textTrackID [lindex $ans 1]
.m tracks add text $textTrackID 0 3000 {Two source video effect. Here it comes...} \
  -scrollin 1 -scrollout 1 -font {Times 24 italic}
.m tracks add text $textTrackID 6000 3000 {It is best if images are of same size.} \
  -scrollin 1 -scrollout 1 -scrollreverse 1 -font {Times 24 italic}
.m tracks configure $textTrackID -graphicsmode transparent -graphicsmodecolor black
.m save

