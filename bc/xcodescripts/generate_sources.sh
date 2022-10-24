#!/bin/sh

cd ${SRCROOT}/bc
sh gen/strgen.sh gen/lib.bc ${BUILT_PRODUCTS_DIR}/lib.c bc_lib bc_lib_name 1 1
sh gen/strgen.sh gen/lib2.bc ${BUILT_PRODUCTS_DIR}/lib2.c bc_lib2 bc_lib2_name 1 1
sh gen/strgen.sh gen/bc_help.txt ${BUILT_PRODUCTS_DIR}/bc_help.c bc_help
sh gen/strgen.sh gen/dc_help.txt ${BUILT_PRODUCTS_DIR}/dc_help.c dc_help
