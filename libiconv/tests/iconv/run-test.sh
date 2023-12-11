#!/bin/sh

# This is a sh(1) rewrite of the Makefile "check" rule, written to make
# it possible to run tests without having to install bmake(1).

# CP1131 wasn't supported by our GNU libiconv.
ENCODING="UTF-8-MAC ASCII ISO8859-1 ISO8859-2 ISO8859-3 ISO8859-4 \
	ISO8859-5 ISO8859-6 \
	ISO8859-7 ISO8859-8 ISO8859-9 ISO8859-10 ISO8859-11 ISO8859-13 \
	ISO8859-14 ISO8859-15 ISO8859-16 ARMSCII-8 BIG5 BIG5-HKSCS \
	CP1251 CP866 CP949 GB18030 GB2312 GBK VISCII KOI8-R KOI8-U \
	PT154 SHIFT_JIS EUC-CN EUC-JP EUC-KR"

GEN_FWD="tablegen/tablegen --gnu"
GEN_REV="tablegen/tablegen --gnu -r"
CMP="tablegen/cmp.sh"

mkdir -p output
for enc in ${ENCODING}; do
	echo "Checking ${enc} --> UTF-32 ..."
	${GEN_FWD} ${enc} > output/${enc}
	${CMP} ref/${enc} output/${enc}
	echo "Checking UTF-32 --> ${enc} ..."
	${GEN_REV} ${enc} > output/${enc}-rev
	${CMP} ref/${enc}-rev output/${enc}-rev
done
