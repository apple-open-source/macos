#!perl -w

use DBI qw(:preparse_flags);

$|=1;

use Test::More;

BEGIN {
	if ($DBI::PurePerl) {
		plan skip_all => 'preparse not supported for DBI::PurePerl';
	}
	else {
		plan tests => 39;
	}
}

my $dbh = DBI->connect("dbi:ExampleP:", "", "", {
	PrintError => 0,
});
isa_ok( $dbh, 'DBI::db' );

sub pp {
    my $dbh = shift;
    my $rv = $dbh->preparse(@_);
    return $rv;
}

# --------------------------------------------------------------------- #
#   DBIpp_cm_cs  /* C style */
#   DBIpp_cm_hs  /* #       */
#   DBIpp_cm_dd  /* --      */
#   DBIpp_cm_br  /* {}      */
#   DBIpp_cm_dw  /* '-- ' dash dash whitespace */
#   DBIpp_cm_XX  /* any of the above */
      
#   DBIpp_ph_qm  /* ?       */
#   DBIpp_ph_cn  /* :1      */
#   DBIpp_ph_cs  /* :name   */
#   DBIpp_ph_sp  /* %s (as return only, not accept)    */
#   DBIpp_ph_XX  /* any of the above */
          
#   DBIpp_st_qq  /* '' char escape */
#   DBIpp_st_bs  /* \  char escape */
#   DBIpp_st_XX  /* any of the above */

# ===================================================================== #
#   pp (h    input      return	        accept	        expected)       #
# ===================================================================== #

## Comments:

is( pp($dbh, "a#b\nc",	DBIpp_cm_cs,	DBIpp_cm_hs),	"a/*b*/\nc" );
is( pp($dbh, "a#b\nc",	DBIpp_cm_dw,	DBIpp_cm_hs),   "a-- b\nc" );
is( pp($dbh, "a/*b*/c",	DBIpp_cm_hs,	DBIpp_cm_cs),	"a#b\nc" );
is( pp($dbh, "a{b}c",	DBIpp_cm_cs,	DBIpp_cm_br),	"a/*b*/c" );
is( pp($dbh, "a--b\nc",	DBIpp_cm_br,	DBIpp_cm_dd),	"a{b}\nc" );

is( pp($dbh, "a-- b\n/*c*/d", DBIpp_cm_br, DBIpp_cm_cs|DBIpp_cm_dw), "a{ b}\n{c}d" );
is( pp($dbh, "a/*b*/c#d\ne--f\nh-- i\nj{k}", 0, DBIpp_cm_XX), "a c\ne\nh\nj " );

## Placeholders:

is( pp($dbh, "a = :1", DBIpp_ph_qm, DBIpp_ph_cn), "a = ?" );
is( pp($dbh, "a = :1", DBIpp_ph_sp, DBIpp_ph_cn), "a = %s" );
is( pp($dbh, "a = ?" , DBIpp_ph_cn, DBIpp_ph_qm), "a = :p1" );
is( pp($dbh, "a = ?" , DBIpp_ph_sp, DBIpp_ph_qm), "a = %s" );

is( pp($dbh, "a = :name",  DBIpp_ph_qm,	DBIpp_ph_cs), "a = ?" );
is( pp($dbh, "a = :name",  DBIpp_ph_sp,	DBIpp_ph_cs), "a = %s" );

is( pp($dbh, "a = ? b = ? c = ?", DBIpp_ph_cn,	DBIpp_ph_XX), "a = :p1 b = :p2 c = :p3" );

## Placeholders inside comments (should be ignored where comments style is accepted):

is( pp( $dbh,
        "a = ? /*b = :1*/ c = ?", 
        DBIpp_cm_dw|DBIpp_ph_cn, 
        DBIpp_cm_cs|DBIpp_ph_qm), 
        "a = :p1 -- b = :1\n c = :p2" );

## Placeholders inside single and double quotes (should be ignored):

is( pp( $dbh,
        "a = ? 'b = :1' c = ?", 
        DBIpp_ph_cn, 
        DBIpp_ph_XX), 
        "a = :p1 'b = :1' c = :p2" );

is( pp( $dbh,
        'a = ? "b = :1" c = ?', 
        DBIpp_ph_cn, 
        DBIpp_ph_XX), 
        'a = :p1 "b = :1" c = :p2' );

## Comments inside single and double quotes (should be ignored):

is( pp( $dbh,
        "a = ? '{b = :1}' c = ?", 
        DBIpp_cm_cs|DBIpp_ph_cn, 
        DBIpp_cm_XX|DBIpp_ph_qm), 
        "a = :p1 '{b = :1}' c = :p2" );

is( pp( $dbh,
        'a = ? "/*b = :1*/" c = ?', 
        DBIpp_cm_dw|DBIpp_ph_cn, 
        DBIpp_cm_XX|DBIpp_ph_qm), 
        'a = :p1 "/*b = :1*/" c = :p2' );

## Single and double quoted strings starting inside comments (should be ignored):

is( pp( $dbh,
        'a = ? /*"b = :1 */ c = ?', 
        DBIpp_cm_br|DBIpp_ph_cn, 
        DBIpp_cm_XX|DBIpp_ph_qm), 
        'a = :p1 {"b = :1 } c = :p2' );

## Check error conditions are trapped:

is( pp($dbh, "a = :value and b = :1", DBIpp_ph_qm, DBIpp_ph_cs|DBIpp_ph_cn), undef );
ok( $DBI::err );
is( $DBI::errstr, "preparse found mixed placeholder styles (:1 / :name)" );

is( pp($dbh, "a = :1 and b = :3", DBIpp_ph_qm,	DBIpp_ph_cn), undef );
ok( $DBI::err );
is( $DBI::errstr, "preparse found placeholder :3 out of sequence, expected :2" );

is( pp($dbh, "foo ' comment", 0, 0), "foo ' comment" );
ok( $DBI::err );
is( $DBI::errstr, "preparse found unterminated single-quoted string" );

is( pp($dbh, 'foo " comment', 0, 0), 'foo " comment' );
ok( $DBI::err );
is( $DBI::errstr, "preparse found unterminated double-quoted string" );

is( pp($dbh, 'foo /* comment', DBIpp_cm_XX, DBIpp_cm_XX), 'foo /* comment' );
ok( $DBI::err );
is( $DBI::errstr, "preparse found unterminated bracketed C-style comment" );

is( pp($dbh, 'foo { comment', DBIpp_cm_XX, DBIpp_cm_XX), 'foo { comment' );
ok( $DBI::err );
is( $DBI::errstr, "preparse found unterminated bracketed {...} comment" );

# --------------------------------------------------------------------- #

$dbh->disconnect;

1;
