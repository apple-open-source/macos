#line 1
package Module::Install::WriteAll;

use strict;
use Module::Install::Base;

use vars qw{$VERSION @ISA $ISCORE};
BEGIN {
	$VERSION = '0.84';
	@ISA     = qw{Module::Install::Base};
	$ISCORE  = 1;
}

sub WriteAll {
	my $self = shift;
	my %args = (
		meta        => 1,
		sign        => 0,
		inline      => 0,
		check_nmake => 1,
		@_,
	);

	$self->sign(1)                if $args{sign};
	$self->admin->WriteAll(%args) if $self->is_admin;

	$self->check_nmake if $args{check_nmake};
	unless ( $self->makemaker_args->{PL_FILES} ) {
		$self->makemaker_args( PL_FILES => {} );
	}

	# Until ExtUtils::MakeMaker support MYMETA.yml, make sure
	# we clean it up properly ourself.
	$self->realclean_files('MYMETA.yml');

	if ( $args{inline} ) {
		$self->Inline->write;
	} else {
		$self->Makefile->write;
	}

	# The Makefile write process adds a couple of dependencies,
	# so write the META.yml files after the Makefile.
	$self->Meta->write        if $args{meta};
	$self->Meta->write_mymeta if $self->mymeta;

	return 1;
}

1;
