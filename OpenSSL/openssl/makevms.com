$!
$! MAKEVMS.COM
$! Original Author:  UNKNOWN
$! Rewritten By:  Robert Byer
$!                Vice-President
$!                A-Com Computing, Inc.
$!                byer@mail.all-net.net
$!
$! Changes by Richard Levitte <richard@levitte.org>
$!
$! This procedure creates the SSL libraries of "[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB"
$! "[.xxx.EXE.SSL]LIBSSL.OLB"
$! The "xxx" denotes the machine architecture of AXP or VAX.
$!
$! This procedures accepts two command line options listed below.
$!
$! Specify one of the following build options for P1.
$!
$!      ALL       Just build "everything".
$!      CONFIG    Just build the "[.CRYPTO]OPENSSLCONF.H" file.
$!      BUILDINF  Just build the "[.CRYPTO]BUILDINF.H" file.
$!      SOFTLINKS Just fix the Unix soft links.
