# ===========================================================================
# Net::LDAP::FilterMatch
# 
# LDAP entry matching
# 
# Hans Klunder <hans.klunder@bigfoot.com>
# Peter Marschall <peter@adpm.de>
#  Copyright (c) 2005-2006.
# 
# See below for documentation.
# 

package Net::LDAP::FilterMatch;

use strict;
use Net::LDAP::Filter;
use Net::LDAP::Schema;

use vars qw($VERSION);
$VERSION   = '0.17';

sub import {
  shift;

  push(@_, @Net::LDAP::Filter::approxMatchers) unless @_;
  @Net::LDAP::Filter::approxMatchers = grep { eval "require $_" } @_ ;
}

package Net::LDAP::Filter;

use vars qw(@approxMatchers);
@approxMatchers = qw(
  String::Approx
  Text::Metaphone
  Text::Soundex
);

sub _filterMatch($@);

sub _cis_equalityMatch($@);
sub _exact_equalityMatch($@);
sub _numeric_equalityMatch($@);
sub _cis_orderingMatch($@);
sub _numeric_orderingMatch($@);
sub _cis_greaterOrEqual($@);
sub _cis_lessOrEqual($@);
sub _cis_approxMatch($@);
sub _cis_substrings($@);
sub _exact_substrings($@);

# all known matches from the OL 2.2 schema,
*_bitStringMatch = \&_exact_equalityMatch;
*_booleanMatch = \&_cis_equalityMatch;             # this might need to be reworked
*_caseExactIA5Match = \&_exact_equalityMatch;
*_caseExactIA5SubstringsMatch = \&_exact_substrings;
*_caseExactMatch = \&_exact_equalityMatch;
*_caseExactOrderingMatch = \&_exact_orderingMatch;
*_caseExactSubstringsMatch = \&_exact_substrings;
*_caseIgnoreIA5Match = \&_cis_equalityMatch;
*_caseIgnoreIA5SubstringsMatch = \&_cis_substrings;
*_caseIgnoreMatch = \&_cis_equalityMatch;
*_caseIgnoreOrderingMatch = \&_cis_orderingMatch;
*_caseIgnoreSubstringsMatch = \&_cis_substrings;
*_certificateExactMatch = \&_exact_equalityMatch;
*_certificateMatch = \&_exact_equalityMatch;
*_distinguishedNameMatch = \&_exact_equalityMatch;
*_generalizedTimeMatch = \&_exact_equalityMatch;
*_generalizedTimeOrderingMatch = \&_exact_orderingMatch;
*_integerBitAndMatch = \&_exact_equalityMatch;      # this needs to be reworked
*_integerBitOrMatch = \&_exact_equalityMatch;       # this needs to be reworked
*_integerFirstComponentMatch = \&_exact_equalityMatch;
*_integerMatch = \&_numeric_equalityMatch;
*_integerOrderingMatch = \&_numeric_orderingMatch;
*_numericStringMatch = \&_numeric_equalityMatch;
*_numericStringOrderingMatch = \&_numeric_orderingMatch;
*_numericStringSubstringsMatch = \&_numeric_substrings;
*_objectIdentifierFirstComponentMatch = \&_exact_equalityMatch; # this needs to be reworked
*_objectIdentifierMatch = \&_exact_equalityMatch;
*_octetStringMatch = \&_exact_equalityMatch;
*_octetStringOrderingMatch = \&_exact_orderingMatch;
*_octetStringSubstringsMatch = \&_exact_substrings;
*_telephoneNumberMatch = \&_exact_equalityMatch;
*_telephoneNumberSubstringsMatch = \&_exact_substrings;
*_uniqueMemberMatch = \&_cis_equalityMatch;          # this needs to be reworked

sub match
{
  my $self = shift;
  my $entry = shift;
  my $schema =shift;

  return _filterMatch($self, $entry, $schema);
}

# map Ops to schema matches 
my %op2schema = qw(
	equalityMatch	equality
	greaterOrEqual	equality
	lessOrEqual	ordering
	approxMatch	approx
	substrings	substr
);

sub _filterMatch($@)
{
  my $filter = shift;
  my $entry = shift;
  my $schema = shift;

  keys(%{$filter}); # re-initialize each() operator
  my ($op, $args) = each(%{$filter});

  # handle combined filters
  if ($op eq 'and') {	# '(&()...)' => fail on 1st mismatch
    foreach my $subfilter (@{$args}) {
      return 0  if (!_filterMatch($subfilter, $entry));
    }  
    return 1;	# all matched or '(&)' => succeed
  }  
  if ($op eq 'or') {	# '(|()...)' => succeed on 1st match
    foreach my $subfilter (@{$args}) {
      return 1  if (_filterMatch($subfilter, $entry));
    }  
    return 0;	# none matched or '(|)' => fail
  }  
  if ($op eq 'not') {
    return (! _filterMatch($args, $entry));
  }  
  if ($op eq 'present') {
    #return 1  if (lc($args) eq 'objectclass');	# "all match" filter
    return ($entry->exists($args));
  }  

  # handle basic filters
  if ($op =~ /^(equalityMatch|greaterOrEqual|lessOrEqual|approxMatch|substrings)/o) {
    my $attr;
    my $assertion;
    my $match;

    if ($op eq 'substrings') {
      $attr = $args->{'type'};
      # build a regexp as assertion value
      $assertion = join('.*', map { "\Q$_\E" } map { values %$_ } @{$args->{'substrings'}});
      $assertion =  '^'. $assertion if (exists $args->{'substrings'}[0]{'initial'});
      $assertion .= '$'     if (exists $args->{'substrings'}[-1]{'final'});
    }
    else {
      $attr = $args->{'attributeDesc'};
      $assertion = $args->{'assertionValue'}
    }

    my @values = $entry->get_value($attr);

    # approx match is not standardized in schema
    if ($schema and ($op ne 'approxMatch') ) {
      # get matchingrule from schema, be sure that matching subs exist for every MR in your schema
      $match='_' . $schema->matchingrule_for_attribute( $attr, $op2schema{$op})
        or return undef;
    }
    else {
      # fall back on build-in logic
      $match='_cis_' . $op;
    }

    return eval( "$match".'($assertion,$op,@values)' ) ;
  }
  
  return undef;	# all other filters => fail with error
}

sub _cis_equalityMatch($@)
{
  my $assertion = shift;
  my $op = shift;

  return grep(/^\Q$assertion\E$/i, @_) ? 1 : 0;
}

sub _exact_equalityMatch($@)
{
  my $assertion = shift;
  my $op = shift;

  return grep(/^\Q$assertion\E$/, @_) ? 1 : 0;
}

sub _numeric_equalityMatch($@)
{
  my $assertion = shift;
  my $op = shift;

  return grep(/^\Q$assertion\E$/, @_) ? 1 : 0;
}

sub _cis_orderingMatch($@)
{
  my $assertion = shift;
  my $op = shift;

  if ($op eq 'greaterOrEqual') {
    return (grep { lc($_) ge lc($assertion) } @_) ? 1 : 0;
  }
  elsif ($op eq 'lessOrEqual') {
    return (grep { lc($_) le lc($assertion) } @_) ? 1 : 0;
  }
  else {
    return undef;   #something went wrong
  };
}

sub _exact_orderingMatch($@)
{
  my $assertion = shift;
  my $op = shift;

  if ($op eq 'greaterOrEqual') {
    return (grep { $_ ge $assertion } @_) ? 1 : 0;
  }
  elsif ($op eq 'lessOrEqual') {
    return (grep { $_ le $assertion } @_) ? 1 : 0;
  }
  else {
    return undef;   #something went wrong
  };
}

sub _numeric_orderingMatch($@)
{
  my $assertion = shift;
  my $op = shift;

  if ($op eq 'greaterOrEqual') {
    return (grep { $_ >= $assertion } @_) ? 1 : 0;
  }
  elsif ($op eq 'lessOrEqual') {
    return (grep { $_ <= $assertion } @_) ? 1 : 0;
  }
  else {
    return undef;   #something went wrong
  };
}

sub _cis_substrings($@)
{
  my $regex=shift;
  my $op=shift;
  return 1 if ($regex =~ /^$/);
  return grep(/\Q$regex\E/i, @_) ? 1 : 0;
}

sub _exact_substrings($@)
{
  my $regex=shift;
  my $op=shift;
  return 1 if ($regex =~ /^$/);
  return grep(/\Q$regex\E/, @_) ? 1 : 0;
}

# this one is here in case we don't use schema

sub _cis_greaterOrEqual($@)
{
  my $assertion=shift;
  my $op=shift;

  if (grep(!/^-?\d+$/o, $assertion, @_)) {	# numerical values only => compare numerically
      return _cis_orderingMatch($assertion,$op,@_);
  }
  else {
      return _numeric_orderingMatch($assertion,$op,@_);
  }  
}

*_cis_lessOrEqual = \&_cis_greaterOrEqual;

sub _cis_approxMatch($@)
{
  my $assertion=shift;
  my $op=shift;

  foreach (@approxMatchers) {
    # print "using $_\n";
    if (/String::Approx/){
      return String::Approx::amatch($assertion, @_) ? 1 : 0;
    }
    elsif (/Text::Metaphone/){
      my $metamatch = Text::Metaphone::Metaphone($assertion);
      return grep((Text::Metaphone::Metaphone($_) eq $metamatch), @_) ? 1 : 0;
    }
    elsif (/Text::Soundex/){
      my $smatch = Text::Soundex::soundex($assertion);
      return grep((Text::Soundex::soundex($_) eq $smatch), @_) ? 1 : 0;
    }
  }
  #we really have nothing, use plain regexp
  return 1 if ($assertion =~ /^$/);
  return grep(/^$assertion$/i, @_) ? 1 : 0;
}

1;
  
  
__END__

=head1 NAME

Net::LDAP::FilterMatch - LDAP entry matching

=head1 SYNOPSIS

  use Net::LDAP::Entry;
  use Net::LDAP::Filter;
  use Net::LDAP::FilterMatch;

  my $entry = new Net::LDAP::Entry;
  $entry->dn("cn=dummy entry");
  $entry->add (
   'cn' => 'dummy entry',
   'street' => [ '1 some road','nowhere' ] );
   
  my @filters = (qw/(cn=dummy*)
                 (ou=*)
                 (&(cn=dummy*)(street=*road))
                 (&(cn=dummy*)(!(street=nowhere)))/);


  for (@filters) {
    my $filter = Net::LDAP::Filter->new($_);
    print $_,' : ', $filter->match($entry) ? 'match' : 'no match' ,"\n";
  }

=head1 ABSTRACT

This extension of the class Net::LDAP::Filter provides entry matching
functionality on the Perl side.

Given an entry it will tell whether the entry matches the filter object.

It can be used on its own or as part of a Net::LDAP::Server based LDAP server.

=head1 METHOD

=over 4

=item match ( ENTRY [ ,SCHEMA ] )

Return whether ENTRY matches the filter object. If a schema object is provided, 
the selection of matching algorithms will be derived from schema.

In case of error undef is returned.

=back

For approximate matching like (cn~=Schmidt) there are several modules that can
be used. By default the following modules will be tried in this order:

  String::Approx
  Text::Metaphone
  Text::Soundex

If none of these modules is found it will fall back on a simple regexp algorithm.

If you want to specifically use one implementation only, simply do

  use Net::LDAP::FilterMatch qw(Text::Soundex);

=head1 SEE ALSO

L<Net::LDAP::Filter> 

=head1 COPYRIGHT

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHORS

Hans Klunder E<lt>hans.klunder@bigfoot.comE<gt>
Peter Marschall E<lt>peter@adpm.deE<gt>

=cut

# EOF
