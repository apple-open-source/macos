package Module::Build::Platform::Unix;

use strict;
use Module::Build::Base;

use vars qw(@ISA);
@ISA = qw(Module::Build::Base);


sub make_tarball {
  my ($self, $dir) = @_;

  my $tar_flags = $self->{properties}{verbose} ? 'cvf' : 'cf';

  my $tar = $self->{args}{tar}  || 'tar';
  $self->do_system("$tar $tar_flags '$dir.tar' '$dir'");
  my $gzip = $self->{args}{gzip} || 'gzip';
  $self->do_system("$gzip '$dir.tar'");
}

1;
__END__


=head1 NAME

Module::Build::Platform::Unix - Builder class for Unix platforms

=head1 DESCRIPTION

The sole purpose of this module is to inherit from
C<Module::Build::Base>.  Please see the L<Module::Build> for the docs.

=head1 AUTHOR

Ken Williams, ken@forum.swarthmore.edu

=head1 SEE ALSO

perl(1), Module::Build(3), ExtUtils::MakeMaker(3)

=cut
