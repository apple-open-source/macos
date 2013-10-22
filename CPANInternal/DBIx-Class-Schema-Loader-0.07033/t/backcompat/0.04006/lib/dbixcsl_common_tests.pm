package dbixcsl_common_tests;

use strict;
use warnings;

use Test::More;
use DBIx::Class::Schema::Loader;
use DBI;

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
    
    $self->{verbose} = $ENV{TEST_VERBOSE} || 0;

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

sub run_tests {
    my $self = shift;

    plan tests => 97;

    $self->create();

    my $schema_class = 'DBIXCSL_Test::Schema';

    my $debug = ($self->{verbose} > 1) ? 1 : 0;

    my @connect_info = ( $self->{dsn}, $self->{user}, $self->{password} );
    my %loader_opts = (
        constraint              => qr/^(?:\S+\.)?loader_test[0-9]+$/i,
        relationships           => 1,
        additional_classes      => 'TestAdditional',
        additional_base_classes => 'TestAdditionalBase',
        left_base_classes       => [ qw/TestLeftBase/ ],
        components              => [ qw/TestComponent/ ],
        inflect_plural          => { loader_test4 => 'loader_test4zes' },
        inflect_singular        => { fkid => 'fkid_singular' },
        moniker_map             => \&_monikerize,
        debug                   => $debug,
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

        my $warn_count = 0;
        $warn_count++ if grep /ResultSetManager/, @loader_warnings;
        $warn_count++ if grep /Dynamic schema detected/, @loader_warnings;
        $warn_count++ for grep /^Bad table or view/, @loader_warnings;

        is(scalar(@loader_warnings), $warn_count)
          or diag "Did not get the expected 0 warnings.  Warnings are: "
            . join('',@loader_warnings);
    }

    my $conn = $schema_class->clone;
    my $monikers = {};
    my $classes = {};
    foreach my $source_name ($schema_class->sources) {
        my $table_name = $schema_class->loader->moniker_to_table->{$source_name};

        my $result_class = $schema_class->source($source_name)->result_class;

        $monikers->{$table_name} = $source_name;
        $classes->{$table_name} = $result_class;

        # some DBs (Firebird, Oracle) uppercase everything
        $monikers->{lc $table_name} = $source_name;
        $classes->{lc $table_name} = $result_class;
    }

# for debugging...
#    {
#        mkdir '/tmp/HLAGH';
#        $conn->_loader->{dump_directory} = '/tmp/HLAGH';
#        $conn->_loader->_dump_to_dir(values %$classes);
#    }

    my $moniker1 = $monikers->{loader_test1};
    my $class1   = $classes->{loader_test1};
    my $rsobj1   = $conn->resultset($moniker1);

    my $moniker2 = $monikers->{loader_test2};
    my $class2   = $classes->{loader_test2};
    my $rsobj2   = $conn->resultset($moniker2);

    my $moniker23 = $monikers->{LOADER_TEST23};
    my $class23   = $classes->{LOADER_TEST23};
    my $rsobj23   = $conn->resultset($moniker1);

    my $moniker24 = $monikers->{LoAdEr_test24};
    my $class24   = $classes->{LoAdEr_test24};
    my $rsobj24   = $conn->resultset($moniker2);

    isa_ok( $rsobj1, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj2, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj23, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj24, "DBIx::Class::ResultSet" );

    my @columns_lt2 = $class2->columns;
    is($columns_lt2[0], 'id', "Column Ordering 0");
    is($columns_lt2[1], 'dat', "Column Ordering 1");
    is($columns_lt2[2], 'dat2', "Column Ordering 2");

    my %uniq1 = $class1->unique_constraints;
    my $uniq1_test = 0;
    foreach my $ucname (keys %uniq1) {
        my $cols_arrayref = $uniq1{$ucname};
        if(@$cols_arrayref == 1 && $cols_arrayref->[0] eq 'dat') {
           $uniq1_test = 1;
           last;
        }
    }
    ok($uniq1_test) or diag "Unique constraints not working";

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
    ok($uniq2_test) or diag "Multi-col unique constraints not working";

    is($moniker2, 'LoaderTest2X', "moniker_map testing");

    {
        my ($skip_tab, $skip_tabo, $skip_taba, $skip_cmeth,
            $skip_tcomp, $skip_trscomp);

        can_ok( $class1, 'test_additional_base' ) or $skip_tab = 1;
        can_ok( $class1, 'test_additional_base_override' ) or $skip_tabo = 1;
        can_ok( $class1, 'test_additional_base_additional' ) or $skip_taba = 1;
        can_ok( $class1, 'dbix_class_testcomponent' ) or $skip_tcomp = 1;
        can_ok( $class1, 'loader_test1_classmeth' ) or $skip_cmeth = 1;

        SKIP: {
            skip "Pre-requisite test failed", 1 if $skip_tab;
            is( $class1->test_additional_base, "test_additional_base",
                "Additional Base method" );
        }

        SKIP: {
            skip "Pre-requisite test failed", 1 if $skip_tabo;
            is( $class1->test_additional_base_override,
                "test_left_base_override",
                "Left Base overrides Additional Base method" );
        }

        SKIP: {
            skip "Pre-requisite test failed", 1 if $skip_taba;
            is( $class1->test_additional_base_additional, "test_additional",
                "Additional Base can use Additional package method" );
        }

        SKIP: {
            skip "Pre-requisite test failed", 1 if $skip_tcomp;
            is( $class1->dbix_class_testcomponent,
                'dbix_class_testcomponent works' );
        }

        SKIP: {
            skip "Pre-requisite test failed", 1 if $skip_cmeth;
            is( $class1->loader_test1_classmeth, 'all is well' );
        }
    }


    my $obj    = $rsobj1->find(1);
    is( $obj->id,  1 );
    is( $obj->dat, "foo" );
    is( $rsobj2->count, 4 );
    my $saved_id;
    eval {
        my $new_obj1 = $rsobj1->create({ dat => 'newthing' });
        $saved_id = $new_obj1->id;
    };
    ok(!$@) or diag "Died during create new record using a PK::Auto key: $@";
    ok($saved_id) or diag "Failed to get PK::Auto-generated id";

    my $new_obj1 = $rsobj1->search({ dat => 'newthing' })->first;
    ok($new_obj1) or diag "Cannot find newly inserted PK::Auto record";
    is($new_obj1->id, $saved_id);

    my ($obj2) = $rsobj2->search({ dat => 'bbb' })->first;
    is( $obj2->id, 2 );

    SKIP: {
        skip $self->{skip_rels}, 63 if $self->{skip_rels};

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

        # basic rel test
        my $obj4 = $rsobj4->find(123);
        isa_ok( $obj4->fkid_singular, $class3);

        my $obj3 = $rsobj3->find(1);
        my $rs_rel4 = $obj3->search_related('loader_test4zes');
        isa_ok( $rs_rel4->first, $class4);

        # test that _id is not stripped and prepositions in rel names are
        # ignored
        ok ($rsobj4->result_source->has_relationship('loader_test5_to_ids'),
            "rel with preposition 'to' and _id pluralized backward-compatibly");

        ok ($rsobj4->result_source->has_relationship('loader_test5_from_ids'),
            "rel with preposition 'from' and _id pluralized backward-compatibly");

        # check that default relationship attributes are not applied in 0.04006 mode
        is $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{cascade_delete}, 1,
            'cascade_delete => 1 on has_many by default';

        is $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{cascade_copy}, 1,
            'cascade_copy => 1 on has_many by default';

        ok ((not exists $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{on_delete}),
            'has_many does not have on_delete');

        ok ((not exists $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{on_update}),
            'has_many does not have on_update');

        ok ((not exists $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{is_deferrable}),
            'has_many does not have is_deferrable');

        isnt $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{on_delete}, 'CASCADE',
            "on_delete => 'CASCADE' not on belongs_to by default";

        isnt $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{on_update}, 'CASCADE',
            "on_update => 'CASCADE' not on belongs_to by default";

        isnt $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{is_deferrable}, 1,
            "is_deferrable => 1 not on belongs_to by default";

        ok ((not exists $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{cascade_delete}),
            'belongs_to does not have cascade_delete');

        ok ((not exists $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{cascade_copy}),
            'belongs_to does not have cascade_copy');

        # find on multi-col pk
        my $obj5 = $rsobj5->find({id1 => 1, id2 => 1});
        is( $obj5->id2, 1 );

        # mulit-col fk def
        my $obj6 = $rsobj6->find(1);
        isa_ok( $obj6->loader_test2, $class2);
        isa_ok( $obj6->loader_test5, $class5);

        # fk that references a non-pk key (UNIQUE)
        my $obj8 = $rsobj8->find(1);
        isa_ok( $obj8->loader_test7, $class7);

        # test double-fk 17 ->-> 16
        my $obj17 = $rsobj17->find(33);

        my $rs_rel16_one = $obj17->loader16_one;
        isa_ok($rs_rel16_one, $class16);
        is($rs_rel16_one->dat, 'y16');

        my $rs_rel16_two = $obj17->loader16_two;
        isa_ok($rs_rel16_two, $class16);
        is($rs_rel16_two->dat, 'z16');

        my $obj16 = $rsobj16->find(2);
        my $rs_rel17 = $obj16->search_related('loader_test17_loader16_ones');
        isa_ok($rs_rel17->first, $class17);
        is($rs_rel17->first->id, 3);
        
        # XXX test m:m 18 <- 20 -> 19
        
        # XXX test double-fk m:m 21 <- 22 -> 21

        # test double multi-col fk 26 -> 25
        my $obj26 = $rsobj26->find(33);

        my $rs_rel25_one = $obj26->loader_test25_id_rel1;
        isa_ok($rs_rel25_one, $class25);
        is($rs_rel25_one->dat, 'x25');

        my $rs_rel25_two = $obj26->loader_test25_id_rel2;
        isa_ok($rs_rel25_two, $class25);
        is($rs_rel25_two->dat, 'y25');

        my $obj25 = $rsobj25->find(3,42);
        my $rs_rel26 = $obj25->search_related('loader_test26_id_rel1s');
        isa_ok($rs_rel26->first, $class26);
        is($rs_rel26->first->id, 3);

        # from Chisel's tests...
        SKIP: {
            if($self->{vendor} =~ /sqlite/i) {
                skip 'SQLite cannot do the advanced tests', 8;
            }

            my $moniker10 = $monikers->{loader_test10};
            my $class10   = $classes->{loader_test10};
            my $rsobj10   = $conn->resultset($moniker10);

            my $moniker11 = $monikers->{loader_test11};
            my $class11   = $classes->{loader_test11};
            my $rsobj11   = $conn->resultset($moniker11);

            isa_ok( $rsobj10, "DBIx::Class::ResultSet" ); 
            isa_ok( $rsobj11, "DBIx::Class::ResultSet" );

            my $obj10 = $rsobj10->create({ subject => 'xyzzy' });

            $obj10->update();
            ok( defined $obj10, '$obj10 is defined' );

            my $obj11 = $rsobj11->create({ loader_test10 => $obj10->id() });
            $obj11->update();
            ok( defined $obj11, '$obj11 is defined' );

            eval {
                my $obj10_2 = $obj11->loader_test10;
                $obj10_2->loader_test11( $obj11->id11() );
                $obj10_2->update();
            };
            is($@, '', 'No errors after eval{}');

            SKIP: {
                skip 'Previous eval block failed', 3
                    unless ($@ eq '');
        
                my $results = $rsobj10->search({ subject => 'xyzzy' });
                is( $results->count(), 1,
                    'One $rsobj10 returned from search' );

                my $obj10_3 = $results->first();
                isa_ok( $obj10_3, $class10 );
                is( $obj10_3->loader_test11()->id(), $obj11->id(),
                    'found same $rsobj11 object we expected' );
            }
        }

        SKIP: {
            skip 'This vendor cannot do inline relationship definitions', 6
                if $self->{no_inline_rels};

            my $moniker12 = $monikers->{loader_test12};
            my $class12   = $classes->{loader_test12};
            my $rsobj12   = $conn->resultset($moniker12);

            my $moniker13 = $monikers->{loader_test13};
            my $class13   = $classes->{loader_test13};
            my $rsobj13   = $conn->resultset($moniker13);

            isa_ok( $rsobj12, "DBIx::Class::ResultSet" ); 
            isa_ok( $rsobj13, "DBIx::Class::ResultSet" );

            my $obj13 = $rsobj13->find(1);
            isa_ok( $obj13->id, $class12 );
            isa_ok( $obj13->loader_test12, $class12);
            isa_ok( $obj13->dat, $class12);

            my $obj12 = $rsobj12->find(1);
            isa_ok( $obj12->loader_test13_ids, "DBIx::Class::ResultSet" );
        }

        SKIP: {
            skip 'This vendor cannot do out-of-line implicit rel defs', 3
                if $self->{no_implicit_rels};
            my $moniker14 = $monikers->{loader_test14};
            my $class14   = $classes->{loader_test14};
            my $rsobj14   = $conn->resultset($moniker14);

            my $moniker15 = $monikers->{loader_test15};
            my $class15   = $classes->{loader_test15};
            my $rsobj15   = $conn->resultset($moniker15);

            isa_ok( $rsobj14, "DBIx::Class::ResultSet" ); 
            isa_ok( $rsobj15, "DBIx::Class::ResultSet" );

            my $obj15 = $rsobj15->find(1);
            isa_ok( $obj15->loader_test14, $class14 );
        }
    }

    # rescan test
    SKIP: {
        skip $self->{skip_rels}, 4 if $self->{skip_rels};

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

        {
            my $dbh = $self->dbconnect(1);
            $dbh->do($_) for @statements_rescan;
            $dbh->disconnect;
        }

        my @new = do {
            local $SIG{__WARN__} = sub {};
            $conn->rescan;
        };
        is(scalar(@new), 1);
        is($new[0], 'LoaderTest30');

        my $rsobj30   = $conn->resultset('LoaderTest30');
        isa_ok($rsobj30, 'DBIx::Class::ResultSet');
        my $obj30 = $rsobj30->find(123);
        isa_ok( $obj30->loader_test2, $class2);
    }
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
    if ($self->{dsn} =~ /^[^:]+:SQLite:/) {
      $dbh->do ('PRAGMA synchronous = OFF');
    }
    elsif ($self->{dsn} =~ /^[^:]+:Pg:/) {
      $dbh->do ('SET client_min_messages=WARNING');
    }

    die "Failed to connect to database: $DBI::errstr" if !$dbh;

    return $dbh;
}

sub create {
    my $self = shift;

    $self->{_created} = 1;

    my $make_auto_inc = $self->{auto_inc_cb} || sub {};
    my @statements = (
        qq{
            CREATE TABLE loader_test1 (
                id $self->{auto_inc_pk},
                dat VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test1 id/),

        q{ INSERT INTO loader_test1 (dat) VALUES('foo') },
        q{ INSERT INTO loader_test1 (dat) VALUES('bar') }, 
        q{ INSERT INTO loader_test1 (dat) VALUES('baz') }, 

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
    );

    my @statements_reltests = (
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
                from_id INTEGER,
                to_id INTEGER,
                PRIMARY KEY (id1,id2),
                FOREIGN KEY (from_id) REFERENCES loader_test4 (id),
                FOREIGN KEY (to_id) REFERENCES loader_test4 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test5 (id1,id2,dat) VALUES (1,1,'aaa') },

        qq{
            CREATE TABLE loader_test6 (
                id INTEGER NOT NULL PRIMARY KEY,
                Id2 INTEGER,
                loader_test2 INTEGER,
                dat VARCHAR(8),
                FOREIGN KEY (loader_test2)  REFERENCES loader_test2 (id),
                FOREIGN KEY(id,Id2) REFERENCES loader_test5 (id1,iD2)
            ) $self->{innodb}
        },

        (q{ INSERT INTO loader_test6 (id, id2,loader_test2,dat) } .
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
    );

    my @statements_advanced = (
        qq{
            CREATE TABLE loader_test10 (
                id10 $self->{auto_inc_pk},
                subject VARCHAR(8),
                loader_test11 INTEGER
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test10 id10/),

        qq{
            CREATE TABLE loader_test11 (
                id11 $self->{auto_inc_pk},
                message VARCHAR(8) DEFAULT 'foo',
                loader_test10 INTEGER,
                FOREIGN KEY (loader_test10) REFERENCES loader_test10 (id10)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test11 id11/),

        (q{ ALTER TABLE loader_test10 ADD CONSTRAINT } .
         q{ loader_test11_fk FOREIGN KEY (loader_test11) } .
         q{ REFERENCES loader_test11 (id11) }),
    );

    my @statements_inline_rels = (
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


    my @statements_implicit_rels = (
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

    $dbh->do($_) for (@statements);
    unless($self->{skip_rels}) {
        # hack for now, since DB2 doesn't like inline comments, and we need
        # to test one for mysql, which works on everyone else...
        # this all needs to be refactored anyways.
        $dbh->do($_) for (@statements_reltests);
        unless($self->{vendor} =~ /sqlite/i) {
            $dbh->do($_) for (@statements_advanced);
        }
        unless($self->{no_inline_rels}) {
            $dbh->do($_) for (@statements_inline_rels);
        }
        unless($self->{no_implicit_rels}) {
            $dbh->do($_) for (@statements_implicit_rels);
        }
    }
    $dbh->disconnect();
}

sub drop_tables {
    my $self = shift;

    my @tables = qw/
        loader_test1
        loader_test2
        LOADER_TEST23
        LoAdEr_test24
    /;
    
    my @tables_auto_inc = (
        [ qw/loader_test1 id/ ],
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

    my $drop_auto_inc = $self->{auto_inc_drop_cb} || sub {};

    unless($self->{skip_rels}) {
        $dbh->do("DROP TABLE $_") for (@tables_reltests);
        unless($self->{vendor} =~ /sqlite/i) {
            if($self->{vendor} =~ /mysql/i) {
                $dbh->do($drop_fk_mysql);
            }
            else {
                $dbh->do($drop_fk);
            }
            $dbh->do("DROP TABLE $_") for (@tables_advanced);
            $dbh->do($_) for map { $drop_auto_inc->(@$_) } @tables_advanced_auto_inc;
        }
        unless($self->{no_inline_rels}) {
            $dbh->do("DROP TABLE $_") for (@tables_inline_rels);
        }
        unless($self->{no_implicit_rels}) {
            $dbh->do("DROP TABLE $_") for (@tables_implicit_rels);
        }
        $dbh->do("DROP TABLE $_") for (@tables_rescan);
    }
    $dbh->do("DROP TABLE $_") for (@tables);
    $dbh->do($_) for map { $drop_auto_inc->(@$_) } @tables_auto_inc;
    $dbh->disconnect;
}

sub DESTROY {
    my $self = shift;
    $self->drop_tables if $self->{_created};
}

1;
