
# From using ResEdit on the file "HD:System Folder:Extensions:QuickTime Extensions:
# QuickTime Channels

# http://stream.qtv.apple.com/favorites/cnn/cnn_fav_ref.mov
# http://www.apple.com/quicktime/favorites/disney1/disney1.mov
# http://www.apple.com/quicktime/favorites/warner1/warner1.mov
# http://www.apple.com/quicktime/favorites/npr1/npr1.mov
# http://www.apple.com/quicktime/favorites/fox2/fox2.mov
# http://www.apple.com/quicktime/favorites/bloomberg1/bloomberg1.mov
# http://www.apple.com/quicktime/favorites/bbc_world1/bbc_world1.mov
# http://www.apple.com/quicktime/favorites/abc1/abc1.mov
# http://stream.qtv.apple.com/favorites/mtv/mtv_fav_ref.mov
# http://stream.qtv.apple.com/favorites/nickelodeon/nick_fav_ref.mov
# http://www.apple.com/quicktime/favorites/fox1/fox1.mov
# http://www.apple.com/quicktime/favorites/espn1/espn1.mov
# http://www.apple.com/quicktime/favorites/hbo1/hbo1.mov
# http://www.apple.com/quicktime/favorites/wgbh1/wgbh1.mov
# http://www.apple.com/quicktime/favorites/vh1/vh1.mov
# http://www.apple.com/quicktime/favorites/rollingstone1/rollingstone1.mov
# http://www.apple.com/quicktime/favorites/weatherchannel1/weatherchannel1.mov
# http://stream.qtv.apple.com/favorites/sony/sony_fav_ref.mov
# http://stream.qtv.apple.com/favorites/zdtv/zdtv_fav_ref.mov
# http://www.apple.com/quicktime/showcase/qtshowcase.mov
# http://www.apple.com/quicktime/resources/qtshowcase.mov
# http://www.apple.com/quicktime/favorites/movietrailers1/trailers.mov
# http://www.apple.com/quicktime/favorites/cnn1/cnn1.mov
# http://stream.qtv.apple.com/favorites/cnn/cnn_fav_ref.mov
# http://stream.qtv.apple.com/favorites/sony/sony_fav_ref.mov
# http://stream.qtv.apple.com/favorites/zdtv/zdtv_fav_ref.mov

package require QuickTimeTcl
wm title . {BBC Live}
movie .m -url \
  "http://www.apple.com/quicktime/favorites/bbc_world1/bbc_world1.mov"
pack .m
