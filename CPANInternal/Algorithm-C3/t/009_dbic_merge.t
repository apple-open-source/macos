#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Algorithm::C3');
}

=pod

This example is taken from the inheritance graph of DBIx::Class::Core in DBIx::Class v0.07002:
(No ASCII art this time, this graph is insane)

The xx:: prefixes are just to be sure these bogus declarations never stomp on real ones

=cut

{
    package xx::DBIx::Class::Core;
    our @ISA = qw/
      xx::DBIx::Class::Serialize::Storable
      xx::DBIx::Class::InflateColumn
      xx::DBIx::Class::Relationship
      xx::DBIx::Class::PK::Auto
      xx::DBIx::Class::PK
      xx::DBIx::Class::Row
      xx::DBIx::Class::ResultSourceProxy::Table
      xx::DBIx::Class::AccessorGroup
    /;

    package xx::DBIx::Class::InflateColumn;
    our @ISA = qw/ xx::DBIx::Class::Row /;

    package xx::DBIx::Class::Row;
    our @ISA = qw/ xx::DBIx::Class /;

    package xx::DBIx::Class;
    our @ISA = qw/
      xx::DBIx::Class::Componentised
      xx::Class::Data::Accessor
    /;

    package xx::DBIx::Class::Relationship;
    our @ISA = qw/
      xx::DBIx::Class::Relationship::Helpers
      xx::DBIx::Class::Relationship::Accessor
      xx::DBIx::Class::Relationship::CascadeActions
      xx::DBIx::Class::Relationship::ProxyMethods
      xx::DBIx::Class::Relationship::Base
      xx::DBIx::Class
    /;

    package xx::DBIx::Class::Relationship::Helpers;
    our @ISA = qw/
      xx::DBIx::Class::Relationship::HasMany
      xx::DBIx::Class::Relationship::HasOne
      xx::DBIx::Class::Relationship::BelongsTo
      xx::DBIx::Class::Relationship::ManyToMany
    /;

    package xx::DBIx::Class::Relationship::ProxyMethods;
    our @ISA = qw/ xx::DBIx::Class /;

    package xx::DBIx::Class::Relationship::Base;
    our @ISA = qw/ xx::DBIx::Class /;

    package xx::DBIx::Class::PK::Auto;
    our @ISA = qw/ xx::DBIx::Class /;

    package xx::DBIx::Class::PK;
    our @ISA = qw/ xx::DBIx::Class::Row /;

    package xx::DBIx::Class::ResultSourceProxy::Table;
    our @ISA = qw/
      xx::DBIx::Class::AccessorGroup
      xx::DBIx::Class::ResultSourceProxy
    /;

    package xx::DBIx::Class::ResultSourceProxy;
    our @ISA = qw/ xx::DBIx::Class /;

    package xx::Class::Data::Accessor; our @ISA = ();
    package xx::DBIx::Class::Relationship::HasMany; our @ISA = ();
    package xx::DBIx::Class::Relationship::HasOne; our @ISA = ();
    package xx::DBIx::Class::Relationship::BelongsTo; our @ISA = ();
    package xx::DBIx::Class::Relationship::ManyToMany; our @ISA = ();
    package xx::DBIx::Class::Componentised; our @ISA = ();
    package xx::DBIx::Class::AccessorGroup; our @ISA = ();
    package xx::DBIx::Class::Serialize::Storable; our @ISA = ();
    package xx::DBIx::Class::Relationship::Accessor; our @ISA = ();
    package xx::DBIx::Class::Relationship::CascadeActions; our @ISA = ();
}

sub supers {
    no strict 'refs';
    @{$_[0] . '::ISA'};
}

is_deeply(
    [ Algorithm::C3::merge('xx::DBIx::Class::Core', \&supers) ],
    [qw/
        xx::DBIx::Class::Core
        xx::DBIx::Class::Serialize::Storable
        xx::DBIx::Class::InflateColumn
        xx::DBIx::Class::Relationship
        xx::DBIx::Class::Relationship::Helpers
        xx::DBIx::Class::Relationship::HasMany
        xx::DBIx::Class::Relationship::HasOne
        xx::DBIx::Class::Relationship::BelongsTo
        xx::DBIx::Class::Relationship::ManyToMany
        xx::DBIx::Class::Relationship::Accessor
        xx::DBIx::Class::Relationship::CascadeActions
        xx::DBIx::Class::Relationship::ProxyMethods
        xx::DBIx::Class::Relationship::Base
        xx::DBIx::Class::PK::Auto
        xx::DBIx::Class::PK
        xx::DBIx::Class::Row
        xx::DBIx::Class::ResultSourceProxy::Table
        xx::DBIx::Class::AccessorGroup
        xx::DBIx::Class::ResultSourceProxy
        xx::DBIx::Class
        xx::DBIx::Class::Componentised
        xx::Class::Data::Accessor
    /],
    '... got the right C3 merge order for DBIx::Class::Core');
