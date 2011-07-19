#!perl

use strict;
use warnings;

use Variable::Magic qw/wizard cast VMG_OP_INFO_NAME/;

{
 my $wiz = wizard set  => sub { print "now set to ${$_[0]}!\n" },
                  free => sub { print "destroyed!\n" };

 my $a = 1;
 cast $a, $wiz;
 $a = 2;        # "now set to 2!"
}               # "destroyed!"

{
 my $wiz = wizard data     => sub { $_[1] },
                  fetch    => sub { $_[2] = $_[1] unless exists $_[0]->{$_[2]}; () },
                  store    => sub { print "key $_[2] stored in $_[-1]\n" },
                  copy_key => 1,
                  op_info  => VMG_OP_INFO_NAME;

 my %h = (_default => 0, apple => 2);
 cast %h, $wiz, '_default';
 print $h{banana}, "\n"; # "0", because the 'banana' key doesn't exist in %h
 $h{pear} = 1;           # "key pear stored in helem"
}
