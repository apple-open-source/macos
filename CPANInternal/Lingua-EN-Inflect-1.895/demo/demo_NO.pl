#! /usr/bin/perl -ws

use Lingua::EN::Inflect qw { :USER_DEFINED classical NO };
use vars qw { $classical $modern };

classical if $classical && !$modern;

def_noun 'kin'   => 'kine';
def_noun '(.*)x' => '$1xen';

def_verb 'foo'  => 'fee',
	 'foo'  => 'fee',
	 'foos' => 'fee';

def_adj  'red'  => 'red|gules';

print "count word> ";
while (<>)
{
	chomp;
	exit if /^\.$/;
	if (/^\-classical$/)	{ classical ; print "[going classical]"}
	elsif (/^-modern$/)	{ classical 0; print "[going modern]" }
	else			
	{
		/\s*([^\s]+)\s+(.*)/ or next;
		print "            ", NO($2,$1), "\n";
	}
	print "\ncount word> ";
}
