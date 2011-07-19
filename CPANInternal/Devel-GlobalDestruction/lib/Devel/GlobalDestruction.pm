#!/usr/bin/perl

package Devel::GlobalDestruction;

use strict;
use warnings;

use vars qw($VERSION @ISA);

BEGIN {
	$VERSION = '0.02';

	local $@;

	eval {
		require XSLoader;
		__PACKAGE__->XSLoader::load($VERSION);
		1;
	} or do {
		require DynaLoader;
		push @ISA, 'DynaLoader';
		__PACKAGE__->bootstrap($VERSION);
	};
}

use Sub::Exporter -setup => {
	exports => [ qw(in_global_destruction) ],
	groups  => { default => [ -all ] },
};

__PACKAGE__

__END__

=pod

=head1 NAME

Devel::GlobalDestruction - Expose PL_dirty, the flag which marks global
destruction.

=head1 SYNOPSIS

	package Foo;
	use Devel::GlobalDestruction;

	use namespace::clean; # to avoid having an "in_global_destruction" method

	sub DESTROY {
		return if in_global_destruction;

		do_something_a_little_tricky();
	}

=head1 DESCRIPTION

Perl's global destruction is a little tricky to deal with WRT finalizers
because it's not ordered and objects can sometimes disappear.

Writing defensive destructors is hard and annoying, and usually if global
destruction is happenning you only need the destructors that free up non
process local resources to actually execute.

For these constructors you can avoid the mess by simply bailing out if global
destruction is in effect.

=head1 EXPORTS

This module uses L<Sub::Exporter> so the exports may be renamed, aliased, etc.

=over 4

=item in_global_destruction

Returns the current value of C<PL_dirty>.

=back

=head1 VERSION CONTROL

This module is maintained using Darcs. You can get the latest version from
L<http://nothingmuch.woobling.org/code>, and use C<darcs send> to commit
changes.

=head1 AUTHOR

Yuval Kogman E<lt>nothingmuch@woobling.orgE<gt>

=head1 COPYRIGHT

	Copyright (c) 2008 Yuval Kogman. All rights reserved
	This program is free software; you can redistribute
	it and/or modify it under the same terms as Perl itself.

=cut


