#!/usr/bin/perl -w
use strict;
BEGIN { require 't/security.pl' };
plan_security tests => 1;

my($xd, $security) = build_test('test');

is_output('security', 'list', ['-d', 'common'],
    ['    "/Library/Keychains/System.keychain"'], 'list -d common');
#is_output('security', 'list', ['-d', 'system'],
#    ['    "/Library/Keychains/System.keychain"'], 'list -d system');

1;
