#!/usr/bin/perl
#
# lsmgen.sh -- generate current LSM for fetchmail
#
$version = $ARGV[0];
$size = substr($ARGV[1], 0, 3);
$wwwhost = $ENV{'WWWVIRTUAL'};

@months
  = ('JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC');

my($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
$month = $months[$mon];

print <<EOF;
Begin3
Title:		fetchmail
Version:	$version
Entered-date:	${mday}${month}${year}
Description:	Full-featured IMAP/POP2/POP3/APOP/RPOP/KPOP client with GUI
		configuration, daemon mode, forwarding via SMTP or local MDA,
		superior reply handling, support for multidrop mailboxes. 
		Not a mail user agent, rather a	pipe-fitting that seamlessly
		forwards fetched mail to your local delivery system. 
		Your one-stop solution for intermittent email connections.
		Home page and FAQ at http://$wwwhost/~esr/fetchmail.
Keywords:	mail, client, POP, POP2, POP3, APOP, RPOP, KPOP, IMAP, SMTP
Author: 	esr\@snark.thyrsus.com (Eric S. Raymond)
Primary-site:	locke.ccil.org /pub/esr/fetchmail
		${size}K fetchmail-$version.tar.gz
Platforms:	All
Copying-policy:	GPL
End
EOF

