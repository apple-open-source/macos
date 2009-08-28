# vim: filetype=perl
use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

use POSIX qw(ceil);

my $schema = DBICTest->init_schema();

plan tests => 879;

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
foreach my $group_id (1..4) {
    foreach (1..6) {
        $employees->create({ name=>'temp', group_id=>$group_id });
    }
}
$employees = $employees->search(undef,{order_by=>'group_id,position'});

foreach my $group_id (1..4) {
    my $group_employees = $employees->search({group_id=>$group_id});
    $group_employees->all();
    ok( check_rs($group_employees), "group intial positions" );
    hammer_rs( $group_employees );
}

my $group_3 = $employees->search({group_id=>3});
my $to_group = 1;
my $to_pos = undef;
while (my $employee = $group_3->next) {
	$employee->move_to_group($to_group, $to_pos);
	$to_pos++;
	$to_group = $to_group==1 ? 2 : 1;
}
foreach my $group_id (1..4) {
    my $group_employees = $employees->search({group_id=>$group_id});
    $group_employees->all();
    ok( check_rs($group_employees), "group positions after move_to_group" );
}

my $employee = $employees->search({group_id=>4})->first;
$employee->position(2);
$employee->update;
ok( check_rs($employees->search_rs({group_id=>4})), "overloaded update 1" );
$employee = $employees->search({group_id=>4})->first;
$employee->update({position=>3});
ok( check_rs($employees->search_rs({group_id=>4})), "overloaded update 2" );
$employee = $employees->search({group_id=>4})->first;
$employee->group_id(1);
$employee->update;
ok(
	check_rs($employees->search_rs({group_id=>1})) && check_rs($employees->search_rs({group_id=>4})),
	"overloaded update 3"
);
$employee = $employees->search({group_id=>4})->first;
$employee->update({group_id=>2});
ok(
	check_rs($employees->search_rs({group_id=>2})) && check_rs($employees->search_rs({group_id=>4})),
	"overloaded update 4"
);
$employee = $employees->search({group_id=>4})->first;
$employee->group_id(1);
$employee->position(3);
$employee->update;
ok(
	check_rs($employees->search_rs({group_id=>1})) && check_rs($employees->search_rs({group_id=>4})),
	"overloaded update 5"
);
$employee = $employees->search({group_id=>4})->first;
$employee->group_id(2);
$employee->position(undef);
$employee->update;
ok(
	check_rs($employees->search_rs({group_id=>2})) && check_rs($employees->search_rs({group_id=>4})),
	"overloaded update 6"
);
$employee = $employees->search({group_id=>4})->first;
$employee->update({group_id=>1,position=>undef});
ok(
	check_rs($employees->search_rs({group_id=>1})) && check_rs($employees->search_rs({group_id=>4})),
	"overloaded update 7"
);

# multicol tests begin here
DBICTest::Employee->grouping_column(['group_id', 'group_id_2']);
$employees->delete();
foreach my $group_id (1..4) {
    foreach my $group_id_2 (1..4) {
        foreach (1..4) {
            $employees->create({ name=>'temp', group_id=>$group_id, group_id_2=>$group_id_2 });
        }
    }
}
$employees = $employees->search(undef,{order_by=>'group_id,group_id_2,position'});

foreach my $group_id (1..3) {
    foreach my $group_id_2 (1..3) {
        my $group_employees = $employees->search({group_id=>$group_id, group_id_2=>$group_id_2});
        $group_employees->all();
        ok( check_rs($group_employees), "group intial positions" );
        hammer_rs( $group_employees );
    }
}

# move_to_group, specifying group by hash
my $group_4 = $employees->search({group_id=>4});
$to_group = 1;
my $to_group_2_base = 7;
my $to_group_2 = 1;
$to_pos = undef;
while (my $employee = $group_4->next) {
	$employee->move_to_group({group_id=>$to_group, group_id_2=>$to_group_2}, $to_pos);
	$to_pos++;
    $to_group = ($to_group % 3) + 1;
    $to_group_2_base++;
    $to_group_2 = (ceil($to_group_2_base/3.0) %3) +1
}
foreach my $group_id (1..4) {
    foreach my $group_id_2 (1..4) {
        my $group_employees = $employees->search({group_id=>$group_id,group_id_2=>$group_id_2});
        $group_employees->all();
        ok( check_rs($group_employees), "group positions after move_to_group" );
    }
}

$employees->delete();
foreach my $group_id (1..4) {
    foreach my $group_id_2 (1..4) {
        foreach (1..4) {
            $employees->create({ name=>'temp', group_id=>$group_id, group_id_2=>$group_id_2 });
        }
    }
}
$employees = $employees->search(undef,{order_by=>'group_id,group_id_2,position'});

$employee = $employees->search({group_id=>4, group_id_2=>1})->first;
$employee->group_id(1);
$employee->update;
ok( 
    check_rs($employees->search_rs({group_id=>4, group_id_2=>1}))
    && check_rs($employees->search_rs({group_id=>1, group_id_2=>1})), 
    "overloaded multicol update 1" 
);

$employee = $employees->search({group_id=>4, group_id_2=>1})->first;
$employee->update({group_id=>2});
ok( check_rs($employees->search_rs({group_id=>4, group_id_2=>1}))
    && check_rs($employees->search_rs({group_id=>2, group_id_2=>1})), 
    "overloaded multicol update 2" 
);

$employee = $employees->search({group_id=>3, group_id_2=>1})->first;
$employee->group_id(1);
$employee->group_id_2(3);
$employee->update();
ok( check_rs($employees->search_rs({group_id=>3, group_id_2=>1}))
    && check_rs($employees->search_rs({group_id=>1, group_id_2=>3})),
    "overloaded multicol update 3" 
);

$employee = $employees->search({group_id=>3, group_id_2=>1})->first;
$employee->update({group_id=>2, group_id_2=>3});
ok( check_rs($employees->search_rs({group_id=>3, group_id_2=>1}))
    && check_rs($employees->search_rs({group_id=>2, group_id_2=>3})), 
    "overloaded multicol update 4" 
);

$employee = $employees->search({group_id=>3, group_id_2=>2})->first;
$employee->update({group_id=>2, group_id_2=>4, position=>2});
ok( check_rs($employees->search_rs({group_id=>3, group_id_2=>2}))
    && check_rs($employees->search_rs({group_id=>2, group_id_2=>4})), 
    "overloaded multicol update 5" 
);

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

