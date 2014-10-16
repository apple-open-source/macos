package File::ExtAttr::Tie;

=head1 NAME

File::ExtAttr::Tie - Tie interface to extended attributes of files

=head1 SYNOPSIS

  use File::ExtAttr::Tie;
  use Data::Dumper;

  tie %a,
    "File::ExtAttr::Tie", "/Applications (Mac  OS 9)/Sherlock 2",
    { namespace => 'user' };
  print Dumper \%a;

produces:

  $VAR1 = {
           'com.apple.FinderInfo' => 'APPLfndf!?',
           'com.apple.ResourceFork' => '?p?p5I'
          };

=head1 DESCRIPTION

File::ExtAttr::Tie provides access to extended attributes of a file
through a tied hash. Creating a new key creates a new extended attribute
associated with the file. Modifying the value or removing a key likewise
modifies/removes the extended attribute.

Internally this module uses the File::ExtAttr module. So it has
the same restrictions as that module in terms of OS support.

=head1 METHODS

=over 4

=item tie "File::ExtAttr::Tie", $filename, [\%flags]

The flags are the same optional flags as in File::ExtAttr.  Any flags
given here will be passed to all operations on the tied hash.
Only the C<namespace> flag makes sense. The hash will be tied
to the default namespace, if no flags are given.

=back

=cut

use strict;
use base qw(Tie::Hash);
use File::ExtAttr qw(:all);

our $VERSION = '0.01';

sub TIEHASH {
  my($class, $file, $flags) = @_;
  my $self = bless { file => $file }, ref $class || $class;
  $self->{flags} = defined($flags) ? $flags : {};
  return $self;
}

sub STORE {
  my($self, $name, $value) = @_;
  return undef unless setfattr($self->{file}, $name, $value, $self->{flags});
  $value;
}

sub FETCH {
  my($self, $name) = @_;
  return getfattr($self->{file}, $name, $self->{flags});
}

sub FIRSTKEY {
  my($self) = @_;
  $self->{each_list} = [listfattr($self->{file}, $self->{flags})];
  shift @{$self->{each_list}};
}

sub NEXTKEY {
  my($self) = @_;
  shift @{$self->{each_list}};
}

sub EXISTS {
  my($self, $name) = @_;
  return getfattr($self->{file}, $name, $self->{flags}) ne undef;
}

sub DELETE {
  my($self, $name) = @_;
  # XXX: Race condition
  my $value = getfattr($self->{file}, $name, $self->{flags});
  return $value if delfattr($self->{file}, $name, $self->{flags});
  undef;
}

sub CLEAR {
  my($self) = @_;
  for(listfattr($self->{file})) {
    delfattr($self->{file}, $_, $self->{flags});
  }
}

#sub SCALAR { }

=head1 SEE ALSO

L<File::ExtAttr>

=head1 AUTHOR

David Leadbeater, L<http://dgl.cx/contact>

Documentation by Richard Dawe, E<lt>richdawe@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by David Leadbeater

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

1;
__END__
