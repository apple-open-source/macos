#!/usr/bin/perl -w

use strict;
use warnings;
BEGIN { require 't/security.pl' };
plan_security tests => 6;

$ENV{HOME}="/tmp/test$$";
ok(mkdir($ENV{HOME}), 'setup home');
is_output('security', 'create-keychain', [qw(-p test test-unlock)],
    [],
    'create kc');
TODO: {
	local $TODO = "<rdar://problem/3835412> Unlocking an unlocked keychain with the wrong password succeeds";
	is_output('security', 'unlock-keychain', ['-p', 'wrong', 'test-unlock'],
	    ['security: SecKeychainUnlock test-unlock: The user name or passphrase you entered is not correct.'],
	    'unlock unlocked kc w/ wrong pw');
};
is_output('security', 'lock-keychain', ['test-unlock'],
    [],
    'lock');
is_output('security', 'unlock-keychain', ['-p', 'wrong', 'test-unlock'],
    ['security: SecKeychainUnlock test-unlock: The user name or passphrase you entered is not correct.'],
    'unlock locked kc w/ wrong pw');
ok(system("rm -rf '$ENV{HOME}'") eq 0, 'cleanup home');

1;
