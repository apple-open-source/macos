#!/usr/bin/perl

use Data::UUID 'NameSpace_URL';

#my $du = Data::UUID->new;


#$du->create_str;


if (fork()) {
    print "GOT :".Data::UUID->new->create_str;
    exit;
}

print "GOT :".Data::UUID->new->create_str;
