#!perl -w
use strict;

# $Id: test.t,v 1.6 2010-06-11 20:08:57 roderick Exp $
#
# Copyright (c) 1997 Roderick Schertler.  All rights reserved.  This
# program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    $| = 1;
    print "1..33\n";
}

use String::ShellQuote;

my $test_num = 0;
sub ok {
    my ($result, @info) = @_;
    $test_num++;
    if ($result) {
	print "ok $test_num\n";
    }
    else {
	print "not ok $test_num\n";
	print "# ", @info, "\n" if @info;
    }
}

my $testsub;
sub test {
    my ($want, @args) = @_;
    my $pid = $$;
    my $got = eval { &$testsub(@args) };
    exit if $$ != $pid;
    if ($@) {
	chomp $@;
	$@ =~ s/ at \S+ line \d+\.?\z//;
	$got = "die: $@";
    }
    my $from_line = (caller)[2];
    ok $got eq $want,
	qq{line $from_line\n# wanted [$want]\n# got    [$got]};
}

$testsub = \&shell_quote;
test '';
test q{''},			'';
test q{''},			undef;
test q{foo},			qw(foo);
test q{foo bar},		qw(foo bar);
test q{'foo*'},			qw(foo*);
test q{'foo bar'},		 q{foo bar};
test q{'foo'\''bar'},		qw(foo'bar);
test q{\''foo'},		qw('foo);
test q{foo 'bar*'},		qw(foo bar*);
test q{'foo'\''foo' bar 'baz'\'}, qw(foo'foo bar baz');
test q{'\'},			qw(\\);
test q{\'},			qw(');
test q{'\'\'},			qw(\');
test q{'a'"''"'b'},		qw(a''b);
test q{azAZ09_!%+,-./:@^},	 q{azAZ09_!%+,-./:@^};
test q{'foo=bar' command},	qw(foo=bar command);
test q{'foo=bar' 'baz=quux' command}, qw(foo=bar baz=quux command);
test
    "die: shell_quote(): No way to quote string containing null (\\000) bytes",
    "t\x00";

$testsub = \&shell_quote_best_effort;
test '';
test q{''},			'';
test q{''},			undef;
test q{'foo*'},			'foo*';
test q{'foo*' asdf},		'foo*', "as\x00df";

$testsub = \&shell_comment_quote;
test '';
test qq{foo},			qq{foo};
test qq{foo\n#bar},		qq{foo\nbar};
test qq{foo\n#bar\n#baz},	qq{foo\nbar\nbaz};
test "die: Too many arguments to shell_comment_quote (got 2 expected 1)",
	    'foo', 'bar';

sub via_shell {
    my @args = @_;
    my $cmd = 'blib/script/shell-quote';
    my $pid = open PIPE, '-|';
    defined $pid
	or return "can't fork: $!\n";
    if (!$pid) {
	if (!open STDERR, '>&STDOUT') {
	    print "$0: can't dup stdout: $!\n";
	    exit 1;
	}
	exec $cmd, @args
	    or die "$0: can't run $cmd: $!\n";
    }
    my $r = join '', <PIPE>;
    if (!close PIPE) {
	$r .= "$cmd failed: " . ($! ? $! : "non-zero exit $?") . "\n";
    }
    return $r;
}

if ($^O eq 'MSWin32') {
    print "ok # skip not working on MSWin32\n" x 4;
} else {
    $testsub = \&via_shell;
    test '';
    test qq{a\n},			'a';
    test qq{''\n},			'';
    test qq{foo 'bar baz' '*'\n},	'foo', 'bar baz', '*';
}
