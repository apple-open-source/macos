# miscellaneous man pages

.PATH: ${.CURDIR}/man

.if ${LIB} == "c"

MAN3 += assert.3 bitstring.3 stdarg.3
MAN7 += environ.7

MLINKS += bitstring.3 bit_alloc.3
MLINKS += bitstring.3 bit_clear.3
MLINKS += bitstring.3 bit_decl.3
MLINKS += bitstring.3 bit_ffs.3
MLINKS += bitstring.3 bit_nclear.3
MLINKS += bitstring.3 bit_nset.3
MLINKS += bitstring.3 bit_set.3
MLINKS += bitstring.3 bit_test.3
MLINKS += bitstring.3 bitstr_size.3

.endif
