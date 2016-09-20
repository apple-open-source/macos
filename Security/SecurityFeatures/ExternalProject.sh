#!/bin/sh
#
#  ExternalProject.sh
#

case $1 in
   clean)
       rm -f "${BUILT_PRODUCTS_DIR}"/include/Security/SecurityFeatures.h
       ;;
   *)
       ;;
esac

exit 0
