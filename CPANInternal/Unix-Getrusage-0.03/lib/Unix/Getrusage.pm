package Unix::Getrusage;

use 5.008006;
use strict;
use warnings;

require Exporter;
use AutoLoader qw(AUTOLOAD);

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Unix::Getrusage ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(getrusage getrusage_children) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(getrusage getrusage_children);

our $VERSION = '0.03';

require XSLoader;
XSLoader::load('Unix::Getrusage', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Unix::Getrusage - Perl interface to the Unix B<getrusage> system call

=head1 SYNOPSIS

  use Unix::Getrusage;

  my $usage = getrusage; # getrusage(RUSAGE_SELF, ...)
  print "CPU time: ", $usage->{ru_utime}, "\n";

=head1 DESCRIPTION

Both I<getrusage> and I<getrusage_children> (no arguments) return what
the B<getrusage()> call returns: ressource utilization of either the
calling process or its children. They return hash references (to avoid
unneccessary copying) of the rusage struct:

  use Unix::Getrusage;
  use Data::Dumper;

  my $usage = getrusage; # see above
  print Data::Dumper->new([$usage])->Dump;

which outputs something like this:

  $VAR1 = {
            'ru_nivcsw' => '12',
            'ru_nvcsw' => '0',
            ...,
            'ru_utime' => '0.104414',
            'ru_stime' => '0.008031',
            'ru_nsignals' => '0'
          };

=head2 EXPORT

getrusage, getrusage_children

=head1 SEE ALSO

man 2 getrusage

=head1 AUTHOR

David Kroeber, E<lt>dk83@gmx.liE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by David Kroeber

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.6 or,
at your option, any later version of Perl 5 you may have available.


=cut

# $Id: Getrusage.pm 12 2006-02-04 00:54:08Z taffy $
