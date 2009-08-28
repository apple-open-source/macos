#! /bin/nawk -f
# $Xorg: fix.nawk,v 1.3 2000/08/17 19:42:50 cpqbld Exp $
#

BEGIN {
	ignore = 1;
}

/FORMFEED\[Page/	{
	sub("FORMFEED", "        ");
	print;
	print "";
	ignore = 1;
	next;
}

$0 == "" {
	if(ignore) next;
}

{
	ignore = 0;
	print;
}
