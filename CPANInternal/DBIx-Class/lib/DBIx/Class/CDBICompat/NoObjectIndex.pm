package # hide from PAUSE
    DBIx::Class::CDBICompat::NoObjectIndex;

use strict;
use warnings;

=head1 NAME

DBIx::Class::CDBICompat::NoObjectIndex - Defines empty methods for object indexing. They do nothing

=head1 SYNOPSIS

    Part of CDBICompat

=head1 DESCRIPTION

Defines empty methods for object indexing.  They do nothing.

Using NoObjectIndex instead of LiveObjectIndex and nocache(1) is a little
faster because it removes code from the object insert and retrieve chains.

=cut

sub nocache { return 1 }

sub purge_object_index_every {}

sub purge_dead_from_object_index {}

sub remove_from_object_index {}

sub clear_object_index {}

1;
