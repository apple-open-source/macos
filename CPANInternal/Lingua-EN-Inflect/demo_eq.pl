#! /usr/bin/perl -ws

use Lingua::EN::Inflect qw { :COMPARISONS };
use vars qw { $classical $modern };

classical if $classical && !$modern;

print "compare> ";
while (<>)
{
	chomp;
	exit if /^\.$/;
	if (/^\-classical$/)	{ classical ; print "[going classical]"}
	elsif (/^-modern$/)	{ classical 0; print "[going modern]" }
	else		
	{
		if (/(\S+)\s+(\S+)/)
		{
			print "PL:	[", PL_eq($1, $2), "]\n";
			print "PL_N:	[", PL_N_eq($1, $2), "]\n";
			print "PL_V:	[", PL_V_eq($1, $2), "]\n";
			print "PL_ADJ:	[", PL_ADJ_eq($1, $2), "]\n";
		}
	}
	print "\ncompare> ";
}
