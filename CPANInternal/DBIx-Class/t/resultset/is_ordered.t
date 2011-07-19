use strict;
use warnings;

use lib qw(t/lib);
use Test::More;
use Test::Exception;
use DBICTest;

my $schema = DBICTest->init_schema();
my $rs = $schema->resultset('Artist');

ok !$rs->is_ordered, 'vanilla resultset is not ordered';

# Simple ordering with a single column
{
  my $ordered = $rs->search(undef, { order_by => 'artistid' });
  ok $ordered->is_ordered, 'Simple column ordering detected by is_ordered';
}

# Hashref order direction
{
  my $ordered = $rs->search(undef, { order_by => { -desc => 'artistid' } });
  ok $ordered->is_ordered, 'resultset with order direction is_ordered';
}

# Column ordering with literal SQL
{
  my $ordered = $rs->search(undef, { order_by => \'artistid DESC' });
  ok $ordered->is_ordered, 'resultset with literal SQL is_ordered';
}

# Multiple column ordering
{
  my $ordered = $rs->search(undef, { order_by => ['artistid', 'name'] });
  ok $ordered->is_ordered, 'ordering with multiple columns as arrayref is ordered';
}

# More complicated ordering
{
  my $ordered = $rs->search(undef, { 
    order_by => [
      { -asc => 'artistid' }, 
      { -desc => 'name' },
    ] 
  });
  ok $ordered->is_ordered, 'more complicated resultset ordering is_ordered';
}

# Empty multi-column ordering arrayref
{
  my $ordered = $rs->search(undef, { order_by => [] });
  ok !$ordered->is_ordered, 'ordering with empty arrayref is not ordered';
}

# Multi-column ordering syntax with empty hashref
{
  my $ordered = $rs->search(undef, { order_by => [{}] });
  ok !$ordered->is_ordered, 'ordering with [{}] is not ordered';
}

# Remove ordering after being set
{
  my $ordered = $rs->search(undef, { order_by => 'artistid' });
  ok $ordered->is_ordered, 'resultset with ordering applied works..';
  my $unordered = $ordered->search(undef, { order_by => undef });
  ok !$unordered->is_ordered, '..and is not ordered with ordering removed';
}

# Search without ordering
{
  my $ordered = $rs->search({ name => 'We Are Goth' }, { join => 'cds' });
  ok !$ordered->is_ordered, 'WHERE clause but no order_by is not ordered';
}

# Other functions without ordering
{
  # Join
  my $joined = $rs->search(undef, { join => 'cds' });
  ok !$joined->is_ordered, 'join but no order_by is not ordered';

  # Group By
  my $grouped = $rs->search(undef, { group_by => 'rank' });
  ok !$grouped->is_ordered, 'group_by but no order_by is not ordered';

  # Paging
  my $paged = $rs->search(undef, { page=> 5 });
  ok !$paged->is_ordered, 'paging but no order_by is not ordered';
}

done_testing;
