#line 1
package Module::Install::MakeMaker;

use strict;
use Module::Install::Base;
use ExtUtils::MakeMaker ();

use vars qw{$VERSION $ISCORE @ISA};
BEGIN {
	$VERSION = '0.68';
	$ISCORE  = 1;
	@ISA     = qw{Module::Install::Base};
}

my $makefile;
sub WriteMakefile {
    my ($self, %args) = @_;
    $makefile = $self->load('Makefile');

    # mapping between MakeMaker and META.yml keys
    $args{MODULE_NAME} = $args{NAME};
    unless ($args{NAME} = $args{DISTNAME} or !$args{MODULE_NAME}) {
        $args{NAME} = $args{MODULE_NAME};
        $args{NAME} =~ s/::/-/g;
    }

    foreach my $key (qw(name module_name version version_from abstract author installdirs)) {
        my $value = delete($args{uc($key)}) or next;
        $self->$key($value);
    }

    if (my $prereq = delete($args{PREREQ_PM})) {
        while (my($k,$v) = each %$prereq) {
            $self->requires($k,$v);
        }
    }

    # put the remaining args to makemaker_args
    $self->makemaker_args(%args);
}

END {
    if ( $makefile ) {
        $makefile->write;
        $makefile->Meta->write;
    }
}

1;
