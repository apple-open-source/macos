use warnings;
use strict;

use Test::More tests => 2;

require_ok "abbrev.pl";

our %x;
my @z = qw(list edit send abort gripe listen);
&abbrev(*x, @z);
is_deeply \%x, {
	a => "abort",
	ab => "abort",
	abo => "abort",
	abor => "abort",
	abort => "abort",
	e => "edit",
	ed => "edit",
	edi => "edit",
	edit => "edit",
	g => "gripe",
	gr => "gripe",
	gri => "gripe",
	grip => "gripe",
	gripe => "gripe",
	list => "list",
	liste => "listen",
	listen => "listen",
	s => "send",
	se => "send",
	sen => "send",
	send => "send",
};

1;
