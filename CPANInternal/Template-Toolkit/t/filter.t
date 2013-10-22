#============================================================= -*-perl-*-
#
# t/filter.t
#
# Template script testing FILTER directive.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2000 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1998-2000 Canon Research Centre Europe Ltd.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib );
use Template::Filters;
use Template qw( :status );
use Template::Parser;
use Template::Test;
use Template::Constants qw( :debug );

my $DEBUG = grep(/^--?d(debug)?$/, @ARGV);

$Template::Test::DEBUG = 0;
$Template::Test::EXTRA = 1;     # ensure redirected file is created
#$Template::Context::DEBUG = 1;
#$Template::DEBUG = 1;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;


#------------------------------------------------------------------------
# hack to allow STDERR to be tied to a variable.
# (I'm really surprised there isn't a standard module which does this)
#------------------------------------------------------------------------

package Tie::File2Str;

sub TIEHANDLE {
    my ($class, $textref) = @_;
    bless $textref, $class;
}
sub PRINT {
    my $self = shift;
    $$self .= join('', @_);
}



#------------------------------------------------------------------------
# now for the main event...
#------------------------------------------------------------------------

package main;

# tie STDERR to a variable
my $stderr = '';
#tie(*STDERR, "Tie::File2Str", \$stderr);

my $dir  = -d 't' ? 't/test/tmp' : 'test/tmp';
my $file = 'xyz';
my ($a, $b, $c, $d) = qw( alpha bravo charlie delta );
my $params = { 
    'a'      => $a,
    'b'      => $b,
    'c'      => $c,
    'd'      => $d,
    'list'   => [ $a, $b, $c, $d ],
    'text'   => 'The cat sat on the mat',
    outfile  => $file,
    stderr   => sub { $stderr },
    despace  => bless(\&despace, 'anything'),
    widetext => "wide:\x{65e5}\x{672c}\x{8a9e}",
};

my $filters = {
    'nonfilt'    => 'nonsense',
    'microjive'  => \&microjive,
    'microsloth' => [ \&microsloth, 0 ],
    'censor'     => [ \&censor_factory, 1 ],
    'badfact'    => [ sub { return 'nonsense' }, 1 ],
    'badfilt'    => [ 'rubbish', 1 ],
    'barfilt'    => [ \&barf_up, 1 ],
};
my $config1 = {
    INTERPOLATE => 1, 
    POST_CHOMP  => 1,
    FILTERS     => $filters,
};
my $config2 = {
    EVAL_PERL   => 1,
    FILTERS     => $filters,
    OUTPUT_PATH => $dir,
    BARVAL      => 'some random value',
};

unlink "$dir/$file" if -f "$dir/$file";

my $tt1 = Template->new($config1)
    || die Template->error();
my $tt2 = Template->new($config2) 
    || die Template->error();

$tt2->context->define_filter('another', \&another, 1);

tie(*STDERR, "Tie::File2Str", \$stderr);

test_expect(\*DATA, [ default => $tt1, evalperl => $tt2 ], $params);

ok( -f "$dir/$file", "$dir/$file exists" );
unlink "$dir/$file" if -f "$dir/$file";



#------------------------------------------------------------------------
# custom filter subs 
#------------------------------------------------------------------------

sub microjive {
    my $text = shift;
    $text =~ s/microsoft/The 'Soft/sig;
    $text;
}

sub microsloth {
    my $text = shift;
    $text =~ s/microsoft/Microsloth/sig;
    $text;
}

sub censor_factory {
    my @forbidden = @_;
    return sub {
	my $text = shift;
	foreach my $word (@forbidden) {
	    $text =~ s/$word/[** CENSORED **]/sig;
	}
	return $text;
    }
}

sub barf_up {
    my $context = shift;
    my $foad    = shift || 0;

    if ($foad == 0) {
        return (undef, "barfed");
    }
    elsif ($foad == 1) {
	return (undef, Template::Exception->new('dead', 'deceased'));
    }
    elsif ($foad == 2) {
	die "keeled over\n";
    }
    else {
	die (Template::Exception->new('unwell', 'sick as a parrot'));
    }
}

sub despace {
    my $text = shift;
    $text =~ s/\s+/_/g;
    return $text;
}

sub another {
    my ($context, $n) = @_;
    return sub {
	my $text = shift;
	return $text x $n;
    }
}

__DATA__
#------------------------------------------------------------------------
# test failures
#------------------------------------------------------------------------
-- test --
[% TRY %]
[% FILTER nonfilt %]
blah blah blah
[% END %]
[% CATCH %]
BZZZT: [% error.type %]: [% error.info %]
[% END %]
-- expect --
BZZZT: filter: invalid FILTER entry for 'nonfilt' (not a CODE ref)

-- test --
[% TRY %]
[% FILTER badfact %]
blah blah blah
[% END %]
[% CATCH %]
BZZZT: [% error.type %]: [% error.info %]
[% END %]
-- expect --
BZZZT: filter: invalid FILTER for 'badfact' (not a CODE ref)

-- test --
[% TRY %]
[% FILTER badfilt %]
blah blah blah
[% END %]
[% CATCH %]
BZZZT: [% error.type %]: [% error.info %]
[% END %]
-- expect --
BZZZT: filter: invalid FILTER entry for 'badfilt' (not a CODE ref)

-- test --
[% TRY;
     "foo" | barfilt;
   CATCH;
     "$error.type: $error.info";
   END
%]
-- expect --
filter: barfed

-- test --
[% TRY;
     "foo" | barfilt(1);
   CATCH;
     "$error.type: $error.info";
   END
%]
-- expect --
dead: deceased

-- test --
[% TRY;
     "foo" | barfilt(2);
   CATCH;
     "$error.type: $error.info";
   END
%]
-- expect --
filter: keeled over

-- test --
[% TRY;
     "foo" | barfilt(3);
   CATCH;
     "$error.type: $error.info";
   END
%]
-- expect --
unwell: sick as a parrot


#------------------------------------------------------------------------
# test filters
#------------------------------------------------------------------------

-- test --
[% FILTER html %]
This is some html text
All the <tags> should be escaped & protected
[% END %]
-- expect --
This is some html text
All the &lt;tags&gt; should be escaped &amp; protected

-- test --
[% text = "The <cat> sat on the <mat>" %]
[% FILTER html %]
   text: $text
[% END %]
-- expect --
   text: The &lt;cat&gt; sat on the &lt;mat&gt;

-- test --
[% text = "The <cat> sat on the <mat>" %]
[% text FILTER html %]
-- expect --
The &lt;cat&gt; sat on the &lt;mat&gt;

-- test --
[% FILTER html %]
"It isn't what I expected", he replied.
[% END %]
-- expect --
&quot;It isn't what I expected&quot;, he replied.

-- test --
[% FILTER xml %]
"It isn't what I expected", he replied.
[% END %]
-- expect --
&quot;It isn&apos;t what I expected&quot;, he replied.

-- test --
[% FILTER format %]
Hello World!
[% END %]
-- expect --
Hello World!

-- test --
# test aliasing of a filter
[% FILTER comment = format('<!-- %s -->') %]
Hello World!
[% END +%]
[% "Goodbye, cruel World" FILTER comment %]
-- expect --
<!-- Hello World! -->
<!-- Goodbye, cruel World -->

-- test --
[% FILTER format %]
Hello World!
[% END %]
-- expect --
Hello World!

-- test --
[% "Foo" FILTER test1 = format('+++ %-4s +++') +%]
[% FOREACH item = [ 'Bar' 'Baz' 'Duz' 'Doze' ] %]
  [% item FILTER test1 +%]
[% END %]
[% "Wiz" FILTER test1 = format("*** %-4s ***") +%]
[% "Waz" FILTER test1 +%]
-- expect --
+++ Foo  +++
  +++ Bar  +++
  +++ Baz  +++
  +++ Duz  +++
  +++ Doze +++
*** Wiz  ***
*** Waz  ***

-- test --
[% FILTER microjive %]
The "Halloween Document", leaked to Eric Raymond from an insider
at Microsoft, illustrated Microsoft's strategy of "Embrace,
Extend, Extinguish"
[% END %]
-- expect --
The "Halloween Document", leaked to Eric Raymond from an insider
at The 'Soft, illustrated The 'Soft's strategy of "Embrace,
Extend, Extinguish"

-- test --
[% FILTER microsloth %]
The "Halloween Document", leaked to Eric Raymond from an insider
at Microsoft, illustrated Microsoft's strategy of "Embrace,
Extend, Extinguish"
[% END %]
-- expect --
The "Halloween Document", leaked to Eric Raymond from an insider
at Microsloth, illustrated Microsloth's strategy of "Embrace,
Extend, Extinguish"

-- test --
[% FILTER censor('bottom' 'nipple') %]
At the bottom of the hill, he had to pinch the
nipple to reduce the oil flow.
[% END %]
-- expect --
At the [** CENSORED **] of the hill, he had to pinch the
[** CENSORED **] to reduce the oil flow.

-- test --
[% FILTER bold = format('<b>%s</b>') %]
This is bold
[% END +%]
[% FILTER italic = format('<i>%s</i>') %]
This is italic
[% END +%]
[% 'This is both' FILTER bold FILTER italic %]
-- expect --
<b>This is bold</b>
<i>This is italic</i>
<i><b>This is both</b></i>

-- test --
[% "foo" FILTER format("<< %s >>") FILTER format("=%s=") %]
-- expect --
=<< foo >>=

-- test --
[% blocktext = BLOCK %]
The cat sat on the mat

Mary had a little Lamb



You shall have a fishy on a little dishy, when the boat comes in.  What 
if I can't wait until then?  I'm hungry!
[% END -%]
[% global.blocktext = blocktext; blocktext %]

-- expect --
The cat sat on the mat

Mary had a little Lamb



You shall have a fishy on a little dishy, when the boat comes in.  What 
if I can't wait until then?  I'm hungry!

-- test --
[% global.blocktext FILTER html_para %]

-- expect --
<p>
The cat sat on the mat
</p>

<p>
Mary had a little Lamb
</p>

<p>
You shall have a fishy on a little dishy, when the boat comes in.  What 
if I can't wait until then?  I'm hungry!
</p>

-- test --
[% global.blocktext FILTER html_break %]

-- expect --
The cat sat on the mat
<br />
<br />
Mary had a little Lamb
<br />
<br />
You shall have a fishy on a little dishy, when the boat comes in.  What 
if I can't wait until then?  I'm hungry!

-- test --
[% global.blocktext FILTER html_para_break %]

-- expect --
The cat sat on the mat
<br />
<br />
Mary had a little Lamb
<br />
<br />
You shall have a fishy on a little dishy, when the boat comes in.  What 
if I can't wait until then?  I'm hungry!

-- test --
[% global.blocktext FILTER html_line_break %]

-- expect --
The cat sat on the mat<br />
<br />
Mary had a little Lamb<br />
<br />
<br />
<br />
You shall have a fishy on a little dishy, when the boat comes in.  What <br />
if I can't wait until then?  I'm hungry!<br />

-- test --
[% global.blocktext FILTER truncate(10) %]

-- expect --
The cat...

-- test --
[% global.blocktext FILTER truncate %]

-- expect --
The cat sat on the mat

Mary ...

-- test --
[% 'Hello World' | truncate(2) +%]
[% 'Hello World' | truncate(8) +%]
[% 'Hello World' | truncate(10) +%]
[% 'Hello World' | truncate(11) +%]
[% 'Hello World' | truncate(20) +%]
-- expect --
..
Hello...
Hello W...
Hello World
Hello World

-- test --
[% "foo..." FILTER repeat(5) %]

-- expect --
foo...foo...foo...foo...foo...

-- test --
[% FILTER truncate(21) %]
I have much to say on this matter that has previously been said
on more than one occassion.
[% END %]

-- expect --
I have much to say...

-- test --
[% FILTER truncate(25) %]
Nothing much to say
[% END %]

-- expect --
Nothing much to say

-- test --
[% FILTER repeat(3) %]
Am I repeating myself?
[% END %]

-- expect --
Am I repeating myself?
Am I repeating myself?
Am I repeating myself?

-- test --
[% text FILTER remove(' ') +%]
[% text FILTER remove('\s+') +%]
[% text FILTER remove('cat') +%]
[% text FILTER remove('at') +%]
[% text FILTER remove('at', 'splat') +%]

-- expect --
Thecatsatonthemat
Thecatsatonthemat
The  sat on the mat
The c s on the m
The c s on the m

-- test --
[% text FILTER replace(' ', '_') +%]
[% text FILTER replace('sat', 'shat') +%]
[% text FILTER replace('at', 'plat') +%]

-- expect --
The_cat_sat_on_the_mat
The cat shat on the mat
The cplat splat on the mplat

-- test --
[% text = 'The <=> operator' %]
[% text|html %]
-- expect --
The &lt;=&gt; operator

-- test --
[% text = 'The <=> operator, blah, blah' %]
[% text | html | replace('blah', 'rhubarb') %]
-- expect --
The &lt;=&gt; operator, rhubarb, rhubarb

-- test --
[% | truncate(25) %]
The cat sat on the mat, and wondered to itself,
"How might I be able to climb up onto the shelf?",
For up there I am sure I'll see,
A tasty fishy snack for me.
[% END %]
-- expect --
The cat sat on the mat...

-- test --
[% FILTER upper %]
The cat sat on the mat
[% END %]
-- expect --
THE CAT SAT ON THE MAT

-- test --
[% FILTER lower %]
The cat sat on the mat
[% END %]
-- expect --
the cat sat on the mat

-- test --
[% 'arse' | stderr %]
stderr: [% stderr %]
-- expect --
stderr: arse


-- test --
[% percent = '%'
   left    = "[$percent"
   right   = "$percent]"
   dir     = "$left a $right blah blah $left b $right"
%]
[% dir +%]
FILTER [[% dir | eval %]]
FILTER [[% dir | evaltt %]]
-- expect --
[% a %] blah blah [% b %]
FILTER [alpha blah blah bravo]
FILTER [alpha blah blah bravo]

-- test -- 
[% TRY %]
[% dir = "[\% FOREACH a = { 1 2 3 } %\]a: [\% a %\]\n[\% END %\]" %]
[% dir | eval %]
[% CATCH %]
error: [[% error.type %]] [[% error.info %]]
[% END %]
-- expect --
error: [file] [parse error - input text line 1: unexpected token (1)
  [% FOREACH a = { 1 2 3 } %]]


-- test --
nothing
[% TRY;
    '$x = 10; $b = 20; $x + $b' | evalperl;
   CATCH;
     "$error.type: $error.info";
   END
+%]
happening
-- expect --
nothing
perl: EVAL_PERL is not set
happening

-- test --
[% TRY -%]
before
[% FILTER redirect('xyz') %]
blah blah blah
here is the news
[% a %]
[% END %]
after
[% CATCH %]
ERROR [% error.type %]: [% error.info %]
[% END %]

-- expect --
before
ERROR redirect: OUTPUT_PATH is not set

-- test --
-- use evalperl --
[% FILTER evalperl %]
   $a = 10;
   $b = 20;
   $stash->{ foo } = $a + $b;
   $stash->{ bar } = $context->config->{ BARVAL };
   "all done"
[% END +%]
foo: [% foo +%]
bar: [% bar %]
-- expect --
all done
foo: 30
bar: some random value

-- test --
[% TRY -%]
before
[% FILTER file(outfile) -%]
blah blah blah
here is the news
[% a %]
[% END -%]
after
[% CATCH %]
ERROR [% error.type %]: [% error.info %]
[% END %]
-- expect --
before
after

-- test --
[% PERL %]
# static filter subroutine
$Template::Filters::FILTERS->{ bar } = sub {
    my $text = shift; 
    $text =~ s/^/bar: /gm;
    return $text;
};
[% END -%]
[% FILTER bar -%]
The cat sat on the mat
The dog sat on the log
[% END %]
-- expect --
bar: The cat sat on the mat
bar: The dog sat on the log

-- test --
[% PERL %]
# dynamic filter factory
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
[% END -%]
[% FILTER baz -%]
The cat sat on the mat
The dog sat on the log
[% END %]
[% FILTER baz('wiz') -%]
The cat sat on the mat
The dog sat on the log
[% END %]

-- expect --
baz: The cat sat on the mat
baz: The dog sat on the log

wiz: The cat sat on the mat
wiz: The dog sat on the log


-- test --
-- use evalperl --
[% PERL %]
$stash->set('merlyn', bless \&merlyn1, 'ttfilter');
sub merlyn1 {
    my $text = shift || '<no text>';
    $text =~ s/stone/henge/g;
    return $text;
}
[% END -%]
[% FILTER $merlyn -%]
Let him who is without sin cast the first stone.
[% END %]
-- expect --
Let him who is without sin cast the first henge.

-- test --
-- use evalperl --
[% PERL %]
$stash->set('merlyn', sub { \&merlyn2 });
sub merlyn2 {
    my $text = shift || '<no text>';
    $text =~ s/stone/henge/g;
    return $text;
}
[% END -%]
[% FILTER $merlyn -%]
Let him who is without sin cast the first stone.
[% END %]
-- expect --
Let him who is without sin cast the first henge.

-- test --
[% myfilter = 'html' -%]
[% FILTER $myfilter -%]
<html>
[% END %]
-- expect --
&lt;html&gt;

-- test --
[% FILTER $despace -%]
blah blah blah
[%- END %]
-- expect --
blah_blah_blah

-- test --
-- use evalperl --
[% PERL %]
$context->filter(\&newfilt, undef, 'myfilter');
sub newfilt {
    my $text = shift;
    $text =~ s/\s+/=/g;
    return $text;
}
[% END -%]
[% FILTER myfilter -%]
This is a test
[%- END %]
-- expect --
This=is=a=test

-- test --
[% PERL %]
$context->define_filter('xfilter', \&xfilter);
sub xfilter {
    my $text = shift;
    $text =~ s/\s+/X/g;
    return $text;
}
[% END -%]
[% FILTER xfilter -%]
blah blah blah
[%- END %]
-- expect --
blahXblahXblah


-- test --
[% FILTER another(3) -%]
foo bar baz
[% END %]
-- expect --
foo bar baz
foo bar baz
foo bar baz

-- test --
[% '$stash->{ a } = 25' FILTER evalperl %]
[% a %]
-- expect --
25
25

-- test --
[% '$stash->{ a } = 25' FILTER perl %]
[% a %]
-- expect --
25
25

-- test --
[% FILTER indent -%]
The cat sat
on the mat
[% END %]
-- expect --
    The cat sat
    on the mat

-- test --
[% FILTER indent(2) -%]
The cat sat
on the mat
[% END %]
-- expect --
  The cat sat
  on the mat

-- test --
[% FILTER indent('>> ') -%]
The cat sat
on the mat
[% END %]
-- expect --
>> The cat sat
>> on the mat

-- test --
[% text = 'The cat sat on the mat';
   text | indent('> ') | indent('+') %]
-- expect --
+> The cat sat on the mat

-- test --
<<[% FILTER trim %]
   
          
The cat sat
on the
mat


[% END %]>>
-- expect --
<<The cat sat
on the
mat>>

-- test --
<<[% FILTER collapse %]
   
          
The    cat     sat
on    the
mat


[% END %]>>
-- expect --
<<The cat sat on the mat>>

-- test --
[% FILTER format('++%s++') %]Hello World[% END %]
[% FILTER format %]Hello World[% END %]
-- expect --
++Hello World++
Hello World

-- test --
[% "my file.html" FILTER uri %]
-- expect --
my%20file.html

-- test --
[% "my<file & your>file.html" FILTER uri %]
-- expect --
my%3Cfile%20%26%20your%3Efile.html

-- test --
[% "foo@bar" FILTER uri %]
-- expect --
foo%40bar

-- test --
[% "foo@bar" FILTER url %]
-- expect --
foo@bar

-- test --
[% "my<file & your>file.html" | uri | html %]
-- expect --
my%3Cfile%20%26%20your%3Efile.html

-- test --
[% widetext | uri %]
-- expect --
wide%3A%E6%97%A5%E6%9C%AC%E8%AA%9E

-- test --
[% 'foobar' | ucfirst %]
-- expect --
Foobar

-- test --
[% 'FOOBAR' | lcfirst %]
-- expect --
fOOBAR


