if [ -x PREFIX/bin/xset ] ; then
        fontpath="PREFIX/lib/X11/fonts/misc/,PREFIX/lib/X11/fonts/TTF/,PREFIX/lib/X11/fonts/OTF,PREFIX/lib/X11/fonts/Type1/,PREFIX/lib/X11/fonts/75dpi/:unscaled,PREFIX/lib/X11/fonts/100dpi/:unscaled,PREFIX/lib/X11/fonts/75dpi/,PREFIX/lib/X11/fonts/100dpi/"

        [ -e "$HOME"/.fonts/fonts.dir ] && fontpath="$fontpath,$HOME/.fonts"
        [ -e "$HOME"/Library/Fonts/fonts.dir ] && fontpath="$fontpath,$HOME/Library/Fonts"
        [ -e /Library/Fonts/fonts.dir ] && fontpath="$fontpath,/Library/Fonts"
        [ -e /System/Library/Fonts/fonts.dir ] && fontpath="$fontpath,/System/Library/Fonts"

        PREFIX/bin/xset fp= "$fontpath"
        unset fontpath
fi
