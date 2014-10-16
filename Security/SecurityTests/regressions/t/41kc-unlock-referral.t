#!/usr/bin/perl -w

use strict;
use warnings;
BEGIN { require 't/security.pl' };
plan_security tests => 7;

$ENV{HOME}="/tmp/test$$";
ok(mkdir($ENV{HOME}), 'setup home');
my $source = "$ENV{HOME}/source";
my $dest = "$ENV{HOME}/dest";
is_output('security', 'create-keychain', ['-p', 'test', $source],
    [],
    'create source');
is_output('security', 'create-keychain', ['-p', 'test', $dest],
    [],
    'create dest');
SKIP: {
	skip "systemkeychain brings up UI", 1;

	is_output('systemkeychain', '-k', [$dest, '-s', $source],
	    [],
	    'systemkeychain');
}
is_output('security', 'lock-keychain', [$source],
    [],
    'lock source');
SKIP: {
	skip "systemkeychain bring up UI", 1;

	is_output('security', 'unlock-keychain', ['-u', $source],
	    [],
	    'unlock source w/ referal');
}
ok(system("rm -rf '$ENV{HOME}'") eq 0, 'cleanup home');

1;
