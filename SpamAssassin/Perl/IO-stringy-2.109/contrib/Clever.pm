package IO::Clever;
require 5.005_03;
use strict;
use vars qw($VERSION @ISA);
@ISA = qw(IO::String);
$VERSION = "1.01";

# ChangeLog:
# 1999-07-21-02:06:47 Uri Guttman told me a critical fix: 
#	$fp->input_record_separator is _Global_; local($/) is safer

my(%params);

sub new {
	my $class = shift;
	return IO::File->new(@_) unless $_[0] =~ /^>/;
	my $self = bless IO::String->new(), ref($class) || $class;
	$params{$self} = [ @_ ];
	$self;
}

sub DESTROY {
	my($self) = @_;
	my $filename = $params{$self}->[0];
	return unless $filename =~ s/^>//;
	my($new) = ${$self->string_ref};
	if (-f $filename) {
		my $fp = IO::File->new("<$filename") || die "$0: $filename: $!\n";
		local ($/);
		return if $new eq $fp->getline;
	}
	IO::File->new(@{$params{$self}})->print($new);
	delete $params{$self};
}

1;
