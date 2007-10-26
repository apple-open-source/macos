# vim: filetype=perl
use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 321;

my $employees = $schema->resultset('Employee');
$employees->delete();

foreach (1..5) {
    $employees->create({ name=>'temp' });
}
$employees = $employees->search(undef,{order_by=>'position'});
ok( check_rs($employees), "intial positions" );

hammer_rs( $employees );

DBICTest::Employee->grouping_column('group_id');
$employees->delete();
foreach my $group_id (1..3) {
    foreach (1..6) {
        $employees->create({ name=>'temp', group_id=>$group_id });
    }
}
$employees = $employees->search(undef,{order_by=>'group_id,position'});

foreach my $group_id (1..3) {
    my $group_employees = $employees->search({group_id=>$group_id});
    $group_employees->all();
    ok( check_rs($group_employees), "group intial positions" );
    hammer_rs( $group_employees );
}

sub hammer_rs {
    my $rs = shift;
    my $employee;
    my $count = $rs->count();
    my $position_column = $rs->result_class->position_column();
    my $row;

    foreach my $position (1..$count) {

        ($row) = $rs->search({ $position_column=>$position })->all();
        $row->move_previous();
        ok( check_rs($rs), "move_previous( $position )" );

        ($row) = $rs->search({ $position_column=>$position })->all();
        $row->move_next();
        ok( check_rs($rs), "move_next( $position )" );

        ($row) = $rs->search({ $position_column=>$position })->all();
        $row->move_first();
        ok( check_rs($rs), "move_first( $position )" );

        ($row) = $rs->search({ $position_column=>$position })->all();
        $row->move_last();
        ok( check_rs($rs), "move_last( $position )" );

        foreach my $to_position (1..$count) {
            ($row) = $rs->search({ $position_column=>$position })->all();
            $row->move_to($to_position);
            ok( check_rs($rs), "move_to( $position => $to_position )" );
        }

        ($row) = $rs->search({ position=>$position })->all();
        if ($position==1) {
            ok( !$row->previous_sibling(), 'no previous sibling' );
            ok( !$row->first_sibling(), 'no first sibling' );
        }
        else {
            ok( $row->previous_sibling(), 'previous sibling' );
            ok( $row->first_sibling(), 'first sibling' );
        }
        if ($position==$count) {
            ok( !$row->next_sibling(), 'no next sibling' );
            ok( !$row->last_sibling(), 'no last sibling' );
        }
        else {
            ok( $row->next_sibling(), 'next sibling' );
            ok( $row->last_sibling(), 'last sibling' );
        }

    }
}

sub check_rs {
    my( $rs ) = @_;
    $rs->reset();
    my $position_column = $rs->result_class->position_column();
    my $expected_position = 0;
    while (my $row = $rs->next()) {
        $expected_position ++;
        if ($row->get_column($position_column)!=$expected_position) {
            return 0;
        }
    }
    return 1;
}

