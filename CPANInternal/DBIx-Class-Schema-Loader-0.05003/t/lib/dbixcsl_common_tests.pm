package dbixcsl_common_tests;

use strict;
use warnings;

use Test::More;
use DBIx::Class::Schema::Loader;
use Class::Unload;
use File::Path;
use DBI;
use Digest::MD5;
use File::Find 'find';

my $DUMP_DIR = './t/_common_dump';
rmtree $DUMP_DIR;

sub new {
    my $class = shift;

    my $self;

    if( ref($_[0]) eq 'HASH') {
       my $args = shift;
       $self = { (%$args) };
    }
    else {
       $self = { @_ };
    }

    # Only MySQL uses this
    $self->{innodb} ||= '';

    # DB2 doesn't support this
    $self->{null} = 'NULL' unless defined $self->{null};
    
    $self->{verbose} = $ENV{TEST_VERBOSE} || 0;

    # Optional extra tables and tests
    $self->{extra} ||= {};

    # Not all DBS do SQL-standard CURRENT_TIMESTAMP
    $self->{default_function} ||= "CURRENT_TIMESTAMP";
    $self->{default_function_def} ||= "TIMESTAMP DEFAULT $self->{default_function}";

    return bless $self => $class;
}

sub skip_tests {
    my ($self, $why) = @_;

    plan skip_all => $why;
}

sub _monikerize {
    my $name = shift;
    return 'LoaderTest2X' if $name =~ /^loader_test2$/i;
    return undef;
}

sub _custom_column_info {
    my ( $table_name, $column_name, $column_info ) = @_;

    $table_name = lc ( $table_name );
    $column_name = lc ( $column_name );

    if ( $table_name eq 'loader_test11' 
        and $column_name eq 'loader_test10' 
    ){
        return { is_numeric => 1 }
    }
    # Set inflate_datetime or  inflate_date to check 
    #   datetime_timezone and datetime_locale
    if ( $table_name eq 'loader_test36' ){
        return { inflate_datetime => 1 } if 
            ( $column_name eq 'b_char_as_data' );
        return { inflate_date => 1 } if 
            ( $column_name eq 'c_char_as_data' );
    }

    return;
}

sub run_tests {
    my $self = shift;

    plan tests => 156 + ($self->{extra}->{count} || 0);

    $self->create();

    my @connect_info = (
	$self->{dsn},
	$self->{user},
	$self->{password},
	$self->{connect_info_opts},
    );

    # First, with in-memory classes
    my $schema_class = $self->setup_schema(@connect_info);
    $self->test_schema($schema_class);
    $self->drop_tables;
}

# defined in sub create
my (@statements, @statements_reltests, @statements_advanced,
    @statements_advanced_sqlite, @statements_inline_rels,
    @statements_implicit_rels);

sub setup_schema {
    my $self = shift;
    my @connect_info = @_;

    my $schema_class = 'DBIXCSL_Test::Schema';

    my $debug = ($self->{verbose} > 1) ? 1 : 0;

    my %loader_opts = (
        constraint              =>
	    qr/^(?:\S+\.)?(?:(?:$self->{vendor}|extra)_)?loader_test[0-9]+(?!.*_)/i,
        relationships           => 1,
        additional_classes      => 'TestAdditional',
        additional_base_classes => 'TestAdditionalBase',
        left_base_classes       => [ qw/TestLeftBase/ ],
        components              => [ qw/TestComponent/ ],
        resultset_components    => [ qw/TestRSComponent/ ],
        inflect_plural          => { loader_test4 => 'loader_test4zes' },
        inflect_singular        => { fkid => 'fkid_singular' },
        moniker_map             => \&_monikerize,
        custom_column_info      => \&_custom_column_info,
        debug                   => $debug,
        use_namespaces          => 0,
        dump_directory          => $DUMP_DIR,
        datetime_timezone       => 'Europe/Berlin',
        datetime_locale         => 'de_DE'
    );

    $loader_opts{db_schema} = $self->{db_schema} if $self->{db_schema};

    {
       my @loader_warnings;
       local $SIG{__WARN__} = sub { push(@loader_warnings, $_[0]); };
        eval qq{
            package $schema_class;
            use base qw/DBIx::Class::Schema::Loader/;
    
            __PACKAGE__->loader_options(\%loader_opts);
            __PACKAGE__->connection(\@connect_info);
        };

       ok(!$@, "Loader initialization") or diag $@;

       my $file_count;
       find sub { return if -d; $file_count++ }, $DUMP_DIR;

       my $expected_count = 36;

       $expected_count += grep /CREATE (?:TABLE|VIEW)/i,
           @{ $self->{extra}{create} || [] };

       $expected_count -= grep /CREATE TABLE/, @statements_inline_rels
           if $self->{skip_rels} || $self->{no_inline_rels};

       $expected_count -= grep /CREATE TABLE/, @statements_implicit_rels
           if $self->{skip_rels} || $self->{no_implicit_rels};

       $expected_count -= grep /CREATE TABLE/, ($self->{vendor} =~ /sqlite/ ? @statements_advanced_sqlite : @statements_advanced), @statements_reltests
           if $self->{skip_rels};

       is $file_count, $expected_count, 'correct number of files generated';

       exit if $file_count != $expected_count;

       my $warn_count = 2;
       $warn_count++ if grep /ResultSetManager/, @loader_warnings;

       $warn_count++ for grep /^Bad table or view/, @loader_warnings;

       $warn_count++ for grep /stripping trailing _id/, @loader_warnings;

       my $vendor = $self->{vendor};
       $warn_count++ for grep /${vendor}_\S+ has no primary key/,
           @loader_warnings;

        if($self->{skip_rels}) {
            SKIP: {
                is(scalar(@loader_warnings), $warn_count, "No loader warnings")
                    or diag @loader_warnings;
                skip "No missing PK warnings without rels", 1;
            }
        }
        else {
	    $warn_count++;
            is(scalar(@loader_warnings), $warn_count, "Expected loader warning")
                or diag @loader_warnings;
            is(grep(/loader_test9 has no primary key/, @loader_warnings), 1,
                 "Missing PK warning");
        }
    }
    
    return $schema_class;
}

sub test_schema {
    my $self = shift;
    my $schema_class = shift;

    my $conn = $schema_class->clone;
    my $monikers = {};
    my $classes = {};
    foreach my $source_name ($schema_class->sources) {
        my $table_name = $schema_class->source($source_name)->from;
        $monikers->{$table_name} = $source_name;
        $classes->{$table_name} = $schema_class . q{::} . $source_name;
    }

    my $moniker1 = $monikers->{loader_test1s};
    my $class1   = $classes->{loader_test1s};
    my $rsobj1   = $conn->resultset($moniker1);
    check_no_duplicate_unique_constraints($class1);

    my $moniker2 = $monikers->{loader_test2};
    my $class2   = $classes->{loader_test2};
    my $rsobj2   = $conn->resultset($moniker2);
    check_no_duplicate_unique_constraints($class2);

    my $moniker23 = $monikers->{LOADER_TEST23};
    my $class23   = $classes->{LOADER_TEST23};
    my $rsobj23   = $conn->resultset($moniker1);

    my $moniker24 = $monikers->{LoAdEr_test24};
    my $class24   = $classes->{LoAdEr_test24};
    my $rsobj24   = $conn->resultset($moniker2);

    my $moniker35 = $monikers->{loader_test35};
    my $class35   = $classes->{loader_test35};
    my $rsobj35   = $conn->resultset($moniker35);

    isa_ok( $rsobj1, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj2, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj23, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj24, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj35, "DBIx::Class::ResultSet" );

    my @columns_lt2 = $class2->columns;
    is_deeply( \@columns_lt2, [ qw/id dat dat2/ ], "Column Ordering" );

    my %uniq1 = $class1->unique_constraints;
    my $uniq1_test = 0;
    foreach my $ucname (keys %uniq1) {
        my $cols_arrayref = $uniq1{$ucname};
        if(@$cols_arrayref == 1 && $cols_arrayref->[0] eq 'dat') {
           $uniq1_test = 1;
           last;
        }
    }
    ok($uniq1_test, "Unique constraint");

    is($moniker1, 'LoaderTest1', 'moniker singularisation');

    my %uniq2 = $class2->unique_constraints;
    my $uniq2_test = 0;
    foreach my $ucname (keys %uniq2) {
        my $cols_arrayref = $uniq2{$ucname};
        if(@$cols_arrayref == 2
           && $cols_arrayref->[0] eq 'dat2'
           && $cols_arrayref->[1] eq 'dat') {
            $uniq2_test = 2;
            last;
        }
    }
    ok($uniq2_test, "Multi-col unique constraint");

    is($moniker2, 'LoaderTest2X', "moniker_map testing");

    SKIP: {
        can_ok( $class1, 'test_additional_base' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->test_additional_base, "test_additional_base",
            "Additional Base method" );
    }

    SKIP: {
        can_ok( $class1, 'test_additional_base_override' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->test_additional_base_override,
            "test_left_base_override",
            "Left Base overrides Additional Base method" );
    }

    SKIP: {
        can_ok( $class1, 'test_additional_base_additional' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->test_additional_base_additional, "test_additional",
            "Additional Base can use Additional package method" );
    }

    SKIP: {
        can_ok( $class1, 'dbix_class_testcomponent' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->dbix_class_testcomponent,
            'dbix_class_testcomponent works',
            'Additional Component' );
    }

    SKIP: {
        can_ok($rsobj1, 'dbix_class_testrscomponent')
            or skip "Pre-requisite test failed", 1;
        is( $rsobj1->dbix_class_testrscomponent,
            'dbix_class_testrscomponent works',
            'ResultSet component' );
    }

    SKIP: {
        can_ok( $class1, 'loader_test1_classmeth' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->loader_test1_classmeth, 'all is well', 'Class method' );
    }

    SKIP: {
        can_ok( $rsobj1, 'loader_test1_rsmeth' )
            or skip "Pre-requisite test failed";
        is( $rsobj1->loader_test1_rsmeth, 'all is still well', 'Result set method' );
    }
    
    ok( $class1->column_info('id')->{is_auto_increment}, 'is_auto_incrment detection' );

    my $obj    = $rsobj1->find(1);
    is( $obj->id,  1, "Find got the right row" );
    is( $obj->dat, "foo", "Column value" );
    is( $rsobj2->count, 4, "Count" );
    my $saved_id;
    eval {
        my $new_obj1 = $rsobj1->create({ dat => 'newthing' });
        $saved_id = $new_obj1->id;
    };
    ok(!$@, "Inserting new record using a PK::Auto key didn't die") or diag $@;
    ok($saved_id, "Got PK::Auto-generated id");

    my $new_obj1 = $rsobj1->search({ dat => 'newthing' })->first;
    ok($new_obj1, "Found newly inserted PK::Auto record");
    is($new_obj1->id, $saved_id, "Correct PK::Auto-generated id");

    my ($obj2) = $rsobj2->search({ dat => 'bbb' })->first;
    is( $obj2->id, 2 );

    is(
        $class35->column_info('a_varchar')->{default_value}, 'foo',
        'constant character default',
    );

    is(
        $class35->column_info('an_int')->{default_value}, 42,
        'constant integer default',
    );

    is(
        $class35->column_info('a_double')->{default_value}, 10.555,
        'constant numeric default',
    );

    my $function_default = $class35->column_info('a_function')->{default_value};

    isa_ok( $function_default, 'SCALAR', 'default_value for function default' );
    is_deeply(
        $function_default, \$self->{default_function},
        'default_value for function default is correct'
    );

    SKIP: {
        skip $self->{skip_rels}, 96 if $self->{skip_rels};

        my $moniker3 = $monikers->{loader_test3};
        my $class3   = $classes->{loader_test3};
        my $rsobj3   = $conn->resultset($moniker3);

        my $moniker4 = $monikers->{loader_test4};
        my $class4   = $classes->{loader_test4};
        my $rsobj4   = $conn->resultset($moniker4);

        my $moniker5 = $monikers->{loader_test5};
        my $class5   = $classes->{loader_test5};
        my $rsobj5   = $conn->resultset($moniker5);

        my $moniker6 = $monikers->{loader_test6};
        my $class6   = $classes->{loader_test6};
        my $rsobj6   = $conn->resultset($moniker6);

        my $moniker7 = $monikers->{loader_test7};
        my $class7   = $classes->{loader_test7};
        my $rsobj7   = $conn->resultset($moniker7);

        my $moniker8 = $monikers->{loader_test8};
        my $class8   = $classes->{loader_test8};
        my $rsobj8   = $conn->resultset($moniker8);

        my $moniker9 = $monikers->{loader_test9};
        my $class9   = $classes->{loader_test9};
        my $rsobj9   = $conn->resultset($moniker9);

        my $moniker16 = $monikers->{loader_test16};
        my $class16   = $classes->{loader_test16};
        my $rsobj16   = $conn->resultset($moniker16);

        my $moniker17 = $monikers->{loader_test17};
        my $class17   = $classes->{loader_test17};
        my $rsobj17   = $conn->resultset($moniker17);

        my $moniker18 = $monikers->{loader_test18};
        my $class18   = $classes->{loader_test18};
        my $rsobj18   = $conn->resultset($moniker18);

        my $moniker19 = $monikers->{loader_test19};
        my $class19   = $classes->{loader_test19};
        my $rsobj19   = $conn->resultset($moniker19);

        my $moniker20 = $monikers->{loader_test20};
        my $class20   = $classes->{loader_test20};
        my $rsobj20   = $conn->resultset($moniker20);

        my $moniker21 = $monikers->{loader_test21};
        my $class21   = $classes->{loader_test21};
        my $rsobj21   = $conn->resultset($moniker21);

        my $moniker22 = $monikers->{loader_test22};
        my $class22   = $classes->{loader_test22};
        my $rsobj22   = $conn->resultset($moniker22);

        my $moniker25 = $monikers->{loader_test25};
        my $class25   = $classes->{loader_test25};
        my $rsobj25   = $conn->resultset($moniker25);

        my $moniker26 = $monikers->{loader_test26};
        my $class26   = $classes->{loader_test26};
        my $rsobj26   = $conn->resultset($moniker26);

        my $moniker27 = $monikers->{loader_test27};
        my $class27   = $classes->{loader_test27};
        my $rsobj27   = $conn->resultset($moniker27);

        my $moniker28 = $monikers->{loader_test28};
        my $class28   = $classes->{loader_test28};
        my $rsobj28   = $conn->resultset($moniker28);

        my $moniker29 = $monikers->{loader_test29};
        my $class29   = $classes->{loader_test29};
        my $rsobj29   = $conn->resultset($moniker29);

        my $moniker31 = $monikers->{loader_test31};
        my $class31   = $classes->{loader_test31};
        my $rsobj31   = $conn->resultset($moniker31);

        my $moniker32 = $monikers->{loader_test32};
        my $class32   = $classes->{loader_test32};
        my $rsobj32   = $conn->resultset($moniker32);

        my $moniker33 = $monikers->{loader_test33};
        my $class33   = $classes->{loader_test33};
        my $rsobj33   = $conn->resultset($moniker33);

        my $moniker34 = $monikers->{loader_test34};
        my $class34   = $classes->{loader_test34};
        my $rsobj34   = $conn->resultset($moniker34);

        my $moniker36 = $monikers->{loader_test36};
        my $class36   = $classes->{loader_test36};
        my $rsobj36   = $conn->resultset($moniker36);
        
        isa_ok( $rsobj3, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj4, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj5, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj6, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj7, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj8, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj9, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj16, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj17, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj18, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj19, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj20, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj21, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj22, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj25, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj26, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj27, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj28, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj29, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj31, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj32, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj33, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj34, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj36, "DBIx::Class::ResultSet" );

        # basic rel test
        my $obj4 = $rsobj4->find(123);
        isa_ok( $obj4->fkid_singular, $class3);

        ok($class4->column_info('fkid')->{is_foreign_key}, 'Foreign key detected');

        my $obj3 = $rsobj3->find(1);
        my $rs_rel4 = $obj3->search_related('loader_test4zes');
        isa_ok( $rs_rel4->first, $class4);

        # find on multi-col pk
        my $obj5 = 
	    eval { $rsobj5->find({id1 => 1, iD2 => 1}) } ||
	    eval { $rsobj5->find({id1 => 1, id2 => 1}) };
	die $@ if $@;

        is( $obj5->id2, 1, "Find on multi-col PK" );

        # mulit-col fk def
        my $obj6 = $rsobj6->find(1);
        isa_ok( $obj6->loader_test2, $class2);
        isa_ok( $obj6->loader_test5, $class5);

        ok($class6->column_info('loader_test2_id')->{is_foreign_key}, 'Foreign key detected');
        ok($class6->column_info('id')->{is_foreign_key}, 'Foreign key detected');

	my $id2_info = eval { $class6->column_info('id2') } ||
			$class6->column_info('Id2');
        ok($id2_info->{is_foreign_key}, 'Foreign key detected');

        # fk that references a non-pk key (UNIQUE)
        my $obj8 = $rsobj8->find(1);
        isa_ok( $obj8->loader_test7, $class7);

        ok($class8->column_info('loader_test7')->{is_foreign_key}, 'Foreign key detected');

        # test double-fk 17 ->-> 16
        my $obj17 = $rsobj17->find(33);

        my $rs_rel16_one = $obj17->loader16_one;
        isa_ok($rs_rel16_one, $class16);
        is($rs_rel16_one->dat, 'y16', "Multiple FKs to same table");

        ok($class17->column_info('loader16_one')->{is_foreign_key}, 'Foreign key detected');

        my $rs_rel16_two = $obj17->loader16_two;
        isa_ok($rs_rel16_two, $class16);
        is($rs_rel16_two->dat, 'z16', "Multiple FKs to same table");

        ok($class17->column_info('loader16_two')->{is_foreign_key}, 'Foreign key detected');

        my $obj16 = $rsobj16->find(2);
        my $rs_rel17 = $obj16->search_related('loader_test17_loader16_ones');
        isa_ok($rs_rel17->first, $class17);
        is($rs_rel17->first->id, 3, "search_related with multiple FKs from same table");
        
        # XXX test m:m 18 <- 20 -> 19
        ok($class20->column_info('parent')->{is_foreign_key}, 'Foreign key detected');
        ok($class20->column_info('child')->{is_foreign_key}, 'Foreign key detected');
        
        # XXX test double-fk m:m 21 <- 22 -> 21
        ok($class22->column_info('parent')->{is_foreign_key}, 'Foreign key detected');
        ok($class22->column_info('child')->{is_foreign_key}, 'Foreign key detected');

        # test double multi-col fk 26 -> 25
        my $obj26 = $rsobj26->find(33);

        my $rs_rel25_one = $obj26->loader_test25_id_rel1;
        isa_ok($rs_rel25_one, $class25);
        is($rs_rel25_one->dat, 'x25', "Multiple multi-col FKs to same table");

        ok($class26->column_info('id')->{is_foreign_key}, 'Foreign key detected');
        ok($class26->column_info('rel1')->{is_foreign_key}, 'Foreign key detected');
        ok($class26->column_info('rel2')->{is_foreign_key}, 'Foreign key detected');

        my $rs_rel25_two = $obj26->loader_test25_id_rel2;
        isa_ok($rs_rel25_two, $class25);
        is($rs_rel25_two->dat, 'y25', "Multiple multi-col FKs to same table");

        my $obj25 = $rsobj25->find(3,42);
        my $rs_rel26 = $obj25->search_related('loader_test26_id_rel1s');
        isa_ok($rs_rel26->first, $class26);
        is($rs_rel26->first->id, 3, "search_related with multiple multi-col FKs from same table");

        # test one-to-one rels
        my $obj27 = $rsobj27->find(1);
        my $obj28 = $obj27->loader_test28;
        isa_ok($obj28, $class28);
        is($obj28->get_column('id'), 1, "One-to-one relationship with PRIMARY FK");

        ok($class28->column_info('id')->{is_foreign_key}, 'Foreign key detected');

        my $obj29 = $obj27->loader_test29;
        isa_ok($obj29, $class29);
        is($obj29->id, 1, "One-to-one relationship with UNIQUE FK");

        ok($class29->column_info('fk')->{is_foreign_key}, 'Foreign key detected');

        $obj27 = $rsobj27->find(2);
        is($obj27->loader_test28, undef, "Undef for missing one-to-one row");
        is($obj27->loader_test29, undef, "Undef for missing one-to-one row");

        # test outer join for nullable referring columns:
        SKIP: {
          skip "unreliable column info from db driver",11 unless 
            ($class32->column_info('rel2')->{is_nullable});

          ok($class32->column_info('rel1')->{is_foreign_key}, 'Foreign key detected');
          ok($class32->column_info('rel2')->{is_foreign_key}, 'Foreign key detected');
          
          my $obj32 = $rsobj32->find(1,{prefetch=>[qw/rel1 rel2/]});
          my $obj34 = $rsobj34->find(
            1,{prefetch=>[qw/loader_test33_id_rel1 loader_test33_id_rel2/]}
          );
          my $skip_outerjoin;
          isa_ok($obj32,$class32) or $skip_outerjoin = 1;
          isa_ok($obj34,$class34) or $skip_outerjoin = 1;

          ok($class34->column_info('id')->{is_foreign_key}, 'Foreign key detected');
          ok($class34->column_info('rel1')->{is_foreign_key}, 'Foreign key detected');
          ok($class34->column_info('rel2')->{is_foreign_key}, 'Foreign key detected');

          SKIP: {
            skip "Pre-requisite test failed", 4 if $skip_outerjoin;
            my $rs_rel31_one = $obj32->rel1;
            my $rs_rel31_two = $obj32->rel2;
            isa_ok($rs_rel31_one, $class31);
            is($rs_rel31_two, undef);

            my $rs_rel33_one = $obj34->loader_test33_id_rel1;
            my $rs_rel33_two = $obj34->loader_test33_id_rel2;

            isa_ok($rs_rel33_one,$class33);
            is($rs_rel33_two, undef);

          }
        }

        # from Chisel's tests...
        my $moniker10 = $monikers->{loader_test10};
        my $class10   = $classes->{loader_test10};
        my $rsobj10   = $conn->resultset($moniker10);

        my $moniker11 = $monikers->{loader_test11};
        my $class11   = $classes->{loader_test11};
        my $rsobj11   = $conn->resultset($moniker11);

        isa_ok( $rsobj10, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj11, "DBIx::Class::ResultSet" );

        ok($class10->column_info('loader_test11')->{is_foreign_key}, 'Foreign key detected');
        ok($class11->column_info('loader_test10')->{is_foreign_key}, 'Foreign key detected');

        # Added by custom_column_info
        ok($class11->column_info('loader_test10')->{is_numeric}, 'custom_column_info');

        is($class36->column_info('a_date')->{locale},'de_DE','datetime_locale');
        is($class36->column_info('a_date')->{timezone},'Europe/Berlin','datetime_timezone');

        ok($class36->column_info('b_char_as_data')->{inflate_datetime},'custom_column_info');
        is($class36->column_info('b_char_as_data')->{locale},'de_DE','datetime_locale');
        is($class36->column_info('b_char_as_data')->{timezone},'Europe/Berlin','datetime_timezone');

        ok($class36->column_info('c_char_as_data')->{inflate_date},'custom_column_info');
        is($class36->column_info('c_char_as_data')->{locale},'de_DE','datetime_locale');
        is($class36->column_info('c_char_as_data')->{timezone},'Europe/Berlin','datetime_timezone');

        my $obj10 = $rsobj10->create({ subject => 'xyzzy' });

        $obj10->update();
        ok( defined $obj10, 'Create row' );

        my $obj11 = $rsobj11->create({ loader_test10 => $obj10->id() });
        $obj11->update();
        ok( defined $obj11, 'Create related row' );

        eval {
            my $obj10_2 = $obj11->loader_test10;
            $obj10_2->loader_test11( $obj11->id11() );
            $obj10_2->update();
        };
        ok(!$@, "Setting up circular relationship");

        SKIP: {
            skip 'Previous eval block failed', 3 if $@;
    
            my $results = $rsobj10->search({ subject => 'xyzzy' });
            is( $results->count(), 1, 'No duplicate row created' );

            my $obj10_3 = $results->first();
            isa_ok( $obj10_3, $class10 );
            is( $obj10_3->loader_test11()->id(), $obj11->id(),
                'Circular rel leads back to same row' );
        }

        SKIP: {
            skip 'This vendor cannot do inline relationship definitions', 9
                if $self->{no_inline_rels};

            my $moniker12 = $monikers->{loader_test12};
            my $class12   = $classes->{loader_test12};
            my $rsobj12   = $conn->resultset($moniker12);

            my $moniker13 = $monikers->{loader_test13};
            my $class13   = $classes->{loader_test13};
            my $rsobj13   = $conn->resultset($moniker13);

            isa_ok( $rsobj12, "DBIx::Class::ResultSet" ); 
            isa_ok( $rsobj13, "DBIx::Class::ResultSet" );

            ok($class13->column_info('id')->{is_foreign_key}, 'Foreign key detected');
            ok($class13->column_info('loader_test12')->{is_foreign_key}, 'Foreign key detected');
            ok($class13->column_info('dat')->{is_foreign_key}, 'Foreign key detected');

            my $obj13 = $rsobj13->find(1);
            isa_ok( $obj13->id, $class12 );
            isa_ok( $obj13->loader_test12, $class12);
            isa_ok( $obj13->dat, $class12);

            my $obj12 = $rsobj12->find(1);
            isa_ok( $obj12->loader_test13, $class13 );
        }

        SKIP: {
            skip 'This vendor cannot do out-of-line implicit rel defs', 4
                if $self->{no_implicit_rels};
            my $moniker14 = $monikers->{loader_test14};
            my $class14   = $classes->{loader_test14};
            my $rsobj14   = $conn->resultset($moniker14);

            my $moniker15 = $monikers->{loader_test15};
            my $class15   = $classes->{loader_test15};
            my $rsobj15   = $conn->resultset($moniker15);

            isa_ok( $rsobj14, "DBIx::Class::ResultSet" ); 
            isa_ok( $rsobj15, "DBIx::Class::ResultSet" );

            ok($class15->column_info('loader_test14')->{is_foreign_key}, 'Foreign key detected');

            my $obj15 = $rsobj15->find(1);
            isa_ok( $obj15->loader_test14, $class14 );
        }
    }

    # rescan and norewrite test
    SKIP: {
        my @statements_rescan = (
            qq{
                CREATE TABLE loader_test30 (
                    id INTEGER NOT NULL PRIMARY KEY,
                    loader_test2 INTEGER NOT NULL,
                    FOREIGN KEY (loader_test2) REFERENCES loader_test2 (id)
                ) $self->{innodb}
            },
            q{ INSERT INTO loader_test30 (id,loader_test2) VALUES(123, 1) },
            q{ INSERT INTO loader_test30 (id,loader_test2) VALUES(321, 2) },
        );

        # get md5
        my $digest  = Digest::MD5->new;

        my $find_cb = sub {
            return if -d;
            return if $_ eq 'LoaderTest30.pm';

            open my $fh, '<', $_ or die "Could not open $_ for reading: $!";
            binmode $fh;
            $digest->addfile($fh);
        };

        find $find_cb, $DUMP_DIR;

        my $before_digest = $digest->digest;

        my $dbh = $self->dbconnect(1);

        {
            # Silence annoying but harmless postgres "NOTICE:  CREATE TABLE..."
            local $SIG{__WARN__} = sub {
                my $msg = shift;
                print STDERR $msg unless $msg =~ m{^NOTICE:\s+CREATE TABLE};
            };

            $dbh->do($_) for @statements_rescan;
        }

        $dbh->disconnect;

        sleep 1;

        my @new = do {
            # kill the 'Dumping manual schema' warnings
            local $SIG{__WARN__} = sub {};
            $conn->rescan;
        };
        is_deeply(\@new, [ qw/LoaderTest30/ ], "Rescan");

        $digest = Digest::MD5->new;
        find $find_cb, $DUMP_DIR;
        my $after_digest = $digest->digest;

        is $before_digest, $after_digest,
            'dumped files are not rewritten when there is no modification';

        my $rsobj30   = $conn->resultset('LoaderTest30');
        isa_ok($rsobj30, 'DBIx::Class::ResultSet');

        skip 'no rels', 2 if $self->{skip_rels};

        my $obj30 = $rsobj30->find(123);
        isa_ok( $obj30->loader_test2, $class2);

        ok($rsobj30->result_source->column_info('loader_test2')->{is_foreign_key},
           'Foreign key detected');
    }

    $self->{extra}->{run}->($conn, $monikers, $classes) if $self->{extra}->{run};
}

sub check_no_duplicate_unique_constraints {
    my ($class) = @_;

    # unique_constraints() automatically includes the PK, if any
    my %uc_cols;
    ++$uc_cols{ join ", ", @$_ }
        for values %{ { $class->unique_constraints } };
    my $dup_uc = grep { $_ > 1 } values %uc_cols;

    is($dup_uc, 0, "duplicate unique constraints ($class)")
        or diag "uc_cols: @{[ %uc_cols ]}";
}

sub dbconnect {
    my ($self, $complain) = @_;

    my $dbh = DBI->connect(
         $self->{dsn}, $self->{user},
         $self->{password},
         {
             RaiseError => $complain,
             PrintError => $complain,
             AutoCommit => 1,
         }
    );

    die "Failed to connect to database: $DBI::errstr" if !$dbh;

    return $dbh;
}

sub create {
    my $self = shift;

    $self->{_created} = 1;

    my $make_auto_inc = $self->{auto_inc_cb} || sub {};
    @statements = (
        qq{
            CREATE TABLE loader_test1s (
                id $self->{auto_inc_pk},
                dat VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test1s id/),

        q{ INSERT INTO loader_test1s (dat) VALUES('foo') },
        q{ INSERT INTO loader_test1s (dat) VALUES('bar') }, 
        q{ INSERT INTO loader_test1s (dat) VALUES('baz') }, 

        qq{ 
            CREATE TABLE loader_test2 (
                id $self->{auto_inc_pk},
                dat VARCHAR(32) NOT NULL,
                dat2 VARCHAR(32) NOT NULL,
                UNIQUE (dat2, dat)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test2 id/),

        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('aaa', 'zzz') }, 
        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('bbb', 'yyy') }, 
        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('ccc', 'xxx') }, 
        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('ddd', 'www') }, 

        qq{
            CREATE TABLE LOADER_TEST23 (
                ID INTEGER NOT NULL PRIMARY KEY,
                DAT VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },

        qq{
            CREATE TABLE LoAdEr_test24 (
                iD INTEGER NOT NULL PRIMARY KEY,
                DaT VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },

        qq{
            CREATE TABLE loader_test35 (
                id INTEGER NOT NULL PRIMARY KEY,
                a_varchar VARCHAR(100) DEFAULT 'foo',
                an_int INTEGER DEFAULT 42,
                a_double DOUBLE PRECISION DEFAULT 10.555,
                a_function $self->{default_function_def}
            ) $self->{innodb}
        },

        qq{
            CREATE TABLE loader_test36 (
                id INTEGER NOT NULL PRIMARY KEY,
                a_date DATE,
                b_char_as_data VARCHAR(100),
                c_char_as_data VARCHAR(100)
            ) $self->{innodb}
        },
    );

    @statements_reltests = (
        qq{
            CREATE TABLE loader_test3 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat VARCHAR(32)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test3 (id,dat) VALUES(1,'aaa') }, 
        q{ INSERT INTO loader_test3 (id,dat) VALUES(2,'bbb') }, 
        q{ INSERT INTO loader_test3 (id,dat) VALUES(3,'ccc') }, 
        q{ INSERT INTO loader_test3 (id,dat) VALUES(4,'ddd') }, 

        qq{
            CREATE TABLE loader_test4 (
                id INTEGER NOT NULL PRIMARY KEY,
                fkid INTEGER NOT NULL,
                dat VARCHAR(32),
                FOREIGN KEY( fkid ) REFERENCES loader_test3 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test4 (id,fkid,dat) VALUES(123,1,'aaa') },
        q{ INSERT INTO loader_test4 (id,fkid,dat) VALUES(124,2,'bbb') }, 
        q{ INSERT INTO loader_test4 (id,fkid,dat) VALUES(125,3,'ccc') },
        q{ INSERT INTO loader_test4 (id,fkid,dat) VALUES(126,4,'ddd') },

        qq{
            CREATE TABLE loader_test5 (
                id1 INTEGER NOT NULL,
                iD2 INTEGER NOT NULL,
                dat VARCHAR(8),
                PRIMARY KEY (id1,iD2)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test5 (id1,iD2,dat) VALUES (1,1,'aaa') },

        qq{
            CREATE TABLE loader_test6 (
                id INTEGER NOT NULL PRIMARY KEY,
                Id2 INTEGER,
                loader_test2_id INTEGER,
                dat VARCHAR(8),
                FOREIGN KEY (loader_test2_id)  REFERENCES loader_test2 (id),
                FOREIGN KEY(id,Id2) REFERENCES loader_test5 (id1,iD2)
            ) $self->{innodb}
        },

        (q{ INSERT INTO loader_test6 (id, Id2,loader_test2_id,dat) } .
         q{ VALUES (1, 1,1,'aaa') }),

        qq{
            CREATE TABLE loader_test7 (
                id INTEGER NOT NULL PRIMARY KEY,
                id2 VARCHAR(8) NOT NULL UNIQUE,
                dat VARCHAR(8)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test7 (id,id2,dat) VALUES (1,'aaa','bbb') },

        qq{
            CREATE TABLE loader_test8 (
                id INTEGER NOT NULL PRIMARY KEY,
                loader_test7 VARCHAR(8) NOT NULL,
                dat VARCHAR(8),
                FOREIGN KEY (loader_test7) REFERENCES loader_test7 (id2)
            ) $self->{innodb}
        },

        (q{ INSERT INTO loader_test8 (id,loader_test7,dat) } .
         q{ VALUES (1,'aaa','bbb') }),

        qq{
            CREATE TABLE loader_test9 (
                loader_test9 VARCHAR(8) NOT NULL
            ) $self->{innodb}
        },

        qq{
            CREATE TABLE loader_test16 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test16 (id,dat) VALUES (2,'x16') },
        qq{ INSERT INTO loader_test16 (id,dat) VALUES (4,'y16') },
        qq{ INSERT INTO loader_test16 (id,dat) VALUES (6,'z16') },

        qq{
            CREATE TABLE loader_test17 (
                id INTEGER NOT NULL PRIMARY KEY,
                loader16_one INTEGER,
                loader16_two INTEGER,
                FOREIGN KEY (loader16_one) REFERENCES loader_test16 (id),
                FOREIGN KEY (loader16_two) REFERENCES loader_test16 (id)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test17 (id, loader16_one, loader16_two) VALUES (3, 2, 4) },
        qq{ INSERT INTO loader_test17 (id, loader16_one, loader16_two) VALUES (33, 4, 6) },

        qq{
            CREATE TABLE loader_test18 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test18 (id,dat) VALUES (1,'x18') },
        qq{ INSERT INTO loader_test18 (id,dat) VALUES (2,'y18') },
        qq{ INSERT INTO loader_test18 (id,dat) VALUES (3,'z18') },

        qq{
            CREATE TABLE loader_test19 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test19 (id,dat) VALUES (4,'x19') },
        qq{ INSERT INTO loader_test19 (id,dat) VALUES (5,'y19') },
        qq{ INSERT INTO loader_test19 (id,dat) VALUES (6,'z19') },

        qq{
            CREATE TABLE loader_test20 (
                parent INTEGER NOT NULL,
                child INTEGER NOT NULL,
                PRIMARY KEY (parent, child),
                FOREIGN KEY (parent) REFERENCES loader_test18 (id),
                FOREIGN KEY (child) REFERENCES loader_test19 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test20 (parent, child) VALUES (1,4) },
        q{ INSERT INTO loader_test20 (parent, child) VALUES (2,5) },
        q{ INSERT INTO loader_test20 (parent, child) VALUES (3,6) },

        qq{
            CREATE TABLE loader_test21 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test21 (id,dat) VALUES (7,'a21')},
        q{ INSERT INTO loader_test21 (id,dat) VALUES (11,'b21')},
        q{ INSERT INTO loader_test21 (id,dat) VALUES (13,'c21')},
        q{ INSERT INTO loader_test21 (id,dat) VALUES (17,'d21')},

        qq{
            CREATE TABLE loader_test22 (
                parent INTEGER NOT NULL,
                child INTEGER NOT NULL,
                PRIMARY KEY (parent, child),
                FOREIGN KEY (parent) REFERENCES loader_test21 (id),
                FOREIGN KEY (child) REFERENCES loader_test21 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test22 (parent, child) VALUES (7,11)},
        q{ INSERT INTO loader_test22 (parent, child) VALUES (11,13)},
        q{ INSERT INTO loader_test22 (parent, child) VALUES (13,17)},

	qq{
            CREATE TABLE loader_test25 (
                id1 INTEGER NOT NULL,
                id2 INTEGER NOT NULL,
                dat VARCHAR(8),
                PRIMARY KEY (id1,id2)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test25 (id1,id2,dat) VALUES (33,5,'x25') },
        q{ INSERT INTO loader_test25 (id1,id2,dat) VALUES (33,7,'y25') },
        q{ INSERT INTO loader_test25 (id1,id2,dat) VALUES (3,42,'z25') },

        qq{
            CREATE TABLE loader_test26 (
               id INTEGER NOT NULL PRIMARY KEY,
               rel1 INTEGER NOT NULL,
               rel2 INTEGER NOT NULL,
               FOREIGN KEY (id, rel1) REFERENCES loader_test25 (id1, id2),
               FOREIGN KEY (id, rel2) REFERENCES loader_test25 (id1, id2)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test26 (id,rel1,rel2) VALUES (33,5,7) },
        q{ INSERT INTO loader_test26 (id,rel1,rel2) VALUES (3,42,42) },

        qq{
            CREATE TABLE loader_test27 (
                id INTEGER NOT NULL PRIMARY KEY
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test27 (id) VALUES (1) },
        q{ INSERT INTO loader_test27 (id) VALUES (2) },

        qq{
            CREATE TABLE loader_test28 (
                id INTEGER NOT NULL PRIMARY KEY,
                FOREIGN KEY (id) REFERENCES loader_test27 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test28 (id) VALUES (1) },

        qq{
            CREATE TABLE loader_test29 (
                id INTEGER NOT NULL PRIMARY KEY,
                fk INTEGER NOT NULL UNIQUE,
                FOREIGN KEY (fk) REFERENCES loader_test27 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test29 (id,fk) VALUES (1,1) },

        qq{
          CREATE TABLE loader_test31 (
            id INTEGER NOT NULL PRIMARY KEY
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test31 (id) VALUES (1) },

        qq{
          CREATE TABLE loader_test32 (
            id INTEGER NOT NULL PRIMARY KEY,
            rel1 INTEGER NOT NULL,
            rel2 INTEGER $self->{null},
            FOREIGN KEY (rel1) REFERENCES loader_test31(id),
            FOREIGN KEY (rel2) REFERENCES loader_test31(id)
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test32 (id,rel1) VALUES (1,1) },

        qq{
          CREATE TABLE loader_test33 (
            id1 INTEGER NOT NULL,
            id2 INTEGER NOT NULL,
            PRIMARY KEY (id1,id2)
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test33 (id1,id2) VALUES (1,2) },

        qq{
          CREATE TABLE loader_test34 (
            id INTEGER NOT NULL PRIMARY KEY,
            rel1 INTEGER NOT NULL,
            rel2 INTEGER $self->{null},
            FOREIGN KEY (id,rel1) REFERENCES loader_test33(id1,id2),
            FOREIGN KEY (id,rel2) REFERENCES loader_test33(id1,id2)
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test34 (id,rel1) VALUES (1,2) },
    );

    @statements_advanced = (
        qq{
            CREATE TABLE loader_test10 (
                id10 $self->{auto_inc_pk},
                subject VARCHAR(8),
                loader_test11 INTEGER $self->{null}
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test10 id10/),

        qq{
            CREATE TABLE loader_test11 (
                id11 $self->{auto_inc_pk},
                a_message VARCHAR(8) DEFAULT 'foo',
                loader_test10 INTEGER $self->{null},
                FOREIGN KEY (loader_test10) REFERENCES loader_test10 (id10)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test11 id11/),

        (q{ ALTER TABLE loader_test10 ADD CONSTRAINT } .
         q{ loader_test11_fk FOREIGN KEY (loader_test11) } .
         q{ REFERENCES loader_test11 (id11) }),
    );

    @statements_advanced_sqlite = (
        qq{
            CREATE TABLE loader_test10 (
                id10 $self->{auto_inc_pk},
                subject VARCHAR(8)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test10 id10/),

        qq{
            CREATE TABLE loader_test11 (
                id11 $self->{auto_inc_pk},
                a_message VARCHAR(8) DEFAULT 'foo',
                loader_test10 INTEGER $self->{null},
                FOREIGN KEY (loader_test10) REFERENCES loader_test10 (id10)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test11 id11/),

        (q{ ALTER TABLE loader_test10 ADD COLUMN } .
         q{ loader_test11 INTEGER REFERENCES loader_test11 (id11) }),
    );

    @statements_inline_rels = (
        qq{
            CREATE TABLE loader_test12 (
                id INTEGER NOT NULL PRIMARY KEY,
                id2 VARCHAR(8) NOT NULL UNIQUE,
                dat VARCHAR(8) NOT NULL UNIQUE
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test12 (id,id2,dat) VALUES (1,'aaa','bbb') },

        qq{
            CREATE TABLE loader_test13 (
                id INTEGER NOT NULL PRIMARY KEY REFERENCES loader_test12,
                loader_test12 VARCHAR(8) NOT NULL REFERENCES loader_test12 (id2),
                dat VARCHAR(8) REFERENCES loader_test12 (dat)
            ) $self->{innodb}
        },

        (q{ INSERT INTO loader_test13 (id,loader_test12,dat) } .
         q{ VALUES (1,'aaa','bbb') }),
    );


    @statements_implicit_rels = (
        qq{
            CREATE TABLE loader_test14 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat VARCHAR(8)
            ) $self->{innodb}
        },
 
        q{ INSERT INTO loader_test14 (id,dat) VALUES (123,'aaa') },

        qq{
            CREATE TABLE loader_test15 (
                id INTEGER NOT NULL PRIMARY KEY,
                loader_test14 INTEGER NOT NULL,
                FOREIGN KEY (loader_test14) REFERENCES loader_test14
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test15 (id,loader_test14) VALUES (1,123) },
    );

    $self->drop_tables;

    my $dbh = $self->dbconnect(1);

    # Silence annoying but harmless postgres "NOTICE:  CREATE TABLE..."
    local $SIG{__WARN__} = sub {
        my $msg = shift;
        print STDERR $msg unless $msg =~ m{^NOTICE:\s+CREATE TABLE};
    };

    $dbh->do($_) for (@statements);
    unless($self->{skip_rels}) {
        # hack for now, since DB2 doesn't like inline comments, and we need
        # to test one for mysql, which works on everyone else...
        # this all needs to be refactored anyways.
        $dbh->do($_) for (@statements_reltests);
        if($self->{vendor} =~ /sqlite/i) {
            $dbh->do($_) for (@statements_advanced_sqlite);
        }
        else {
            $dbh->do($_) for (@statements_advanced);
        }
        unless($self->{no_inline_rels}) {
            $dbh->do($_) for (@statements_inline_rels);
        }
        unless($self->{no_implicit_rels}) {
            $dbh->do($_) for (@statements_implicit_rels);
        }
    }

    $dbh->do($_) for @{ $self->{extra}->{create} || [] };
    $dbh->disconnect();
}

sub drop_tables {
    my $self = shift;

    my @tables = qw/
        loader_test1s
        loader_test2
        LOADER_TEST23
        LoAdEr_test24
        loader_test35
        loader_test36
    /;
    
    my @tables_auto_inc = (
        [ qw/loader_test1s id/ ],
        [ qw/loader_test2 id/ ],
    );

    my @tables_reltests = qw/
        loader_test4
        loader_test3
        loader_test6
        loader_test5
        loader_test8
        loader_test7
        loader_test9
        loader_test17
        loader_test16
        loader_test20
        loader_test19
        loader_test18
        loader_test22
        loader_test21
        loader_test26
        loader_test25
        loader_test28
        loader_test29
        loader_test27
        loader_test32
        loader_test31
        loader_test34
        loader_test33
    /;

    my @tables_advanced = qw/
        loader_test11
        loader_test10
    /;
    
    my @tables_advanced_auto_inc = (
        [ qw/loader_test10 id10/ ],
        [ qw/loader_test11 id11/ ],
    );

    my @tables_inline_rels = qw/
        loader_test13
        loader_test12
    /;

    my @tables_implicit_rels = qw/
        loader_test15
        loader_test14
    /;

    my @tables_rescan = qw/ loader_test30 /;

    my $drop_fk_mysql =
        q{ALTER TABLE loader_test10 DROP FOREIGN KEY loader_test11_fk};

    my $drop_fk =
        q{ALTER TABLE loader_test10 DROP CONSTRAINT loader_test11_fk};

    my $dbh = $self->dbconnect(0);

    $dbh->do("DROP TABLE $_") for @{ $self->{extra}->{drop} || [] };

    my $drop_auto_inc = $self->{auto_inc_drop_cb} || sub {};

    unless($self->{skip_rels}) {
        $dbh->do("DROP TABLE $_") for (@tables_reltests);
        if($self->{vendor} =~ /mysql/i) {
            $dbh->do($drop_fk_mysql);
        }
        else {
            $dbh->do($drop_fk);
        }
        $dbh->do("DROP TABLE $_") for (@tables_advanced);
        $dbh->do($_) for map { $drop_auto_inc->(@$_) } @tables_advanced_auto_inc;

        unless($self->{no_inline_rels}) {
            $dbh->do("DROP TABLE $_") for (@tables_inline_rels);
        }
        unless($self->{no_implicit_rels}) {
            $dbh->do("DROP TABLE $_") for (@tables_implicit_rels);
        }
    }
    $dbh->do("DROP TABLE $_") for (@tables, @tables_rescan);
    $dbh->do($_) for map { $drop_auto_inc->(@$_) } @tables_auto_inc;
    $dbh->disconnect;
}

sub DESTROY {
    my $self = shift;
    unless ($ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
	$self->drop_tables if $self->{_created};
	rmtree $DUMP_DIR
    }
}

1;
