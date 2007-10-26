#!/usr/bin/perl -w
use Test::More tests => 1;


eval "use Test::Signature 1.04";
SKIP: {
    skip "Test::Signature not installed.", 1 if $@;
    signature_ok();
}

