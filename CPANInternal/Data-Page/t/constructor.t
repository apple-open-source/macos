#!perl -T

use warnings;
use strict;
use Test::More tests => 23;
use Test::Exception;
use_ok('Data::Page');

my $page = Data::Page->new(7, 10, 12);
isa_ok($page, 'Data::Page');

is($page->first_page, 1, "Adjusted to first possible page");

$page = Data::Page->new(0, 10, -1);
isa_ok($page, 'Data::Page');

is($page->first_page, 1, "Adjusted to first possible page");

throws_ok {
  my $page = Data::Page->new(12, -1, 1);
  }
  qr/one entry per page/, "Can't have entries-per-page less than 1";

# The new empty constructor means we might be empty, let's check for sensible defaults
$page = Data::Page->new;
is($page->entries_per_page,     10);
is($page->total_entries,        0);
is($page->entries_on_this_page, 0);
is($page->first_page,           1);
is($page->last_page,            1);
is($page->first,                0);
is($page->last,                 0);
is($page->previous_page,        undef);
is($page->current_page,         1);
is($page->next_page,            undef);
is($page->skipped,              0);
my @integers = (0 .. 100);
@integers = $page->splice(\@integers);
my $integers = join ',', @integers;
is($integers, '');

$page->current_page(undef);
is($page->current_page, 1);

$page->current_page(-5);
is($page->current_page, 1);

$page->current_page(5);
is($page->current_page, 1);

$page->total_entries(100);
$page->entries_per_page(20);
$page->current_page(2);
is($page->first, 21);
$page->current_page(3);
is($page->first, 41);
