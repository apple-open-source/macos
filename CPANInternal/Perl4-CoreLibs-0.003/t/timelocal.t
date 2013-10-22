use warnings;
use strict;

use Config;
use Test::More tests => 135;

require_ok "timelocal.pl";

foreach(
	#year,mon,day,hour,min,sec
	[1950,  4, 12,  9, 30, 31],
	[1969, 12, 31, 16, 59, 59],
	[1970,  1,  2, 00, 00, 00],
	[1980,  2, 28, 12, 00, 00],
	[1980,  2, 29, 12, 00, 00],
	[1999, 12, 31, 23, 59, 59],
	[2000,  1,  1, 00, 00, 00],
	[2010, 10, 12, 14, 13, 12],
	[2020,  2, 29, 12, 59, 59],
	[2030,  7,  4, 17, 07, 06],
) {
	my($year, $mon, $mday, $hour, $min, $sec) = @$_;
	$year -= 1900;
	$mon--;

	# Test timelocal()
	{
		my $year_in = $year < 70 ? $year + 1900 : $year;
		my $time = &timelocal($sec,$min,$hour,$mday,$mon,$year_in);
		my($s,$m,$h,$D,$M,$Y) = localtime($time);
		is $s, $sec, "timelocal second for @$_";
		is $m, $min, "timelocal minute for @$_";
		is $h, $hour, "timelocal hour for @$_";
		is $D, $mday, "timelocal day for @$_";
		is $M, $mon, "timelocal month for @$_";
		is $Y, $year, "timelocal year for @$_";
	}

	# Test timegm()
	{
		my $year_in = $year < 70 ? $year + 1900 : $year;
		my $time = &timegm($sec,$min,$hour,$mday,$mon,$year_in);
		my($s,$m,$h,$D,$M,$Y) = gmtime($time);
		is $s, $sec, "timegm second for @$_";
		is $m, $min, "timegm minute for @$_";
		is $h, $hour, "timegm hour for @$_";
		is $D, $mday, "timegm day for @$_";
		is $M, $mon, "timegm month for @$_";
		is $Y, $year, "timegm year for @$_";
	}
}


foreach(
	# month too large
	[1995, 13, 01, 01, 01, 01],
	# day too large
	[1995, 02, 30, 01, 01, 01],
	# hour too large
	[1995, 02, 10, 25, 01, 01],
	# minute too large
	[1995, 02, 10, 01, 60, 01],
	# second too large
	[1995, 02, 10, 01, 01, 60],
) {
	my($year, $mon, $mday, $hour, $min, $sec) = @$_;
	$year -= 1900;
	$mon--;
	eval { &timegm($sec,$min,$hour,$mday,$mon,$year) };
	like $@, qr/.*out of range.*/, 'invalid time caused an error';
}

is &timelocal(0,0,1,1,0,90) - &timelocal(0,0,0,1,0,90), 3600,
	'one hour difference between two calls to timelocal';

is &timelocal(1,2,3,1,0,100) - &timelocal(1,2,3,31,11,99), 24 * 3600,
	'one day difference between two calls to timelocal';

# Diff beween Jan 1, 1980 and Mar 1, 1980 = (31 + 29 = 60 days)
is &timegm(0,0,0, 1, 2, 80) - &timegm(0,0,0, 1, 0, 80), 60 * 24 * 3600,
	'60 day difference between two calls to timegm';

# bugid #19393
# At a DST transition, the clock skips forward, eg from 01:59:59 to
# 03:00:00. In this case, 02:00:00 is an invalid time, and should be
# treated like 03:00:00 rather than 01:00:00 - negative zone offsets used
# to do the latter
{
	my $hour = (localtime(&timelocal(0, 0, 2, 7, 3, 102)))[2];
	# testers in US/Pacific should get 3,
	# other testers should get 2
	ok $hour == 2 || $hour == 3, 'hour should be 2 or 3';
}

eval { &timegm(0,0,0,29,1,1900) };
like $@, qr/Day '29' out of range 1\.\.28/, 'does not accept leap day in 1900';

eval { &timegm(0,0,0,29,1,0) };
is $@, '', 'no error with leap day of 2000 (year passed as 0)';

eval { &timegm(0,0,0,29,1,1904) };
is $@, '', 'no error with leap day of 1904';

eval { &timegm(0,0,0,29,1,4) };
is $@, '', 'no error with leap day of 2004 (year passed as 4)';

eval { &timegm(0,0,0,29,1,96) };
is $@, '', 'no error with leap day of 1996 (year passed as 96)';

1;
