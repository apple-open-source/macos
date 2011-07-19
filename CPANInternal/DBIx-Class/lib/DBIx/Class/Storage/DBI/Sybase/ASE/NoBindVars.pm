package DBIx::Class::Storage::DBI::Sybase::ASE::NoBindVars;

use base qw/
  DBIx::Class::Storage::DBI::NoBindVars
  DBIx::Class::Storage::DBI::Sybase::ASE
/;
use mro 'c3';
use List::Util ();
use Scalar::Util ();

sub _init {
  my $self = shift;
  $self->disable_sth_caching(1);
  $self->_identity_method('@@IDENTITY');
  $self->next::method (@_);
}

sub _fetch_identity_sql { 'SELECT ' . $_[0]->_identity_method }

my $number = sub { Scalar::Util::looks_like_number($_[0]) };

my $decimal = sub { $_[0] =~ /^ [-+]? \d+ (?:\.\d*)? \z/x };

my %noquote = (
    int => sub { $_[0] =~ /^ [-+]? \d+ \z/x },
    bit => => sub { $_[0] =~ /^[01]\z/ },
    money => sub { $_[0] =~ /^\$ \d+ (?:\.\d*)? \z/x },
    float => $number,
    real => $number,
    double => $number,
    decimal => $decimal,
    numeric => $decimal,
);

sub interpolate_unquoted {
  my $self = shift;
  my ($type, $value) = @_;

  return $self->next::method(@_) if not defined $value or not defined $type;

  if (my $key = List::Util::first { $type =~ /$_/i } keys %noquote) {
    return 1 if $noquote{$key}->($value);
  }
  elsif ($self->is_datatype_numeric($type) && $number->($value)) {
    return 1;
  }

  return $self->next::method(@_);
}

sub _prep_interpolated_value {
  my ($self, $type, $value) = @_;

  if ($type =~ /money/i && defined $value) {
    # change a ^ not followed by \$ to a \$
    $value =~ s/^ (?! \$) /\$/x;
  }

  return $value;
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::Sybase::ASE::NoBindVars - Storage::DBI subclass for
Sybase ASE without placeholder support

=head1 DESCRIPTION

If you're using this driver then your version of Sybase or the libraries you
use to connect to it do not support placeholders.

You can also enable this driver explicitly using:

  my $schema = SchemaClass->clone;
  $schema->storage_type('::DBI::Sybase::ASE::NoBindVars');
  $schema->connect($dsn, $user, $pass, \%opts);

See the discussion in L<< DBD::Sybase/Using ? Placeholders & bind parameters to
$sth->execute >> for details on the pros and cons of using placeholders.

One advantage of not using placeholders is that C<select @@identity> will work
for obtaining the last insert id of an C<IDENTITY> column, instead of having to
do C<select max(col)> in a transaction as the base Sybase driver does.

When using this driver, bind variables will be interpolated (properly quoted of
course) into the SQL query itself, without using placeholders.

The caching of prepared statements is also explicitly disabled, as the
interpolation renders it useless.

=head1 AUTHORS

See L<DBIx::Class/CONTRIBUTORS>.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
# vim:sts=2 sw=2:
