#! /usr/bin/perl -ws

use Lingua::EN::Inflect qw { &inflect &classical };
use vars qw { $classical $modern };

classical if $classical && !$modern;

print "inflect> ";
while (<>)
{
	chomp;
	exit if /^\.$/;
	if (/^\-classical$/)	{ classical ; print "[going classical]"}
	elsif (/^-modern$/)	{ classical 0; print "[going modern]" }
	else			{ print inflect $_ }
	print "\ninflect> ";
}
