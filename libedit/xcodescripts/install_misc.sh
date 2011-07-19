#!/bin/sh

set -ex

if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	DSTROOT="${DSTROOT}${SDKROOT}"

	install_name_tool -id /usr/lib/libedit.3.dylib ${DSTROOT}/usr/lib/libedit.3.dylib

	# Don't keep static libraries for the simulator SDK
	rm -rf ${DSTROOT}/usr/local/lib
fi

mkdir -p $DSTROOT/usr/lib/pkgconfig $DSTROOT/usr/local/lib
# -- libedit.la gets mastered out, so lets try not installing it at all and verify it has no impact
# on other projects building (it shouldn't)
# install -g $INSTALL_GROUP -o $INSTALL_OWNER $SRCROOT/local/libedit.la $DSTROOT/usr/local/lib
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m $ALTERNATE_MODE $SRCROOT/local/libedit.pc $DSTROOT/usr/lib/pkgconfig
for l in libedit.2.dylib libedit.3.0.dylib libedit.dylib libreadline.dylib ; do
	ln -s libedit.3.dylib $DSTROOT/usr/lib/$l
done

mkdir -p $DSTROOT/usr/include/readline $DSTROOT/usr/include/editline
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m $ALTERNATE_MODE $SRCROOT/src/histedit.h $DSTROOT/usr/include
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m $ALTERNATE_MODE $SRCROOT/src/editline/readline.h $DSTROOT/usr/include/editline
for h in readline.h history.h ; do
	ln -s ../editline/readline.h $DSTROOT/usr/include/readline/$h
done

if [ "${RC_ProjectName%_Sim}" == "${RC_ProjectName}" ] ; then
	mkdir -p $DSTROOT/usr/share/man/man{3,5}
	gzip -9 <$SRCROOT/doc/editrc.5 >$DSTROOT/usr/share/man/man5/editrc.5.gz 
	gzip -9 <$SRCROOT/doc/editline.3 >$DSTROOT/usr/share/man/man3/editline.3.gz
	for m in el_deletestr.3.gz el_end.3.gz el_get.3.gz el_getc.3.gz el_gets.3.gz el_history.3.gz el_history_end.3.gz el_history_init.3.gz el_init.3.gz el_insertstr.3.gz el_line.3.gz el_parse.3.gz el_push.3.gz el_reset.3.gz el_resize.3.gz el_set.3.gz el_source.3.gz el_tok_end.3.gz el_tok_init.3.gz el_tok_line.3.gz el_tok_reset.3.gz el_tok_str.3.gz ; do
		ln -s editline.3.gz $DSTROOT/usr/share/man/man3/$m
	done
fi

mkdir -p $DSTROOT/usr/local/OpenSource{Licenses,Versions}
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m a=r $SRCROOT/local/libedit.plist $DSTROOT/usr/local/OpenSourceVersions
/usr/bin/sed -e '1,2d' -e '/^$/,$d' $SRCROOT/src/el.c >$DSTROOT/usr/local/OpenSourceLicenses/libedit.txt
