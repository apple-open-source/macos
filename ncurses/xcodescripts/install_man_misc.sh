#!/bin/sh

# check if we're building for the simulator
[ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] && DSTROOT="${DSTROOT}${SDKROOT}"

mkdir -p "$DSTROOT"/usr/local/OpenSource{Licenses,Versions}
install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$ALTERNATE_MODE" ncurses.txt "$DSTROOT"/usr/local/OpenSourceLicenses
install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$ALTERNATE_MODE" ncurses.plist "$DSTROOT"/usr/local/OpenSourceVersions

[ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] && exit 0

mkdir -p "$DSTROOT"/usr/share/man/man{1,3,5,7} "$DSTROOT"/usr/share/tabset 

install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$ALTERNATE_MODE" ncurses5.4-config.1 "$DSTROOT"/usr/share/man/man1

install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$INSTALL_MODE_FLAG" ncurses/misc/tabset/* "$DSTROOT"/usr/share/tabset
cd ncurses/man
sh MKterminfo.sh terminfo.head ../include/Caps terminfo.tail >terminfo.5
for m in *.[1357]? *.[1357]; do
	# x=${m:e}
	x=$(echo $m | sed -E 's,^.*\.([^.]*),\1,')
	# section=$x[1,1]
	section=$(echo $x | cut -c1-1)
	dst="$DSTROOT"/usr/share/man/man$section/$m.gz
	echo $dst
	sed -f fix_man.sed <"$m" | gzip -9 >"$dst"
	for l in $(sed -f fix_man.sed <"$m" | sed -f manlinks.sed | sort -u); do
		echo \ \ -\> $l
		ldst="$DSTROOT/usr/share/man/man$section/$l.$x.gz"
		[[ ! -a "$ldst" ]] && ln "$dst" "$ldst"
	done
done

ln "$DSTROOT/usr/share/man/man3/curs_termcap.3x.gz" "$DSTROOT/usr/share/man/man3/termcap.3.gz"

/Developer/Makefiles/bin/compress-man-pages.pl -d $DSTROOT/usr/share/man/ man1 man2 man3 man4 man5 man6 man7 man8 man9

exit 0
