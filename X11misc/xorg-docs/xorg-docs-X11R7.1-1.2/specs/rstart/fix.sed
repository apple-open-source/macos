#! /bin/sed -f
#
# $Xorg: fix.sed,v 1.3 2000/08/17 19:42:50 cpqbld Exp $
#
s/o+/./g
s/|-/+/g
s/.//g
/FORMFEED\[Page/{
s/FORMFEED/        /
a\

}
