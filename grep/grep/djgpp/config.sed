s|po2tbl\.sed\.in|po2tblsed.in|g

/ac_given_INSTALL=/,/^CEOF/ {
  /^s%@l@%/a\
  s,po2tbl\\.sed\\.in,po2tblsed.in,g\
  s,Makefile\\.in\\.in,Makefile.in-in,g
}

/^CONFIG_FILES=/,/^EOF/ {
  s|po/Makefile.in|po/Makefile.in:po/Makefile.in-in|
}
