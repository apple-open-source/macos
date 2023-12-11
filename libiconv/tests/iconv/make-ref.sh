# $FreeBSD$

# CP1131 was here, but not in GNU libiconv. 
ENCODING="ASCII ISO8859-1 ISO8859-2 ISO8859-3 ISO8859-4	ISO8859-5 ISO8859-6 \
	ISO8859-7 ISO8859-8 ISO8859-9 ISO8859-10 ISO8859-11 ISO8859-13 \
	ISO8859-14 ISO8859-15 ISO8859-16 ARMSCII-8 BIG5 BIG5-HKSCS \
	CP1251 CP866 CP949 GB18030 GB2312 GBK VISCII KOI8-R KOI8-U \
	PT154 SHIFT_JIS EUC-CN EUC-JP EUC-KR UTF-8-MAC DEC-KANJI"

REF_FWD="refgen/refgen"
REF_REV="refgen/refgen -r"

mkdir -p ref
for enc in ${ENCODING}; do
	echo "Checking ${enc} --> UTF-32 ..."
	echo '$FreeBSD$' > ref/${enc}
	${REF_FWD} ${enc} >> ref/${enc}
	echo "Checking UTF-32 --> ${enc} ..."
	echo '$FreeBSD$' > ref/${enc}-rev
	${REF_REV} ${enc} >> ref/${enc}-rev
done

