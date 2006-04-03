#!/bin/sh

SRCROOT=$1
DSTROOT=$2
INSTALLERDIR=$3

CFMGLUE=${SRCROOT}/KerberosFramework/KerberosCFMGlue/Binaries
PKGDIR=${INSTALLERDIR}/Kerberos.pkg

RESOURCES=${SRCROOT}/KerberosInstaller/Resources
INFO=${RESOURCES}/Kerberos.info

if [ -d "${PKGDIR}" ]; then rm -r "${PKGDIR}"; fi
/usr/bin/package "${DSTROOT}" "${INFO}" -d "${INSTALLERDIR}"

# Pieces of the CFM Support that won't make it through the paxing
/bin/cp -f "${CFMGLUE}/Kerberos._r"          "${PKGDIR}/Kerberos._r"
/bin/cp -f "${CFMGLUE}/Kerberos._type"       "${PKGDIR}/Kerberos._type"
/bin/cp -f "${CFMGLUE}/Kerberos._creator"    "${PKGDIR}/Kerberos._creator"
/bin/cp -f "${CFMGLUE}/Kerberos._attributes" "${PKGDIR}/Kerberos._attributes"

# Tools we will need to reassemble them.  Note: processor specific
/bin/cp -fp "/Developer/Tools/SetFile" "${PKGDIR}/SetFile.ppc"
/bin/cp -fp "/Developer/Tools/Rez"     "${PKGDIR}/Rez.ppc"
chmod 755 "${PKGDIR}/SetFile.ppc"
chmod 755 "${PKGDIR}/Rez.ppc"

# Check for behavioral differences between Puma and Jaguar tools
PKGRESOURCES=${PKGDIR}/Contents/Resources
if [ ! -d "${PKGRESOURCES}" ]
    then PKGRESOURCES=${PKGDIR}
fi

# Copy the resource scripts manually because Jaguar package command refuses the -r option:
if [ ! -f "${PKGRESOURCES}/preflight" ]
    then cp -f "${RESOURCES}/preflight" "${PKGRESOURCES}/preflight"
fi
chmod 755 "${PKGRESOURCES}/preflight"

if [ ! -f "${PKGRESOURCES}/postflight" ]
    then cp -f "${RESOURCES}/postflight" "${PKGRESOURCES}/postflight"
fi
chmod 755 "${PKGRESOURCES}/postflight"

# copy the banner message
if [ ! -d "${PKGRESOURCES}/English.lproj" ]
    then mkdir -p "${PKGRESOURCES}/English.lproj"
fi
if [ ! -f "${PKGRESOURCES}/English.lproj/Welcome.rtf" ]
    then cp -f "${RESOURCES}/English.lproj/Welcome.rtf" "${PKGRESOURCES}/English.lproj/Welcome.rtf"
fi
