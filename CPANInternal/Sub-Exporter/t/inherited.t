#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests check that the inherited form of a routine is the exported one.

=cut

use Test::More tests => 3;

BEGIN { use_ok('Sub::Exporter'); }

package E::Parent;
use Sub::Exporter -setup => { exports => [ qw(foo) ] };

sub foo { return 1; }

package E::Child;
use base qw(E::Parent);

sub foo { return 2; }

package Test::Sub::Exporter::EPARENT;
E::Parent->import('foo');

main::is(foo(), 1, "get result of parent's import");

package Test::Sub::Exporter::ECHILD;
E::Child->import('foo');

main::is(foo(), 2, "get result of child's import");
