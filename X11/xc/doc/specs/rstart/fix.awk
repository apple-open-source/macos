#! /bin/awk -f
# $Xorg: fix.awk,v 1.3 2000/08/17 19:42:50 cpqbld Exp $
#

BEGIN {
	ignore = 1;
}

# following line starts /^L/
//	{
	print;
	ignore = 1;
	next;
}

/^$/ {
	if(ignore) next;
}

{
	ignore = 0;
	print;
}
