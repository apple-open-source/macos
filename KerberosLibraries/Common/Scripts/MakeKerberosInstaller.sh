#!/bin/sh

SRCROOT=$1
DSTROOT=$2
INSTALLERDIR=$3

NAME=Kerberos

# source directories
INSTALLFILES="${DSTROOT}"
RESOURCES="${SRCROOT}/Common/Resources"
INFOFILE="${RESOURCES}/${NAME}.info"
WELCOMEFILE="${RESOURCES}/English.lproj/Welcome.rtf"
PREFLIGHT="${RESOURCES}/preflight"
POSTFLIGHT="${RESOURCES}/postflight"

# destination directories
PKGDIR="${INSTALLERDIR}/${NAME}.pkg"
PKGINFO="${PKGDIR}/Contents/PkgInfo"
PKGRESOURCES="${PKGDIR}/Contents/Resources"
PKGBOM="${PKGRESOURCES}/${NAME}.bom"
PKGSIZES="${PKGRESOURCES}/${NAME}.sizes"
PKGTIFF="${PKGRESOURCES}/${NAME}.tiff"
PKGARCHIVE="${PKGRESOURCES}/${NAME}.pax.gz"
PKGINFOFILE="${PKGRESOURCES}/${NAME}.info"
PKGPREFLIGHT="${PKGRESOURCES}/preflight"
PKGPOSTFLIGHT="${PKGRESOURCES}/postflight"
PKGLPROJ="${PKGRESOURCES}/English.lproj"
PKGWELCOMEFILE="${PKGLPROJ}/Welcome.rtf"

echo "Generating Installer package \"${PKGDIR}\" ..."

# delete old package, if present
if [ -d "${PKGDIR}" ]; then rm -rf "${PKGDIR}"; fi

# create package directory
/bin/mkdir -p -m 755 "${PKGLPROJ}"

# create package info file, creator and type info
echo "  adding package info file ... "
echo pmkrpkg1 > "${PKGINFO}"
chmod 444 "${PKGINFO}"

# (gnu)tar/pax and compress root directory to package archive
echo -n "       creating package archive ... "
(cd "${INSTALLFILES}"; /bin/pax -w -z -x cpio .) > "${PKGARCHIVE}"
/bin/chmod 444 "${PKGARCHIVE}"
echo "done."

# copy resources to package
if [ -f "${INFOFILE}" ]; then
    echo -n "       copying `basename ${INFOFILE}` ... "
    /bin/cp "${INFOFILE}" "${PKGINFOFILE}"
    /bin/chmod 444 "${PKGINFOFILE}"
    echo "done."
fi

if [ -f "${WELCOMEFILE}" ]; then
    echo -n "       copying `basename ${WELCOMEFILE}` ... "
    /bin/cp "${WELCOMEFILE}" "${PKGWELCOMEFILE}"
    /bin/chmod 444 "${PKGWELCOMEFILE}"
    echo "done."
fi

if [ -f "${PREFLIGHT}" ]; then
    echo -n "       copying `basename ${PREFLIGHT}` ... "
    /bin/cp "${PREFLIGHT}" "${PKGPREFLIGHT}"
    /bin/chmod 555 "${PKGPREFLIGHT}"
    echo "done."
fi

if [ -f "${POSTFLIGHT}" ]; then
    echo -n "       copying `basename ${POSTFLIGHT}` ... "
    /bin/cp "${POSTFLIGHT}" "${PKGPOSTFLIGHT}"
    /bin/chmod 555 "${PKGPOSTFLIGHT}"
    echo "done."
fi

# generate bom file
echo -n "       generating bom file ... "
/usr/bin/mkbom "${INSTALLFILES}" "${PKGBOM}" >& /dev/null
/bin/chmod 444 "${PKGBOM}"
echo done.

# generate sizes file
echo -n "       generating sizes file ... "

# compute number of files in package
NUMFILES=`/usr/bin/lsbom -s "${PKGBOM}" | /usr/bin/wc -l`

# compute package size when compressed
COMPRESSEDSIZE=`/usr/bin/du -k -s "${PKGRESOURCES}" | /usr/bin/awk '{print $1}'`
COMPRESSEDSIZE=$((${COMPRESSEDSIZE} + 3))  # add 1KB each for sizes, location, status files

INFOSIZE=`/bin/ls -s "${PKGINFO}" | /usr/bin/awk '{print $1}'`
BOMSIZE=`/bin/ls -s "${PKGBOM}" | /usr/bin/awk '{print $1}'`
if [ -f "${PKGTIFF}" ]; then
    TIFFSIZE=`/bin/ls -s "${PKGTIFF}" | /usr/bin/awk '{print $1}'`
else
    TIFFSIZE=0
fi 

INSTALLEDSIZE=`/usr/bin/du -k -s "${INSTALLFILES}" | /usr/bin/awk '{print $1}'`
INSTALLEDSIZE=$((${INSTALLEDSIZE} + ${INFOSIZE} + ${BOMSIZE} + ${TIFFSIZE} + 3))

# echo size parameters to sizes file
echo NumFiles ${NUMFILES}             >  "${PKGSIZES}"
echo InstalledSize ${INSTALLEDSIZE}   >> "${PKGSIZES}"
echo CompressedSize ${COMPRESSEDSIZE} >> "${PKGSIZES}"
/bin/chmod 444 "${PKGSIZES}"
echo "done."

echo "... package ${PKGDIR} created."

exit 0
