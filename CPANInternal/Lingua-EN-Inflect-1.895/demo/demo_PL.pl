#! /usr/bin/perl -ws

use Lingua::EN::Inflect qw { classical PL def_noun def_verb def_adj };
use vars qw { $classical $modern };

classical if $classical && !$modern;

def_noun 'kin'  => 'kine';

def_verb 'foo'  => 'fee',
	 'foo'  => 'fee',
	 'foos' => 'fee';

def_adj  'red'  => 'red|gules';

print "singular> one ";
while (<>)
{
	chomp;
	exit if /^\.$/;
	if (/^\-classical$/)	{ classical ; print "[going classical]"}
	elsif (/^-modern$/)	{ classical 0; print "[going modern]" }
	else			{ print "   plural> two ", PL $_ }
	print "\nsingular> one ";
}
