# $Id: AuthDBI.pm 1140245 2011-06-27 17:25:53Z phred $
package Apache::AuthDBI;

$Apache::AuthDBI::VERSION = '1.11';

# 1: report about cache miss
# 2: full debug output
$Apache::AuthDBI::DEBUG = 0;

use constant MP2 => (exists $ENV{MOD_PERL_API_VERSION}
    && $ENV{MOD_PERL_API_VERSION} == 2) ? 1 : 0;

BEGIN {
  my @constants = qw( OK AUTH_REQUIRED FORBIDDEN DECLINED SERVER_ERROR );
  if (MP2) {
	require Apache2::Access;
    require Apache2::Const;
    require Apache2::RequestRec;
    require Apache2::RequestUtil;
    require Apache2::Log;
    import Apache2::Const @constants;
  }
  else {
    require Apache::Constants;
    import Apache::Constants @constants;
  }
}

use strict;
use DBI ();
use Digest::SHA1 ();
use Digest::MD5 ();

sub debug {
    print STDERR "$_[1]\n" if $_[0] <= $Apache::AuthDBI::DEBUG;
}

sub push_handlers {
  if (MP2) {
		require Apache2::ServerUtil;
		my $s = Apache2::ServerUtil->server;
		$s->push_handlers(@_);
  }
  else {
    Apache->push_handlers(@_);
  }
}

# configuration attributes, defaults will be overwritten with values
# from .htaccess.
my %Config = (
              'Auth_DBI_data_source'      => '',
              'Auth_DBI_username'         => '',
              'Auth_DBI_password'         => '',
              'Auth_DBI_pwd_table'        => '',
              'Auth_DBI_uid_field'        => '',
              'Auth_DBI_pwd_field'        => '',
              'Auth_DBI_pwd_whereclause'  => '',
              'Auth_DBI_grp_table'        => '',
              'Auth_DBI_grp_field'        => '',
              'Auth_DBI_grp_whereclause'  => '',
              'Auth_DBI_log_field'        => '',
              'Auth_DBI_log_string'       => '',
              'Auth_DBI_authoritative'    => 'on',
              'Auth_DBI_nopasswd'         => 'off',
              'Auth_DBI_encrypted'        => 'on',
              'Auth_DBI_encryption_salt'  => 'password',
              #Using Two (or more) Methods Will Allow for Fallback to older Methods
              'Auth_DBI_encryption_method'=> 'sha1hex/md5/crypt',
              'Auth_DBI_uidcasesensitive' => 'on',
              'Auth_DBI_pwdcasesensitive' => 'on',
              'Auth_DBI_placeholder'      => 'off',
              'Auth_DBI_expeditive'       => 'on',
             );

# stores the configuration of current URL.
# initialized  during authentication, eventually re-used for authorization.
my $Attr = {};

# global cache: all records are put into one string.
# record separator is a newline. Field separator is $;.
# every record is a list of id, time of last access, password, groups
#(authorization only).
# the id is a comma separated list of user_id, data_source, pwd_table,
# uid_field.
# the first record is a timestamp, which indicates the last run of the
# CleanupHandler followed by the child counter.
my $Cache = time . "$;0\n";

# unique id which serves as key in $Cache.
# the id is generated during authentication and re-used for authorization.
my $ID;

# minimum lifetimes of cache entries in seconds.
# setting the CacheTime to 0 will not use the cache at all.
my $CacheTime = 0;

# supposed to be called in a startup script.
# sets CacheTime to a user defined value.
sub setCacheTime {
    my $class      = shift;
    my $cache_time = shift;

    # sanity check
    $CacheTime = $cache_time if $cache_time =~ /\d+/;
}

# minimum time interval in seconds between two runs of the PerlCleanupHandler.
# setting CleanupTime to 0 will run the PerlCleanupHandler after every request.
# setting CleanupTime to a negative value will disable the PerlCleanupHandler.
my $CleanupTime = -1;

# supposed to be called in a startup script.
# sets CleanupTime to a user defined value.
sub setCleanupTime {
    my $class        = shift;
    my $cleanup_time = shift;

    # sanity check
    $CleanupTime = $cleanup_time if $cleanup_time =~ /\-*\d+/;
}

# optionally the string with the global cache can be stored in a shared memory
# segment. the segment will be created from the first child and it will be
# destroyed if the last child exits. the reason for not handling everything
# in the main server is simply, that there is no way to setup
# an ExitHandler which runs in the main server and which would remove the
# shared memory and the semaphore.hence we have to keep track about the
# number of children, so that the last one can do all the cleanup.
# creating the shared memory in the first child also has the advantage,
# that we don't have to cope  with changing the ownership. if a shm-function
# fails, the global cache will automatically fall back to one string
# per process.
my $SHMKEY  =     0; # unique key for shared memory segment and semaphore set
my $SEMID   =     0; # id of semaphore set
my $SHMID   =     0; # id of shared memory segment
my $SHMSIZE = 50000; # default size of shared memory segment
my $SHMPROJID =   1; # default project id for shared memory segment

# Supposed to be called in a startup script.
# Sets SHMPROJID to a user defined value
sub setProjID {
    my $class = shift;
    my $shmprojid = shift;

    #Set ProjID prior to calling initIPC!
    return if $SHMKEY;

    # sanity check - Must be numeric and less than or equal to 255
    $SHMPROJID = int($shmprojid)
        if $shmprojid =~ /\d{1,3}/ && $shmprojid <= 255 && $shmprojid > 0;
}

# shortcuts for semaphores
my $obtain_lock  = pack("sss", 0,  0, 0) . pack("sss", 0, 1, 0);
my $release_lock = pack("sss", 0, -1, 0);

# supposed to be called in a startup script.
# sets SHMSIZE to a user defined value and initializes the unique key,
# used for the shared memory segment and for the semaphore set.
# creates a PerlChildInitHandler which creates the shared memory segment
# and the semaphore set. creates a PerlChildExitHandler which removes
# the shared memory segment and the semaphore set upon server shutdown.
# keep in mind, that this routine runs only once, when the main server
#starts up.
sub initIPC {
    my $class   = shift;
    my $shmsize = shift;

    require IPC::SysV;

    # make sure, this method is called only once
    return if $SHMKEY;

    # ensure minimum size of shared memory segment
    $SHMSIZE = $shmsize if $shmsize >= 500;

    # generate unique key based on path of AuthDBI.pm + SHMPROJID
    foreach my $file (keys %INC) {
        if ($file eq 'Apache/AuthDBI.pm') {
            $SHMKEY = IPC::SysV::ftok($INC{$file}, $SHMPROJID);
            last;
        }
    }

    # provide a handler which initializes the shared memory segment
    #(first child) or which increments the child counter.
    push_handlers(PerlChildInitHandler => \&childinit);

    # provide a handler which decrements the child count or which
    # destroys the shared memory
    # segment upon server shutdown, which is defined by the exit of the
    # last child.
    push_handlers(PerlChildExitHandler => \&childexit);
}

# authentication handler
sub authen {
    my ($r) = @_;

    my ($key, $val, $dbh);
    my $prefix = "$$ Apache::AuthDBI::authen";

    if ($Apache::AuthDBI::DEBUG > 1) {
        my $type = '';
        if (MP2) {
          $type .= 'initial ' if $r->is_initial_req();
          $type .= 'main'     if $r->main();
        }
        else {
          $type .= 'initial ' if $r->is_initial_req;
          $type .= 'main'     if $r->is_main;
        }
        debug (1, "==========\n$prefix request type = >$type<");
    }

    return MP2 ? Apache2::Const::OK() : Apache::Constants::OK()
        unless $r->is_initial_req; # only the first internal request

    debug (2, "REQUEST:" . $r->as_string);

    # here the dialog pops up and asks you for username and password
    my ($res, $passwd_sent) = $r->get_basic_auth_pw;
    {
      no warnings qw(uninitialized);
      debug (2, "$prefix get_basic_auth_pw: res = >$res<, password sent = >$passwd_sent<");
    }
    return $res if $res; # e.g. HTTP_UNAUTHORIZED

    # get username
    my $user_sent = $r->user;
    debug(2, "$prefix user sent = >$user_sent<");

    # do we use shared memory for the global cache ?
    debug (2, "$prefix cache in shared memory, shmid $SHMID, shmsize $SHMSIZE, semid $SEMID");

    # get configuration
    while(($key, $val) = each %Config) {
        $val = $r->dir_config($key) || $val;
        $key =~ s/^Auth_DBI_//;
        $Attr->{$key} = $val;
        debug(2, sprintf("$prefix Config{ %-16s } = %s", $key, $val));
    }

    # parse connect attributes, which may be tilde separated lists
    my @data_sources = split /~/, $Attr->{data_source};
    my @usernames    = split /~/, $Attr->{username};
    my @passwords    = split /~/, $Attr->{password};
    # use ENV{DBI_DSN} if not defined
    $data_sources[0] = '' unless $data_sources[0];

    # obtain the id for the cache
    # remove any embedded attributes, because of trouble with regexps
    my $data_src = $Attr->{data_source};
    $data_src =~ s/\(.+\)//g;

    $ID = join ',',
        $user_sent, $data_src, $Attr->{pwd_table}, $Attr->{uid_field};

    # if not configured decline
    unless ($Attr->{pwd_table} && $Attr->{uid_field} && $Attr->{pwd_field}) {
        debug (2, "$prefix not configured, return DECLINED");
        return MP2 ? Apache2::Const::DECLINED() :
            Apache::Constants::DECLINED();
    }

    # do we want Windows-like case-insensitivity?
    $user_sent   = lc $user_sent   if $Attr->{uidcasesensitive} eq "off";
    $passwd_sent = lc $passwd_sent if $Attr->{pwdcasesensitive} eq "off";

    # check whether the user is cached but consider that the password
    # possibly has changed
    my $passwd = '';
    if ($CacheTime) { # do we use the cache ?
        if ($SHMID) { # do we keep the cache in shared memory ?
            semop($SEMID, $obtain_lock)
                or warn "$prefix semop failed \n";
            shmread($SHMID, $Cache, 0, $SHMSIZE)
                or warn "$prefix shmread failed \n";
            substr($Cache, index($Cache, "\0")) = '';
            semop($SEMID, $release_lock) 
                or warn "$prefix semop failed \n";
        }
        # find id in cache
        my ($last_access, $passwd_cached, $groups_cached);
        if ($Cache =~ /$ID$;(\d+)$;(.+)$;(.*)\n/) {
            $last_access   = $1;
            $passwd_cached = $2;
            $groups_cached = $3;
            debug(2, "$prefix cache: found >$ID< >$last_access< >$passwd_cached<");

            my @passwds_to_check =
                &get_passwds_to_check(
                                      $Attr,
                                      user_sent   => $user_sent,
                                      passwd_sent => $passwd_sent,
                                      password    => $passwd_cached
                                     );

            debug(2, "$prefix " . scalar(@passwds_to_check) . " passwords to check");
            foreach my $passwd_to_check (@passwds_to_check) {
              # match cached password with password sent
              $passwd = $passwd_cached if $passwd_to_check eq $passwd_cached;
              last if $passwd;
            }
        }
    }

    # found in cache
    if ($passwd) {
        debug(2, "$prefix passwd found in cache");
    }
    else {
        # password not cached or changed
        debug (2, "$prefix passwd not found in cache");

        # connect to database, use all data_sources until the connect succeeds
        for (my $j = 0; $j <= $#data_sources; $j++) {
            last if (
                     $dbh = DBI->connect(
                                         $data_sources[$j],
                                         $usernames[$j],
                                         $passwords[$j]
                                        )
                    );
        }
        unless ($dbh) {
            $r->log_reason(
                           "$prefix db connect error with data_source " .
                           ">$Attr->{data_source}<: $DBI::errstr",
                           $r->uri
                          );
            return MP2 ? Apache2::Const::SERVER_ERROR() :
                Apache::Constants::SERVER_ERROR();
        }

        # generate statement
        my $user_sent_quoted = $dbh->quote($user_sent);
        my $select    = "SELECT $Attr->{pwd_field}";
        my $from      = "FROM $Attr->{pwd_table}";
        my $where     = ($Attr->{uidcasesensitive} eq "off") ?
            "WHERE lower($Attr->{uid_field}) =" :
                "WHERE $Attr->{uid_field} =";
        my $compare   = ($Attr->{placeholder} eq "on")  ?
            "?" : "$user_sent_quoted";
        my $statement = "$select $from $where $compare";
        $statement   .= " AND $Attr->{pwd_whereclause}"
            if $Attr->{pwd_whereclause};

        debug(2, "$prefix statement: $statement");

        # prepare statement
        my $sth;
        unless ($sth = $dbh->prepare($statement)) {
            $r->log_reason("$prefix can not prepare statement: $DBI::errstr", $r->uri);
            $dbh->disconnect;
            return MP2 ? Apache2::Const::SERVER_ERROR() :
                Apache::Constants::SERVER_ERROR();
        }

        # execute statement
        my $rv;
        unless ($rv = ($Attr->{placeholder} eq "on") ?
                $sth->execute($user_sent) : $sth->execute) {
            $r->log_reason("$prefix can not execute statement: $DBI::errstr", $r->uri);
            $dbh->disconnect;
            return MP2 ? Apache2::Const::SERVER_ERROR() :
                Apache::Constants::SERVER_ERROR();
        }

        my $password;
        $sth->execute();
        $sth->bind_columns(\$password);
        my $cnt = 0;
        while ($sth->fetch()) {
            $password =~ s/ +$// if $password;
            $passwd .= "$password$;";
            $cnt++;
        }

        chop $passwd if $passwd;
        # so we can distinguish later on between no password and empty password
        undef $passwd if 0 == $cnt;

        if ($sth->err) {
            $dbh->disconnect;
            return MP2 ? Apache2::Const::SERVER_ERROR() :
                Apache::Constants::SERVER_ERROR();
        }
        $sth->finish;

        # re-use dbh for logging option below
        $dbh->disconnect unless $Attr->{log_field} && $Attr->{log_string};
    }

    $r->subprocess_env(REMOTE_PASSWORDS => $passwd);
    debug(2, "$prefix passwd = >$passwd<");

    # check if password is needed
    unless ($passwd) { # not found in database
        # if authoritative insist that user is in database
        if ($Attr->{authoritative} eq 'on') {
            $r->log_reason("$prefix password for user $user_sent not found", $r->uri);
            $r->note_basic_auth_failure;
            return MP2 ? Apache2::Const::AUTH_REQUIRED() :
                Apache::Constants::AUTH_REQUIRED();
        }
        else {
            # else pass control to the next authentication module
            return MP2 ? Apache2::Const::DECLINED() :
                Apache::Constants::DECLINED();
        }
    }

    # allow any password if nopasswd = on and the retrieved password is empty
    if ($Attr->{nopasswd} eq 'on' && !$passwd) {
        return MP2 ? Apache2::Const::OK() : Apache::Constants::OK();
    }

    # if nopasswd is off, reject user
    unless ($passwd_sent && $passwd) {
        $r->log_reason("$prefix user $user_sent: empty password(s) rejected", $r->uri);
        $r->note_basic_auth_failure;
        return MP2 ? Apache2::Const::AUTH_REQUIRED() :
            Apache::Constants::AUTH_REQUIRED();
    }

    # compare passwords
    my $found = 0;
    foreach my $password (split /$;/, $passwd) {
        # compare all the passwords using as many encryption methods
        # in fallback as needed
        my @passwds_to_check =
            &get_passwds_to_check(
                                  $Attr,
                                  user_sent   => $user_sent,
                                  passwd_sent => $passwd_sent,
                                  password    => $password
                                 );

        debug (2, "$prefix " . scalar(@passwds_to_check) . " passwords to check");

        foreach my $passwd_to_check (@passwds_to_check) {
          debug(
                2,
                "$prefix user $user_sent: Password after Preparation " .
                ">$passwd_to_check< - trying for a match with >$password<"
               );

          if ($passwd_to_check eq $password) {
              $found = 1;
              $r->subprocess_env(REMOTE_PASSWORD => $password);
              debug (
                     2,
                     "$prefix user $user_sent: Password from Web Server " .
                     ">$passwd_sent< - Password after Preparation >$passwd_to_check< - " .
                     "password match for >$password<"
                    );

            # update timestamp and cache userid/password if CacheTime
            # is configured
            if ($CacheTime) { # do we use the cache ?
                if ($SHMID) { # do we keep the cache in shared memory ?
                    semop($SEMID, $obtain_lock)
                        or warn "$prefix semop failed \n";
                    shmread($SHMID, $Cache, 0, $SHMSIZE)
                        or warn "$prefix shmread failed \n";
                    substr($Cache, index($Cache, "\0")) = '';
                }

                # update timestamp and password or append new record
                my $now = time;
                if (!($Cache =~ s/$ID$;\d+$;.*$;(.*)\n/$ID$;$now$;$password$;$1\n/)) {
                    $Cache .= "$ID$;$now$;$password$;\n";
                }

                if ($SHMID) { # write cache to shared memory
                    shmwrite($SHMID, $Cache, 0, $SHMSIZE)
                        or warn "$prefix shmwrite failed \n";
                    semop($SEMID, $release_lock)
                        or warn "$prefix semop failed \n";
                }
            }
            last;
          }
        }

        #if the passwd matched (encrypted or otherwise), don't check the
        # myriad other passwords that may or may not exist
        last if $found > 0 ;
    }

    unless ($found) {
        $r->log_reason("$prefix user $user_sent: password mismatch", $r->uri);
        $r->note_basic_auth_failure;
        return MP2 ? Apache2::Const::AUTH_REQUIRED() :
            Apache::Constants::AUTH_REQUIRED();
    }

    # logging option
    if ($Attr->{log_field} && $Attr->{log_string}) {
        if (!$dbh) { # connect to database if not already done
            my $connect;
            for (my $j = 0; $j <= $#data_sources; $j++) {
                if ($dbh = DBI->connect(
                                        $data_sources[$j],
                                        $usernames[$j],
                                        $passwords[$j]
                                       )) {
                    $connect = 1;
                    last;
                }
            }
            unless ($connect) {
                $r->log_reason("$prefix db connect error with $Attr->{data_source}", $r->uri);
                return MP2 ? Apache2::Const::SERVER_ERROR() :
                    Apache::Constants::SERVER_ERROR();
            }
        }
        my $user_sent_quoted = $dbh->quote($user_sent);
        my $statement = "UPDATE $Attr->{pwd_table} SET $Attr->{log_field} = " .
            "$Attr->{log_string} WHERE $Attr->{uid_field}=$user_sent_quoted";

        debug(2, "$prefix statement: $statement");

        unless ($dbh->do($statement)) {
            $r->log_reason("$prefix can not do statement: $DBI::errstr", $r->uri);
            $dbh->disconnect;
            return MP2 ? Apache2::Const::SERVER_ERROR() :
                Apache::Constants::SERVER_ERROR();
        }
        $dbh->disconnect;
    }

    # Unless the cache or the CleanupHandler is disabled, the
    # CleanupHandler is initiated if the last run was more than
    # $CleanupTime seconds before.
    # Note, that it runs after the request, hence it cleans also the
    # authorization entries
    if ($CacheTime and $CleanupTime >= 0) {
        my $diff = time - substr $Cache, 0, index($Cache, "$;");
        debug(
              2,
              "$prefix secs since last CleanupHandler: $diff, CleanupTime: " .
              "$CleanupTime"
             );

        if ($diff > $CleanupTime) {
            debug (2, "$prefix push PerlCleanupHandler");
            push_handlers(PerlCleanupHandler => \&cleanup);
        }
    }

    debug (2, "$prefix return OK\n");
    return MP2 ? Apache2::Const::OK() : Apache::Constants::OK();
}

#Encrypts a password in all supported/requested methods and passes back
#array for comparison
sub get_passwds_to_check {
    my $Attr = shift;
    my %params = @_;


    my ($prefix) = "$$ Apache::AuthDBI::get_passwds_to_check ";

    my ($salt, @passwds_to_check);

    if ($Attr->{encrypted} eq 'on') {
        #SHA1
        if ($Attr->{encryption_method} =~ /(^|\/)sha1hex($|\/)/i) {
            push @passwds_to_check, SHA1_digest(
                                                text   => $params{'passwd_sent'},
                                                format => 'hex'
                                               );
        }

        #MD5
        if ($Attr->{encryption_method} =~ /(^|\/)md5hex($|\/)/i) {
            push @passwds_to_check, MD5_digest(
                                               text  => $params{'passwd_sent'},
                                               format => 'hex'
                                              );
        }

        #CRYPT
        if ($Attr->{encryption_method} =~ /(^|\/)crypt($|\/)/i) {
            $salt = $Attr->{encryption_salt} eq 'userid' ?
                $params{'user_sent'} : $params{'password'};
            #Bug Fix in v0.94 (marked as 0.93 in file.  salt was NOT being sent
            # to crypt) - KAM - 06-16-2005
            push @passwds_to_check, crypt($params{'passwd_sent'}, $salt);
        }

        #WE DIDN'T GET ANY PASSWORDS TO CHECK.  MUST BE A PROBLEM
        if (scalar(@passwds_to_check) < 1) {
            debug (2, "$prefix Error: No Valid Encryption Method Specified");
        }
    }
    else {
        #IF NO ENCRYPTION, JUST PUSH THE CLEARTEXT PASS
        push @passwds_to_check, $params{'passwd_sent'};
    }

    return (@passwds_to_check);
}

# authorization handler, it is called immediately after the authentication
sub authz {
    my $r = shift;

    my ($key, $val, $dbh);
    my $prefix = "$$ Apache::AuthDBI::authz ";

    if ($Apache::AuthDBI::DEBUG > 1) {
        my $type = '';
        if (MP2) {
          $type .= 'initial ' if $r->is_initial_req();
          $type .= 'main'     if $r->main();
        }
        else {
          $type .= 'initial ' if $r->is_initial_req;
          $type .= 'main'     if $r->is_main;
        }
        debug(1, "==========\n$prefix request type = >$type<");
    }

    # only the first internal request
    unless ($r->is_initial_req) {
      return MP2 ? Apache2::Const::OK() : Apache::Constants::OK();
    }

    my $user_result  = MP2 ? Apache2::Const::DECLINED() :
        Apache::Constants::DECLINED();
    my $group_result = MP2 ? Apache2::Const::DECLINED() :
        Apache::Constants::DECLINED();

    # get username
    my $user_sent = $r->user;
    debug(2, "$prefix user sent = >$user_sent<");

    # here we could read the configuration, but we re-use the configuration
    # from the authentication

    # parse connect attributes, which may be tilde separated lists
    my @data_sources = split /~/, $Attr->{data_source};
    my @usernames    = split /~/, $Attr->{username};
    my @passwords    = split /~/, $Attr->{password};
    # use ENV{DBI_DSN} if not defined
    $data_sources[0] = '' unless $data_sources[0];

    # if not configured decline
    unless ($Attr->{pwd_table} && $Attr->{uid_field} && $Attr->{grp_field}) {
        debug(2, "$prefix not configured, return DECLINED");
        return MP2 ? Apache2::Const::DECLINED() :
            Apache::Constants::DECLINED();
    }

    # do we want Windows-like case-insensitivity?
    $user_sent = lc $user_sent if $Attr->{uidcasesensitive} eq "off";

    # select code to return if authorization is denied:
    my $authz_denied;
    if (MP2) {
      $authz_denied = $Attr->{expeditive} eq 'on' ?
          Apache2::Const::FORBIDDEN() : Apache2::Const::AUTH_REQUIRED();
    }
    else {
      $authz_denied = $Attr->{expeditive} eq 'on' ?
          Apache::Constants::FORBIDDEN() : Apache::Constants::AUTH_REQUIRED();
    }

    # check if requirements exists
    my $ary_ref = $r->requires;
    unless ($ary_ref) {
        if ($Attr->{authoritative} eq 'on') {
            $r->log_reason("user $user_sent denied, no access rules specified (DBI-Authoritative)", $r->uri);
            if ($authz_denied == (MP2 ? Apache2::Const::AUTH_REQUIRED() :
                Apache::Constants::AUTH_REQUIRED())) {
                $r->note_basic_auth_failure;
            }
            return $authz_denied;
        }
        debug (2, "$prefix no requirements and not authoritative, return DECLINED");
        return MP2 ? Apache2::Const::DECLINED() :
            Apache::Constants::DECLINED();
    }

    # iterate over all requirement directives and store them according to
    # their type (valid-user, user, group)
    my($valid_user, $user_requirements, $group_requirements);
    foreach my $hash_ref (@$ary_ref) {
        while (($key,$val) = each %$hash_ref) {
            last if $key eq 'requirement';
        }
        $val =~ s/^\s*require\s+//;

        # handle different requirement-types
        if ($val =~ /valid-user/) {
            $valid_user = 1;
        }
        elsif ($val =~ s/^user\s+//g) {
            $user_requirements .= " $val";
        }
        elsif ($val =~ s/^group\s+//g) {
            $group_requirements .= " $val";
        }
    }
    $user_requirements  =~ s/^ //g if $user_requirements;
    $group_requirements =~ s/^ //g if $group_requirements;

    {
        no warnings qw(uninitialized);

                                      debug(
                                            2,
                                            "$prefix requirements: [valid-user=>$valid_user<] [user=>" .
                                            "$user_requirements<] [group=>$group_requirements<]"
                                           );
    }

    # check for valid-user
    if ($valid_user) {
        $user_result = MP2 ? Apache2::Const::OK() : Apache::Constants::OK();
        debug(2, "$prefix user_result = OK: valid-user");
    }

    # check for users
    if (($user_result != (MP2 ? Apache2::Const::OK() :
         Apache::Constants::OK())) && $user_requirements) {

        $user_result = MP2 ? Apache2::Const::AUTH_REQUIRED() :
            Apache::Constants::AUTH_REQUIRED();

        foreach my $user_required (split /\s+/, $user_requirements) {
            if ($user_required eq $user_sent) {
                debug (2, "$prefix user_result = OK for $user_required");
                $user_result = MP2 ? Apache2::Const::OK() :
                    Apache::Constants::OK();
                last;
            }
        }
    }

    my $user_result_valid = MP2 ? Apache2::Const::OK() :
        Apache::Constants::OK();

    # check for groups
    if ($user_result != $user_result_valid  && $group_requirements) {
        debug(2, "$prefix: checking for groups >$group_requirements<");
        $group_result = MP2 ? Apache2::Const::AUTH_REQUIRED() : Apache::Constants::AUTH_REQUIRED();
        my $group;

        # check whether the user is cached but consider that the group
        # possibly has changed
        my $groups = '';
        if ($CacheTime) { # do we use the cache ?
            # we need to get the cached groups for the current id,
            # which has been read already
            # during authentication, so we do not read the Cache from
            # shared memory again
            my ($last_access, $passwd_cached, $groups_cached);
            if ($Cache =~ /$ID$;(\d+)$;(.*)$;(.+)\n/) {
                $last_access   = $1;
                $passwd_cached = $2;
                $groups_cached = $3;
                debug(2, "$prefix cache: found >$ID< >$last_access< >$groups_cached");

                REQUIRE_1:
                foreach my $group_required (split /\s+/, $group_requirements) {
                    foreach $group (split(/,/, $groups_cached)) {
                        if ($group_required eq $group) {
                            $groups = $groups_cached;
                            last REQUIRE_1;
                        }
                    }
                }
            }
        }

        # found in cache
        if ($groups) {
            debug(2, "$prefix groups found in cache");
        }
        else {
            # groups not cached or changed
            debug(2, "$prefix groups not found in cache");

            # connect to database, use all data_sources until the connect
            # succeeds
            my $connect;
            for (my $j = 0; $j <= $#data_sources; $j++) {
                if ($dbh = DBI->connect(
                                        $data_sources[$j],
                                        $usernames[$j],
                                        $passwords[$j]
                                       )) {
                    $connect = 1;
                    last;
                }
            }
            unless ($connect) {
                $r->log_reason(
                               "$prefix db connect error with " .
                               "$Attr->{data_source}",
                               $r->uri
                              );
                return MP2 ? Apache2::Const::SERVER_ERROR() :
                    Apache::Constants::SERVER_ERROR();
            }

            # generate statement
            my $user_sent_quoted = $dbh->quote($user_sent);
            my $select    = "SELECT $Attr->{grp_field}";
            my $from      = ($Attr->{grp_table}) ?
                "FROM $Attr->{grp_table}" : "FROM $Attr->{pwd_table}";
            my $where     = ($Attr->{uidcasesensitive} eq "off") ?
                "WHERE lower($Attr->{uid_field}) =" :
                    "WHERE $Attr->{uid_field} =";
            my $compare   = ($Attr->{placeholder}      eq "on")  ?
                "?" : "$user_sent_quoted";
            my $statement = "$select $from $where $compare";
            $statement   .= " AND $Attr->{grp_whereclause}"
                if ($Attr->{grp_whereclause});

            debug(2, "$prefix statement: $statement");

            # prepare statement
            my $sth;
            unless ($sth = $dbh->prepare($statement)) {
                $r->log_reason(
                               "can not prepare statement: $DBI::errstr",
                               $r->uri
                              );
                $dbh->disconnect;
                return MP2 ? Apache2::Const::SERVER_ERROR() :
                    Apache::Constants::SERVER_ERROR();
            }

            # execute statement
            my $rv;
            unless ($rv = ($Attr->{placeholder} eq "on") ?
                    $sth->execute($user_sent) : $sth->execute) {
                $r->log_reason(
                               "can not execute statement: $DBI::errstr",
                               $r->uri
                              );
                $dbh->disconnect;
                return MP2 ? Apache2::Const::SERVER_ERROR() :
                    Apache::Constants::SERVER_ERROR();
            }

            # fetch result and build a group-list
            # strip trailing blanks for fixed-length data-type
            while (my $group = $sth->fetchrow_array) {
                $group =~ s/ +$//;
                $groups .= "$group,";
            }
            chop $groups if $groups;

            $sth->finish;
            $dbh->disconnect;
        }

        $r->subprocess_env(REMOTE_GROUPS => $groups);
        debug(2, "$prefix groups = >$groups<\n");

        # skip through the required groups until the first matches
      REQUIRE_2:
        foreach my $group_required (split /\s+/, $group_requirements) {
            foreach my $group (split(/,/, $groups)) {
                # check group
                if ($group_required eq $group) {
                    $group_result = MP2 ? Apache2::Const::OK() :
                        Apache::Constants::OK();
                    $r->subprocess_env(REMOTE_GROUP => $group);

                    debug(
                          2,
                          "$prefix user $user_sent: group_result = OK " .
                          "for >$group<"
                         );

                    # update timestamp and cache userid/groups if
                    # CacheTime is configured
                    if ($CacheTime) { # do we use the cache ?
                        if ($SHMID) { # do we keep the cache in shared memory ?
                            semop($SEMID, $obtain_lock)
                                or warn "$prefix semop failed \n";
                            shmread($SHMID, $Cache, 0, $SHMSIZE)
                                or warn "$prefix shmread failed \n";
                            substr($Cache, index($Cache, "\0")) = '';
                        }

                        # update timestamp and groups
                        my $now = time;
                        # entry must exists from authentication
                        $Cache =~ s/$ID$;\d+$;(.*)$;.*\n/$ID$;$now$;$1$;$groups\n/;
                        if ($SHMID) { # write cache to shared memory
                            shmwrite($SHMID, $Cache, 0, $SHMSIZE)
                                or warn "$prefix shmwrite failed \n";
                            semop($SEMID, $release_lock)
                                or warn "$prefix semop failed \n";
                        }
                    }
                    last REQUIRE_2;
                }
            }
        }
    }

    # check the results of the requirement checks
    if ($Attr->{authoritative} eq 'on' &&
        (
         $user_result != (MP2 ?
         Apache2::Const::OK() :
         Apache::Constants::OK())
        )
        && (
            $group_result != (MP2 ? Apache2::Const::OK() :
            Apache::Constants::OK())
           )
       ) {
        my $reason;
        if ($user_result == (MP2 ? Apache2::Const::AUTH_REQUIRED() :
            Apache::Constants::AUTH_REQUIRED())) {
            $reason .= " USER";
        }
        if ($group_result == (MP2 ? Apache2::Const::AUTH_REQUIRED() :
            Apache::Constants::AUTH_REQUIRED())) {
            $reason .= " GROUP";
        }
        $r->log_reason(
                       "DBI-Authoritative: Access denied on $reason rule(s)",
                       $r->uri
                      );

        if ($authz_denied == (MP2 ? Apache2::Const::AUTH_REQUIRED() :
            Apache::Constants::AUTH_REQUIRED())) {
            $r->note_basic_auth_failure;
        }

        return $authz_denied;
    }

    # return OK if authorization was successful
    my $success  = MP2 ? Apache2::Const::OK() :
        Apache::Constants::OK();
    my $declined = MP2 ? Apache2::Const::DECLINED() :
        Apache::Constants::DECLINED();

    if (
        ($user_result != $declined && $user_result == $success)
        ||
        ($group_result != $declined && $group_result == $success)
       ) {
        debug(2, "$prefix return OK");
        return MP2 ? Apache2::Const::OK() : Apache::Constants::OK();
    }

    # otherwise fall through
    debug(2, "$prefix fall through, return DECLINED");
    return MP2 ? Apache2::Const::DECLINED() : Apache::Constants::DECLINED();
}

sub dec2hex {
    my $dec = shift;

    return sprintf("%lx", $dec);
}

# The PerlChildInitHandler initializes the shared memory segment (first child)
# or increments the child counter.
# Note: this handler runs in every child server, but not in the main server.
# create (or re-use existing) semaphore set
sub childinit {

    my $prefix = "$$ Apache::AuthDBI         PerlChildInitHandler";

    my $SHMKEY_hex = dec2hex($SHMKEY);

    debug(
          2,
          "$prefix SHMProjID = >$SHMPROJID< Shared Memory Key >$SHMKEY " .
          "Decimal - $SHMKEY_hex Hex<"
         );

    $SEMID = semget(
                    $SHMKEY,
                    1,
                    IPC::SysV::IPC_CREAT() |
                    IPC::SysV::S_IRUSR() |
                    IPC::SysV::S_IWUSR()
                   );
    unless (defined $SEMID) {
        warn "$prefix semget failed - SHMKEY $SHMKEY - Error $!\n";
        if (uc chomp $! eq 'PERMISSION DENIED') {
            warn " $prefix Read/Write Permission Denied to Shared Memory Array.\n";
            warn " $prefix Use ipcs -s to list semaphores and look for " .
                "$SHMKEY_hex. If found, shutdown Apache and use ipcrm sem " .
                    "$SHMKEY_hex to remove the colliding (and hopefully " .
                        "unused) semaphore.  See documentation for setProjID " .
                            "for more information. \n";
        }

        return;
    }

    # create (or re-use existing) shared memory segment
    $SHMID = shmget(
                    $SHMKEY,
                    $SHMSIZE,
                    IPC::SysV::IPC_CREAT() |
                    IPC::SysV::S_IRUSR() |
                    IPC::SysV::S_IWUSR()
                   );
    unless (defined $SHMID) {
        warn "$prefix shmget failed - Error $!\n";
        return;
    }

    # make ids accessible to other handlers
    $ENV{AUTH_SEMID} = $SEMID;
    $ENV{AUTH_SHMID} = $SHMID;

    # read shared memory, increment child count and write shared memory
    # segment
    semop($SEMID, $obtain_lock) or warn "$prefix semop failed \n";
    shmread($SHMID, $Cache, 0, $SHMSIZE)
        or warn "$prefix shmread failed \n";
    substr($Cache, index($Cache, "\0")) = '';

    # segment already exists (eg start of additional server)
    my $child_count_new = 1;
    if ($Cache =~ /^(\d+)$;(\d+)\n/) {
        my $time_stamp   = $1;
        my $child_count  = $2;
        $child_count_new = $child_count + 1;
        $Cache =~ s/^$time_stamp$;$child_count\n/$time_stamp$;$child_count_new\n/;
    }
    else {
        # first child => initialize segment
        $Cache = time . "$;$child_count_new\n";
    }
    debug(2, "$prefix child count = $child_count_new");

    shmwrite($SHMID, $Cache, 0, $SHMSIZE)
        or warn "$prefix shmwrite failed \n";
    semop($SEMID, $release_lock) or warn "$prefix semop failed \n";

    1;
}

# The PerlChildExitHandler decrements the child count or destroys the shared
# memory segment upon server shutdown, which is defined by the exit of the
# last child.
# Note: this handler runs in every child server, but not in the main server.
sub childexit {

    my $prefix = "$$ Apache::AuthDBI         PerlChildExitHandler";

    # read Cache from shared memory, decrement child count and exit or write
    #Cache to shared memory
    semop($SEMID, $obtain_lock) or warn "$prefix semop failed \n";
    shmread($SHMID, $Cache, 0, $SHMSIZE)
        or warn "$prefix shmread failed \n";
    substr($Cache, index($Cache, "\0")) = '';
    $Cache =~ /^(\d+)$;(\d+)\n/;

    my $time_stamp  = $1;
    my $child_count = $2;
    my $child_count_new = $child_count - 1;
    if ($child_count_new) {
        debug(2, "$prefix child count = $child_count");

        # write Cache into shared memory
        $Cache =~ s/^$time_stamp$;$child_count\n/$time_stamp$;$child_count_new\n/;
        shmwrite($SHMID, $Cache, 0, $SHMSIZE)
            or warn "$prefix shmwrite failed \n";
        semop($SEMID, $release_lock) or warn "$prefix semop failed \n";
    }
    else {
        # last child
        # remove shared memory segment and semaphore set
        debug(
              2,
              "$prefix child count = $child_count, remove shared memory " .
              "$SHMID and semaphore $SEMID"
             );
        shmctl($SHMID, IPC::SysV::IPC_RMID(), 0)
            or warn "$prefix shmctl failed \n";
        semctl($SEMID, 0, IPC::SysV::IPC_RMID(), 0)
            or warn "$prefix semctl failed \n";
    }

    1;
}

# The PerlCleanupHandler skips through the cache and deletes any outdated 
# entry.
# Note: this handler runs after the response has been sent to the client.
sub cleanup {

    my $prefix = "$$ Apache::AuthDBI         PerlCleanupHandler";
    debug(2, "$prefix");

    # do we keep the cache in shared memory ?
    my $now = time;
    if ($SHMID) {
        semop($SEMID, $obtain_lock) or warn "$prefix semop failed \n";
        shmread($SHMID, $Cache, 0, $SHMSIZE)
            or warn "$prefix shmread failed \n";
        substr($Cache, index($Cache, "\0")) = '';
    }

    # initialize timestamp for CleanupHandler
    my $newCache = "$now$;";
    my ($time_stamp, $child_count);
    foreach my $record (split(/\n/, $Cache)) {
        # first record: timestamp of CleanupHandler and child count
        if (!$time_stamp) {
            ($time_stamp, $child_count) = split /$;/, $record;
            $newCache .= "$child_count\n";
            next;
        }
        my ($id, $last_access, $passwd, $groups) = split /$;/, $record;
        my $diff = $now - $last_access;
        if ($diff >= $CacheTime) {
            debug(2, "$prefix delete >$id<, last access $diff s before");
        }
        else {
            debug(2, "$prefix keep   >$id<, last access $diff s before");
            $newCache .= "$id$;$now$;$passwd$;$groups\n";
        }
    }

    # write Cache to shared memory
    $Cache = $newCache;
    if ($SHMID) {
        shmwrite($SHMID, $Cache, 0, $SHMSIZE)
            or warn "$prefix shmwrite failed \n";
        semop($SEMID, $release_lock) or warn "$prefix semop failed \n";
    }

    1;
}

# Added 06-14-2005 - KAM - Returns SHA1 digest - Modified from PerlCMS' more
# generic routine to remove IO::File requirement
sub SHA1_digest {
    my %params = @_;

    my $prefix = "$$ Apache::AuthDBI         SHA1_digest";
    debug(2, $prefix);

    $params{'format'} ||= "base64";

    my $sha1 = Digest::SHA1->new();

    if ($params{'text'} ne '') {
        $sha1->add($params{'text'});
    }
    else {
        return -1;
    }

    if ($params{'format'} =~ /base64/i) {
        return $sha1->b64digest;
    }
    elsif ($params{'format'} =~ /hex/i) {
        return $sha1->hexdigest;
    }
    elsif ($params{'format'} =~ /binary/i) {
        return $sha1->binary;
    }

    -1;
}

# Added 06-20-2005 - KAM - Returns MD5 digest - Modified from PerlCMS' more 
# generic routine to remove IO::File requirement
sub MD5_digest {
    my %params = @_;

    my $prefix = "$$ Apache::AuthDBI         MD5_digest";
    debug(2, $prefix);

    $params{'format'} ||= "hex";

    my $md5 = Digest::MD5->new();

    if ($params{'text'} ne '') {
        $md5->add($params{'text'});
    }
    else {
        return -1;
    }

    if ($params{'format'} =~ /base64/i) {
        return $md5->b64digest;
    }
    elsif ($params{'format'} =~ /hex/i) {
        return $md5->hexdigest;
    }
    elsif ($params{'format'} =~ /binary/i) {
        return $md5->digest;
    }

    -1;
}

1;

__END__

=head1 NAME

Apache::AuthDBI - Authentication and Authorization via Perl's DBI

=head1 SYNOPSIS

 # Configuration in httpd.conf or startup.pl:

 PerlModule Apache::AuthDBI

 # Authentication and Authorization in .htaccess:

 AuthName DBI
 AuthType Basic

 PerlAuthenHandler Apache::AuthDBI::authen
 PerlAuthzHandler  Apache::AuthDBI::authz

 PerlSetVar Auth_DBI_data_source   dbi:driver:dsn
 PerlSetVar Auth_DBI_username      db_username
 PerlSetVar Auth_DBI_password      db_password
 #DBI->connect($data_source, $username, $password)

 PerlSetVar Auth_DBI_pwd_table     users
 PerlSetVar Auth_DBI_uid_field     username
 PerlSetVar Auth_DBI_pwd_field     password
 # authentication: SELECT pwd_field FROM pwd_table WHERE uid_field=$user
 PerlSetVar Auth_DBI_grp_field     groupname
 # authorization: SELECT grp_field FROM pwd_table WHERE uid_field=$user

 require valid-user
 require user   user_1  user_2 ...
 require group group_1 group_2 ...

The AuthType is limited to Basic. You may use one or more valid require lines.
For a single require line with the requirement 'valid-user' or with the
requirements 'user user_1 user_2 ...' it is sufficient to use only the
authentication handler.

=head1 DESCRIPTION

This module allows authentication and authorization against a database
using Perl's DBI. For supported DBI drivers see:

 http://dbi.perl.org/

Authentication:

For the given username the password is looked up in the cache. If the cache
is not configured or if the user is not found in the cache, or if the given
password does not match the cached password, it is requested from the database.

If the username does not exist and the authoritative directive is set to 'on',
the request is rejected. If the authoritative directive is set to 'off', the
control is passed on to next module in line.

If the password from the database for the given username is empty and the
nopasswd directive is set to 'off', the request is rejected. If the nopasswd
directive is set to 'on', any password is accepted.

Finally the passwords (multiple passwords per userid are allowed) are
retrieved from the database. The result is put into the environment variable
REMOTE_PASSWORDS. Then it is compared to the password given. If the encrypted
directive is set to 'on', the given password is encrypted using perl's crypt()
function before comparison. If the encrypted directive is set to 'off' the
plain-text passwords are compared.

If this comparison fails the request is rejected, otherwise the request is
accepted and the password is put into the environment variable REMOTE_PASSWORD.

The SQL-select used for retrieving the passwords is as follows:

 SELECT pwd_field FROM pwd_table WHERE uid_field = user

If a pwd_whereclause exists, it is appended to the SQL-select.

This module supports in addition a simple kind of logging mechanism. Whenever
the handler is called and a log_string is configured, the log_field will be
updated with the log_string. As log_string - depending upon the database -
macros like TODAY can be used.

The SQL-select used for the logging mechanism is as follows:

 UPDATE pwd_table SET log_field = log_string WHERE uid_field = user

Authorization:

When the authorization handler is called, the authentication has already been
done. This means, that the given username/password has been validated.

The handler analyzes and processes the requirements line by line. The request
is accepted if the first requirement is fulfilled.

In case of 'valid-user' the request is accepted.

In case of one or more user-names, they are compared with the given user-name
until the first match.

In case of one or more group-names, all groups of the given username are
looked up in the cache. If the cache is not configured or if the user is not
found in the cache, or if the requested group does not match the cached group,
the groups are requested from the database. A comma separated list of all
these groups is put into the environment variable REMOTE_GROUPS. Then these
groups are compared with the required groups until the first match.

If there is no match and the authoritative directive is set to 'on' the
request is rejected.

In case the authorization succeeds, the environment variable REMOTE_GROUP is
set to the group name, which can be used by user scripts without accessing
the database again.

The SQL-select used for retrieving the groups is as follows (depending upon
the existence of a grp_table):

 SELECT grp_field FROM pwd_table WHERE uid_field = user
 SELECT grp_field FROM grp_table WHERE uid_field = user

This way the group-information can either be held in the main users table, or
in an extra table, if there is an m:n relationship between users and groups.
From all selected groups a comma-separated list is build, which is compared
with the required groups. If you don't like normalized group records you can
put such a comma-separated list of groups (no spaces) into the grp_field
instead of single groups.

If a grp_whereclause exists, it is appended to the SQL-select.

Cache:

The module maintains an optional cash for all passwords/groups. See the
method setCacheTime(n) on how to enable the cache. Every server has it's
own cache. Optionally the cache can be put into a shared memory segment,
so that it can be shared among all servers. See the CONFIGURATION section
on how to enable the usage of shared memory.

In order to prevent the cache from growing indefinitely a CleanupHandler can
be initialized, which skips through the cache and deletes all outdated entries.
This can be done once per request after sending the response, hence without
slowing down response time to the client. The minimum time between two
successive runs of the CleanupHandler is configurable (see the CONFIGURATION
section). The default is 0, which runs the CleanupHandler after every request.

=head1 LIST OF TOKENS

=over

=item * Auth_DBI_data_source (Authentication and Authorization)

The data_source value has the syntax 'dbi:driver:dsn'. This parameter is
passed to the database driver for processing during connect. The data_source
parameter (as well as the username and the password parameters) may be a
tilde ('~') separated list of several data_sources. All of these triples will
be used until a successful connect is made. This way several backup-servers
can be configured. if you want to use the environment variable DBI_DSN
instead of a data_source, do not specify this parameter at all.

=item * Auth_DBI_username (Authentication and Authorization)

The username argument is passed to the database driver for processing during
connect. This parameter may be a tilde ('~') separated list.
See the data_source parameter above for the usage of a list.

=item * Auth_DBI_password (Authentication and Authorization)

The password argument is passed to the database driver for processing during
connect. This parameter may be a tilde ('~')  separated list.
See the data_source parameter above for the usage of a list.

=item * Auth_DBI_pwd_table (Authentication and Authorization)

Contains at least the fields with the username and the (possibly encrypted)
password. The username should be unique.

=item * Auth_DBI_uid_field (Authentication and Authorization)

Field name containing the username in the Auth_DBI_pwd_table.

=item * Auth_DBI_pwd_field (Authentication only)

Field name containing the password in the Auth_DBI_pwd_table.

=item * Auth_DBI_pwd_whereclause (Authentication only)

Use this option for specifying more constraints to the SQL-select.

=item * Auth_DBI_grp_table (Authorization only)

Contains at least the fields with the username and the groupname.

=item * Auth_DBI_grp_field (Authorization only)

Field-name containing the groupname in the Auth_DBI_grp_table.

=item * Auth_DBI_grp_whereclause (Authorization only)

Use this option for specifying more constraints to the SQL-select.

=item * Auth_DBI_log_field (Authentication only)

Field name containing the log string in the Auth_DBI_pwd_table.

=item * Auth_DBI_log_string (Authentication only)

String to update the Auth_DBI_log_field in the Auth_DBI_pwd_table. Depending
upon the database this can be a macro like 'TODAY'.

=item * Auth_DBI_authoritative  < on / off> (Authentication and Authorization)

Default is 'on'. When set 'on', there is no fall-through to other
authentication methods if the authentication check fails. When this directive
is set to 'off', control is passed on to any other authentication modules. Be
sure you know what you are doing when you decide to switch it off.

=item * Auth_DBI_nopasswd  < on / off > (Authentication only)

Default is 'off'. When set 'on' the password comparison is skipped if the
password retrieved from the database is empty, i.e. allow any password.
This is 'off' by default to ensure that an empty Auth_DBI_pwd_field does not 
allow people to log in with a random password. Be sure you know what you are 
doing when you decide to switch it on.

=item * Auth_DBI_encrypted  < on / off > (Authentication only)

Default is 'on'. When set to 'on', the password retrieved from the database
is assumed to be crypted. Hence the incoming password will be crypted before
comparison. When this directive is set to 'off', the comparison is done
directly with the plain-text entered password.

=item *
Auth_DBI_encryption_method < sha1hex/md5hex/crypt > (Authentication only)

Default is blank. When set to one or more encryption method, the password
retrieved from the database is assumed to be crypted. Hence the incoming
password will be crypted before comparison.  The method supports falling
back so specifying 'sha1hex/md5hex' would allow for a site that is upgrading 
to sha1 to support both methods.  sha1 is the recommended method.

=item * Auth_DBI_encryption_salt < password / userid > (Authentication only)

When crypting the given password AuthDBI uses per default the password
selected from the database as salt. Setting this parameter to 'userid',
the module uses the userid as salt.

=item *
Auth_DBI_uidcasesensitive  < on / off > (Authentication and Authorization)

Default is 'on'. When set 'off', the entered userid is converted to lower case.
Also the userid in the password select-statement is converted to lower case.

=item * Auth_DBI_pwdcasesensitive  < on / off > (Authentication only)

Default is 'on'. When set 'off', the entered password is converted to lower
case.

=item * Auth_DBI_placeholder < on / off > (Authentication and Authorization)

Default is 'off'.  When set 'on', the select statement is prepared using a
placeholder for the username.  This may result in improved performance for
databases supporting this method.

=back

=head1 CONFIGURATION

The module should be loaded upon startup of the Apache daemon.
Add the following line to your httpd.conf:

 PerlModule Apache::AuthDBI

A common usage is to load the module in a startup file via the PerlRequire
directive. See eg/startup.pl for an example.

There are three configurations which are server-specific and which can be done
in a startup file:

 Apache::AuthDBI->setCacheTime(0);

This configures the lifetime in seconds for the entries in the cache.
Default is 0, which turns off the cache. When set to any value n > 0, the
passwords/groups of all users will be cached for at least n seconds. After
finishing the request, a special handler skips through the cache and deletes
all outdated entries (entries, which are older than the CacheTime).

 Apache::AuthDBI->setCleanupTime(-1);

This configures the minimum time in seconds between two successive runs of the
CleanupHandler, which deletes all outdated entries from the cache. The default
is -1, which disables the CleanupHandler. Setting the interval to 0 runs the
CleanupHandler after every request. For a heavily loaded server this should be
set to a value, which reflects a compromise between scanning a large cache
possibly containing many outdated entries and between running many times the
CleanupHandler on a cache containing only few entries.

 Apache::AuthDBI->setProjID(1);

This configures the project ID used to create a semaphore ID for shared memory.
It can be set to any integer 1 to 255 or it will default to a value of 1.

NOTE: This must be set prior to calling initIPC.

If you are running multiple instances of Apache on the same server\
(for example, Apache1 and Apache2), you may not want (or be able) to use
shared memory between them.  In this case, use a different project ID on
each server.

If you are reading this because you suspect you have a permission issue or a
collision with a semaphore, use 'ipcs -s' to list semaphores and look for the
Semaphore ID from the apache error log.  If found, shutdown Apache (all of
them) and use 'ipcrm sem <semaphore key>' to remove the colliding
(and hopefully unused) semaphore.

You may also want to remove any orphaned shared memory segments by using
'ipcs -m' and removing the orphans with ipcrm shm <shared memory id>.

 Apache::AuthDBI->initIPC(50000);

This enables the usage of shared memory for the cache. Instead of every server
maintaining it's own cache, all servers have access to a common cache. This
should minimize the database load considerably for sites running many servers.
The number indicates the size of the shared memory segment in bytes. This size
is fixed, there is no dynamic allocation of more segments. As a rule of thumb
multiply the estimated maximum number of simultaneously cached users by 100 to
get a rough estimate of the needed size. Values below 500 will be overwritten
with the default 50000.

To enable debugging the variable $Apache::AuthDBI::DEBUG must be set. This
can either be done in startup.pl or in the user script. Setting the variable
to 1, just reports about a cache miss. Setting the variable to 2 enables full
debug output.

=head1 PREREQUISITES

=head2 MOD_PERL 2.0

Apache::DBI version 0.96 and should work under mod_perl 2.0 RC5 and later
with httpd 2.0.49 and later.

Apache::DBI versions less than 1.00 are NO longer supported.  Additionally, 
mod_perl versions less then 2.0.0 are NO longer supported.

=head2 MOD_PERL 1.0

Note that this module needs mod_perl-1.08 or higher, apache_1.3.0 or higher
and that mod_perl needs to be configured with the appropriate call-back hooks:

  PERL_AUTHEN=1 PERL_AUTHZ=1 PERL_CLEANUP=1 PERL_STACKED_HANDLERS=1

Apache::DBI v0.94 was the last version before dual mod_perl 2.x support was begun.
It still recommened that you use the latest version of Apache::DBI because Apache::DBI
versions less than 1.00 are NO longer supported.

=head1 SECURITY

In some cases it is more secure not to put the username and the password in
the .htaccess file. The following example shows a solution to this problem:

httpd.conf:

 <Perl>
 my($uid,$pwd) = My::dbi_pwd_fetch();
 $Location{'/foo/bar'}->{PerlSetVar} = [
     [ Auth_DBI_username  => $uid ],
     [ Auth_DBI_password  => $pwd ],
 ];
 </Perl>


=head1 SEE ALSO

L<Apache>, L<mod_perl>, L<DBI>

=head1 AUTHORS

=over

=item *
Apache::AuthDBI by Edmund Mergl; now maintained and supported by the
modperl mailinglist, subscribe by sending mail to 
modperl-subscribe@perl.apache.org.

=item *
mod_perl by Doug MacEachern.

=item *
DBI by Tim Bunce <dbi-users-subscribe@perl.org>

=back

=head1 COPYRIGHT

The Apache::AuthDBI module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
