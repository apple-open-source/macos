dnl Functions to check what database to use for libsasldb

dnl Berkeley DB specific checks first..

dnl this is unbelievably painful due to confusion over what db-3 should be
dnl named and where the db-3 header file is located.  arg.
AC_DEFUN(BERKELEY_DB_CHK_LIB,
[
	BDB_SAVE_LIBS=$LIBS

	if test -d $with_bdb_lib; then
	    LIBS="$LIBS -L$with_bdb_lib"
	    BDB_LIBADD="-L$with_bdb_lib -R $with_bdb_lib"
	else
	    BDB_LIBADD=""
	fi

        for dbname in db-4.0 db4.0 db-4 db-3.3 db3.3 db-3.2 db3.2 db-3.1 db3.1 db-3 db3 db
          do
            AC_CHECK_LIB($dbname, db_create, SASL_DB_LIB="$BDB_LIBADD -l$dbname";
              dblib="berkeley"; break, dblib="no")
          done
        if test "$dblib" = "no"; then
          AC_CHECK_LIB(db, db_open, SASL_DB_LIB="$BDB_LIBADD -ldb";
            dblib="berkeley"; dbname=db,
            dblib="no")
        fi

	LIBS=$BDB_SAVE_LIBS
])

AC_DEFUN(BERKELEY_DB_CHK,
[
	if test -d $with_bdb_inc; then
	    CPPFLAGS="$CPPFLAGS -I$with_bdb_inc"
	    BDB_INCADD="-I$with_bdb_inc"
	else
	    BDB_INCADD=""
	fi

	dnl FreeBSD puts it in a wierd place
	AC_CHECK_HEADER(db3/db.h,
                       BERKELEY_DB_CHK_LIB()
                       if test "$dblib" = "berkeley"; then
			 SASL_DB_INC=$BDB_INCADD
                         AC_DEFINE(HAVE_DB3_DB_H)
                       fi,
               AC_CHECK_HEADER(db.h,
                       	       BERKELEY_DB_CHK_LIB()
			       SASL_DB_INC=$BDB_INCADD,
                               dblib="no"))
])

dnl Figure out what database type we're using
AC_DEFUN(SASL_DB_CHECK, [
cmu_save_LIBS="$LIBS"
AC_ARG_WITH(dblib, [  --with-dblib=DBLIB      set the DB library to use [berkeley] ],
  dblib=$withval,
  dblib=auto_detect)

AC_ARG_WITH(bdb-libdir,
	[  --with-bdb-libdir=DIR   Berkeley DB lib files are in DIR],
	with_bdb_lib=$withval,
	with_bdb_lib=none)
AC_ARG_WITH(bdb-incdir,
	[  --with-bdb-incdir=DIR   Berkeley DB include files are in DIR],
	with_bdb_inc=$withval,
	with_bdb_inc=none)

SASL_DB_LIB=""

case "$dblib" in
dnl this is unbelievably painful due to confusion over what db-3 should be
dnl named.  arg.
  berkeley)
	BERKELEY_DB_CHK()
	;;
  gdbm)
	AC_ARG_WITH(with-gdbm,[  --with-gdbm=PATH        use gdbm from PATH],
                    with_gdbm="${withval}")

        case "$with_gdbm" in
           ""|yes)
               AC_CHECK_HEADER(gdbm.h,
			AC_CHECK_LIB(gdbm, gdbm_open, SASL_DB_LIB="-lgdbm",
                                           dblib="no"),
			dblib="no")
               ;;
           *)
               if test -d $with_gdbm; then
                 CPPFLAGS="${CPPFLAGS} -I${with_gdbm}/include"
                 LDFLAGS="${LDFLAGS} -L${with_gdbm}/lib"
                 SASL_DB_LIB="-lgdbm" 
               else
                 with_gdbm="no"
               fi
       esac
	;;
  ndbm)
	dnl We want to attempt to use -lndbm if we can, just in case
	dnl there's some version of it installed and overriding libc
	AC_CHECK_HEADER(ndbm.h,
			AC_CHECK_LIB(ndbm, dbm_open, SASL_DB_LIB="-lndbm",
				AC_CHECK_FUNC(dbm_open,,dblib="no")),
				dblib="no")
	;;
  auto_detect)
        dnl How about berkeley db?
	BERKELEY_DB_CHK()
	if test "$dblib" = no; then
	  dnl How about ndbm?
	  AC_CHECK_HEADER(ndbm.h, 
		AC_CHECK_LIB(ndbm, dbm_open,
			     dblib="ndbm"; SASL_DB_LIB="-lndbm",
		   	     dblib="weird"),
		   dblib="no")
	  if test "$dblib" = "weird"; then
	    dnl Is ndbm in the standard library?
            AC_CHECK_FUNC(dbm_open, dblib="ndbm", dblib="no")
	  fi

	  if test "$dblib" = no; then
            dnl Can we use gdbm?
   	    AC_CHECK_HEADER(gdbm.h,
		AC_CHECK_LIB(gdbm, gdbm_open, dblib="gdbm";
					     SASL_DB_LIB="-lgdbm", dblib="no"),
  			     dblib="no")
	  fi
	fi
	;;
  none)
	;;
  no)
	;;
  *)
	AC_MSG_WARN([Bad DB library implementation specified;])
	AC_ERROR([Use either \"berkeley\", \"gdbm\", \"ndbm\" or \"none\"])
	dblib=no
	;;
esac
LIBS="$cmu_save_LIBS"

AC_MSG_CHECKING(DB library to use)
AC_MSG_RESULT($dblib)

SASL_DB_BACKEND="db_${dblib}.lo"
SASL_DB_BACKEND_STATIC="../sasldb/db_${dblib}.o ../sasldb/allockey.o"
SASL_DB_UTILS="saslpasswd2 sasldblistusers2"
SASL_DB_MANS="saslpasswd2.8 sasldblistusers2.8"

case "$dblib" in
  gdbm) 
    SASL_MECHS="$SASL_MECHS libsasldb.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/sasldb.o"
    AC_DEFINE(STATIC_SASLDB)
    AC_DEFINE(SASL_GDBM)
    ;;
  ndbm)
    SASL_MECHS="$SASL_MECHS libsasldb.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/sasldb.o"
    AC_DEFINE(STATIC_SASLDB)
    AC_DEFINE(SASL_NDBM)
    ;;
  berkeley)
    SASL_MECHS="$SASL_MECHS libsasldb.la"
    SASL_STATIC_OBJS="$SASL_STATIC_OBJS ../plugins/sasldb.o"
    AC_DEFINE(STATIC_SASLDB)
    AC_DEFINE(SASL_BERKELEYDB)
    ;;
  *)
    AC_MSG_WARN([Disabling SASL authentication database support])
    SASL_DB_BACKEND="db_none.lo"
    SASL_DB_BACKEND_STATIC="../sasldb/db_none.o"
    SASL_DB_UTILS=""
    SASL_DB_MANS=""
    SASL_DB_LIB=""
    ;;
esac
AC_SUBST(SASL_DB_UTILS)
AC_SUBST(SASL_DB_MANS)
AC_SUBST(SASL_DB_BACKEND)
AC_SUBST(SASL_DB_BACKEND_STATIC)
AC_SUBST(SASL_DB_INC)
AC_SUBST(SASL_DB_LIB)
])

dnl Figure out what database path we're using
AC_DEFUN(SASL_DB_PATH_CHECK, [
AC_ARG_WITH(dbpath, [  --with-dbpath=PATH      set the DB path to use [/etc/sasldb2] ],
  dbpath=$withval,
  dbpath=/etc/sasldb2)
AC_MSG_CHECKING(DB path to use)
AC_MSG_RESULT($dbpath)
AC_DEFINE_UNQUOTED(SASL_DB_PATH, "$dbpath")])
