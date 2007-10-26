USING SQL FOR LOOKUPS, LOG/REPORTING AND QUARANTINE
===================================================

This text contains a general SQL-related documentation. For aspects
specific to using SQL database for lookups, please see README.lookups .

For general aspects of lookups, please see README.lookups.
For MySQL-specific notes and schema please see README.sql-mysql.
For PostgreSQL-specific notes and schema please see README.sql-pg
(which in most respects applies also to a SQLite database).


Since version of amavisd-new-20020630 a SQL is supported for lookups.
Since amavisd-new-2.3.0, SQL is also supported for storing information
about processed mail (logging/reporting) and optionally for quarantining
to a SQL database.

The amavisd.conf variables @storage_sql_dsn and @lookup_sql_dsn control
access to a SQL server and specify a database (dsn = data source name).
The @lookup_sql_dsn enables and specifies a database for lookups,
the @storage_sql_dsn enables and specifies a database for reporting
and quarantining. Both settings are independent.

Interpretation of @lookup_sql_dsn and @storage_sql_dsn lists is as follows:
- empty list disables the function and is a default;
- if both lists are empty no SQL support code will be compiled-in,
  reducing the amount of virtual memory needed for each child process;
- a list can contain one or more triples: [dsn,user,passw]; more than one
  triple may be specified to specify multiple (backup) SQL servers - the first
  that responds will be used as long as it works, then search is retried;
- if both lists contain refs to the _same_ triples (not just equal triples),
  only one connection to a SQL server will be used; otherwise two independent
  connections to databases will be used, possibly to different SQL servers,
  which may even be of different type (e.g. SQLlite for lookups (read-only),
  and PostgreSQL or MySQL for transactional reporting, offering fine lock
  granularity).

Example setting:
  @lookup_sql_dsn =
  ( ['DBI:mysql:database=mail;host=127.0.0.1;port=3306', 'user1', 'passwd1'],
    ['DBI:mysql:database=mail;host=host2', 'username2', 'password2'],
    ['DBI:Pg:database=mail;host=host1', 'amavis', '']
    ["DBI:SQLite:dbname=$MYHOME/sql/mail_prefs.sqlite", '', ''] );

  @storage_sql_dsn = @lookup_sql_dsn;  # none, same, or separate database

See man page for the Perl module DBI, and corresponding DBD modules
man pages (DBD::mysql, DBD::Pg, DBD::SQLite, ...) for syntax of the
first argument.

Since version 2.3.0 amavisd-new also offers quarantining to a SQL database,
along with a mechanism to release quarantined messages (either from SQL or
from normal files, possibly gzipped). To enable quarantining to SQL, the
@storage_sql_dsn must be enabled (facilitating quarantine management), and
some or all variables $virus_quarantine_method, $spam_quarantine_method,
$banned_files_quarantine_method and $bad_header_quarantine_method should
specify the value 'sql:'. Specifying 'sql:' as a quarantine method without
also specifying a database in @storage_sql_dsn is an error.

When setting up access controls to a database, keep in mind that amavisd-new
only needs read-only access to the database used for lookups, the permission
to do a SELECT suffices. For security reasons it is undesirable to permit
other operations such as INSERT, DELETE or UPDATE to a dataset used for
lookups. For managing the lookups database one should preferably use a
different username with more privileges.

The database specified in @storage_sql_dsn needs to provide read/write access
(SELECT, INSERT, UPDATE), and a database server offering transactions
must be used.

Database schemas are available in README.sql-mysql (for MySQL)
and in README.sql-pg (for PostgreSQL and SQLite).

There are two parts of a schema, an read-only part used for lookups,
and a R/W part used for logging and quarantining. They are completely
independent and may reside on different SQL servers (even on different
types of SQL server), but may also coexist in a single database if desired.

Note that some databases are very well suited for lookups, but less so for
highly concurent transactional use in logging/quarantining. Some experience:

- SQLite works nicely for lookups, avoiding a need for a separate server
  process, but its coarse locking granularity makes its unquitable for
  logging and quarantining;

- MySQL and PostgreSQL are both fine for lookups;

- PostgreSQL is better suited for SQL logging/quarantining because maintenance
  operations (cleaning of old records) are much faster than with MySQL;
  Note that SQL logging is needed for amavisd-new pen-pals feature to work;

- if using MySQL for logging/quarantining, a sufficiently recent version
  must be used, as support for transactions is required for the R/W access;

- if using SpamAssassin with its Bayes (and AWL) database on SQL,
  bayes plugin works faster with MySQL than with PostgreSQL; note that
  SA databases are independent from amavisd-new databases and may reside
  on a separate SQL server, possibly of a different type. See SpamAssassin
  documentation that comes with its distribution, files sql/README* .



=====================
Example data follows:
=====================

INSERT INTO users VALUES ( 1, 9, 5, 'user1+foo@y.example.com','Name1 Surname1', 'Y');
INSERT INTO users VALUES ( 2, 7, 5, 'user1@y.example.com', 'Name1 Surname1', 'Y');
INSERT INTO users VALUES ( 3, 7, 2, 'user2@y.example.com', 'Name2 Surname2', 'Y');
INSERT INTO users VALUES ( 4, 7, 7, 'user3@z.example.com', 'Name3 Surname3', 'Y');
INSERT INTO users VALUES ( 5, 7, 7, 'user4@example.com',   'Name4 Surname4', 'Y');
INSERT INTO users VALUES ( 6, 7, 1, 'user5@example.com',   'Name5 Surname5', 'Y');
INSERT INTO users VALUES ( 7, 5, 0, '@sub1.example.com', NULL, 'Y');
INSERT INTO users VALUES ( 8, 5, 7, '@sub2.example.com', NULL, 'Y');
INSERT INTO users VALUES ( 9, 5, 5, '@example.com',      NULL, 'Y');
INSERT INTO users VALUES (10, 3, 8, 'userA', 'NameA SurnameA anywhere', 'Y');
INSERT INTO users VALUES (11, 3, 9, 'userB', 'NameB SurnameB', 'Y');
INSERT INTO users VALUES (12, 3,10, 'userC', 'NameC SurnameC', 'Y');
INSERT INTO users VALUES (13, 3,11, 'userD', 'NameD SurnameD', 'Y');
INSERT INTO users VALUES (14, 3, 0, '@sub1.example.net', NULL, 'Y');
INSERT INTO users VALUES (15, 3, 7, '@sub2.example.net', NULL, 'Y');
INSERT INTO users VALUES (16, 3, 5, '@example.net',      NULL, 'Y');
INSERT INTO users VALUES (17, 7, 5, 'u1@example.org',    'u1', 'Y');
INSERT INTO users VALUES (18, 7, 6, 'u2@example.org',    'u2', 'Y');
INSERT INTO users VALUES (19, 7, 3, 'u3@example.org',    'u3', 'Y');

-- INSERT INTO users VALUES (20, 0, 5, '@.',             NULL, 'N');  -- catchall

INSERT INTO policy (id, policy_name,
  virus_lover, spam_lover, banned_files_lover, bad_header_lover,
  bypass_virus_checks, bypass_spam_checks,
  bypass_banned_checks, bypass_header_checks, spam_modifies_subj,
  spam_tag_level, spam_tag2_level, spam_kill_level) VALUES
  (1, 'Non-paying',    'N','N','N','N', 'Y','Y','Y','N', 'Y', 3.0,   7, 10),
  (2, 'Uncensored',    'Y','Y','Y','Y', 'N','N','N','N', 'N', 3.0, 999, 999),
  (3, 'Wants all spam','N','Y','N','N', 'N','N','N','N', 'Y', 3.0, 999, 999),
  (4, 'Wants viruses', 'Y','N','Y','Y', 'N','N','N','N', 'Y', 3.0, 6.9, 6.9),
  (5, 'Normal',        'N','N','N','N', 'N','N','N','N', 'Y', 3.0, 6.9, 6.9),
  (6, 'Trigger happy', 'N','N','N','N', 'N','N','N','N', 'Y', 3.0,   5, 5),
  (7, 'Permissive',    'N','N','N','Y', 'N','N','N','N', 'Y', 3.0,  10, 20),
  (8, '6.5/7.8',       'N','N','N','N', 'N','N','N','N', 'N', 3.0, 6.5, 7.8),
  (9, 'userB',         'N','N','N','Y', 'N','N','N','N', 'Y', 3.0, 6.3, 6.3),
  (10,'userC',         'N','N','N','N', 'N','N','N','N', 'N', 3.0, 6.0, 6.0),
  (11,'userD',         'Y','N','Y','Y', 'N','N','N','N', 'N', 3.0,   7, 7);

-- sender envelope addresses needed for white/blacklisting
INSERT INTO mailaddr VALUES (1, 5, '@example.com');
INSERT INTO mailaddr VALUES (2, 9, 'owner-postfix-users@postfix.org');
INSERT INTO mailaddr VALUES (3, 9, 'amavis-user-admin@lists.sourceforge.net');
INSERT INTO mailaddr VALUES (4, 9, 'makemoney@example.com');
INSERT INTO mailaddr VALUES (5, 5, '@example.net');
INSERT INTO mailaddr VALUES (6, 9, 'spamassassin-talk-admin@lists.sourceforge.net');
INSERT INTO mailaddr VALUES (7, 9, 'spambayes-bounces@python.org');

-- whitelist for user 14, i.e. default for recipients in domain sub1.example.net
INSERT INTO wblist VALUES (14, 1, 'W');
INSERT INTO wblist VALUES (14, 3, 'W');

-- whitelist and blacklist for user 17, i.e. u1@example.org
INSERT INTO wblist VALUES (17, 2, 'W');
INSERT INTO wblist VALUES (17, 3, 'W');
INSERT INTO wblist VALUES (17, 6, 'W');
INSERT INTO wblist VALUES (17, 7, 'W');
INSERT INTO wblist VALUES (17, 5, 'B');
INSERT INTO wblist VALUES (17, 4, 'B');

-- $sql_select_policy setting in amavisd.conf tells amavisd
-- how to fetch per-recipient policy settings.
-- See comments there. Example:
--
-- SELECT *,users.id FROM users,policy
--   WHERE (users.policy_id=policy.id) AND (users.email IN (%k))
--   ORDER BY users.priority DESC;
--
-- $sql_select_white_black_list in amavisd.conf tells amavisd
-- how to check sender in per-recipient whitelist/blacklist.
-- See comments there. Example:
--
-- SELECT wb FROM wblist,mailaddr
--   WHERE (wblist.rid=?) AND (wblist.sid=mailaddr.id) AND (mailaddr.email IN (%k))
--   ORDER BY mailaddr.priority DESC;



NOTE: the SELECT, INSERT and UPDATE clauses as used by the amavisd-new
program are configurable through %sql_clause; see amavisd.conf-default

Upgrading from pre 2.4.0 amavisd-new SQL schema to the 2.4.0 schema requires
adding column 'quar_loc' to table msgs, and creating FOREIGN KEY constraint
to facilitate deletion of expired records.

The following clauses should be executed for upgrading pre-2.4.0 amavisd-new
SQL schema to the 2.4.0 schema:

-- mandatory change:
  ALTER TABLE msgs ADD quar_loc varchar(255) DEFAULT '';

-- optional but highly recommended:
  ALTER TABLE quarantine
    ADD FOREIGN KEY (mail_id) REFERENCES msgs(mail_id) ON DELETE CASCADE;
  ALTER TABLE msgrcpt
    ADD FOREIGN KEY (mail_id) REFERENCES msgs(mail_id) ON DELETE CASCADE;

-- the following two ALTERs are not essential; if data type of maddr.id is
-- incompatible with msgs.sid and msgs.rid (e.g. BIGINT vs. INT) and MySQL
-- complains, don't bother to apply the constraint:
  ALTER TABLE msgs
    ADD FOREIGN KEY (sid) REFERENCES maddr(id) ON DELETE RESTRICT;
  ALTER TABLE msgrcpt
    ADD FOREIGN KEY (rid) REFERENCES maddr(id) ON DELETE RESTRICT;
