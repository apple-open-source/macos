package Bencode;
BEGIN {
  $Bencode::VERSION = '1.4';
}
use strict;
use Carp;
use Exporter;

# ABSTRACT: BitTorrent serialisation format

use vars qw( $VERSION @ISA @EXPORT_OK $DEBUG $do_lenient_decode $max_depth );

@ISA = qw( Exporter );
@EXPORT_OK = qw( bencode bdecode );

sub _msg { sprintf "@_", pos() || 0 }

sub _bdecode_string {

	if ( m/ \G ( 0 | [1-9] \d* ) : /xgc ) {
		my $len = $1;

		croak _msg 'unexpected end of string data starting at %s'
			if $len > length() - pos();

		my $str = substr $_, pos(), $len;
		pos() = pos() + $len;

		warn _msg STRING => "(length $len)", $len < 200 ? "[$str]" : () if $DEBUG;

		return $str;
	}
	else {
		my $pos = pos();
		if ( m/ \G -? 0? \d+ : /xgc ) {
			pos() = $pos;
			croak _msg 'malformed string length at %s';
		}
	}

	return;
}

sub _bdecode_chunk {
	warn _msg 'decoding at %s' if $DEBUG;

	local $max_depth = $max_depth - 1 if defined $max_depth;

	if ( defined( my $str = _bdecode_string() ) ) {
		return $str;
	}
	elsif ( m/ \G i /xgc ) {
		croak _msg 'unexpected end of data at %s' if m/ \G \z /xgc;

		m/ \G ( 0 | -? [1-9] \d* ) e /xgc
			or croak _msg 'malformed integer data at %s';

		warn _msg INTEGER => $1 if $DEBUG;
		return $1;
	}
	elsif ( m/ \G l /xgc ) {
		warn _msg 'LIST' if $DEBUG;

		croak _msg 'nesting depth exceeded at %s'
			if defined $max_depth and $max_depth < 0;

		my @list;
		until ( m/ \G e /xgc ) {
			warn _msg 'list not terminated at %s, looking for another element' if $DEBUG;
			push @list, _bdecode_chunk();
		}
		return \@list;
	}
	elsif ( m/ \G d /xgc ) {
		warn _msg 'DICT' if $DEBUG;

		croak _msg 'nesting depth exceeded at %s'
			if defined $max_depth and $max_depth < 0;

		my $last_key;
		my %hash;
		until ( m/ \G e /xgc ) {
			warn _msg 'dict not terminated at %s, looking for another pair' if $DEBUG;

			croak _msg 'unexpected end of data at %s'
				if m/ \G \z /xgc;

			my $key = _bdecode_string();
			defined $key or croak _msg 'dict key is not a string at %s';

			croak _msg 'duplicate dict key at %s'
				if exists $hash{ $key };

			croak _msg 'dict key not in sort order at %s'
				if not( $do_lenient_decode ) and defined $last_key and $key lt $last_key;

			croak _msg 'dict key is missing value at %s'
				if m/ \G e /xgc;

			$last_key = $key;
			$hash{ $key } = _bdecode_chunk();
		}
		return \%hash;
	}
	else {
		croak _msg m/ \G \z /xgc ? 'unexpected end of data at %s' : 'garbage at %s';
	}
}

sub bdecode {
	local $_ = shift;
	local $do_lenient_decode = shift;
	local $max_depth = shift;
	my $deserialised_data = _bdecode_chunk();
	croak _msg 'trailing garbage at %s' if $_ !~ m/ \G \z /xgc;
	return $deserialised_data;
}

sub _bencode {
	my ( $data ) = @_;
	if ( not ref $data ) {
		return sprintf 'i%se', $data if $data =~ m/\A (?: 0 | -? [1-9] \d* ) \z/x;
		return length( $data ) . ':' . $data;
	}
	elsif ( ref $data eq 'SCALAR' ) {
		# escape hatch -- use this to avoid num/str heuristics
		return length( $$data ) . ':' . $$data;
	}
	elsif ( ref $data eq 'ARRAY' ) {
		return 'l' . join( '', map _bencode( $_ ), @$data ) . 'e';
	}
	elsif ( ref $data eq 'HASH' ) {
		return 'd' . join( '', map { _bencode( \$_ ), _bencode( $data->{ $_ } ) } sort keys %$data ) . 'e';
	}
	else {
		croak 'unhandled data type';
	}
}

sub bencode {
	croak 'need exactly one argument' if @_ != 1;
	goto &_bencode;
}

bdecode( 'i1e' );



=pod

=head1 NAME

Bencode - BitTorrent serialisation format

=head1 VERSION

version 1.4

=head1 SYNOPSIS

 use Bencode qw( bencode bdecode );
 
 my $bencoded = bencode { 'age' => 25, 'eyes' => 'blue' };
 print $bencoded, "\n";
 my $decoded = bdecode $bencoded;

=head1 DESCRIPTION

This module implements the BitTorrent I<bencode> serialisation format as described in L<http://www.bittorrent.org/protocol.html>.

=head1 INTERFACE 

=head2 C<bencode( $datastructure )>

Takes a single argument which may be a scalar or a reference to a scalar, array or hash. Arrays and hashes may in turn contain values of these same types. Simple scalars that look like canonically represented integers will be serialised as such. To bypass the heuristic and force serialisation as a string, use a reference to a scalar.

Croaks on unhandled data types.

=head2 C<bdecode( $string [, $do_lenient_decode [, $max_depth ] ] )>

Takes a string and returns the corresponding deserialised data structure.

If you pass a true value for the second option, it will disregard the sort order of dict keys. This violation of the I<bencode> format is somewhat common.

If you pass an integer for the third option, it will croak when attempting to parse dictionaries nested deeper than this level, to prevent DoS attacks using maliciously crafted input.

Croaks on malformed data.

=head1 DIAGNOSTICS

=over

=item C<trailing garbage at %s>

Your data does not end after the first I<bencode>-serialised item.

You may also get this error if a malformed item follows.

=item C<garbage at %s>

Your data is malformed.

=item C<unexpected end of data at %s>

Your data is truncated.

=item C<unexpected end of string data starting at %s>

Your data includes a string declared to be longer than the available data.

=item C<malformed string length at %s>

Your data contained a string with negative length or a length with leading zeroes.

=item C<malformed integer data at %s>

Your data contained something that was supposed to be an integer but didn't make sense.

=item C<dict key not in sort order at %s>

Your data violates the I<bencode> format constaint that dict keys must appear in lexical sort order.

=item C<duplicate dict key at %s>

Your data violates the I<bencode> format constaint that all dict keys must be unique.

=item C<dict key is not a string at %s>

Your data violates the I<bencode> format constaint that all dict keys be strings.

=item C<dict key is missing value at %s>

Your data contains a dictionary with an odd number of elements.

=item C<nesting depth exceeded at %s>

Your data contains dicts or lists that are nested deeper than the $max_depth passed to C<bdecode()>.

=item C<unhandled data type>

You are trying to serialise a data structure that consists of data types other than

=over

=item * scalars

=item * references to arrays

=item * references to hashes

=item * references to scalars

=back

The format does not support this.

=back

=head1 BUGS AND LIMITATIONS

Strings and numbers are practically indistinguishable in Perl, so C<bencode()> has to resort to a heuristic to decide how to serialise a scalar. This cannot be fixed.

Please report any bugs or feature requests through the web interface at L<http://github.com/ap/Bencode/issues>.

=head1 AUTHOR

  Aristotle Pagaltzis <pagaltzis@gmx.de>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2010 by Aristotle Pagaltzis.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

