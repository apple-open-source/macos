#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test     qw/connect_ok @CALL_FUNCS/;
use Test::More;
use Test::NoWarnings qw/had_no_warnings clear_warnings/;

use DBD::SQLite;

plan tests => 24 * @CALL_FUNCS + 1;

# hooks : just count the commits / rollbacks / updates
my ($n_commits, $n_rollbacks, $n_updates, @update_args);
sub commit_hook   {  $n_commits   += 1; return 0; }
sub rollback_hook {  $n_rollbacks += 1; return 0; }
sub update_hook   {  $n_updates   += 1; 
                     @update_args  = @_;          }

my $sql_count_rows = "SELECT COUNT(foo) FROM hook_test";

foreach my $call_func (@CALL_FUNCS) {

  # connect 
  my $dbh = connect_ok( RaiseError => 1 );
  $dbh->do( 'CREATE TEMP TABLE hook_test ( foo )' );

  # register the hooks
  my $previous_commit_hook   = $dbh->$call_func(\&commit_hook,
                                                "commit_hook");
  my $previous_rollback_hook = $dbh->$call_func(\&rollback_hook,
                                                "rollback_hook");
  my $previous_update_hook   = $dbh->$call_func(\&update_hook, 
                                                "update_hook");
  ok(!$previous_commit_hook,   "initial commit hook was undef");
  ok(!$previous_rollback_hook, "initial rollback hook was undef");
  ok(!$previous_update_hook,   "initial update hook was undef");

  # a couple of transactions
  do_transaction($dbh) for 1..3;

  # commit hook should have been called three times
  is($n_commits, 3, "3 commits");

  # update hook should have been called 30 times
  is($n_updates, 30, "30 updates");

  # check args transmitted to update hook;
  is($update_args[0], DBD::SQLite::INSERT, 'update hook arg 0: INSERT');
  is($update_args[1], 'temp',              'update hook arg 1: database');
  is($update_args[2], 'hook_test',         'update hook arg 2: table');
  ok($update_args[3],                      'update hook arg 3: rowid');

  # unregister the commit and update hooks, check if previous hooks are returned
  $previous_commit_hook = $dbh->$call_func(undef, "commit_hook");
  ok($previous_commit_hook eq \&commit_hook, 
     "previous commit hook correctly returned");
  $previous_update_hook = $dbh->$call_func(undef, "update_hook");
  ok($previous_update_hook eq \&update_hook, 
     "previous update hook correctly returned");

  # some more transactions .. commit and update hook should not be called
  $n_commits = 0;
  $n_updates = 0;
  do_transaction($dbh) for 1..3;
  is($n_commits, 0, "commit hook unregistered");
  is($n_updates, 0, "update hook unregistered");

  # check here explicitly for warnings, before we clear them
  had_no_warnings();

  # remember how many rows we had so far
  my ($n_rows) = $dbh->selectrow_array($sql_count_rows);

  # a commit hook that rejects the transaction
  $dbh->$call_func(sub {return 1}, "commit_hook");
  eval {do_transaction($dbh)}; # in eval() because of RaiseError
  ok ($@, "transaction was rejected: $@" );

  # no explicit rollback, because SQLite already did it
  # eval {$dbh->rollback;};
  # ok (!$@, "rollback OK $@");

  # rollback hook should have been called
  is($n_rollbacks, 1, "1 rollback");

  # unregister the rollback hook, check if previous hook is returned
  $previous_rollback_hook = $dbh->$call_func(undef, "rollback_hook");
  ok($previous_rollback_hook eq \&rollback_hook, 
     "previous hook correctly returned");

  # try transaction again .. rollback hook should not be called
  $n_rollbacks = 0;
  eval {do_transaction($dbh)};
  is($n_rollbacks, 0, "rollback hook unregistered");

  # check that the rollbacks did really occur
  my ($n_rows_after) = $dbh->selectrow_array($sql_count_rows);
  is($n_rows, $n_rows_after, "no rows added" );

  # unregister commit hook, register an authorizer that forbids delete ops
  $dbh->$call_func(undef, "commit_hook");
  my @authorizer_args;
  my $authorizer = sub {
    @authorizer_args = @_;
    my $action_code = shift;
    my $retval = $action_code == DBD::SQLite::DELETE ? DBD::SQLite::DENY
                                                     : DBD::SQLite::OK;
    return $retval;
  };
  $dbh->$call_func($authorizer, "set_authorizer");

  # try an insert (should be authorized) and check authorizer args
  $dbh->do("INSERT INTO hook_test VALUES ('auth_test')");
  is_deeply(\@authorizer_args, 
            [DBD::SQLite::INSERT, 'hook_test', undef, 'temp', undef],
            "args to authorizer (INSERT)");

  # try a delete (should be unauthorized)
  eval {$dbh->do("DELETE FROM hook_test WHERE foo = 'auth_test'")};
  ok($@, "delete was rejected with message $@");
  is_deeply(\@authorizer_args, 
            [DBD::SQLite::DELETE, 'hook_test', undef, 'temp', undef],
            "args to authorizer (DELETE)");


  # unregister the authorizer ... now DELETE should be authorized
  $dbh->$call_func(undef, "set_authorizer");
  eval {$dbh->do("DELETE FROM hook_test WHERE foo = 'auth_test'")};
  ok(!$@, "delete was accepted");


  # sqlite3 did warn in tests above, so avoid complains from Test::Warnings
  # (would be better to turn off warnings from sqlite3, but I didn't find
  #  any way to do that)
  clear_warnings();
}


sub do_transaction {
  my $dbh = shift;

  $dbh->begin_work;
  for my $count (1 .. 10) {
    my $rand = rand;
    $dbh->do( "INSERT INTO hook_test(foo) VALUES ( $rand )" );
  }
  $dbh->commit;
}
