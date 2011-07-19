use strict;
use warnings;

use t::std;

sub cmp
{
	my $str = shift;

	if ($str eq "fergal")
	{
		return 1;
	}
	elsif ($str eq "feargal")
	{
		return (0, "your name's not down, you're not coming in");
	}
  else
  {
    return 0;
  }
}

{
	check_test(
		sub {
			cmp_deeply("fergal", code(\&cmp));
		},
		{
			actual_ok => 1,
			diag => '',
		},
		"code ok"
	);

  my ($prem, $res);
	($prem, $res) = check_test(
		sub {
			cmp_deeply("feargal", code(\&cmp));
		},
		{
			actual_ok => 0,
		},
		"code not ok"
	);

	like($res->{diag}, "/your name's not down/", "diagnostics");
	like($res->{diag}, "/feargal/", "diagnostics");

	($prem, $res)  = check_test(
		sub {
			cmp_deeply("fazzer", code(\&cmp));
		},
		{
			actual_ok => 0,
		},
		"code not ok"
	);

	like($res->{diag}, "/it failed but it didn't say why/", "no diagnostics");
	like($res->{diag}, "/fazzer/", "no diagnostics");
}
