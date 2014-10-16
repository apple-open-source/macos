#!/usr/bin/perl -w
#
# A script to test and demonstrate the XS version of Template::Stash. 
#
# Sadly, it looks like getrusage() may not return reasonable RSS values
# on Linux and many other operating systems. It works great on FreeBSD
# though.
#
# $Id$
#

# paths to Template version we want to test
use lib qw( ./lib
            ../lib 
	    ./blib/arch/auto/Template/Stash/XS
	    ../blib/arch/auto/Template/Stash/XS' 
);

use strict;
use Template;
use Template::Stash;
use Template::Stash::XS;
use BSD::Resource;
use CGI;

# test package
package Foo;

sub new 	{ return bless {}, $_[0]; }
sub new_av      { return bless [ 1,2,3 ], $_[0]; }
sub new_sv      { return bless "hello", $_[0]; }
sub bar 	{ return 1001; }
sub baz 	{ return "This is baz method from Foo."; }
sub more 	{ return "I got " . $_[1] . " for more"; }
sub newnew 	{ return new Foo; }
sub err		{ return ( undef, "This is the ''error message''\n" ); }

# main
package main;

# test data
my $params = {
    nodef     => undef,
    zero      => 0,
    one       => 1,
    a_0	      => [],
    a_1	      => [1],
    a_2	      => [2,1],
    bar	      => { baz => { boz => { bean => 20 } } },
    i	      => 0,
    string    => 'The quick brown fox jumped over the lazy dog',
    spaced    => "This  is  a   test       string with   s  p  a  c  e s",
    hash      => { a => 'b', c => 'd' },
    metavars  => [ qw( foo bar baz qux wiz waz woz ) ],
    people    => [ { id => 'tom',   name => 'Tomas' },
	 	   { id => 'dick',  name => 'Richard' },
		   { id => 'harry', name => 'Harold' },
		   { id => 'wes',   name => 'Wesley' },
		   { id => 'andy',  name => 'Andrew' },
		   { id => 'jen',   name => 'Jennifer' },
		   { id => 'larry', name => 'Larry' } ],
    primes    => [ 6899, 13, 11, 69931, 17, 19, 682547, 2, 3, 5, 7 ],
    phones    => { 3141 => 'Alpha', 5131 => 'Beta', 4131 => 'Gamma' },
    groceries => { qw( a 1 b 2 c 3 d 4 e 5 f 6 g 7 h 8 i 9 j 10),
		   'Flour' => 3, 'Milk' => 1,    'Peanut Butter' => 21,
		   'Eggs'  => 5, 'Celery' => 15, 'Apples' => 12  },
    stuff     => [ { a => 'apple',   b => 'banana',  c => 'carrot'    },
    		   { a => 'apache',  b => 'bsd',     c => 'commodore' },
    		   { a => 'asada',   b => 'beef',    c => 'carne'  } ],
    method    => new Foo,
    quux      => [ new Foo, new_av Foo, new_av Foo ],
    cgi       => CGI->new('mode=submit&debug=1'),
    ref       => { a => sub { return "a sub [@_]" },
	 	   j => { k => 3, l => 5, m => { n => sub { "nsub [@_]" } } },
	           z => sub { return "z called ".&{shift()}(10, 20, 30); } },
};

$params->{r}->{e}->{c}->{u}->{r}->{s}->{e} = $params;
$params->{recurse} = $params;


print "XS Module: " , Template::Stash::XS::cvsid() , "\n";

# setup template object, etc.
my $TMPDIR = $ENV{'TMPDIR'} || '.';
my $o;

my $fast_tt = new Template ({ 
	STASH => new Template::Stash::XS,
	ABSOLUTE => 1,
	RELATIVE => 1,
	COMPILE_DIR => $TMPDIR,
        COMPILE_EXT => '.ttc2',
	CACHE_SIZE => 64,
	OUTPUT => \$o });

my $slow_tt = new Template ({ 
	STASH => new Template::Stash,
	ABSOLUTE => 1,
	RELATIVE => 1,
	COMPILE_DIR => $TMPDIR,
        COMPILE_EXT => '.ttc2',
	CACHE_SIZE => 64,
	OUTPUT => \$o });

$Template::Filters::FILTERS->{ baz } = [];	# avoid silly warning
$Template::Filters::FILTERS->{ baz } = [
    sub {
        my $context = shift;
        my $word = shift || 'baz';
        return sub {
            my $text = shift;
            $text =~ s/^/$word: /gm;
            return $text;
        };
    }, 1 ]; 

my $template;
my $expected;

if (@ARGV) {
  $template = shift @ARGV;

} else {
  $template = "$TMPDIR/testing.tmpl";
  unlink $template;
  open FH, ">$template"; 
  while(<DATA>) { last if /^__END__/; print FH; } close FH;
}

# verify that we get expected results
{
  $slow_tt->process($template, $params) or die "SLOW TT: " . $slow_tt->error;
  print " Template: ", $template, "\n";
  print "   Length: ", length($o)," bytes\n";
  $expected = $o;
  $o = '';

  $fast_tt->process($template, $params) or die "FAST TT: " . $fast_tt->error;
  if($expected ne $o) {
    print "   GOT: $o\n";
    print "=" x 60, "\n";
    print "WANTED: $expected\n";
    die "unexpected output from fast_tt->process\n";
  } else {
    print "   Status: XS output OK\n\n";
  }
}

# here comes the big test
for my $loops (10, 100, 1000) {

  print STDERR "=" x 70, "\n\n";

  print STDERR "Evaluating template with original stash $loops times...";

  my $slow_stat = get_usage();
  # Do the slow test  
  for (1..$loops) {
    $o = '';
    $slow_tt->process($template, $params) or die $Template::ERROR;
    die "''$o'' ne ''$expected''" unless $o eq $expected;
  }
  my $slow_result = get_usage($slow_stat);
  print STDERR "done.\n";

  printf(STDERR
	"Usr: %.2fs, Sys: %.2fs, Total: %.2fs, RSS: %dKB (%dKB change)\n",
	@$slow_result);

  print STDERR "\nEvaluating template with new XS stash $loops times...";

  my $fast_stat = get_usage();
  # Do the fast test  
  for (1..$loops) {
    $o = '';
    $fast_tt->process($template, $params) or die $Template::ERROR;
    die "''$o'' ne ''$expected''" unless $o eq $expected;
  }
  my $fast_result = get_usage($fast_stat);

  print STDERR "done.\n";

  printf(STDERR
	"Usr: %.2fs, Sys: %.2fs, Total: %.2fs, RSS: %dKB (%dKB change)\n", 
	@$fast_result);

  printf(STDERR
	"\n\tImprovement: %.2fX\n\n", $slow_result->[2] / $fast_result->[2]);
 
}

# If it's been enabled...
# print Template::Stash::XS::performance(1);

unlink "$TMPDIR/testing.tmpl";
exit(0);


# returns arrayref with user, system and total time
# optionally subtracts given arrayref.
sub get_usage {
  my $a_ref = shift;

  my ($usertime, $systemtime, $maxrss) = getrusage(RUSAGE_SELF);
  my $maxrss_delta = $maxrss;

  if (defined($a_ref)) {
    $usertime     -= $a_ref->[0];
    $systemtime   -= $a_ref->[1];
    $maxrss_delta -= $a_ref->[3];
  } else {
    $maxrss_delta = 0;
  }

  return [ $usertime, $systemtime, $usertime + $systemtime, 
	   $maxrss, $maxrss_delta ];
}


__DATA__

< here is the template >

defined tests:
0) [% totallynotdef.defined ? 'fail' : 'okay' %] 
1) [% totallynotdef.defined ? 'fail' : 'okay' %] 
2) [% zero.defined ? 'okay' : 'fail' %]
3) [% one.defined ? 'okay' : 'fail' %] 
4) [% nodef ? 'fail' : 'okay' %]

foreach:
0) size: [% primes.size %] ([% primes.nsort.first %]..[% primes.nsort.last %])
1) forward: [% FOREACH p = primes %] [% p %] [% END %]
2) reverse: [% FOREACH p = primes.reverse %] [% p %] [% END %]
3) hash: [% FOREACH p = stuff %] [% p.a %] [% p.b %] [% p.c %] [% END %]
4) hash sort keys: [% FOREACH p = phones.sort %] [% p %] [% END %]
5) [% FOREACH people.sort('id') -%] [% name +%] [% END %]
6) reverse 0. [% a_0.reverse.join(",") %]
7) reverse 1. [% a_1.reverse.join(",") %]
8) reverse 2. [% a_2.reverse.join(",") %]

first and last:
0) [% string %] [% metavars.first %] [% metavars.last %]
1) [% r.e.c.u.r.s.e.primes.nsort.first %]...[% recurse.primes.nsort.last %]
2) [% r.e.c.u.r.s.e.primes.sort.first %]...[% recurse.primes.sort.last %]

string split.join:
0) [% string.length %]
1) [% string.split.join('_') %]
2) [% spaced.split.join('_') %]

hash: (each, keys, values) join:
0) [% hash.each.join(', ') %]
1) [% hash.keys.join(', ') %] 
2) [% hash.values.join(', ') %]

first, last, size:
0) [% metavars.first %] 
1) [% metavars.last %]
2) [% metavars.size %] 
3) [% metavars.max %] 

joins:
0) [% metavars.join %]
1) [% metavars.join(', ') %]

assign and repeat:
0) [% string = 'foo' %] [% string.repeat(3) %]

more foreach, sort, etc:
0) [% FOREACH person = people.sort('id') -%] [% person.name +%] [% END %]
1) [% FOREACH person = people.sort('name') -%] [% person.name +%] [% END %]
2) [% FOREACH n = phones.sort -%] [% phones.$n %] is [% n %], [% END %]
3) [% FOREACH n = groceries.nsort.reverse -%] I got [% groceries.$n %] kilos of [% n %]! [% END %]
4) [% FOREACH item = [ 'foo', 'bar', 'baz' ] -%]
           [%- "<ul>\n" IF loop.first %]
           <li>[% loop.count %]/[% loop.size %]: [% item %]
           [%- "</ul>\n" IF loop.last %]
   [% END %]

commify:
0) [% FOREACH item = people %][%item.name%][%UNLESS loop.last%],[%END%][%END%]
1) [% FOREACH item = people; item.name; ',' UNLESS loop.last; END; %]

methods:
0) [% method.bar %] 
1) [% method.baz %] 
2) [% method.bad %] 
3) [% method.newnew.bar %] 
4) [% method.more("stuff") %]
5) [% quux.first.bar %] -- [% quux.last.more("junk") %]
6) [% x = quux.1; x.0 %] 

lots o' dots:
0) [% bar.baz.boz.bean %]
1) [% a.b.c.d.e.f = [ 1 2 3 4 ] %]
2) [% a.b.c.d.e.f.join(",") %]

include/block: [% BLOCK testblock %]bar=[% bar %]foo=[% foo %][% END %]
0) [% INCLUDE testblock bar=2, foo='cat sat on mat' %]

process/block: [% BLOCK testproc %]one=[% one %]zero=[% zero %]foo=[% foo %][% END %]
0) [% PROCESS testproc bar=2, foo='matt sat on cat' %]

slices:
0) [% items = [ 'foo', 'bar', 'baz' ];
   take  = [ 0, 2 ];
   slice = items.$take;
   slice.join(', '); -%]

1) [% items = { foo = 'one', bar = 'two', baz = 'three' };
   take  = [ 'foo', 'baz' ];
   slice = items.$take;
   slice.join(', '); %]

cgi:
0) [% cgi.param('mode') %]
1) [% cgi.start_form %]
   [% cgi.popup_menu(name   =>   'items', 
                     values => [ 'foo' 'bar' 'baz' ]) %]
   [% cgi.end_form %]

if/else:
0) [% IF one %] okay [% END %]
1) [% IF one %] okay [% ELSE %] fail [% END %]
2) [% IF one and string %] okay [% ELSE %] fail [% END %]
3) [% IF one && string %] okay [% ELSE %] fail [% END %]
4) [% IF false || one %] okay [% ELSE %] fail [% END %]
5) [% IF zero && one %] fail [% ELSE %] okay [% END %]
6) [% " okay" UNLESS zero %]
7) [% IF recurse.one %] okay [% ELSE %] fail [% END %]
8) [% IF r.e.c.u.r.s.e.one %] okay [% ELSE %] fail [% END %]
9) [% IF r.e.c.u.r.s.e.one.defined %] okay [% ELSE %] fail [% END %]

ref:
0) a: [% ref.a %] a(5): [% ref.a(5) %] a(5,10): [% ref.a(5,10) %]

assignments:
[% ten    = 10 
   twenty = 20
   thirty = twenty + ten
   forty  = 2 * twenty 
   fifty  = 100 div 2
   six    = twenty mod 7 -%]
0) 10=[%ten%] 20=[%twenty%] 30=[%thirty%] 40=[%forty%] 50=[%fifty%] 6=[%six%]

[%- DEFAULT
    seventy = 70
    fifty   = -5 -%]
1) 50=[%fifty%] 70=[%seventy%]

[%- foo = { bar = 'Baz' } -%]
2) foo=[% foo.bar %]

3) [% META title   = 'The Cat in the Hat'
           author  = 'Dr. Seuss'
           version = 1.23 -%][% template.title %]
   [% template.author %]

errors, try, catch, etc:

0) [% TRY %]
     ...blah...blah...
     [% CALL somecode %]
     ...etc...
     [% INCLUDE someblock %]
     ...and so on...
  [% CATCH %]
     An error of type [% error.type %] occurred!
  [% END %]

1) [% TRY -%]
   [% INCLUDE missingfile -%]
   [% CATCH file ; "File Error! $error.info"; END %]

2) [% TRY -%]
   This gets printed 
   [% THROW food 'carrots' %]
   This doesn't
   [% CATCH food %]
   culinary delights: [% error.info %]
   [% END %]    

3) [% TRY -%][% method.err %][% CATCH %]error type=[% error.type %] 
   info=[% error.info %][% END %]

views & filters
0) [% VIEW my.foo quux=33; END -%] [% my.foo.quux %]
1) [% FILTER baz('quux') -%] the itsy bitsy spider [% END %]
2) [% FILTER baz -%] the itsy bitsy spider [% END %]

more tests (crashme, etc.)
0) [% x = [1]; x.y.z = 2; %]
0) [% x = [1,2]; x.a.b.c = 3; %]

--------------------
end of template test ... have a pleasant day
--------------------

__END__

This stuff leaks HUGE amounts of memory:

ref:
[% b = \ref.a -%]
b: [% b %] b(5): [% b(5) %] b(5,10): [% b(5,10) %]
[% c = \ref.a(10,20) -%]
c: [% c %] c(30): [% c(30) %] c(30,40): [% c(30,40) %]
[% f = \ref.j.k -%]
f: [% f %]
[% f = \ref.j.m.n -%]
f: [% f %] f(11): [% f(11) %]


