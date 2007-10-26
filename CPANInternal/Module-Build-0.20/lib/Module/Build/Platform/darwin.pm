package Module::Build::Platform::darwin;

use strict;
use Module::Build::Platform::Unix;

use vars qw(@ISA);
@ISA = qw(Module::Build::Platform::Unix);

sub compile_c {
  my ($self, $file) = @_;

  # Perhaps they'll fix this in later versions, so don't tinker if it's fixed
  return $self->SUPER::compile_c($file) unless $self->{config}{ccflags} =~ /-flat_namespace/;

  # -flat_namespace isn't a compile flag, it's a linker flag.  But
  # it's mistakenly in Config.pm as both.  Make the correction here.
  local $self->{config}{ccflags} = $self->{config}{ccflags};
  $self->{config}{ccflags} =~ s/-flat_namespace//;
  $self->SUPER::compile_c($file);
}


1;
__END__


=head1 NAME

Module::Build::Platform::darwin - Builder class for Mac OS X platform

=head1 DESCRIPTION

This module provides some routines very specific to the Mac OS X
platform.

Please see the L<Module::Build> for the general docs.

=head1 AUTHOR

Ken Williams, ken@forum.swarthmore.edu

=head1 SEE ALSO

perl(1), Module::Build(3), ExtUtils::MakeMaker(3)

=cut
